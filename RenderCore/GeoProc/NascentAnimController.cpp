// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS   // warning C4996: 'std::_Copy_impl': Function call with parameters that may be unsafe

#include "NascentAnimController.h"
#include "NascentRawGeometry.h"
#include "GeometryAlgorithm.h"
#include "GeoProcUtil.h"
#include "../Assets/AssetUtils.h"
#include "../Format.h"
#include "../Types.h"
#include "../../Assets/BlockSerializer.h"
#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/LogUtil.h"
#include "../../Assets/Assets.h"
#include "../../Math/Transformations.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/StreamUtils.h"

namespace RenderCore { namespace Assets { namespace GeoProc
{
    static const bool SkinNormals = true;

    NascentBoundSkinnedGeometry BindController(
        const NascentRawGeometry& sourceGeo,
        const UnboundSkinController& controller,
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

        std::vector<std::pair<uint32,uint32>> unifiedVertexIndexToBucketIndex;
        unifiedVertexIndexToBucketIndex.reserve(unifiedVertexCount);

        for (uint32 c=0; c<unifiedVertexCount; ++c) {
            uint32 positionIndex = sourceGeo._unifiedVertexIndexToPositionIndex[c];
            uint32 bucketIndex   = controller._positionIndexToBucketIndex[positionIndex];
            unifiedVertexIndexToBucketIndex.push_back(std::make_pair(c, bucketIndex));
        }

            //
            //      Resort by bucket index...
            //

        std::sort(unifiedVertexIndexToBucketIndex.begin(), unifiedVertexIndexToBucketIndex.end(), CompareSecond<uint32, uint32>());

        std::vector<uint32> unifiedVertexReordering;       // unifiedVertexReordering[oldIndex] = newIndex;
        std::vector<uint32> newUnifiedVertexIndexToPositionIndex;
        unifiedVertexReordering.resize(unifiedVertexCount, (uint32)~uint32(0x0));
        newUnifiedVertexIndexToPositionIndex.resize(unifiedVertexCount, (uint32)~uint32(0x0));

            //
            //      \todo --    it would better if we tried to maintain the vertex ordering within
            //                  the bucket. That is, the relative positions of vertices within the
            //                  bucket should be the same as the relative positions of those vertices
            //                  as they were in the original
            //

        uint32 indexAccumulator = 0;
        const size_t bucketCount = dimof(((UnboundSkinController*)nullptr)->_bucket);
        uint32 bucketStart  [bucketCount];
        uint32 bucketEnd    [bucketCount];
        uint32 currentBucket = 0; bucketStart[0] = 0;
        for (auto i=unifiedVertexIndexToBucketIndex.cbegin(); i!=unifiedVertexIndexToBucketIndex.cend(); ++i) {
            if ((i->second >> 16)!=currentBucket) {
                bucketEnd[currentBucket] = indexAccumulator;
                bucketStart[++currentBucket] = indexAccumulator;
            }
            uint32 newIndex = indexAccumulator++;
            uint32 oldIndex = i->first;
            unifiedVertexReordering[oldIndex] = newIndex;
            newUnifiedVertexIndexToPositionIndex[newIndex] = (uint32)sourceGeo._unifiedVertexIndexToPositionIndex[oldIndex];
        }
        bucketEnd[currentBucket] = indexAccumulator;
        for (unsigned b=currentBucket+1; b<bucketCount; ++b) {
            bucketStart[b] = bucketEnd[b] = indexAccumulator;
        }
        if (indexAccumulator != unifiedVertexCount) {
            Throw(::Exceptions::BasicLabel("Vertex count mismatch in node (%s)", nodeName));
        }

            //
            //      Move vertex data for vertex elements that will be skinned into a separate vertex buffer
            //      Note that we don't really know which elements will be skinned. We can assume that at
            //      least "POSITION" will be skinned. But actually this is defined by the particular
            //      shader. We could wait until binding with the material to make this decision...?
            //
        auto unanimatedVertexLayout = sourceGeo._mainDrawInputAssembly._elements;
        decltype(unanimatedVertexLayout) animatedVertexLayout;

        for (auto i=unanimatedVertexLayout.begin(); i!=unanimatedVertexLayout.end();) {
            const bool mustBeSkinned = 
                std::find_if(   elementsToBeSkinned.begin(), elementsToBeSkinned.end(), 
                                [&](const std::string& s){ return !XlCompareStringI(i->_semanticName, s.c_str()); }) 
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
                elementOffset += BitsPerPixel(i->_nativeFormat)/8;
            }
            elementOffset = 0;
            for (auto i=animatedVertexLayout.begin(); i!=animatedVertexLayout.end();++i) {
                i->_alignedByteOffset = elementOffset;
                elementOffset += BitsPerPixel(i->_nativeFormat)/8;
            }
        }

