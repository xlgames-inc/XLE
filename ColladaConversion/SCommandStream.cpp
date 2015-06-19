// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SCommandStream.h"
#include "STransformationMachine.h"
#include "SRawGeometry.h"
#include "SAnimation.h"

#include "NascentCommandStream.h"
#include "NascentRawGeometry.h"
#include "NascentAnimController.h"

#include "SkeletonRegistry.h"
#include "Scaffold.h"
#include "ScaffoldParsingUtil.h"    // for AsString
#include "ConversionUtil.h"
#include "../RenderCore/Assets/Material.h"  // for MakeMaterialGuid
#include "../Utility/MemoryUtils.h"
#include "ConversionCore.h"
#include <string>

namespace RenderCore { namespace ColladaConversion
{
    using namespace ::ColladaConversion;

    static std::string SkeletonBindingName(const Node& node);
    static ObjectGuid AsObjectGuid(const Node& node);

    static bool IsUseful(const Node& node, const SkeletonRegistry& skeletonReferences);

///////////////////////////////////////////////////////////////////////////////////////////////////

    void BuildFullSkeleton(
        NascentSkeleton& skeleton,
        const Node& node,
        SkeletonRegistry& skeletonReferences)
    {
        auto nodeId = AsObjectGuid(node);
        auto bindingName = skeletonReferences.GetNode(nodeId)._bindingName;
        if (bindingName.empty()) bindingName = SkeletonBindingName(node);

        unsigned pushCount = PushTransformations(
            skeleton.GetTransformationMachine(),
            node.GetFirstTransform(), bindingName.c_str(),
            skeletonReferences, true);

            // DavidJ -- hack! -- 
            //      When writing a "skeleton" file we need to include all nodes, even those that aren't
            //      referenced within the same file. This is because the node might become an output-interface
            //      node... Maybe there is a better way to do this. Perhaps we could identify which nodes are
            //      output interface transforms / bones... Or maybe we could just include everything when
            //      compiling a skeleton...?
        auto thisOutputMatrix = skeletonReferences.GetOutputMatrixIndex(nodeId);
        skeleton.GetTransformationMachine().MakeOutputMatrixMarker(thisOutputMatrix);
        skeletonReferences.TryRegisterNode(nodeId, bindingName.c_str());

            // note -- also consider instance_nodes?

        auto child = node.GetFirstChild();
        while (child) {
            BuildFullSkeleton(skeleton, child, skeletonReferences);
            child = child.GetNextSibling();
        }

        skeleton.GetTransformationMachine().Pop(pushCount);
    }

    void BuildMinimalSkeleton(
        NascentSkeleton& skeleton,
        const Node& node,
        SkeletonRegistry& skeletonReferences)
    {
        if (!IsUseful(node, skeletonReferences)) return;

        auto nodeId = AsObjectGuid(node);
        auto bindingName = skeletonReferences.GetNode(nodeId)._bindingName;
        if (bindingName.empty()) bindingName = SkeletonBindingName(node);

        unsigned pushCount = PushTransformations(
            skeleton.GetTransformationMachine(),
            node.GetFirstTransform(), bindingName.c_str(),
            skeletonReferences);

        bool isReferenced = skeletonReferences.IsImportant(nodeId);
        if (isReferenced) {
            auto thisOutputMatrix = skeletonReferences.GetOutputMatrixIndex(nodeId);
            skeleton.GetTransformationMachine().MakeOutputMatrixMarker(thisOutputMatrix);
            skeletonReferences.TryRegisterNode(nodeId, bindingName.c_str());
        }

            // note -- also consider instance_nodes?

        auto child = node.GetFirstChild();
        while (child) {
            BuildMinimalSkeleton(skeleton, child, skeletonReferences);
            child = child.GetNextSibling();
        }

        skeleton.GetTransformationMachine().Pop(pushCount);
    }

    void RegisterNodeBindingNames(
        NascentSkeleton& skeleton,
        const SkeletonRegistry& registry)
    {
        auto importantNodesCount = registry.GetImportantNodesCount();
        for (unsigned c=0; c<importantNodesCount; ++c) {
            auto nodeDesc = registry.GetImportantNode(c);
            auto success = skeleton.GetTransformationMachine().TryRegisterJointName(
                nodeDesc._bindingName, nodeDesc._inverseBind, nodeDesc._transformMarker);
            if (!success)
                LogWarning << "Found possible duplicate joint name in transformation machine: " << nodeDesc._bindingName;
        }
    }

