// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SCommandStream.h"
#include "STransformationMachine.h"
#include "SRawGeometry.h"
#include "SAnimation.h"

#include "Scaffold.h"
#include "ScaffoldParsingUtil.h"

#include "../RenderCore/GeoProc/NascentCommandStream.h"
#include "../RenderCore/GeoProc/NascentRawGeometry.h"
#include "../RenderCore/GeoProc/NascentAnimController.h"
#include "../RenderCore/GeoProc/NascentGeometryObjects.h"
#include "../RenderCore/GeoProc/SkeletonRegistry.h"
#include "../RenderCore/GeoProc/GeoProcUtil.h"

#include "../RenderCore/Assets/MaterialScaffold.h"  // for MakeMaterialGuid
#include "../RenderCore/Format.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringFormat.h"
#include <string>

namespace ColladaConversion
{
    std::vector<std::basic_string<utf8>> GetJointNames(const SkinController& controller, const URIResolveContext& resolveContext);

    static std::string  SkeletonBindingName(const Node& node);
    static NascentObjectGuid   AsObjectGuid(const Node& node);
    static bool         IsUseful(const Node& node, const SkeletonRegistry& skeletonReferences);

///////////////////////////////////////////////////////////////////////////////////////////////////

    struct LODDesc
    {
    public:
        unsigned                _lod;
        bool                    _isLODRoot;
        StringSection<utf8>     _remainingName;
    };

    static LODDesc GetLevelOfDetail(const ::ColladaConversion::Node& node)
    {
        // We're going assign a level of detail to this node based on naming conventions. We'll
        // look at the name of the node (rather than the name of the geometry object) and look
        // for an indicator that it includes a LOD number.
        // We're looking for something like "$lod" or "_lod". This should be followed by an integer,
        // and with an underscore following.
        if (    XlBeginsWithI(node.GetName(), MakeStringSection(u("_lod")))
            ||  XlBeginsWithI(node.GetName(), MakeStringSection(u("$lod")))) {

            auto nextSection = MakeStringSection(node.GetName().begin()+4, node.GetName().end());
            uint32 lod = 0;
            auto* parseEnd = FastParseElement(lod, nextSection.begin(), nextSection.end());
            if (parseEnd < nextSection.end() && *parseEnd == '_')
                return LODDesc { lod, true, MakeStringSection(parseEnd+1, node.GetName().end()) };
            Log(Warning) << "Node name (" << Conversion::Convert<std::string>(node.GetName().AsString()) << ") looks like it contains a lod index, but parse failed. Defaulting to lod 0." << std::endl;
        }
        return LODDesc { 0, false, StringSection<utf8>() };
    }


///////////////////////////////////////////////////////////////////////////////////////////////////

