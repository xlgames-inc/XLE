// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS   // warning C4996: 'std::_Copy_impl': Function call with parameters that may be unsafe

#include "OCInterface.h"
#include "../ModelCommandStream.h"
#include "../ProcessingUtil.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/StringUtils.h"

#pragma warning(push)
#pragma warning(disable:4201)       // nonstandard extension used : nameless struct/union
#pragma warning(disable:4245)       // conversion from 'int' to 'const COLLADAFW::SamplerID', signed/unsigned mismatch
#pragma warning(disable:4512)       // assignment operator could not be generated
    #include <COLLADAFWTranslate.h>
    #include <COLLADAFWMatrix.h>
    #include <COLLADAFWRotate.h>
    #include <COLLADAFWScale.h>

    #include <COLLADAFWMaterialBinding.h>
    #include <COLLADAFWNode.h>
#pragma warning(pop)

#pragma warning(disable:4127)       // C4127: conditional expression is constant

namespace RenderCore { namespace ColladaConversion
{ 
    using ::Assets::Exceptions::FormatError;

    static std::string GetNodeStringID(const COLLADAFW::Node& node)
    {
        return node.getOriginalId();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::tuple<bool,bool> NeedOutputMatrix(  const COLLADAFW::Node& node, const TableOfObjects& accessableObjects,
                                                    const NodeReferences& skeletonReferences)
    {
        using namespace COLLADAFW;
        bool needAnOutputMatrix = false;
        for (size_t c=0; c<node.getInstanceGeometries().getCount(); ++c) {

                //
                //      Ignore material bindings for the moment...!
                //

            const UniqueId& id = node.getInstanceGeometries()[c]->getInstanciatedObjectId();
            if (!accessableObjects.Get<NascentRawGeometry>(Convert(id))) {
                LogAlwaysWarningF( "LogAlwaysWarningF -- bad instanced geometry link found in node (%s)\n", GetNodeStringID(node).c_str());
            } else {
                needAnOutputMatrix = true;
            }
        }

        for (size_t c=0; c<node.getInstanceNodes().getCount(); ++c) {
            const UniqueId& id = node.getInstanceNodes()[c]->getInstanciatedObjectId();
            if (!accessableObjects.Get<NascentModelCommandStream>(Convert(id))) {
                LogAlwaysWarningF("LogAlwaysWarningF -- bad instanced geometry link found in node (%s)\n", GetNodeStringID(node).c_str());
            } else {
                needAnOutputMatrix = true;
            }
        }

        for (size_t c=0; c<node.getInstanceControllers().getCount(); ++c) {
            const UniqueId& id = node.getInstanceControllers()[c]->getInstanciatedObjectId();
            if (    !accessableObjects.Get<UnboundSkinControllerAndAttachedSkeleton>(Convert(id))
                &&  !accessableObjects.Get<UnboundMorphController>(Convert(id))) {

                LogAlwaysWarningF("LogAlwaysWarningF -- bad instanced controller link found in node (%s)\n", GetNodeStringID(node).c_str());
            } else {
                needAnOutputMatrix = true;
            }
        }

        needAnOutputMatrix |= ImportCameras && !node.getInstanceCameras().empty();

            //
            //      Check to see if this node is referenced by any instance controllers
            //      if it is, it's probably part of a skeleton (and we need an output
            //      matrix and a joint tag)
            //

        bool isReferencedJoint = false;
        if (skeletonReferences.IsImportant(Convert(node.getUniqueId()))) {
            isReferencedJoint = needAnOutputMatrix = true;
        }

        return std::make_tuple(needAnOutputMatrix, isReferencedJoint);
    }

    void PushNode(
        NascentSkeleton& skeleton,
        const COLLADAFW::Node& node, const TableOfObjects& accessableObjects,
        const NodeReferences& skeletonReferences)
    {
        using namespace COLLADAFW;
        COLLADABU::Math::Matrix4 matrix;
        node.getTransformationMatrix(matrix);
        unsigned pushCount = PushTransformations(
            skeleton.GetTransformationMachine(),
            node.getTransformations(), GetNodeStringID(node).c_str());

            //
            //      We have to assume we need an output matrix. We don't really know
            //      which nodes need output matrices at this point (because we haven't 
            //      got all the downstream skinning data). So, let's just assume it's needed.
            //
        bool needAnOutputMatrix, isReferencedJoint;
        std::tie(needAnOutputMatrix, isReferencedJoint) = NeedOutputMatrix(node, accessableObjects, skeletonReferences);
            // DavidJ -- hack! -- When writing a "skeleton" we need to include all nodes, even those that aren't
            //              referenced within the same file. This is because the node might become an output-interface
            //              node... Maybe there is a better way to do this. Perhaps we could identify which nodes are
            //              output interface transforms / bones... Or maybe we could just include everything when
            //              compiling a skeleton...?
        needAnOutputMatrix = isReferencedJoint = true;
        if (needAnOutputMatrix) {
            unsigned thisOutputMatrix = skeleton.GetTransformationMachine().GetOutputMatrixMarker();

                //
                //      (We can't instantiate the skin controllers yet, because we can't be sure
                //          we've already parsed the skeleton nodes)
                //      But we can write a tag to find the output matrix later
                //          (we also need a tag for all nodes with instance controllers in them)
                //

            if (isReferencedJoint || node.getInstanceControllers().getCount() || node.getInstanceGeometries().getCount()) {
                auto id = Convert(node.getUniqueId());
                Float4x4 inverseBindMatrix = Identity<Float4x4>();

                auto* t = skeletonReferences.GetInverseBindMatrix(id);
                if (t) inverseBindMatrix = *t;

                    // note -- there may be problems here, because the "name" of the node isn't necessarily
                    //          unique. There are unique ids in collada, however. We some some unique identifier
                    //          can can be seen in Max, and can be used to associate different files with shared
                    //          references (eg, animations, skeletons and skins in separate files)
                skeleton.GetTransformationMachine().RegisterJointName(
                    GetNodeStringID(node), inverseBindMatrix, thisOutputMatrix);
            }
        }

        const NodePointerArray& childNodes = node.getChildNodes();
        for (size_t c=0; c<childNodes.getCount(); ++c) {
            PushNode(skeleton, *childNodes[c], accessableObjects, skeletonReferences);
        }

        skeleton.GetTransformationMachine().Pop(pushCount);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    auto BuildMaterialTable(
        const COLLADAFW::MaterialBindingArray& bindingArray, 
        const std::vector<uint64>& geometryMaterialOrdering,
        const TableOfObjects& accessableObjects) -> std::vector<NascentModelCommandStream::MaterialGuid>
    {
                        //
                        //        For each material referenced in the raw geometry, try to 
                        //        match it with a material we've built during collada processing
                        //            We have to map it via the binding table in the InstanceGeometry
                        //
                        
        using MaterialGuid = NascentModelCommandStream::MaterialGuid;
        auto invalidGuid = NascentModelCommandStream::s_materialGuid_Invalid;

        std::vector<MaterialGuid> materialGuids;
        materialGuids.resize(geometryMaterialOrdering.size(), invalidGuid);

        for (unsigned c=0; c<bindingArray.getCount(); ++c)
            for (auto i=geometryMaterialOrdering.cbegin(); i!=geometryMaterialOrdering.cend(); ++i)
                if (*i == bindingArray[c].getMaterialId()) {
                    assert(materialGuids[std::distance(geometryMaterialOrdering.cbegin(), i)] == invalidGuid);

                    const auto* matRef = accessableObjects.Get<ReferencedMaterial>(Convert(bindingArray[c].getReferencedMaterial()));

                        //      The "ReferenceMaterial" contains a guid value that will be used for matching
                        //      this reference against a material scaffold file
                    auto guid = invalidGuid;
                    if (matRef) guid = matRef->_guid;

                    materialGuids[std::distance(geometryMaterialOrdering.cbegin(), i)] = guid;
                    break;
                }

        return std::move(materialGuids);
    }

    void PushNode(   
        NascentModelCommandStream& stream,
        const COLLADAFW::Node& node, const TableOfObjects& accessableObjects,
        const NodeReferences& skeletonReferences)
    {
        if (!IsUseful(node, accessableObjects, skeletonReferences)) {
            return;
        }

        using namespace COLLADAFW;

            //
            //      If we have any "instances" attached to this node... Execute those draw calls
            //      and thingamabob's
            //

        bool needAnOutputMatrix, isReferencedJoint;
        std::tie(needAnOutputMatrix, isReferencedJoint) = NeedOutputMatrix(node, accessableObjects, skeletonReferences);
        if (needAnOutputMatrix) {
            const auto thisOutputMatrix = stream.RegisterTransformationMachineOutput(
                GetNodeStringID(node), Convert(node.getUniqueId()));
            
            for (size_t c=0; c<node.getInstanceGeometries().getCount(); ++c) {
                const InstanceGeometry& instanceGeo = *node.getInstanceGeometries()[c];
                const UniqueId& id  = instanceGeo.getInstanciatedObjectId();
                const auto* inputGeometry = accessableObjects.Get<NascentRawGeometry>(Convert(id));
                if (inputGeometry) {
                    auto materials = BuildMaterialTable(
                        instanceGeo.getMaterialBindings(), inputGeometry->_matBindingSymbols, accessableObjects);
                    stream._geometryInstances.push_back(
                        NascentModelCommandStream::GeometryInstance(
                            accessableObjects.GetIndex<NascentRawGeometry>(Convert(id)), 
                            (unsigned)thisOutputMatrix, std::move(materials), 0));
                }
            }

            // for (size_t c=0; c<node.getInstanceNodes().getCount(); ++c) {
            //     const UniqueId& id  = node.getInstanceNodes()[c]->getInstanciatedObjectId();
            //     stream._modelInstances.push_back(
            //         NascentModelCommandStream::ModelInstance(
            //             Convert(id), 
            //             (unsigned)thisOutputMatrix));
            // }

            for (size_t c=0; c<node.getInstanceCameras().getCount(); ++c) {

                    //
                    //      Ignore camera parameters for the moment
                    //          (they should come from another node in the <library_cameras> part
                    //  

                stream._cameraInstances.push_back(NascentModelCommandStream::CameraInstance((unsigned)thisOutputMatrix));
            }
        }

            //
            //      There's also instanced nodes, lights, cameras, controllers
            //
            //      Now traverse to child nodes.
            //

        const auto& childNodes = node.getChildNodes();
        for (size_t c=0; c<childNodes.getCount(); ++c) {
            PushNode(stream, *childNodes[c], accessableObjects, skeletonReferences);
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void InstantiateControllers(
        NascentModelCommandStream& stream, 
        const COLLADAFW::Node& node, 
        const TableOfObjects& accessableObjects, 
        TableOfObjects& destinationForNewObjects)
    {
            //
            //      This is usually a second pass through the node hierarchy (after PushNode)
            //      Look for all of the skin controllers and instantiate a proper bound controllers
            //      for each!
            //
            //      We have to do a second pass, because this must happen after we've pushed in all of the 
            //      joint nodes from the skeleton (which we can't be sure has happened until after PushNode
            //      has gone through the entire tree.
            //
        using namespace COLLADAFW;

        for (size_t instanceController=0; instanceController<node.getInstanceControllers().getCount(); ++instanceController) {
            const UniqueId& id = node.getInstanceControllers()[instanceController]->getInstanciatedObjectId();

            const auto* controllerAndSkeleton = 
                accessableObjects.Get<UnboundSkinControllerAndAttachedSkeleton>(Convert(id));

            if (controllerAndSkeleton) {
                const auto* controller = 
                    accessableObjects.Get<UnboundSkinController>(
                        controllerAndSkeleton->_unboundControllerId);
                if (controller) {

                        //
                        //      Assume the "source" of this controller is a geometry. Collada can 
                        //      support cascading controllers so that the output of one controller 
                        //      is used as input to the next controller. This can be useful for 
                        //      combining skinning and morph targets on the same geometry.
                        //

                    const auto* source = accessableObjects.Get<NascentRawGeometry>(controllerAndSkeleton->_source);
                    if (!source) {
                        const auto* morphController = accessableObjects.Get<UnboundMorphController>(
                            controllerAndSkeleton->_source);
                        if (morphController) {
                            source = accessableObjects.Get<NascentRawGeometry>(morphController->_source);
                        }
                    }
                    if (source) {

                            //      We need to map from from our joint indices to output matrix index
                        const size_t jointCount = controllerAndSkeleton->_jointIds.size();
                        std::unique_ptr<uint16[]> jointMatrices = std::make_unique<uint16[]>(jointCount);
                        for (auto i = controllerAndSkeleton->_jointIds.cbegin(); i!=controllerAndSkeleton->_jointIds.cend(); ++i) {
                            jointMatrices[std::distance(controllerAndSkeleton->_jointIds.cbegin(), i)] = 
                                (uint16)stream.FindTransformationMachineOutput(*i);
                        }

                        auto result = InstantiateSkinnedController(
                            *source, *controller, accessableObjects, destinationForNewObjects,
                            DynamicArray<uint16>(std::move(jointMatrices), jointCount),
                            GetNodeStringID(node).c_str());

                        auto desc = accessableObjects.GetDesc<UnboundSkinController>(controllerAndSkeleton->_unboundControllerId);
                        destinationForNewObjects.Add(
                            controllerAndSkeleton->_unboundControllerId,
                            std::get<0>(desc), std::get<1>(desc),
                            std::move(result));

                        auto materials = BuildMaterialTable(
                            node.getInstanceControllers()[instanceController]->getMaterialBindings(), 
                            source->_matBindingSymbols, accessableObjects);

                        NascentModelCommandStream::SkinControllerInstance newInstance(
                            destinationForNewObjects.GetIndex<UnboundSkinController>(controllerAndSkeleton->_unboundControllerId), 
                            stream.FindTransformationMachineOutput(Convert(node.getUniqueId())), std::move(materials), 0);
                        stream._skinControllerInstances.push_back(newInstance);

                    } else {
                        LogAlwaysWarningF("LogAlwaysWarningF -- skin controller attached to bad source object in node (%s). Note that skin controllers must be attached directly to geometry. We don't support cascading controllers.\n", GetNodeStringID(node).c_str());
                    }

                } else {
                    LogAlwaysWarningF("LogAlwaysWarningF -- skin controller with attached skeleton points to invalid skin controller in node (%s)\n", GetNodeStringID(node).c_str());
                }

            }
        }
            
        const NodePointerArray& childNodes = node.getChildNodes();
        for (size_t c=0; c<childNodes.getCount(); ++c) {
            InstantiateControllers(stream, *childNodes[c], accessableObjects, destinationForNewObjects);
        }
    }


    bool IsUseful(  const COLLADAFW::Node& node, const TableOfObjects& objects,
                    const NodeReferences& skeletonReferences)
    {
            //
            //      Traverse all of the nodes in the hierarchy
            //      and look for something that is useful for us.
            //
            //          Many node types we just ignore (like lights, etc)
            //          Let's strip them off first.
            //
            //      This node is useful if there is an instantiation of 
            //      something useful, or if a child node is useful.
            //
            //      But note that some nodes are just an "armature" or a
            //      set of skeleton bones. These don't have anything attached,
            //      but they are still important. 
            //
            //      We need to make sure that
            //      if we're instantiating skin instance_controller, then
            //      we consider all of the nodes in the attached "skeleton"
            //      to be important. To do this, we may need to scan to find
            //      all of the instance_controllers first, and use that to
            //      decide which pure nodes to keep.
            //
            //      It looks like there may be another problem in OpenCollada
            //      here. In pure collada, one skin controller can be attached
            //      to different skeletons in different instance_controllers. But
            //      it's not clear if that's also the case in OpenCollada (OpenCollada
            //      has a different scheme for binding instances to skeletons.
            //
        using namespace COLLADAFW;
        const InstanceGeometryPointerArray& instanceGeometrys = node.getInstanceGeometries();
        for (size_t c=0; c<instanceGeometrys.getCount(); ++c)
            if (objects.Has<NascentRawGeometry>(Convert(instanceGeometrys[c]->getInstanciatedObjectId())))
                return true;
        
        const InstanceNodePointerArray& instanceNodes = node.getInstanceNodes();
        for (size_t c=0; c<instanceNodes.getCount(); ++c)
            if (objects.Has<NascentModelCommandStream>(Convert(instanceNodes[c]->getInstanciatedObjectId())))
                return true;

        const InstanceControllerPointerArray& instanceControllers = node.getInstanceControllers();
        for (size_t c=0; c<instanceControllers.getCount(); ++c)
            if (objects.Has<UnboundSkinControllerAndAttachedSkeleton>(Convert(instanceControllers[c]->getInstanciatedObjectId())))
                return true;

        if (ImportCameras && !node.getInstanceCameras().empty())
            return true;

            // if this node is part of any of the skeletons we need, then it's "useful"
        if (skeletonReferences.IsImportant(Convert(node.getUniqueId())))
            return true;

        const NodePointerArray& childNodes = node.getChildNodes();
        for (size_t c=0; c<childNodes.getCount(); ++c)
            if (IsUseful(*childNodes[c], objects, skeletonReferences))
                return true;

        return false;
    }

    void FindInstancedSkinControllers(  const COLLADAFW::Node& node, const TableOfObjects& objects,
                                        NodeReferences& results)
    {
        using namespace COLLADAFW;
        const InstanceControllerPointerArray& instanceControllers = node.getInstanceControllers();
        for (size_t c=0; c<instanceControllers.getCount(); ++c) {
            const auto* controller = objects.Get<UnboundSkinControllerAndAttachedSkeleton>(
                Convert(instanceControllers[c]->getInstanciatedObjectId()));
            if (controller) {

                const auto* skinController = 
                    objects.Get<UnboundSkinController>(controller->_unboundControllerId);

                for (size_t c=0; c<controller->_jointIds.size(); ++c) {
                    auto inverseBindMatrix = Identity<Float4x4>();

                        //
                        //      look for an inverse bind matrix associated with this joint
                        //      from any attached skinning controllers
                        //      We need to know inverse bind matrices when combining skeletons
                        //      and animation data from different exports
                        //
                    if (c<skinController->_inverseBindMatrices.size()) {
                        inverseBindMatrix = skinController->_inverseBindMatrices[(unsigned)c];
                    }

                    results.AttachInverseBindMatrix(controller->_jointIds[c], inverseBindMatrix);
                    results.MarkImportant(controller->_jointIds[c]);
                }

            } else {
                LogAlwaysWarningF("LogAlwaysWarningF -- couldn't match skin controller in node (%s)\n", GetNodeStringID(node).c_str());
            }
        }

        const NodePointerArray& childNodes = node.getChildNodes();
        for (size_t c=0; c<childNodes.getCount(); ++c)
            FindInstancedSkinControllers(*childNodes[c], objects, results);
    }



    #if MATHLIBRARY_ACTIVE == MATHLIBRARY_CML

        Float4x4 AsFloat4x4(const COLLADABU::Math::Matrix4& matrix)
        {
                // (Float4x4 constructor doesn't take 16 elements...?)
            Float4x4 result;
            for (unsigned i=0; i<4; i++)
                for (unsigned j=0; j<4; j++)
                    result(i,j) = Float4x4::value_type(matrix[i][j]);
            return result;
        }

        Float3 AsFloat3(const COLLADABU::Math::Vector3& vector)
        {
            return Float3(Float3::value_type(vector.x), Float3::value_type(vector.y), Float3::value_type(vector.z));
        }

    #endif
}}