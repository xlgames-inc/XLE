// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS   // warning C4996: 'std::_Copy_impl': Function call with parameters that may be unsafe

#include "ModelCommandStream.h"
#include "ProcessingUtil.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringUtils.h"

namespace RenderCore { namespace ColladaConversion
{
    using ::Assets::Exceptions::FormatError;

    static const bool SkinNormals = true;

    NascentBoundSkinnedGeometry InstantiateSkinnedController(
        const NascentRawGeometry& sourceGeo,
        const UnboundSkinController& controller,
        const TableOfObjects& accessableObjects, 
        TableOfObjects& destinationForNewObjects,
        DynamicArray<uint16>&& jointMatrices,
        const char nodeName[])
    {
        std::vector<std::string> elementsToBeSkinned;
        elementsToBeSkinned.push_back("POSITION");
        if (constant_expression<SkinNormals>::result()) {
            elementsToBeSkinned.push_back("NORMAL");
        }

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
                            
        size_t unifiedVertexCount = sourceGeo._unifiedVertexIndexToPositionIndex.size();

        std::vector<std::pair<uint16,uint32>> unifiedVertexIndexToBucketIndex;
        unifiedVertexIndexToBucketIndex.reserve(unifiedVertexCount);

        for (uint16 c=0; c<unifiedVertexCount; ++c) {
            uint32 positionIndex = sourceGeo._unifiedVertexIndexToPositionIndex[c];
            uint32 bucketIndex   = controller._positionIndexToBucketIndex[positionIndex];
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
            newUnifiedVertexIndexToPositionIndex[newIndex] = (uint16)sourceGeo._unifiedVertexIndexToPositionIndex[oldIndex];
        }
        bucketEnd[currentBucket] = indexAccumulator;
        for (unsigned b=currentBucket+1; b<bucketCount; ++b) {
            bucketStart[b] = bucketEnd[b] = indexAccumulator;
        }
        if (indexAccumulator != unifiedVertexCount) {
            ThrowException(FormatError("Vertex count mismatch in node (%s)", nodeName));
        }

            //
            //      Move vertex data for vertex elements that will be skinned into a separate vertex buffer
            //      Note that we don't really know which elements will be skinned. We can assume that at
            //      least "POSITION" will be skinned. But actually this is defined by the particular
            //      shader. We could wait until binding with the material to make this decision...?
            //
        std::vector<Metal::InputElementDesc> unanimatedVertexLayout = sourceGeo._mainDrawInputAssembly._vertexInputLayout;
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
            ThrowException(FormatError("Could not find any animated vertex elements in skinning controller in node (%s). There must be a problem with vertex input semantics.", nodeName));
        }
                            
            //      Copy out those parts of the vertex buffer that are unanimated and animated
            //      (we also do the vertex reordering here)
        std::unique_ptr<uint8[]> unanimatedVertexBuffer  = std::make_unique<uint8[]>(unanimatedVertexStride*unifiedVertexCount);
        std::unique_ptr<uint8[]> animatedVertexBuffer    = std::make_unique<uint8[]>(animatedVertexStride*unifiedVertexCount);
        CopyVertexElements( unanimatedVertexBuffer.get(),                   unanimatedVertexStride, 
                            sourceGeo._vertices.get(),                      sourceGeo._mainDrawInputAssembly._vertexStride,
                            AsPointer(unanimatedVertexLayout.begin()),      AsPointer(unanimatedVertexLayout.end()),
                            AsPointer(sourceGeo._mainDrawInputAssembly._vertexInputLayout.begin()), AsPointer(sourceGeo._mainDrawInputAssembly._vertexInputLayout.end()),
                            AsPointer(unifiedVertexReordering.begin()),     AsPointer(unifiedVertexReordering.end()));

        CopyVertexElements( animatedVertexBuffer.get(),                     animatedVertexStride,
                            sourceGeo._vertices.get(),                      sourceGeo._mainDrawInputAssembly._vertexStride,
                            AsPointer(animatedVertexLayout.begin()),        AsPointer(animatedVertexLayout.end()),
                            AsPointer(sourceGeo._mainDrawInputAssembly._vertexInputLayout.begin()), AsPointer(sourceGeo._mainDrawInputAssembly._vertexInputLayout.end()),
                            AsPointer(unifiedVertexReordering.begin()),     AsPointer(unifiedVertexReordering.end()));

            //      We have to remap the index buffer, also.
        std::unique_ptr<uint8[]> newIndexBuffer = std::make_unique<uint8[]>(sourceGeo._indices.size());
        if (sourceGeo._indexFormat == Metal::NativeFormat::R16_UINT) {
            std::transform(
                (const uint16*)sourceGeo._indices.begin(), (const uint16*)sourceGeo._indices.end(),
                (uint16*)newIndexBuffer.get(),
                [&unifiedVertexReordering](uint16 inputIndex){return unifiedVertexReordering[inputIndex];});
        } else if (sourceGeo._indexFormat == Metal::NativeFormat::R8_UINT) {
            std::transform(
                (const uint8*)sourceGeo._indices.begin(), (const uint8*)sourceGeo._indices.end(),
                (uint8*)newIndexBuffer.get(),
                [&unifiedVertexReordering](uint8 inputIndex){return unifiedVertexReordering[inputIndex];});
        } else {
            ThrowException(FormatError("Unrecognised index format when instantiating skin controller in node (%s).", nodeName));
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
                AsPointer(controller._bucket[b]._vertexInputLayout.begin()), 
                AsPointer(controller._bucket[b]._vertexInputLayout.end()));

