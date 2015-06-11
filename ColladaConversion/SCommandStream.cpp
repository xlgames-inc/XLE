// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TableOfObjects.h"
#include "Scaffold.h"
#include "ModelCommandStream.h"
#include "STransformationMachine.h"
#include "SRawGeometry.h"
#include "ParsingUtil.h"
#include "../RenderCore/Assets/Material.h"
#include "../Utility/Conversion.h"
#include "../Utility/MemoryUtils.h"

namespace RenderCore { namespace ColladaConversion
{
    using namespace ::ColladaConversion;

    static std::string SkeletonBindingName(const Node& node);
    static ObjectGuid AsObjectGuid(const Node& node);

    static bool IsUseful(const Node& node, const NodeReferences& skeletonReferences);

///////////////////////////////////////////////////////////////////////////////////////////////////

    void BuildSkeleton(
        NascentSkeleton& skeleton,
        const Node& node,
        NodeReferences& skeletonReferences)
    {
        using namespace COLLADAFW;

        if (!IsUseful(node, skeletonReferences)) return;

        unsigned pushCount = PushTransformations(
            skeleton.GetTransformationMachine(),
            node.GetFirstTransform(), SkeletonBindingName(node).c_str());

            //
            //      We have to assume we need an output matrix. We don't really know
            //      which nodes need output matrices at this point (because we haven't 
            //      got all the downstream skinning data). So, let's just assume it's needed.
            //
        auto nodeId = AsObjectGuid(node);
        bool isReferenced = skeletonReferences.IsImportant(nodeId);

            // DavidJ -- hack! -- When writing a "skeleton" we need to include all nodes, even those that aren't
            //              referenced within the same file. This is because the node might become an output-interface
            //              node... Maybe there is a better way to do this. Perhaps we could identify which nodes are
            //              output interface transforms / bones... Or maybe we could just include everything when
            //              compiling a skeleton...?
        // isReferenced = true;
        if (isReferenced) {
            unsigned thisOutputMatrix = skeleton.GetTransformationMachine().GetOutputMatrixMarker();
            skeletonReferences.SetOutputMatrix(nodeId, thisOutputMatrix);

                //
                //      (We can't instantiate the skin controllers yet, because we can't be sure
                //          we've already parsed the skeleton nodes)
                //      But we can write a tag to find the output matrix later
                //          (we also need a tag for all nodes with instance controllers in them)
                //

            auto* inverseBind = skeletonReferences.GetInverseBindMatrix(nodeId);
            if (inverseBind) {
                    // note -- there may be problems here, because the "name" of the node isn't necessarily
                    //          unique. There are unique ids in collada, however. We some some unique identifier
                    //          can can be seen in Max, and can be used to associate different files with shared
                    //          references (eg, animations, skeletons and skins in separate files)
                skeleton.GetTransformationMachine().RegisterJointName(
                    SkeletonBindingName(node), 
                    *inverseBind, thisOutputMatrix);
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

    static const NascentRawGeometry* 
        FindOrCreateGeometry(
            GuidReference ref,
            const URIResolveContext& resolveContext, 
            TableOfObjects& accessableObjects)
    {
        const auto* geo = accessableObjects.Get<NascentRawGeometry>(ref._id);
        if (geo) return geo;

        auto* file = resolveContext.FindFile(ref._fileHash);
        if (!file) return nullptr;

        auto* mesh = file->FindMeshGeometry(ref._id);
        if (!mesh) return nullptr;

        accessableObjects.Add(
            ref._id,
            AsString(mesh->GetName()), AsString(mesh->GetId().GetOriginal()),
            ::ColladaConversion::Convert(*mesh, resolveContext));

            return accessableObjects.Get<NascentRawGeometry>(ref._id);
    }

    void InstantiateGeometry(
        NascentModelCommandStream& stream,
        const VisualScene& scene, unsigned instanceGeoIndex,
        const URIResolveContext& resolveContext,
        TableOfObjects& accessableObjects,
        const NodeReferences& nodeRefs)
    {
        auto attachedNode = scene.GetInstanceGeometry_Attach(instanceGeoIndex);

        auto instGeo = scene.GetInstanceGeometry(instanceGeoIndex);
        GuidReference refGuid(instGeo._reference);
        auto* geo = FindOrCreateGeometry(refGuid, resolveContext, accessableObjects);
        if (!geo)
            Throw(::Assets::Exceptions::FormatError("Could not found geometry object to instantiate (%s)",
                AsString(instGeo._reference).c_str()));

        auto materials = BuildMaterialTable(
            AsPointer(instGeo._matBindings.cbegin()), AsPointer(instGeo._matBindings.cend()),
            geo->_matBindingSymbols, resolveContext);

        stream._geometryInstances.push_back(
            NascentModelCommandStream::GeometryInstance(
                refGuid._id, (unsigned)nodeRefs.GetOutputMatrixIndex(attachedNode.GetId().GetHash()), 
                std::move(materials), 0));
    }

    void FindImportantNodes(
        NodeReferences& skeletonReferences,
        VisualScene& scene)
    {
        for (unsigned c=0; c<scene.GetInstanceGeometryCount(); ++c)
            skeletonReferences.MarkImportant(AsObjectGuid(scene.GetInstanceGeometry_Attach(c)));
        for (unsigned c=0; c<scene.GetInstanceControllerCount(); ++c)
            skeletonReferences.MarkImportant(AsObjectGuid(scene.GetInstanceController_Attach(c)));
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::string SkeletonBindingName(const Node& node)
    {
        return Conversion::Convert<std::string>(
            std::basic_string<utf8>(node.GetName()._start, node.GetName()._end));
    }

    static ObjectGuid AsObjectGuid(const Node& node)
    {
        return node.GetId().GetHash();
    }

    static bool IsUseful(const Node& node, const NodeReferences& skeletonReferences)
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

