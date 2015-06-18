// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SCommandStream.h"
#include "STransformationMachine.h"
#include "NascentCommandStream.h"
#include "NascentRawGeometry.h"
#include "NascentAnimController.h"
#include "SkeletonRegistry.h"
#include "Scaffold.h"
#include "ScaffoldParsingUtil.h"    // for AsString
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

    void BuildSkeleton(
        NascentSkeleton& skeleton,
        const Node& node,
        SkeletonRegistry& skeletonReferences)
    {
        // if (!IsUseful(node, skeletonReferences)) return;

        auto nodeId = AsObjectGuid(node);
        auto bindingName = skeletonReferences.GetBindingName(nodeId);
        if (bindingName.empty()) bindingName = SkeletonBindingName(node);

        unsigned pushCount = PushTransformations(
            skeleton.GetTransformationMachine(),
            node.GetFirstTransform(), bindingName.c_str(),
            skeletonReferences);

            //
            //      We have to assume we need an output matrix. We don't really know
            //      which nodes need output matrices at this point (because we haven't 
            //      got all the downstream skinning data). So, let's just assume it's needed.
            //
        bool isReferenced = skeletonReferences.IsImportant(nodeId);

            // DavidJ -- hack! -- When writing a "skeleton" we need to include all nodes, even those that aren't
            //              referenced within the same file. This is because the node might become an output-interface
            //              node... Maybe there is a better way to do this. Perhaps we could identify which nodes are
            //              output interface transforms / bones... Or maybe we could just include everything when
            //              compiling a skeleton...?
        isReferenced = true;
        if (isReferenced) {
            // unsigned thisOutputMatrix = skeleton.GetTransformationMachine().GetOutputMatrixMarker();
            // skeletonReferences.SetOutputMatrix(nodeId, thisOutputMatrix);

            auto thisOutputMatrix = skeletonReferences.GetOutputMatrixIndex(nodeId);
            skeleton.GetTransformationMachine().MakeOutputMatrixMarker(thisOutputMatrix);

                //
                //      (We can't instantiate the skin controllers yet, because we can't be sure
                //          we've already parsed the skeleton nodes)
                //      But we can write a tag to find the output matrix later
                //          (we also need a tag for all nodes with instance controllers in them)
                //

            Float4x4 inverseBind;
            auto* inverseBindP = skeletonReferences.GetInverseBindMatrix(nodeId);
            if (inverseBindP) inverseBind = *inverseBindP;
            else inverseBind = Identity<Float4x4>();

                // note -- there may be problems here, because the "name" of the node isn't necessarily
                //          unique. There are unique ids in collada, however. We some some unique identifier
                //          can can be seen in Max, and can be used to associate different files with shared
                //          references (eg, animations, skeletons and skins in separate files)
            if (!bindingName.empty() || inverseBindP) {
                bool success = skeleton.GetTransformationMachine().TryRegisterJointName(
                    bindingName, inverseBind, thisOutputMatrix);
                if (!success)
                    LogWarning << "Found possible duplicate joint name in transformation machine: " << bindingName;
            }
        }

            // note -- also consider instance_nodes?

        auto child = node.GetFirstChild();
        while (child) {
            BuildSkeleton(skeleton, child, skeletonReferences);
            child = child.GetNextSibling();
        }

        skeleton.GetTransformationMachine().Pop(pushCount);
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
        TableOfObjects& accessableObjects,
        SkeletonRegistry& nodeRefs)
    {
        GuidReference refGuid(instGeo._reference);
        auto* geo = accessableObjects.Get<NascentRawGeometry>(ObjectGuid(refGuid._id, refGuid._fileHash));
        if (!geo)
            Throw(::Assets::Exceptions::FormatError("Could not found geometry object to instantiate (%s)",
                AsString(instGeo._reference).c_str()));

        auto materials = BuildMaterialTable(
            AsPointer(instGeo._matBindings.cbegin()), AsPointer(instGeo._matBindings.cend()),
            geo->_matBindingSymbols, resolveContext);

        auto bindingMatIndex = nodeRefs.GetOutputMatrixIndex(AsObjectGuid(attachedNode));
        nodeRefs.TryRegisterNode(AsObjectGuid(attachedNode), SkeletonBindingName(attachedNode).c_str());

        // auto bindingMatIndex = stream.RegisterTransformationMachineOutput(
        //     SkeletonBindingName(attachedNode),
        //     AsObjectGuid(attachedNode));

        return NascentModelCommandStream::GeometryInstance(
            accessableObjects.GetIndex<NascentRawGeometry>(ObjectGuid(refGuid._id, refGuid._fileHash)), 
            bindingMatIndex, std::move(materials), 0);
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
                // auto bindingMatIndex = stream.RegisterTransformationMachineOutput(
                //     SkeletonBindingName(node),
                //     AsObjectGuid(node));
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
        TableOfObjects& accessableObjects,
        SkeletonRegistry& nodeRefs)
    {
        GuidReference controllerRef(instGeo._reference);
        ObjectGuid controllerId(controllerRef._id, controllerRef._fileHash);
        auto* controller = accessableObjects.Get<UnboundSkinController>(controllerId);
        if (!controller)
            Throw(::Assets::Exceptions::FormatError("Could not find controller object to instantiate (%s)",
                AsString(instGeo._reference).c_str()));

        auto* source = accessableObjects.Get<NascentRawGeometry>(controller->_sourceRef);
        if (!source)
            Throw(::Assets::Exceptions::FormatError("Could not find geometry object to instantiate (%s)",
                AsString(instGeo._reference).c_str()));

        auto jointMatrices = BuildJointArray(instGeo.GetSkeleton(), *controller, resolveContext, nodeRefs);

        auto materials = BuildMaterialTable(
            AsPointer(instGeo._matBindings.cbegin()), AsPointer(instGeo._matBindings.cend()),
            source->_matBindingSymbols, resolveContext);

        auto result = BindController(
            *source, *controller, accessableObjects, accessableObjects,
            std::move(jointMatrices),
            AsString(instGeo._reference).c_str());

        auto desc = accessableObjects.GetDesc<UnboundSkinController>(controllerId);
        accessableObjects.Add(
            controllerId,
            std::get<0>(desc), std::get<1>(desc),
            std::move(result));

        // auto bindingMatIndex = stream.RegisterTransformationMachineOutput(
        //     SkeletonBindingName(attachedNode),
        //     AsObjectGuid(attachedNode));
        // nodeRefs.SetOutputMatrixIndex(AsObjectGuid(attachedNode), bindingMatIndex);

        auto bindingMatIndex = nodeRefs.GetOutputMatrixIndex(AsObjectGuid(attachedNode));
        nodeRefs.TryRegisterNode(AsObjectGuid(attachedNode), SkeletonBindingName(attachedNode).c_str());

        return NascentModelCommandStream::SkinControllerInstance(
            accessableObjects.GetIndex<NascentBoundSkinnedGeometry>(controllerId), 
            bindingMatIndex, std::move(materials), 0);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::string SkeletonBindingName(const Node& node)
    {
        return AsString(node.GetName());
    }

    static ObjectGuid AsObjectGuid(const Node& node)
    {
        return node.GetId().GetHash();
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

}}