        unsigned unanimatedVertexStride  = CalculateVertexSize(AsPointer(unanimatedVertexLayout.begin()), AsPointer(unanimatedVertexLayout.end()));
        unsigned animatedVertexStride    = CalculateVertexSize(AsPointer(animatedVertexLayout.begin()), AsPointer(animatedVertexLayout.end()));

        if (!animatedVertexStride) {
            Throw(::Exceptions::BasicLabel("Could not find any animated vertex elements in skinning controller in node (%s). There must be a problem with vertex input semantics.", nodeName));
        }
                            
            //      Copy out those parts of the vertex buffer that are unanimated and animated
            //      (we also do the vertex reordering here)
        std::unique_ptr<uint8[]> unanimatedVertexBuffer  = std::make_unique<uint8[]>(unanimatedVertexStride*unifiedVertexCount);
        std::unique_ptr<uint8[]> animatedVertexBuffer    = std::make_unique<uint8[]>(animatedVertexStride*unifiedVertexCount);
        CopyVertexElements( unanimatedVertexBuffer.get(),                   unanimatedVertexStride, 
                            sourceGeo._vertices.get(),                      sourceGeo._mainDrawInputAssembly._vertexStride,
                            AsPointer(unanimatedVertexLayout.begin()),      AsPointer(unanimatedVertexLayout.end()),
                            AsPointer(sourceGeo._mainDrawInputAssembly._elements.begin()), AsPointer(sourceGeo._mainDrawInputAssembly._elements.end()),
                            AsPointer(unifiedVertexReordering.begin()),     AsPointer(unifiedVertexReordering.end()));

        CopyVertexElements( animatedVertexBuffer.get(),                     animatedVertexStride,
                            sourceGeo._vertices.get(),                      sourceGeo._mainDrawInputAssembly._vertexStride,
                            AsPointer(animatedVertexLayout.begin()),        AsPointer(animatedVertexLayout.end()),
                            AsPointer(sourceGeo._mainDrawInputAssembly._elements.begin()), AsPointer(sourceGeo._mainDrawInputAssembly._elements.end()),
                            AsPointer(unifiedVertexReordering.begin()),     AsPointer(unifiedVertexReordering.end()));

            //      We have to remap the index buffer, also.
        std::unique_ptr<uint8[]> newIndexBuffer = std::make_unique<uint8[]>(sourceGeo._indices.size());
        if (sourceGeo._indexFormat == Format::R32_UINT) {
            std::transform(
                (const uint32*)sourceGeo._indices.begin(), (const uint32*)sourceGeo._indices.end(),
                (uint32*)newIndexBuffer.get(),
                [&unifiedVertexReordering](uint32 inputIndex) { return unifiedVertexReordering[inputIndex]; });
        } else if (sourceGeo._indexFormat == Format::R16_UINT) {
            std::transform(
                (const uint16*)sourceGeo._indices.begin(), (const uint16*)sourceGeo._indices.end(),
                (uint16*)newIndexBuffer.get(),
                [&unifiedVertexReordering](uint16 inputIndex) -> uint16 { auto result = unifiedVertexReordering[inputIndex]; assert(result <= 0xffff); return (uint16)result; });
        } else if (sourceGeo._indexFormat == Format::R8_UINT) {
            std::transform(
                (const uint8*)sourceGeo._indices.begin(), (const uint8*)sourceGeo._indices.end(),
                (uint8*)newIndexBuffer.get(),
                [&unifiedVertexReordering](uint8 inputIndex) -> uint8 { auto result = unifiedVertexReordering[inputIndex]; assert(result <= 0xff); return (uint8)result; });
        } else {
            Throw(::Exceptions::BasicLabel("Unrecognised index format when instantiating skin controller in node (%s).", nodeName));
        }
                                
            //      We have to define the draw calls that perform the pre-skinning step