    void RegisterNodeBindingNames(
        NascentModelCommandStream& stream,
        const SkeletonRegistry& registry)
    {
        auto importantNodesCount = registry.GetImportantNodesCount();
        for (unsigned c=0; c<importantNodesCount; ++c) {
            auto nodeDesc = registry.GetImportantNode(c);
            stream.RegisterTransformationMachineOutput(
                nodeDesc._bindingName, nodeDesc._id, nodeDesc._transformMarker);
        }
    }

    static auto BuildMaterialTable(
        const InstanceGeometry::MaterialBinding* bindingStart, 
        const InstanceGeometry::MaterialBinding* bindingEnd,
        const std::vector<uint64>& rawGeoBindingSymbols,
        const URIResolveContext& resolveContext)

        -> std::vector<NascentModelCommandStream::MaterialGuid>
    {

            //
            //  For each material referenced in the raw geometry, try to 
            //  match it with a material we've built during collada processing
            //      We have to map it via the binding table in the InstanceGeometry
            //
                        
        using MaterialGuid = NascentModelCommandStream::MaterialGuid;
        auto invalidGuid = NascentModelCommandStream::s_materialGuid_Invalid;

        std::vector<MaterialGuid> materialGuids;
        materialGuids.resize(rawGeoBindingSymbols.size(), invalidGuid);

        for (auto b=bindingStart; b<bindingEnd; ++b) {
            auto hashedSymbol = Hash64(b->_bindingSymbol._start, b->_bindingSymbol._end);

            for (auto i=rawGeoBindingSymbols.cbegin(); i!=rawGeoBindingSymbols.cend(); ++i) {
                if (*i != hashedSymbol) continue;
            
                auto index = std::distance(rawGeoBindingSymbols.cbegin(), i);
                assert(materialGuids[index] == invalidGuid);

                GuidReference ref(b->_reference);
                auto* file = resolveContext.FindFile(ref._fileHash);
                if (file) {
                    const auto* mat = file->FindMaterial(ref._id);
                    if (mat) {
                        materialGuids[index] = 
                            RenderCore::Assets::MakeMaterialGuid(
                                (const char*)mat->_name._start, (const char*)mat->_name._end);
                    }
                }
            }
        }

        return std::move(materialGuids);
    }

    NascentModelCommandStream::GeometryInstance InstantiateGeometry(
        const ::ColladaConversion::InstanceGeometry& instGeo,
        const ::ColladaConversion::Node& attachedNode,
        const URIResolveContext& resolveContext,
        NascentGeometryObjects& objects,
        SkeletonRegistry& nodeRefs,
        const ImportConfiguration& cfg)
    {
        GuidReference refGuid(instGeo._reference);
        ObjectGuid geoId(refGuid._id, refGuid._fileHash);
        auto geo = objects.GetGeo(geoId);
        if (geo == ~unsigned(0x0)) {
            auto* scaffoldGeo = FindElement(refGuid, resolveContext, &IDocScopeIdResolver::FindMeshGeometry);
            if (!scaffoldGeo)
                Throw(::Assets::Exceptions::FormatError("Could not found geometry object to instantiate (%s)",
                    AsString(instGeo._reference).c_str()));
            objects._rawGeos.push_back(std::make_pair(geoId, Convert(*scaffoldGeo, resolveContext, cfg)));
            geo = (unsigned)(objects._rawGeos.size()-1);
        }

        auto materials = BuildMaterialTable(
            AsPointer(instGeo._matBindings.cbegin()), AsPointer(instGeo._matBindings.cend()),
            objects._rawGeos[geo].second._matBindingSymbols, resolveContext);

        auto bindingMatIndex = nodeRefs.GetOutputMatrixIndex(AsObjectGuid(attachedNode));
        nodeRefs.TryRegisterNode(AsObjectGuid(attachedNode), SkeletonBindingName(attachedNode).c_str());

        return NascentModelCommandStream::GeometryInstance(
            geo, bindingMatIndex, std::move(materials), 0);
    }