    static void BuildSkeleton(
        NascentSkeleton& skeleton,
        const Node& node,
        SkeletonRegistry& skeletonReferences,
        int ignoreTransforms, bool terminateOnLODNodes, bool fullSkeleton)
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
                skeleton.GetSkeletonMachine(),
				skeleton.GetInterface(),
				skeleton.GetDefaultParameters(),
                node.GetFirstTransform(), bindingName.c_str(),
                skeletonReferences, fullSkeleton);
        }

        bool isReferenced = fullSkeleton || skeletonReferences.IsImportant(nodeId);
        if (isReferenced) {
                // (prevent a reference if the transformation machine is completely empty)
            if (!skeleton.GetSkeletonMachine().IsEmpty()) {
				uint32 outputMarker = ~0u;
				if (skeleton.GetInterface().TryRegisterJointName(outputMarker, MakeStringSection(bindingName))) {
					skeleton.GetSkeletonMachine().WriteOutputMarker(outputMarker);
					skeletonReferences.TryRegisterNode(nodeId, bindingName.c_str());
				} else {
					Throw(::Exceptions::BasicLabel("Couldn't register joint name in skeleton interface for node (%s)", bindingName.c_str()));
				}
            }
        }

            // note -- also consider instance_nodes?

        auto child = node.GetFirstChild();
        while (child) {
            if (terminateOnLODNodes && GetLevelOfDetail(child)._isLODRoot) { child = child.GetNextSibling(); continue; }

            BuildSkeleton(skeleton, child, skeletonReferences, ignoreTransforms-1, terminateOnLODNodes, fullSkeleton);
            child = child.GetNextSibling();
        }

        skeleton.GetSkeletonMachine().Pop(pushCount);
    }

    void BuildSkeleton(
        NascentSkeleton& skeleton,
        const ::ColladaConversion::Node& sceneRoot,
        StringSection<utf8> rootNode,
        SkeletonRegistry& skeletonReferences,
        bool fullSkeleton)
    {
        if (rootNode.IsEmpty()) {
            BuildSkeleton(skeleton, sceneRoot, skeletonReferences, 0, false, fullSkeleton);
        } else {
            // Scan through look for a node that matches the name exactly, or is an 
            // LOD of that node.
            auto roots = sceneRoot.FindAllBreadthFirst(
                [rootNode](const Node& n)
                {
                    if (XlEqString(n.GetName(), rootNode)) return true;
                    auto desc = GetLevelOfDetail(n);
                    return desc._isLODRoot && XlEqString(desc._remainingName, rootNode);
                });

            for (const auto&r:roots)
                BuildSkeleton(skeleton, r, skeletonReferences, 1, true, fullSkeleton);
        }
    }

    bool ReferencedGeometries::Gather(
        const ::ColladaConversion::Node& sceneRoot,
        StringSection<utf8> rootNode,
        SkeletonRegistry& nodeRefs)
    {
        if (rootNode.IsEmpty()) {
            Gather(sceneRoot, nodeRefs);
            return true;
        } else {
            // Scan through look for a node that matches the name exactly, or is an 
            // LOD of that node.
            auto roots = sceneRoot.FindAllBreadthFirst(
                [rootNode](const Node& n)
                {
                    if (XlEqString(n.GetName(), rootNode)) return true;
                    auto desc = GetLevelOfDetail(n);
                    return desc._isLODRoot && XlEqString(desc._remainingName, rootNode);
                });
            if (roots.empty()) return false;

            for (const auto&r:roots)
                Gather(r, nodeRefs, true);
            return true;
        }
    }

    void ReferencedGeometries::Gather(
        const ::ColladaConversion::Node& node,
        SkeletonRegistry& nodeRefs,
        bool terminateOnLODNodes)
    {
            // Just collect all of the instanced geometries and instanced controllers
            // that hang off this node (or any children).
        bool gotAttachment = false;
        auto nodeAsGuid = AsObjectGuid(node);
        const auto& scene = node.GetScene();
        for (unsigned c=0; c<scene.GetInstanceGeometryCount(); ++c)
            if (scene.GetInstanceGeometry_Attach(c).GetIndex() == node.GetIndex()) {
                _meshes.push_back(AttachedObject{nodeAsGuid, c, GetLevelOfDetail(node)._lod});
                gotAttachment = true;
            }

        for (unsigned c=0; c<scene.GetInstanceControllerCount(); ++c)
            if (scene.GetInstanceController_Attach(c).GetIndex() == node.GetIndex()) {
                _skinControllers.push_back(AttachedObject{nodeAsGuid, c, GetLevelOfDetail(node)._lod});
                gotAttachment = true;
            }

            // Register the names with attachments node -- early on
            // Note that if we get a resolve failure (or compile failure) then the
            // node will remain registered in the skeleton
        if (gotAttachment)
            nodeRefs.TryRegisterNode(nodeAsGuid, SkeletonBindingName(node).c_str());

        auto child = node.GetFirstChild();
        while (child) {
            if (terminateOnLODNodes && GetLevelOfDetail(child)._isLODRoot) { child = child.GetNextSibling(); continue; }

            Gather(child, nodeRefs, terminateOnLODNodes);
            child = child.GetNextSibling();
        }
    }

    void ReferencedGeometries::FindSkinJoints(
        const VisualScene& scene, 
        const URIResolveContext& resolveContext,
        SkeletonRegistry& nodeRefs)
    {
        for (auto c:_skinControllers) {
            const auto& instGeo = scene.GetInstanceController(c._objectIndex);
            GuidReference controllerRef(instGeo._reference);
            NascentObjectGuid controllerId(controllerRef._id, controllerRef._fileHash);
            auto* scaffoldController = FindElement(controllerRef, resolveContext, &IDocScopeIdResolver::FindSkinController);
            if (scaffoldController) {
                auto skeleton = FindElement(instGeo.GetSkeleton(), resolveContext, &IDocScopeIdResolver::FindNode);
                if (skeleton) {
                    auto jointNames = GetJointNames(*scaffoldController, resolveContext);
                    for (auto& j:jointNames) {
                        auto compareSection = MakeStringSection(j);
                        Node node = skeleton.FindBreadthFirst(
                            [&compareSection](const Node& compare) { return XlEqString(compare.GetSid(), compareSection); });

                        if (node)
                            nodeRefs.TryRegisterNode(AsObjectGuid(node), SkeletonBindingName(node).c_str());
                    }
                }
            }
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

                auto newMaterialGuid = invalidGuid;

                GuidReference ref(b->_reference);
                auto* file = resolveContext.FindFile(ref._fileHash);
                if (file) {
                    const auto* mat = file->FindMaterial(ref._id);
                    if (mat) {
                        newMaterialGuid = 
                            RenderCore::Assets::MakeMaterialGuid(mat->_name);
                    }
                }


                if (materialGuids[index] != invalidGuid && materialGuids[index] != newMaterialGuid) {

                        // Some collada files can actually have multiple instance_material elements for
                        // the same binding symbol. Let's throw an exception in this case (but only
                        // if the bindings don't agree)
                    Throw(::Exceptions::BasicLabel("Single material binding symbol is bound to multiple different materials in geometry instantiation"));
                }

                materialGuids[index] = newMaterialGuid;
            }
        }

        return std::move(materialGuids);
    }

	InstantiatedGeo InstantiateGeometry(
        const ::ColladaConversion::InstanceGeometry& instGeo,
        const URIResolveContext& resolveContext,
		const Float4x4& mergedTransform,
        NascentGeometryObjects& objects,
        const ImportConfiguration& cfg)
    {
        GuidReference refGuid(instGeo._reference);
        NascentObjectGuid geoId(refGuid._id, refGuid._fileHash);
        auto geo = objects.GetGeo(geoId);
        if (geo == ~unsigned(0x0)) {
            auto* scaffoldGeo = FindElement(refGuid, resolveContext, &IDocScopeIdResolver::FindMeshGeometry);
            if (!scaffoldGeo) {
                    // look for a skin controller instead... We will use the geometry object that is referenced
                    // by the controller
                auto* scaffoldController = FindElement(refGuid, resolveContext, &IDocScopeIdResolver::FindSkinController);

                GuidReference sourceMeshRefGuid(scaffoldController->GetBaseMesh());
                geo = objects.GetGeo(NascentObjectGuid(sourceMeshRefGuid._id, refGuid._fileHash));
                if (geo == ~unsigned(0x0))
                    scaffoldGeo = FindElement(sourceMeshRefGuid, resolveContext, &IDocScopeIdResolver::FindMeshGeometry);
            }

            if (geo == ~unsigned(0x0)) {
                if (!scaffoldGeo)
                    Throw(::Exceptions::BasicLabel("Could not found geometry object to instantiate (%s)",
                        instGeo._reference.AsString().c_str()));

                auto convertedMesh = Convert(*scaffoldGeo, mergedTransform, resolveContext, cfg);
                if (convertedMesh._mainDrawCalls.empty()) {
                    
                        // everything else should be empty as well...
                    assert(convertedMesh._vertices.empty());
                    assert(convertedMesh._indices.empty());
                    assert(convertedMesh._matBindingSymbols.empty());
                    assert(convertedMesh._unifiedVertexIndexToPositionIndex.empty());
                
                    Throw(::Exceptions::BasicLabel(
                        "Geometry object is empty (%s)", instGeo._reference.AsString().c_str()));
                }

                objects._rawGeos.push_back(std::make_pair(geoId, std::move(convertedMesh)));
                geo = unsigned(objects._rawGeos.size()-1);
            }
        }
        
        auto materials = BuildMaterialTable(
            AsPointer(instGeo._matBindings.cbegin()), AsPointer(instGeo._matBindings.cend()),
            objects._rawGeos[geo].second._matBindingSymbols, resolveContext);

        return InstantiatedGeo{geo, std::move(materials)};
    }

    static DynamicArray<uint16> BuildJointArray(
        const GuidReference skeletonRef,
        const UnboundSkinController& unboundController,
        const URIResolveContext& resolveContext,
		const JointToTransformMarker& jointToTransformMarker)
    {
            // Build the joints array for the given controller instantiation
            // the <instance_controller> references a skeleton, which contains
            // nodes that map on to the joints array in the <controller>
            // We want to find the node guids for each of those objects, can then
            // build transformation machine output indices for each of them.
        auto skeleton = FindElement(skeletonRef, resolveContext, &IDocScopeIdResolver::FindNode);
        if (!skeleton) return DynamicArray<uint16>();

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
                auto objectGuid = AsObjectGuid(node);
				// We need to transform from this guid value to the skeleton machine output marker.
                result[c] = (uint16)jointToTransformMarker(objectGuid);
            } else {
                result[c] = (uint16)~uint16(0);
            }
        }

        return std::move(result);
    }

	InstantiatedGeo InstantiateController(
        const ::ColladaConversion::InstanceController& instGeo,
        const URIResolveContext& resolveContext,
		const JointToTransformMarker& jointToTransformMarker,
		NascentGeometryObjects& objects,
        const ImportConfiguration& cfg)
    {
        GuidReference controllerRef(instGeo._reference);
        NascentObjectGuid controllerId(controllerRef._id, controllerRef._fileHash);
        auto* scaffoldController = FindElement(controllerRef, resolveContext, &IDocScopeIdResolver::FindSkinController);
        if (!scaffoldController)
            Throw(::Exceptions::BasicLabel("Could not find controller object to instantiate (%s)",
                instGeo._reference.AsString().c_str()));

        auto controller = Convert(*scaffoldController, resolveContext, cfg);

        auto jointMatrices = BuildJointArray(instGeo.GetSkeleton(), controller, resolveContext, jointToTransformMarker);
        if (!jointMatrices.size() || !jointMatrices.get())
            Throw(::Exceptions::BasicLabel("Skin controller object has no joints. Cannot instantiate as skinned object. (%s)",
                instGeo._reference.AsString().c_str()));

            // If the the raw geometry object is already converted, then we should use it. Otherwise
            // we need to do the conversion (but store it only in a temporary -- we don't need to
            // write it to disk)
        NascentRawGeometry* source = nullptr;
        NascentRawGeometry tempBuffer;
        {
            auto geo = objects.GetGeo(controller._sourceRef);
            if (geo == ~unsigned(0x0)) {
                auto* scaffoldGeo = FindElement(
                    GuidReference(controller._sourceRef._objectId, controller._sourceRef._namespaceId),
                    resolveContext, &IDocScopeIdResolver::FindMeshGeometry);
                if (!scaffoldGeo)
                    Throw(::Exceptions::BasicLabel("Could not find geometry object to instantiate (%s)",
                        instGeo._reference.AsString().c_str()));
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
                    ColladaConversion::AsString(instGeo._reference).c_str())));

		return InstantiatedGeo{unsigned(objects._skinnedGeos.size()-1), std::move(materials)};
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::string SkeletonBindingName(const Node& node)    
    {
            // Both "name" and "id" are optional. Let's prioritize "name"
            //  -- if it exists. If there is no name, we'll fall back to "id"
        if (node.GetName()._end > node.GetName()._start)
            return ColladaConversion::AsString(node.GetName()); 
        if (!node.GetId().IsEmpty())
            return ColladaConversion::AsString(node.GetId().GetOriginal());
        return XlDynFormatString("Unnamed%i", (unsigned)node.GetIndex());
    }

    static NascentObjectGuid AsObjectGuid(const Node& node)
    { 
        if (!node.GetId().IsEmpty())
            return node.GetId().GetHash(); 
        if (!node.GetName().IsEmpty())
            return Hash64(node.GetName().begin(), node.GetName().end());

        // If we have no name & no id -- it is truly anonymous. 
        // We can just use the index of the node, it's the only unique
        // thing we have.
        return NascentObjectGuid(node.GetIndex());
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

}

