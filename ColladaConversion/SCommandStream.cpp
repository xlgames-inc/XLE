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
#include "../Utility/StringFormat.h"
#include "ConversionCore.h"
#include <string>

namespace RenderCore { namespace ColladaConversion
{
    using namespace ::ColladaConversion;

    static std::string  SkeletonBindingName(const Node& node);
    static ObjectGuid   AsObjectGuid(const Node& node);
    static bool         IsUseful(const Node& node, const SkeletonRegistry& skeletonReferences);

///////////////////////////////////////////////////////////////////////////////////////////////////

    void BuildSkeleton(
        NascentSkeleton& skeleton,
        const Node& node,
        SkeletonRegistry& skeletonReferences,
        int ignoreTransforms, bool fullSkeleton)
    {
        if (!fullSkeleton && !IsUseful(node, skeletonReferences)) return;

        auto nodeId = AsObjectGuid(node);
        auto bindingName = skeletonReferences.GetNode(nodeId)._bindingName;
        if (bindingName.empty()) bindingName = SkeletonBindingName(node);

        auto pushCount = 0u;
        if (ignoreTransforms <= 0) {
                // Sometimes we will ignore the top most transform(s)
                // so ignoreTransforms is a countdown on the depth of
                // the node in the hierarchy.
            pushCount = PushTransformations(
                skeleton.GetTransformationMachine(),
                node.GetFirstTransform(), bindingName.c_str(),
                skeletonReferences, fullSkeleton);
        }

        bool isReferenced = fullSkeleton || skeletonReferences.IsImportant(nodeId);
        if (isReferenced) {
                // (prevent a reference if the transformation machine is completely empty)
            if (!skeleton.GetTransformationMachine().IsEmpty()) {
                auto thisOutputMatrix = skeletonReferences.GetOutputMatrixIndex(nodeId);
                skeleton.GetTransformationMachine().MakeOutputMatrixMarker(thisOutputMatrix);
                skeletonReferences.TryRegisterNode(nodeId, bindingName.c_str());
            }
        }

            // note -- also consider instance_nodes?

        auto child = node.GetFirstChild();
        while (child) {
            BuildSkeleton(skeleton, child, skeletonReferences, ignoreTransforms-1, fullSkeleton);
            child = child.GetNextSibling();
        }

        skeleton.GetTransformationMachine().Pop(pushCount);
    }

    void ReferencedGeometries::Gather(
        const ::ColladaConversion::Node& node,
        SkeletonRegistry& nodeRefs)
    {
            // Just collect all of the instanced geometries and instanced controllers
            // that hang off this node (or any children).
        bool gotAttachment = false;
        auto nodeAsGuid = AsObjectGuid(node);
        const auto& scene = node.GetScene();
        for (unsigned c=0; c<scene.GetInstanceGeometryCount(); ++c)
            if (scene.GetInstanceGeometry_Attach(c).GetIndex() == node.GetIndex()) {
                _meshes.push_back(AttachedObject{nodeRefs.GetOutputMatrixIndex(nodeAsGuid), c});
                gotAttachment = true;
            }

        for (unsigned c=0; c<scene.GetInstanceControllerCount(); ++c)
            if (scene.GetInstanceController_Attach(c).GetIndex() == node.GetIndex()) {
                _skinControllers.push_back(AttachedObject{nodeRefs.GetOutputMatrixIndex(nodeAsGuid), c});
                gotAttachment = true;
            }

            // Register the names with attachments node -- early on
            // Note that if we get a resolve failure (or compile failure) then the
            // node will remain registered in the skeleton
        if (gotAttachment)
            nodeRefs.TryRegisterNode(nodeAsGuid, SkeletonBindingName(node).c_str());

        auto child = node.GetFirstChild();
        while (child) {
            Gather(child, nodeRefs);
            child = child.GetNextSibling();
        }
    }

    void RegisterNodeBindingNames(
        NascentSkeleton& skeleton,
        const SkeletonRegistry& registry)
    {
        for (const auto& nodeDesc:registry.GetImportantNodes()) {
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
        for (const auto& nodeDesc:registry.GetImportantNodes())
            stream.RegisterTransformationMachineOutput(
                nodeDesc._bindingName, nodeDesc._id, nodeDesc._transformMarker);
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

                auto newMaterialGuid = invalidGuid;

                GuidReference ref(b->_reference);
                auto* file = resolveContext.FindFile(ref._fileHash);
                if (file) {
                    const auto* mat = file->FindMaterial(ref._id);
                    if (mat) {
                        newMaterialGuid = 
                            RenderCore::Assets::MakeMaterialGuid(mat->_name._start, mat->_name._end);
                    }
                }


                if (materialGuids[index] != invalidGuid && materialGuids[index] != newMaterialGuid) {

                        // Some collada files can actually have multiple instance_material elements for
                        // the same binding symbol. Let's throw an exception in this case (but only
                        // if the bindings don't agree)
                    Throw(::Assets::Exceptions::FormatError("Single material binding symbol is bound to multiple different materials in geometry instantiation"));
                }

                materialGuids[index] = newMaterialGuid;
            }
        }

        return std::move(materialGuids);
    }