    DynamicArray<uint16> BuildJointArray(
        const GuidReference skeletonRef,
        const UnboundSkinController& unboundController,
        const URIResolveContext& resolveContext,
        SkeletonRegistry& nodeRefs)
    {
            // Build the joints array for the given controller instantiation
            // the <instance_controller> references a skeleton, which contains
            // nodes that map on to the joints array in the <controller>
            // We want to find the node guids for each of those objects, can then
            // build transformation machine output indices for each of them.
        auto skeleton = FindElement(skeletonRef, resolveContext, &IDocScopeIdResolver::FindNode);
        if (!skeleton) return DynamicArray<uint16>();

        const auto& invBindMats = unboundController._inverseBindMatrices;

            // data is stored in xml list format, with whitespace deliminated elements
            // there should be an <accessor> that describes how to read this list
            // But we're going to assume it just contains a single entry like:
            //      <param name="JOINT" type="name"/>
        const auto& jointNames = unboundController._jointNames;
        auto count = unboundController._jointNames.size();
        DynamicArray<uint16> result(std::make_unique<uint16[]>(count), count);
        for (unsigned c=0; c<count; ++c) {
            Node node = skeleton.FindBySid(AsPointer(jointNames[c].cbegin()), AsPointer(jointNames[c].cend()));
            if (node) {
                auto bindingMatIndex = nodeRefs.GetOutputMatrixIndex(AsObjectGuid(node));
                nodeRefs.TryRegisterNode(AsObjectGuid(node), SkeletonBindingName(node).c_str());

                result[c] = (uint16)bindingMatIndex;
            } else {
                result[c] = (uint16)~uint16(0);
            }

            if (c < invBindMats.size())
                nodeRefs.AttachInverseBindMatrix(
                    AsObjectGuid(node), invBindMats[c]);
        }

        return std::move(result);
    }