        std::vector<DrawCallDesc> preskinningDrawCalls;
        if (bucketEnd[0] > bucketStart[0]) {
            preskinningDrawCalls.push_back(DrawCallDesc(
                ~unsigned(0x0), bucketEnd[0] - bucketStart[0], bucketStart[0],
                4, Topology::PointList));
        }
        if (bucketEnd[1] > bucketStart[1]) {
            preskinningDrawCalls.push_back(DrawCallDesc(
                ~unsigned(0x0), bucketEnd[1] - bucketStart[1], bucketStart[1],
                2, Topology::PointList));
        }
        if (bucketEnd[2] > bucketStart[2]) {
            preskinningDrawCalls.push_back(DrawCallDesc(
                ~unsigned(0x0), bucketEnd[2] - bucketStart[2], bucketStart[2],
                1, Topology::PointList));
        }

        assert(bucketEnd[2] <= unifiedVertexCount);

            //      Build the final vertex weights buffer (our weights are currently stored
            //      per vertex-position. So we need to expand to per-unified vertex -- blaggh!)
            //      This means the output weights vertex buffer is going to be larger than input ones combined.

        assert(newUnifiedVertexIndexToPositionIndex.size()==unifiedVertexCount);
        size_t destinationWeightVertexStride = 0;
        const std::vector<InputElementDesc>* finalWeightBufferFormat = nullptr;

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
                LogLine(Warning, "vertex buffer had to be expanded for vertex alignment restrictions in node {}. This will leave some wasted space in the vertex buffer. This can be caused when using skinning when only 1 weight is really required.\n", nodeName);
                destinationWeightVertexStride = alignedDestinationWeightVertexStride;
            }
        }