    NascentModelCommandStream::GeometryInstance InstantiateGeometry(
        const ::ColladaConversion::InstanceGeometry& instGeo,
        unsigned outputTransformIndex, const Float4x4& mergedTransform,
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

            auto convertedMesh = Convert(*scaffoldGeo, mergedTransform, resolveContext, cfg);
            if (convertedMesh._mainDrawCalls.empty()) {
                    
                    // everything else should be empty as well...
                assert(convertedMesh._vertices.empty());
                assert(convertedMesh._indices.empty());
                assert(convertedMesh._matBindingSymbols.empty());
                assert(convertedMesh._unifiedVertexIndexToPositionIndex.empty());
                
                Throw(::Assets::Exceptions::FormatError(
                    "Geometry object is empty (%s)", AsString(instGeo._reference).c_str()));
            }

            objects._rawGeos.push_back(std::make_pair(geoId, std::move(convertedMesh)));
            geo = (unsigned)(objects._rawGeos.size()-1);
        }
        
        auto materials = BuildMaterialTable(
            AsPointer(instGeo._matBindings.cbegin()), AsPointer(instGeo._matBindings.cend()),
            objects._rawGeos[geo].second._matBindingSymbols, resolveContext);

        return NascentModelCommandStream::GeometryInstance(
            geo, outputTransformIndex, std::move(materials), 0);
    }

    static DynamicArray<uint16> BuildJointArray(
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
            
            auto compareSection = MakeStringSection(jointNames[c]);
            Node node = skeleton.FindBreadthFirst(
                [&compareSection](const Node& compare) { return XlEqString(compare.GetSid(), compareSection); });

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
        unsigned outputTransformIndex,
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

        auto jointMatrices = BuildJointArray(instGeo.GetSkeleton(), controller, resolveContext, nodeRefs);
        if (!jointMatrices.size() || !jointMatrices.get())
            Throw(::Assets::Exceptions::FormatError("Skin controller object has no joints. Cannot instantiate as skinned object. (%s)",
                AsString(instGeo._reference).c_str()));

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
                tempBuffer = Convert(*scaffoldGeo, Identity<Float4x4>(), resolveContext, cfg);
                source = &tempBuffer;
            } else {
                source = &objects._rawGeos[geo].second;
            }
        }

        auto materials = BuildMaterialTable(
            AsPointer(instGeo._matBindings.cbegin()), AsPointer(instGeo._matBindings.cend()),
            source->_matBindingSymbols, resolveContext);

        objects._skinnedGeos.push_back(
            std::make_pair(
                controllerId,
                BindController(
                    *source, controller, std::move(jointMatrices),
                    AsString(instGeo._reference).c_str())));

        return NascentModelCommandStream::SkinControllerInstance(
            (unsigned)(objects._skinnedGeos.size()-1), 
            outputTransformIndex, std::move(materials), 0);
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
            IteratorRange<const Float4x4*> transforms
        ) const
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
            if (inst._localToWorldId < transforms.size())
                localToWorld = transforms[inst._localToWorldId];

            const void*         vertexBuffer = geo->_vertices.get();
            const unsigned      vertexStride = geo->_mainDrawInputAssembly._vertexStride;

            auto positionDesc = FindPositionElement(
                AsPointer(geo->_mainDrawInputAssembly._elements.begin()),
                geo->_mainDrawInputAssembly._elements.size());

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
            if (inst._localToWorldId < transforms.size())
                localToWorld = transforms[inst._localToWorldId];

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

        assert(!isinf(result.first[0]) && !isinf(result.first[1]) && !isinf(result.first[2]));
        assert(!isinf(result.second[0]) && !isinf(result.second[1]) && !isinf(result.second[2]));

        return result;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::string SkeletonBindingName(const Node& node)    
    {
            // Both "name" and "id" are optional. Let's prioritize "name"
            //  -- if it exists. If there is no name, we'll fall back to "id"
        if (node.GetName()._end > node.GetName()._start)
            return AsString(node.GetName()); 
        if (!node.GetId().IsEmpty())
            return AsString(node.GetId().GetOriginal());
        return XlDynFormatString("Unnamed%i", (unsigned)node.GetIndex());
    }

    static ObjectGuid AsObjectGuid(const Node& node)
    { 
        if (!node.GetId().IsEmpty())
            return node.GetId().GetHash(); 
        if (!node.GetName().Empty())
            return Hash64(node.GetName().begin(), node.GetName().end());

        // If we have no name & no id -- it is truly anonymous. 
        // We can just use the index of the node, it's the only unique
        // thing we have.
        return ObjectGuid(node.GetIndex());
    }

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

///////////////////////////////////////////////////////////////////////////////////////////////////

    #include "../Utility/ExposeStreamOp.h"

    std::ostream& operator<<(std::ostream& stream, const NascentGeometryObjects& geos)
    {
        using namespace Operators;

        stream << " --- Geos:" << std::endl;
        unsigned c=0;
        for (const auto& g:geos._rawGeos)
            stream << "[" << c++ << "] (0x" << std::hex << g.first._objectId << std::dec << ") Geo --- " << std::endl << g.second << std::endl;

        stream << " --- Skinned Geos:" << std::endl;
        c=0;
        for (const auto& g:geos._skinnedGeos)
            stream << "[" << c++ << "] (0x" << std::hex << g.first._objectId << std::dec << ") Skinned geo --- " << std::endl << g.second << std::endl;
        return stream;
    }

}}