    NascentModelCommandStream::SkinControllerInstance InstantiateController(
        const ::ColladaConversion::InstanceController& instGeo,
        const ::ColladaConversion::Node& attachedNode,
        const URIResolveContext& resolveContext,
        NascentGeometryObjects& objects,
        SkeletonRegistry& nodeRefs,
        const ImportConfiguration& cfg)
    {
        GuidReference controllerRef(instGeo._reference);
        ObjectGuid controllerId(controllerRef._id, controllerRef._fileHash);
        auto* scaffoldController = FindElement(controllerRef, resolveContext, &IDocScopeIdResolver::FindSkinController);
        if (!scaffoldController)
            Throw(::Assets::Exceptions::FormatError("Could not find controller object to instantiate (%s)",
                AsString(instGeo._reference).c_str()));

        auto controller = Convert(*scaffoldController, resolveContext, cfg);

            // If the the raw geometry object is already converted, then we should use it. Otherwise
            // we need to do the conversion (but store it only in a temporary -- we don't need to
            // write it to disk)
        NascentRawGeometry* source = nullptr;
        NascentRawGeometry tempBuffer;
        {
            auto geo = objects.GetGeo(controller._sourceRef);
            if (geo == ~unsigned(0x0)) {
                auto* scaffoldGeo = FindElement(
                    GuidReference(controller._sourceRef._objectId, controller._sourceRef._fileId),
                    resolveContext, &IDocScopeIdResolver::FindMeshGeometry);
                if (!scaffoldGeo)
                    Throw(::Assets::Exceptions::FormatError("Could not find geometry object to instantiate (%s)",
                        AsString(instGeo._reference).c_str()));
                tempBuffer = Convert(*scaffoldGeo, resolveContext, cfg);
                source = &tempBuffer;
            } else {
                source = &objects._rawGeos[geo].second;
            }
        }

        auto jointMatrices = BuildJointArray(instGeo.GetSkeleton(), controller, resolveContext, nodeRefs);

        auto materials = BuildMaterialTable(
            AsPointer(instGeo._matBindings.cbegin()), AsPointer(instGeo._matBindings.cend()),
            source->_matBindingSymbols, resolveContext);

        objects._skinnedGeos.push_back(
            std::make_pair(
                controllerId,
                BindController(
                    *source, controller, std::move(jointMatrices),
                    AsString(instGeo._reference).c_str())));

        auto bindingMatIndex = nodeRefs.GetOutputMatrixIndex(AsObjectGuid(attachedNode));
        nodeRefs.TryRegisterNode(AsObjectGuid(attachedNode), SkeletonBindingName(attachedNode).c_str());

        return NascentModelCommandStream::SkinControllerInstance(
            (unsigned)(objects._skinnedGeos.size()-1), 
            bindingMatIndex, std::move(materials), 0);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    unsigned NascentGeometryObjects::GetGeo(ObjectGuid id)
    {
        for (const auto& i:_rawGeos)
            if (i.first == id) return unsigned(&i - AsPointer(_rawGeos.cbegin()));
        return ~0u;
    }

   unsigned NascentGeometryObjects::GetSkinnedGeo(ObjectGuid id)
    {
        for (const auto& i:_skinnedGeos)
            if (i.first == id) return unsigned(&i - AsPointer(_skinnedGeos.cbegin()));
        return ~0u;
    }

    std::pair<Float3, Float3> NascentGeometryObjects::CalculateBoundingBox
        (
            const NascentModelCommandStream& scene,
            const Float4x4* transformsBegin, 
            const Float4x4* transformsEnd
        )
    {
            //
            //      For all the parts of the model, calculate the bounding box.
            //      We just have to go through each vertex in the model, and
            //      transform it into model space, and calculate the min and max values
            //      found;
            //
        using namespace ColladaConversion;
        auto result = InvalidBoundingBox();
        // const auto finalMatrices = 
        //     _skeleton.GetTransformationMachine().GenerateOutputTransforms(
        //         _animationSet.BuildTransformationParameterSet(0.f, nullptr, _skeleton, _objects));

            //
            //      Do the unskinned geometry first
            //

        for (auto i=scene._geometryInstances.cbegin(); i!=scene._geometryInstances.cend(); ++i) {
            const NascentModelCommandStream::GeometryInstance& inst = *i;

            if (inst._id >= _rawGeos.size()) continue;
            const auto* geo = &_rawGeos[inst._id].second;

            Float4x4 localToWorld = Identity<Float4x4>();
            if ((transformsBegin + inst._localToWorldId) < transformsEnd)
                localToWorld = *(transformsBegin + inst._localToWorldId);

            const void*         vertexBuffer = geo->_vertices.get();
            const unsigned      vertexStride = geo->_mainDrawInputAssembly._vertexStride;

            Metal::InputElementDesc positionDesc = FindPositionElement(
                AsPointer(geo->_mainDrawInputAssembly._vertexInputLayout.begin()),
                geo->_mainDrawInputAssembly._vertexInputLayout.size());

            if (positionDesc._nativeFormat != Metal::NativeFormat::Unknown && vertexStride) {
                AddToBoundingBox(
                    result, vertexBuffer, vertexStride, 
                    geo->_vertices.size() / vertexStride, positionDesc, localToWorld);
            }
        }

            //
            //      Now also do the skinned geometry. But use the default pose for
            //      skinned geometry (ie, don't apply the skinning transforms to the bones).
            //      Obvious this won't give the correct result post-animation.
            //

        for (auto i=scene._skinControllerInstances.cbegin(); i!=scene._skinControllerInstances.cend(); ++i) {
            const NascentModelCommandStream::SkinControllerInstance& inst = *i;

            if (inst._id >= _skinnedGeos.size()) continue;
            const auto* controller = &_skinnedGeos[inst._id].second;
            if (!controller) continue;

            Float4x4 localToWorld = Identity<Float4x4>();
            if ((transformsBegin + inst._localToWorldId) < transformsEnd)
                localToWorld = *(transformsBegin + inst._localToWorldId);

                //  We can't get the vertex position data directly from the vertex buffer, because
                //  the "bound" object is already using an opaque hardware object. However, we can
                //  transform the local space bounding box and use that.

            const unsigned indices[][3] = 
            {
                {0,0,0}, {0,1,0}, {1,0,0}, {1,1,0},
                {0,0,1}, {0,1,1}, {1,0,1}, {1,1,1}
            };

            const Float3* A = (const Float3*)&controller->_localBoundingBox.first;
            for (unsigned c=0; c<dimof(indices); ++c) {
                Float3 position(A[indices[c][0]][0], A[indices[c][1]][1], A[indices[c][2]][2]);
                AddToBoundingBox(result, position, localToWorld);
            }
        }

        return result;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::string SkeletonBindingName(const Node& node)    { return AsString(node.GetName()); }
    static ObjectGuid AsObjectGuid(const Node& node)            { return node.GetId().GetHash(); }

    static bool IsUseful(const Node& node, const SkeletonRegistry& skeletonReferences)
    {
        if (skeletonReferences.IsImportant(AsObjectGuid(node))) return true;

        auto child = node.GetFirstChild();
        while (child) {
            if (IsUseful(child, skeletonReferences)) return true;
            child = child.GetNextSibling();
        }
        return false;
    }

}}