        std::unique_ptr<uint8[]> skeletonBindingVertices;
        if (destinationWeightVertexStride && finalWeightBufferFormat) {
            skeletonBindingVertices = std::make_unique<uint8[]>(destinationWeightVertexStride*unifiedVertexCount);
            XlSetMemory(skeletonBindingVertices.get(), 0, destinationWeightVertexStride*unifiedVertexCount);

            for (auto i2=newUnifiedVertexIndexToPositionIndex.begin(); i2!=newUnifiedVertexIndexToPositionIndex.end(); ++i2) {
                const size_t destinationVertexIndex = i2-newUnifiedVertexIndexToPositionIndex.begin();
                unsigned sourceVertexPositionIndex = *i2;
                                
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
                            const InputElementDesc* dstElement = AsPointer(finalWeightBufferFormat->cbegin());
                            for (   auto srcElement=controller._bucket[b]._vertexInputLayout.cbegin(); 
                                    srcElement!=controller._bucket[b]._vertexInputLayout.cend(); ++srcElement, ++dstElement) {
                                unsigned elementSize = std::min(BitsPerPixel(srcElement->_nativeFormat)/8, BitsPerPixel(dstElement->_nativeFormat)/8);
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
        auto positionDesc = FindPositionElement(
            AsPointer(animatedVertexLayout.begin()),
            animatedVertexLayout.size());
        if (positionDesc._nativeFormat != Format::Unknown) {
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
        result._mainDrawUnanimatedIA._elements = std::move(unanimatedVertexLayout);
        result._indexFormat = sourceGeo._indexFormat;

        result._mainDrawAnimatedIA._vertexStride = animatedVertexStride;
        result._mainDrawAnimatedIA._elements = std::move(animatedVertexLayout);

        result._preskinningDrawCalls = preskinningDrawCalls;

        if (finalWeightBufferFormat) {
            result._preskinningIA = RenderCore::Assets::CreateGeoInputAssembly(
                *finalWeightBufferFormat, (unsigned)destinationWeightVertexStride);
        }

        result._localBoundingBox = boundingBox;
        return std::move(result);
    }


///////////////////////////////////////////////////////////////////////////////////////////////////


    void NascentBoundSkinnedGeometry::Serialize( 
        Serialization::NascentBlockSerializer& outputSerializer, 
        std::vector<uint8>& largeResourcesBlock) const
    {
        using namespace Serialization;

        auto vbOffset0 = largeResourcesBlock.size();
        auto vbSize0 = _unanimatedVertexElements.size();
        largeResourcesBlock.insert(largeResourcesBlock.end(), _unanimatedVertexElements.begin(), _unanimatedVertexElements.end());

        auto vbOffset1 = largeResourcesBlock.size();
        auto vbSize1 = _animatedVertexElements.size();
        largeResourcesBlock.insert(largeResourcesBlock.end(), _animatedVertexElements.begin(), _animatedVertexElements.end());

        auto vbOffset2 = largeResourcesBlock.size();
        auto vbSize2 = _skeletonBinding.size();
        largeResourcesBlock.insert(largeResourcesBlock.end(), _skeletonBinding.begin(), _skeletonBinding.end());

        auto ibOffset = largeResourcesBlock.size();
        auto ibSize = _indices.size();
        largeResourcesBlock.insert(largeResourcesBlock.end(), _indices.begin(), _indices.end());

            // first part is just like "NascentRawGeometry::Serialize"
        ::Serialize(
            outputSerializer, 
            RenderCore::Assets::VertexData 
                { _mainDrawUnanimatedIA, unsigned(vbOffset0), unsigned(vbSize0) });

        ::Serialize(
            outputSerializer, 
            RenderCore::Assets::IndexData 
                { _indexFormat, unsigned(ibOffset), unsigned(ibSize) });

        ::Serialize(outputSerializer, _mainDrawCalls);

            // append skinning related information
        ::Serialize(
            outputSerializer, 
            RenderCore::Assets::VertexData 
                { _mainDrawAnimatedIA, unsigned(vbOffset1), unsigned(vbSize1) });
        ::Serialize(
            outputSerializer, 
            RenderCore::Assets::VertexData 
                { _preskinningIA, unsigned(vbOffset2), unsigned(vbSize2) });

        outputSerializer.SerializeSubBlock(MakeIteratorRange(_inverseBindMatrices));
        outputSerializer.SerializeValue(_inverseBindMatrices.size());

        DynamicArray<Float4x4> inverseBindByBindShape = DynamicArray<Float4x4>::Copy(_inverseBindMatrices);
        for (unsigned c=0; c<inverseBindByBindShape.size(); ++c) {
            inverseBindByBindShape[c] = Combine(
                _bindShapeMatrix,
                inverseBindByBindShape[c]);
        }
        outputSerializer.SerializeSubBlock(MakeIteratorRange(inverseBindByBindShape.cbegin(), inverseBindByBindShape.cend()));
        outputSerializer.SerializeValue(inverseBindByBindShape.size());
        outputSerializer.SerializeSubBlock(MakeIteratorRange(_jointMatrices));
        outputSerializer.SerializeValue(_jointMatrices.size());
        
        ::Serialize(outputSerializer, _bindShapeMatrix);

        outputSerializer.SerializeSubBlock(MakeIteratorRange(_preskinningDrawCalls));
        outputSerializer.SerializeValue(_preskinningDrawCalls.size());

        ::Serialize(outputSerializer, _localBoundingBox.first);
        ::Serialize(outputSerializer, _localBoundingBox.second);
    }

    NascentBoundSkinnedGeometry::NascentBoundSkinnedGeometry(     DynamicArray<uint8>&& unanimatedVertexElements,
                                                    DynamicArray<uint8>&& animatedVertexElements,
                                                    DynamicArray<uint8>&& skeletonBinding,
                                                    DynamicArray<uint8>&& indices)
    :       _unanimatedVertexElements(std::forward<DynamicArray<uint8>>(unanimatedVertexElements))
    ,       _animatedVertexElements(std::forward<DynamicArray<uint8>>(animatedVertexElements))
    ,       _skeletonBinding(std::forward<DynamicArray<uint8>>(skeletonBinding))
    ,       _indices(std::forward<DynamicArray<uint8>>(indices))
    ,       _inverseBindMatrices(nullptr, 0)
    ,       _jointMatrices(nullptr, 0)
    ,       _animatedVertexBufferSize(0)
    ,       _localBoundingBox(InvalidBoundingBox())
    ,       _indexFormat(Format::Unknown)
    {
    }

    NascentBoundSkinnedGeometry::NascentBoundSkinnedGeometry(NascentBoundSkinnedGeometry&& moveFrom)
    :       _unanimatedVertexElements(std::move(moveFrom._unanimatedVertexElements))
    ,       _animatedVertexElements(std::move(moveFrom._animatedVertexElements))
    ,       _skeletonBinding(std::move(moveFrom._skeletonBinding))
    ,       _skeletonBindingVertexStride(moveFrom._skeletonBindingVertexStride)
    ,       _indices(std::move(moveFrom._indices))
    ,       _inverseBindMatrices(std::move(moveFrom._inverseBindMatrices))
    ,       _jointMatrices(std::move(moveFrom._jointMatrices))
    ,       _bindShapeMatrix(moveFrom._bindShapeMatrix)
    ,       _mainDrawCalls(std::move(moveFrom._mainDrawCalls))
    ,       _mainDrawUnanimatedIA(std::move(moveFrom._mainDrawUnanimatedIA))
    ,       _mainDrawAnimatedIA(std::move(moveFrom._mainDrawAnimatedIA))
    ,       _preskinningDrawCalls(std::move(moveFrom._preskinningDrawCalls))
    ,       _preskinningIA(std::move(moveFrom._preskinningIA))
    ,       _animatedVertexBufferSize(moveFrom._animatedVertexBufferSize)
    ,       _localBoundingBox(moveFrom._localBoundingBox)
    ,       _indexFormat(moveFrom._indexFormat)
    {}

    NascentBoundSkinnedGeometry& NascentBoundSkinnedGeometry::operator=(NascentBoundSkinnedGeometry&& moveFrom)
    {
        _unanimatedVertexElements = std::move(moveFrom._unanimatedVertexElements);
        _animatedVertexElements = std::move(moveFrom._animatedVertexElements);
        _skeletonBinding = std::move(moveFrom._skeletonBinding);
        _skeletonBindingVertexStride = moveFrom._skeletonBindingVertexStride;
        _indices = std::move(moveFrom._indices);
        _inverseBindMatrices = std::move(moveFrom._inverseBindMatrices);
        _jointMatrices = std::move(moveFrom._jointMatrices);
        _bindShapeMatrix = moveFrom._bindShapeMatrix;
        _mainDrawCalls = std::move(moveFrom._mainDrawCalls);
        _mainDrawUnanimatedIA = std::move(moveFrom._mainDrawUnanimatedIA);
        _mainDrawAnimatedIA = std::move(moveFrom._mainDrawAnimatedIA);
        _preskinningDrawCalls = std::move(moveFrom._preskinningDrawCalls);
        _preskinningIA = std::move(moveFrom._preskinningIA);
        _animatedVertexBufferSize = moveFrom._animatedVertexBufferSize;
        _localBoundingBox = moveFrom._localBoundingBox;
        _indexFormat = moveFrom._indexFormat;
        return *this;
    }




        



    UnboundSkinController::Bucket::Bucket() { _weightCount = 0; _vertexBufferSize = 0; }
    UnboundSkinController::Bucket::Bucket(Bucket&& moveFrom) never_throws
    :       _vertexInputLayout(std::move(moveFrom._vertexInputLayout))
    ,       _weightCount(moveFrom._weightCount)
    ,       _vertexBufferData(std::move(moveFrom._vertexBufferData))
    ,       _vertexBufferSize(moveFrom._vertexBufferSize)
    ,       _vertexBindings(std::move(moveFrom._vertexBindings))
    {
    }

    auto UnboundSkinController::Bucket::operator=(Bucket&& moveFrom) never_throws -> Bucket&
    {
        _vertexInputLayout = std::move(moveFrom._vertexInputLayout);
        _weightCount = moveFrom._weightCount;
        _vertexBufferData = std::move(moveFrom._vertexBufferData);
        _vertexBufferSize = moveFrom._vertexBufferSize;
        _vertexBindings = std::move(moveFrom._vertexBindings);
        return *this;
    }

    UnboundSkinController::UnboundSkinController(   
        Bucket&& bucket4, Bucket&& bucket2, Bucket&& bucket1, Bucket&& bucket0,
        DynamicArray<Float4x4>&& inverseBindMatrices, const Float4x4& bindShapeMatrix,
        std::vector<std::basic_string<utf8>>&& jointNames,
        NascentObjectGuid sourceRef,
        std::vector<uint32>&& vertexPositionToBucketIndex)
    :       _inverseBindMatrices(std::forward<DynamicArray<Float4x4>>(inverseBindMatrices))
    ,       _bindShapeMatrix(bindShapeMatrix)
    ,       _positionIndexToBucketIndex(vertexPositionToBucketIndex)
    ,       _jointNames(jointNames)
    ,       _sourceRef(sourceRef)
    {
        _bucket[0] = std::forward<Bucket>(bucket4);
        _bucket[1] = std::forward<Bucket>(bucket2);
        _bucket[2] = std::forward<Bucket>(bucket1);
        _bucket[3] = std::forward<Bucket>(bucket0);
    }

    UnboundSkinController::UnboundSkinController(UnboundSkinController&& moveFrom) never_throws
    :       _inverseBindMatrices(std::move(moveFrom._inverseBindMatrices))
    ,       _bindShapeMatrix(moveFrom._bindShapeMatrix)
    ,       _positionIndexToBucketIndex(std::move(moveFrom._positionIndexToBucketIndex))
    ,       _jointNames(moveFrom._jointNames)
    ,       _sourceRef(moveFrom._sourceRef)
    {
        for (unsigned c=0; c<dimof(_bucket); ++c)
            _bucket[c] = std::move(moveFrom._bucket[c]);
    }

    UnboundSkinController& UnboundSkinController::operator=(UnboundSkinController&& moveFrom) never_throws
    {
        for (unsigned c=0; c<dimof(_bucket); ++c)
            _bucket[c] = std::move(moveFrom._bucket[c]);
        _inverseBindMatrices = std::move(moveFrom._inverseBindMatrices);
        _bindShapeMatrix = moveFrom._bindShapeMatrix;
        _positionIndexToBucketIndex = std::move(moveFrom._positionIndexToBucketIndex);
        _jointNames = std::move(moveFrom._jointNames);
        _sourceRef = moveFrom._sourceRef;
        return *this;
    }


    UnboundMorphController::UnboundMorphController()
    {}

    UnboundMorphController::UnboundMorphController(UnboundMorphController&& moveFrom) never_throws
    :       _source(std::move(moveFrom._source))
    {}

    UnboundMorphController& UnboundMorphController::operator=(UnboundMorphController&& moveFrom) never_throws
    {
        _source = std::move(moveFrom._source);
        return *this;
    }

    std::ostream& StreamOperator(std::ostream& stream, const NascentBoundSkinnedGeometry& geo)
    {
        using namespace RenderCore::Assets::Operators;
        stream << "   Unanimated VB bytes: " << ByteCount(geo._unanimatedVertexElements.size()) << " (" << geo._unanimatedVertexElements.size() / std::max(1u, geo._mainDrawUnanimatedIA._vertexStride) << "*" << geo._mainDrawUnanimatedIA._vertexStride << ")" << std::endl;
        stream << "     Animated VB bytes: " << ByteCount(geo._animatedVertexElements.size()) << " (" << geo._animatedVertexElements.size() / std::max(1u, geo._mainDrawAnimatedIA._vertexStride) << "*" << geo._mainDrawAnimatedIA._vertexStride << ")" << std::endl;
        stream << "Skele binding VB bytes: " << ByteCount(geo._skeletonBinding.size()) << " (" << geo._skeletonBinding.size() / std::max(1u, geo._skeletonBindingVertexStride) << "*" << geo._skeletonBindingVertexStride << ")" << std::endl;
        stream << "     Animated VB bytes: " << ByteCount(geo._animatedVertexBufferSize) << " (" << geo._animatedVertexBufferSize / std::max(1u, geo._mainDrawAnimatedIA._vertexStride) << "*" << geo._mainDrawAnimatedIA._vertexStride << ")" << std::endl;
        stream << "              IB bytes: " << ByteCount(geo._indices.size()) << " (" << (geo._indices.size()*8/BitsPerPixel(geo._indexFormat)) << "*" << BitsPerPixel(geo._indexFormat)/8 << ")" << std::endl;
        stream << " Unanimated IA: " << geo._mainDrawUnanimatedIA << std::endl;
        stream << "   Animated IA: " << geo._mainDrawAnimatedIA << std::endl;
        stream << "Preskinning IA: " << geo._preskinningIA << std::endl;
        stream << "Index fmt: " << AsString(geo._indexFormat) << std::endl;
        unsigned c=0;
        for(const auto& dc:geo._mainDrawCalls)
            stream << "Draw [" << c++ << "] " << dc << std::endl;
        c=0;
        for(const auto& dc:geo._preskinningDrawCalls)
            stream << "Preskinning Draw [" << c++ << "] " << dc << std::endl;

        stream << "Joint matrices: ";
        for (size_t q=0; q<geo._jointMatrices.size(); ++q) {
            if (q != 0) stream << ", ";
            stream << geo._jointMatrices[q];
        }
        stream << std::endl;
        return stream;
    }


}}}

