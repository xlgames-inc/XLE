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
                        NascentModelCommandStream::GeometryInstance(Convert(id), (unsigned)thisOutputMatrix, std::move(materials), 0));
                }
            }

            for (size_t c=0; c<node.getInstanceNodes().getCount(); ++c) {
                const UniqueId& id  = node.getInstanceNodes()[c]->getInstanciatedObjectId();
                stream._modelInstances.push_back(NascentModelCommandStream::ModelInstance(Convert(id), (unsigned)thisOutputMatrix));
            }

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

    static const bool SkinNormals = true;

    void InstantiateControllers(NascentModelCommandStream& stream, const COLLADAFW::Node& node, const TableOfObjects& accessableObjects, TableOfObjects& destinationForNewObjects)
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

        std::vector<std::string> elementsToBeSkinned;
        elementsToBeSkinned.push_back("POSITION");
        if (SkinNormals) {
            elementsToBeSkinned.push_back("NORMAL");
        }

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

                            //
                            //      Our instantiation of this geometry needs to be slightly different
                            //      (but still similar) to the basic raw geometry case.
                            //
                            //      Basic geometry:
                            //          vertex buffer
                            //          index buffer
                            //          input assembly setup
                            //          draw calls
                            //
                            //      Skinned Geometry:
                            //              (this part is mostly the same, except we've reordered the
                            //              vertex buffers, and removed the part of the vertex buffer 
                            //              that will be animated)
                            //          unanimated vertex buffer
                            //          index buffer
                            //          input assembly setup (final draw calls)
                            //          draw calls (final draw calls)
                            //
                            //              (this part is new)
                            //          animated vertex buffer
                            //          input assembly setup (skinning calculation pass)
                            //          draw calls (skinning calculation pass)
                            //
                            //      Note that we need to massage the vertex buffers slightly. So the
                            //      raw geometry input must be in a format that allows us to read from
                            //      the vertex and index buffers.
                            //
                            
                        size_t unifiedVertexCount = source->_unifiedVertexIndexToPositionIndex.size();

                        std::vector<std::pair<uint16,uint32>> unifiedVertexIndexToBucketIndex;
                        unifiedVertexIndexToBucketIndex.reserve(unifiedVertexCount);

                        for (uint16 c=0; c<unifiedVertexCount; ++c) {
                            uint32 positionIndex = source->_unifiedVertexIndexToPositionIndex[c];
                            uint32 bucketIndex   = controller->_positionIndexToBucketIndex[positionIndex];
                            unifiedVertexIndexToBucketIndex.push_back(std::make_pair(c, bucketIndex));
                        }

                            //
                            //      Resort by bucket index...
                            //

                        std::sort(unifiedVertexIndexToBucketIndex.begin(), unifiedVertexIndexToBucketIndex.end(), CompareSecond<uint16, uint32>());

                        std::vector<uint16> unifiedVertexReordering;       // unifiedVertexReordering[oldIndex] = newIndex;
                        std::vector<uint16> newUnifiedVertexIndexToPositionIndex;
                        unifiedVertexReordering.resize(unifiedVertexCount, (uint16)~uint16(0x0));
                        newUnifiedVertexIndexToPositionIndex.resize(unifiedVertexCount, (uint16)~uint16(0x0));

                            //
                            //      \todo --    it would better if we tried to maintain the vertex ordering within
                            //                  the bucket. That is, the relative positions of vertices within the
                            //                  bucket should be the same as the relative positions of those vertices
                            //                  as they were in the original
                            //

                        uint16 indexAccumulator = 0;
                        const size_t bucketCount = dimof(((UnboundSkinController*)nullptr)->_bucket);
                        uint16 bucketStart  [bucketCount];
                        uint16 bucketEnd    [bucketCount];
                        uint16 currentBucket = 0; bucketStart[0] = 0;
                        for (auto i=unifiedVertexIndexToBucketIndex.cbegin(); i!=unifiedVertexIndexToBucketIndex.cend(); ++i) {
                            if ((i->second >> 16)!=currentBucket) {
                                bucketEnd[currentBucket] = indexAccumulator;
                                bucketStart[++currentBucket] = indexAccumulator;
                            }
                            uint16 newIndex = indexAccumulator++;
                            uint16 oldIndex = i->first;
                            unifiedVertexReordering[oldIndex] = newIndex;
                            newUnifiedVertexIndexToPositionIndex[newIndex] = (uint16)source->_unifiedVertexIndexToPositionIndex[oldIndex];
                        }
                        bucketEnd[currentBucket] = indexAccumulator;
                        for (unsigned b=currentBucket+1; b<bucketCount; ++b) {
                            bucketStart[b] = bucketEnd[b] = indexAccumulator;
                        }
                        if (indexAccumulator != unifiedVertexCount) {
                            ThrowException(FormatError("Vertex count mismatch in node (%s)", GetNodeStringID(node).c_str()));
                        }

                            //
                            //      Move vertex data for vertex elements that will be skinned into a separate vertex buffer
                            //      Note that we don't really know which elements will be skinned. We can assume that at
                            //      least "POSITION" will be skinned. But actually this is defined by the particular
                            //      shader. We could wait until binding with the material to make this decision...?
                            //
                        std::vector<Metal::InputElementDesc> unanimatedVertexLayout = source->_mainDrawInputAssembly._vertexInputLayout;
                        std::vector<Metal::InputElementDesc> animatedVertexLayout;

                        for (auto i=unanimatedVertexLayout.begin(); i!=unanimatedVertexLayout.end();) {
                            const bool mustBeSkinned = 
                                std::find_if(   elementsToBeSkinned.begin(), elementsToBeSkinned.end(), 
                                                [&](const std::string& s){ return !XlCompareStringI(i->_semanticName.c_str(), s.c_str()); }) 
                                        != elementsToBeSkinned.end();
                            if (mustBeSkinned) {
                                animatedVertexLayout.push_back(*i);
                                i=unanimatedVertexLayout.erase(i);
                            } else ++i;
                        }

                        {
                            unsigned elementOffset = 0;     // reset the _alignedByteOffset members in the vertex layout
                            for (auto i=unanimatedVertexLayout.begin(); i!=unanimatedVertexLayout.end();++i) {
                                i->_alignedByteOffset = elementOffset;
                                elementOffset += Metal::BitsPerPixel(i->_nativeFormat)/8;
                            }
                            elementOffset = 0;
                            for (auto i=animatedVertexLayout.begin(); i!=animatedVertexLayout.end();++i) {
                                i->_alignedByteOffset = elementOffset;
                                elementOffset += Metal::BitsPerPixel(i->_nativeFormat)/8;
                            }
                        }

                        unsigned unanimatedVertexStride  = CalculateVertexSize(AsPointer(unanimatedVertexLayout.begin()), AsPointer(unanimatedVertexLayout.end()));
                        unsigned animatedVertexStride    = CalculateVertexSize(AsPointer(animatedVertexLayout.begin()), AsPointer(animatedVertexLayout.end()));

                        if (!animatedVertexStride) {
                            ThrowException(FormatError("Could not find any animated vertex elements in skinning controller in node (%s). There must be a problem with vertex input semantics.", GetNodeStringID(node).c_str()));
                        }
                            
                            //      Copy out those parts of the vertex buffer that are unanimated and animated
                            //      (we also do the vertex reordering here)
                        std::unique_ptr<uint8[]> unanimatedVertexBuffer  = std::make_unique<uint8[]>(unanimatedVertexStride*unifiedVertexCount);
                        std::unique_ptr<uint8[]> animatedVertexBuffer    = std::make_unique<uint8[]>(animatedVertexStride*unifiedVertexCount);
                        CopyVertexElements( unanimatedVertexBuffer.get(),                   unanimatedVertexStride, 
                                            source->_vertices.get(),                        source->_mainDrawInputAssembly._vertexStride,
                                            AsPointer(unanimatedVertexLayout.begin()),      AsPointer(unanimatedVertexLayout.end()),
                                            AsPointer(source->_mainDrawInputAssembly._vertexInputLayout.begin()), AsPointer(source->_mainDrawInputAssembly._vertexInputLayout.end()),
                                            AsPointer(unifiedVertexReordering.begin()),     AsPointer(unifiedVertexReordering.end()));

                        CopyVertexElements( animatedVertexBuffer.get(),                     animatedVertexStride,
                                            source->_vertices.get(),                        source->_mainDrawInputAssembly._vertexStride,
                                            AsPointer(animatedVertexLayout.begin()),        AsPointer(animatedVertexLayout.end()),
                                            AsPointer(source->_mainDrawInputAssembly._vertexInputLayout.begin()), AsPointer(source->_mainDrawInputAssembly._vertexInputLayout.end()),
                                            AsPointer(unifiedVertexReordering.begin()),     AsPointer(unifiedVertexReordering.end()));

                            //      We have to remap the index buffer, also.
                        std::unique_ptr<uint8[]> newIndexBuffer = std::make_unique<uint8[]>(source->_indices.size());
                        if (source->_indexFormat == Metal::NativeFormat::R16_UINT) {
                            std::transform(
                                (const uint16*)source->_indices.begin(), (const uint16*)source->_indices.end(),
                                (uint16*)newIndexBuffer.get(),
                                [&unifiedVertexReordering](uint16 inputIndex){return unifiedVertexReordering[inputIndex];});
                        } else if (source->_indexFormat == Metal::NativeFormat::R8_UINT) {
                            std::transform(
                                (const uint8*)source->_indices.begin(), (const uint8*)source->_indices.end(),
                                (uint8*)newIndexBuffer.get(),
                                [&unifiedVertexReordering](uint8 inputIndex){return unifiedVertexReordering[inputIndex];});
                        } else {
                            ThrowException(FormatError("Unrecognised index format when instantiating skin controller in node (%s).", GetNodeStringID(node).c_str()));
                        }
                                
                            //      We have to define the draw calls that perform the pre-skinning step

                        std::vector<NascentDrawCallDesc> preskinningDrawCalls;
                        if (bucketEnd[0] > bucketStart[0]) {
                            preskinningDrawCalls.push_back(NascentDrawCallDesc(
                                ~unsigned(0x0), bucketEnd[0] - bucketStart[0], bucketStart[0],
                                4, Metal::Topology::PointList));
                        }
                        if (bucketEnd[1] > bucketStart[1]) {
                            preskinningDrawCalls.push_back(NascentDrawCallDesc(
                                ~unsigned(0x0), bucketEnd[1] - bucketStart[1], bucketStart[1],
                                2, Metal::Topology::PointList));
                        }
                        if (bucketEnd[2] > bucketStart[2]) {
                            preskinningDrawCalls.push_back(NascentDrawCallDesc(
                                ~unsigned(0x0), bucketEnd[2] - bucketStart[2], bucketStart[2],
                                1, Metal::Topology::PointList));
                        }

                        assert(bucketEnd[2] <= unifiedVertexCount);

                            //      Build the final vertex weights buffer (our weights are currently stored
                            //      per vertex-position. So we need to expand to per-unified vertex -- blaggh!)
                            //      This means the output weights vertex buffer is going to be larger than input ones combined.

                        assert(newUnifiedVertexIndexToPositionIndex.size()==unifiedVertexCount);
                        size_t destinationWeightVertexStride = 0;
                        const std::vector<Metal::InputElementDesc>* finalWeightBufferFormat = nullptr;

                        unsigned bucketVertexSizes[bucketCount];
                        for (unsigned b=0; b<bucketCount; ++b) {
                            bucketVertexSizes[b] = CalculateVertexSize(     
                                AsPointer(controller->_bucket[b]._vertexInputLayout.begin()), 
                                AsPointer(controller->_bucket[b]._vertexInputLayout.end()));

                            if (controller->_bucket[b]._vertexBufferSize) {
                                if (bucketVertexSizes[b] > destinationWeightVertexStride) {
                                    destinationWeightVertexStride = bucketVertexSizes[b];
                                    finalWeightBufferFormat = &controller->_bucket[b]._vertexInputLayout;
                                }
                            }
                        }

                        unsigned alignedDestinationWeightVertexStride = (unsigned)std::max(destinationWeightVertexStride, size_t(4));
                        if (alignedDestinationWeightVertexStride != destinationWeightVertexStride) {
                            LogAlwaysWarningF("LogAlwaysWarningF -- vertex buffer had to be expanded for vertex alignment restrictions in node (%s). This will leave some wasted space in the vertex buffer. This can be caused when using skinning when only 1 weight is really required.\n", GetNodeStringID(node).c_str());
                            destinationWeightVertexStride = alignedDestinationWeightVertexStride;
                        }

                        std::unique_ptr<uint8[]> skeletonBindingVertices;
                        if (destinationWeightVertexStride && finalWeightBufferFormat) {
                            skeletonBindingVertices = std::make_unique<uint8[]>(destinationWeightVertexStride*unifiedVertexCount);
                            XlSetMemory(skeletonBindingVertices.get(), 0, destinationWeightVertexStride*unifiedVertexCount);

                            for (auto i=newUnifiedVertexIndexToPositionIndex.begin(); i!=newUnifiedVertexIndexToPositionIndex.end(); ++i) {
                                const size_t destinationVertexIndex = i-newUnifiedVertexIndexToPositionIndex.begin();
                                unsigned sourceVertexPositionIndex = *i;
                                
                                    //
                                    //      We actually need to find the source position vertex from one of the buckets.
                                    //      We can make a guess from the ordering, but it's safest to find it again
                                    //      This lookup could get quite expensive for large meshes!
                                    //
                                for (unsigned b=0; b<bucketCount; ++b) {
                                    auto i = std::find( controller->_bucket[b]._vertexBindings.begin(), 
                                                        controller->_bucket[b]._vertexBindings.end(), 
                                                        sourceVertexPositionIndex);
                                    if (i!=controller->_bucket[b]._vertexBindings.end()) {

                                            //
                                            //      Note that sometimes we'll be expanding the vertex format in this process
                                            //      If some buckets are using R8G8, and others are R8G8B8A8 (for example)
                                            //      then they will all be expanded to the largest size
                                            //

                                        auto sourceVertexStride = bucketVertexSizes[b];
                                        size_t sourceVertexInThisBucket = std::distance(controller->_bucket[b]._vertexBindings.begin(), i);
                                        void* destinationVertex = PtrAdd(skeletonBindingVertices.get(), destinationVertexIndex*destinationWeightVertexStride);
                                        assert((sourceVertexInThisBucket+1)*sourceVertexStride <= controller->_bucket[b]._vertexBufferSize);
                                        const void* sourceVertex = PtrAdd(controller->_bucket[b]._vertexBufferData.get(), sourceVertexInThisBucket*sourceVertexStride);

                                        if (sourceVertexStride == destinationWeightVertexStride) {
                                            XlCopyMemory(destinationVertex, sourceVertex, sourceVertexStride);
                                        } else {
                                            const Metal::InputElementDesc* dstElement = AsPointer(finalWeightBufferFormat->cbegin());
                                            for (   auto srcElement=controller->_bucket[b]._vertexInputLayout.cbegin(); 
                                                    srcElement!=controller->_bucket[b]._vertexInputLayout.cend(); ++srcElement, ++dstElement) {
                                                unsigned elementSize = std::min(Metal::BitsPerPixel(srcElement->_nativeFormat)/8, Metal::BitsPerPixel(dstElement->_nativeFormat)/8);
                                                assert(PtrAdd(destinationVertex, dstElement->_alignedByteOffset+elementSize) <= PtrAdd(skeletonBindingVertices.get(), destinationWeightVertexStride*unifiedVertexCount));
                                                assert(PtrAdd(sourceVertex, srcElement->_alignedByteOffset+elementSize) <= PtrAdd(controller->_bucket[b]._vertexBufferData.get(), controller->_bucket[b]._vertexBufferSize));
                                                XlCopyMemory(   PtrAdd(destinationVertex, dstElement->_alignedByteOffset), 
                                                                PtrAdd(sourceVertex, srcElement->_alignedByteOffset), 
                                                                elementSize);   // (todo -- precalculate this min of element sizes)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                            //  Double check that weights are normalized in the binding buffer
                        #if 0 // defined(_DEBUG)

                            {
                                unsigned weightsOffset = 0;
                                Metal::NativeFormat::Enum weightsFormat = Metal::NativeFormat::Unknown;
                                for (auto i=finalWeightBufferFormat->cbegin(); i!=finalWeightBufferFormat->cend(); ++i) {
                                    if (!XlCompareStringI(i->_semanticName.c_str(), "WEIGHTS") && i->_semanticIndex == 0) {
                                        weightsOffset = i->_alignedByteOffset;
                                        weightsFormat = i->_nativeFormat;
                                        break;
                                    }
                                }

                                size_t stride = destinationWeightVertexStride;
                                if (weightsFormat == Metal::NativeFormat::R8G8_UNORM) {
                                    for (unsigned c=0; c<unifiedVertexCount; ++c) {
                                        const void* p = PtrAdd(skeletonBindingVertices.get(), c*stride+weightsOffset);
                                        unsigned char zero   = ((unsigned char*)p)[0];
                                        unsigned char one    = ((unsigned char*)p)[1];
                                        assert((zero+one) >= 0xfd);
                                    }
                                } else if (weightsFormat == Metal::NativeFormat::R8G8B8A8_UNORM) {
                                    for (unsigned c=0; c<unifiedVertexCount; ++c) {
                                        const void* p = PtrAdd(skeletonBindingVertices.get(), c*stride+weightsOffset);
                                        unsigned char zero   = ((unsigned char*)p)[0];
                                        unsigned char one    = ((unsigned char*)p)[1];
                                        unsigned char two    = ((unsigned char*)p)[2];
                                        unsigned char three  = ((unsigned char*)p)[3];
                                        assert((zero+one+two+three) >= 0xfd);
                                    }
                                } else {
                                    assert(weightsFormat == Metal::NativeFormat::R8_UNORM);
                                }
                            }
                                
                        #endif

                            //      We need to map from from our joint indices to output matrix index
                        const size_t jointCount = controllerAndSkeleton->_jointIds.size();
                        std::unique_ptr<uint16[]> jointMatrices = std::make_unique<uint16[]>(jointCount);
                        for (auto i = controllerAndSkeleton->_jointIds.cbegin(); i!=controllerAndSkeleton->_jointIds.cend(); ++i) {
                            jointMatrices[std::distance(controllerAndSkeleton->_jointIds.cbegin(), i)] = 
                                (uint16)stream.FindTransformationMachineOutput(*i);
                        }

                            //      Calculate the local space bounding box for the input vertex buffer
                            //      (assuming the position will appear in the animated vertex buffer)
                        auto boundingBox = InvalidBoundingBox();
                        Metal::InputElementDesc positionDesc = FindPositionElement(
                            AsPointer(animatedVertexLayout.begin()),
                            animatedVertexLayout.size());
                        if (positionDesc._nativeFormat != Metal::NativeFormat::Unknown) {
                            AddToBoundingBox(
                                boundingBox,
                                animatedVertexBuffer.get(), animatedVertexStride, unifiedVertexCount,
                                positionDesc, Identity<Float4x4>());
                        }

                            //      Build the final "BoundSkinnedGeometry" object
                        NascentBoundSkinnedGeometry result(
                            DynamicArray<uint8>(std::move(unanimatedVertexBuffer), unanimatedVertexStride*unifiedVertexCount),
                            DynamicArray<uint8>(std::move(animatedVertexBuffer), animatedVertexStride*unifiedVertexCount),
                            DynamicArray<uint8>(std::move(skeletonBindingVertices), destinationWeightVertexStride*unifiedVertexCount),
                            DynamicArray<uint8>(std::move(newIndexBuffer), source->_indices.size()));

                        result._skeletonBindingVertexStride = (unsigned)destinationWeightVertexStride;
                        result._animatedVertexBufferSize = (unsigned)(animatedVertexStride*unifiedVertexCount);

                        result._inverseBindMatrices = DynamicArray<Float4x4>::Copy(controller->_inverseBindMatrices);
                        result._jointMatrices = DynamicArray<uint16>(std::move(jointMatrices), jointCount);
                        result._bindShapeMatrix = controller->_bindShapeMatrix;

                        result._mainDrawCalls = source->_mainDrawCalls;
                        result._mainDrawUnanimatedIA._vertexStride = unanimatedVertexStride;
                        result._mainDrawUnanimatedIA._vertexInputLayout = std::move(unanimatedVertexLayout);
                        result._indexFormat = source->_indexFormat;

                        result._mainDrawAnimatedIA._vertexStride = animatedVertexStride;
                        result._mainDrawAnimatedIA._vertexInputLayout = std::move(animatedVertexLayout);

                        result._preskinningDrawCalls = preskinningDrawCalls;

                        if (finalWeightBufferFormat) {
                            result._preskinningIA._vertexInputLayout = *finalWeightBufferFormat;
                            result._preskinningIA._vertexStride = (unsigned)destinationWeightVertexStride;
                        }

                        result._localBoundingBox = boundingBox;

                        auto desc = accessableObjects.GetDesc<UnboundSkinController>(controllerAndSkeleton->_unboundControllerId);
                        destinationForNewObjects.Add(
                            controllerAndSkeleton->_unboundControllerId,
                            std::get<0>(desc), std::get<1>(desc),
                            std::move(result));
                                
                            //
                            //  Have to build the material bindings, as well..
                            //
                        auto materials = BuildMaterialTable(
                            node.getInstanceControllers()[instanceController]->getMaterialBindings(), source->_matBindingSymbols, accessableObjects);

                        NascentModelCommandStream::SkinControllerInstance newInstance(
                            controllerAndSkeleton->_unboundControllerId, 
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