            if (controller._bucket[b]._vertexBufferSize) {
                if (bucketVertexSizes[b] > destinationWeightVertexStride) {
                    destinationWeightVertexStride = bucketVertexSizes[b];
                    finalWeightBufferFormat = &controller._bucket[b]._vertexInputLayout;
                }
            }
        }

        if (destinationWeightVertexStride) {
            unsigned alignedDestinationWeightVertexStride = (unsigned)std::max(destinationWeightVertexStride, size_t(4));
            if (alignedDestinationWeightVertexStride != destinationWeightVertexStride) {
                LogAlwaysWarningF("LogAlwaysWarningF -- vertex buffer had to be expanded for vertex alignment restrictions in node (%s). This will leave some wasted space in the vertex buffer. This can be caused when using skinning when only 1 weight is really required.\n", nodeName);
                destinationWeightVertexStride = alignedDestinationWeightVertexStride;
            }
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
                    auto i = std::find( 
                        controller._bucket[b]._vertexBindings.begin(), 
                        controller._bucket[b]._vertexBindings.end(), 
                        sourceVertexPositionIndex);

                    if (i!=controller._bucket[b]._vertexBindings.end()) {

                            //
                            //      Note that sometimes we'll be expanding the vertex format in this process
                            //      If some buckets are using R8G8, and others are R8G8B8A8 (for example)
                            //      then they will all be expanded to the largest size
                            //

                        auto sourceVertexStride = bucketVertexSizes[b];
                        size_t sourceVertexInThisBucket = std::distance(controller._bucket[b]._vertexBindings.begin(), i);
                        void* destinationVertex = PtrAdd(skeletonBindingVertices.get(), destinationVertexIndex*destinationWeightVertexStride);
                        assert((sourceVertexInThisBucket+1)*sourceVertexStride <= controller._bucket[b]._vertexBufferSize);
                        const void* sourceVertex = PtrAdd(controller._bucket[b]._vertexBufferData.get(), sourceVertexInThisBucket*sourceVertexStride);

                        if (sourceVertexStride == destinationWeightVertexStride) {
                            XlCopyMemory(destinationVertex, sourceVertex, sourceVertexStride);
                        } else {
                            const Metal::InputElementDesc* dstElement = AsPointer(finalWeightBufferFormat->cbegin());
                            for (   auto srcElement=controller._bucket[b]._vertexInputLayout.cbegin(); 
                                    srcElement!=controller._bucket[b]._vertexInputLayout.cend(); ++srcElement, ++dstElement) {
                                unsigned elementSize = std::min(Metal::BitsPerPixel(srcElement->_nativeFormat)/8, Metal::BitsPerPixel(dstElement->_nativeFormat)/8);
                                assert(PtrAdd(destinationVertex, dstElement->_alignedByteOffset+elementSize) <= PtrAdd(skeletonBindingVertices.get(), destinationWeightVertexStride*unifiedVertexCount));
                                assert(PtrAdd(sourceVertex, srcElement->_alignedByteOffset+elementSize) <= PtrAdd(controller._bucket[b]._vertexBufferData.get(), controller._bucket[b]._vertexBufferSize));
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
            DynamicArray<uint8>(std::move(newIndexBuffer), sourceGeo._indices.size()));

        result._skeletonBindingVertexStride = (unsigned)destinationWeightVertexStride;
        result._animatedVertexBufferSize = (unsigned)(animatedVertexStride*unifiedVertexCount);

        result._inverseBindMatrices = DynamicArray<Float4x4>::Copy(controller._inverseBindMatrices);
        result._jointMatrices = std::move(jointMatrices);
        result._bindShapeMatrix = controller._bindShapeMatrix;

        result._mainDrawCalls = sourceGeo._mainDrawCalls;
        result._mainDrawUnanimatedIA._vertexStride = unanimatedVertexStride;
        result._mainDrawUnanimatedIA._vertexInputLayout = std::move(unanimatedVertexLayout);
        result._indexFormat = sourceGeo._indexFormat;

        result._mainDrawAnimatedIA._vertexStride = animatedVertexStride;
        result._mainDrawAnimatedIA._vertexInputLayout = std::move(animatedVertexLayout);

        result._preskinningDrawCalls = preskinningDrawCalls;

        if (finalWeightBufferFormat) {
            result._preskinningIA._vertexInputLayout = *finalWeightBufferFormat;
            result._preskinningIA._vertexStride = (unsigned)destinationWeightVertexStride;
        }

        result._localBoundingBox = boundingBox;
        return std::move(result);
    }


}}

