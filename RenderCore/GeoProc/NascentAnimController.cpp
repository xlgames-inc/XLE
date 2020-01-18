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
#include "../VertexUtil.h"
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

	static uint32_t ControllerAndBucketIndex(unsigned controllerIdx, unsigned bucketIdx)
	{
		assert((bucketIdx & ~0x3ffff) == 0);
		return (controllerIdx << 18) | (bucketIdx & 0x3ffff);
	}

    NascentBoundSkinnedGeometry BindController(
        const NascentRawGeometry& sourceGeo,
        IteratorRange<const UnboundSkinControllerAndJointMatrices*> controllers,
        const char nodeName[])
    {
		assert(!controllers.empty());

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
                            
        size_t unifiedVertexCount = sourceGeo._unifiedVertexCount;
		assert(sourceGeo._unifiedVertexIndexToPositionIndex.empty() || sourceGeo._unifiedVertexIndexToPositionIndex.size() >= unifiedVertexCount);

		struct VertexIndices
		{
			uint32_t _inputUnifiedVertexIndex;
			uint32_t _bucketIndex;
			uint32_t _inputPositionIndex;
		};
        std::vector<VertexIndices> vertexMappingByFinalOrdering;
        vertexMappingByFinalOrdering.reserve(unifiedVertexCount);

        for (uint32_t c=0; c<unifiedVertexCount; ++c) {
            uint32_t positionIndex = c < sourceGeo._unifiedVertexIndexToPositionIndex.size() ? sourceGeo._unifiedVertexIndexToPositionIndex[c] : c;
			uint32_t bucketIndex = ~0u;

			unsigned controllerIdx=0;
			for (; controllerIdx!=controllers.size(); ++controllerIdx) {
				auto& controller = *controllers[controllerIdx]._controller;
				if (positionIndex < controller._positionIndexToBucketIndex.size() && controller._positionIndexToBucketIndex[positionIndex] != ~0u) {
					bucketIndex = ControllerAndBucketIndex(controllerIdx, controller._positionIndexToBucketIndex[positionIndex]);
					break;
				}
			}
			// If we did not get an assigment, our bucketIndex will still be ~0u
			assert(bucketIndex != ~0u);
			// A vertex can actually get associated with multiple controllers
			// In these cases, we always select the first controller that applies to the vertex
			// (basically under the assumption that all controllers will generate the same final position for the vertex)
			// In general, we don't want to split vertices for controllers -- because that could lead
			// to situations where the animation causes the mesh manifold to separate (which doesn't
			// seem like something that would be desireable)
			vertexMappingByFinalOrdering.push_back(VertexIndices{c, bucketIndex, positionIndex});
        }

            //
            //      Resort by bucket index...
            //

        std::stable_sort(
			vertexMappingByFinalOrdering.begin(), vertexMappingByFinalOrdering.end(), 
			[](const VertexIndices& lhs, const VertexIndices& rhs) { return lhs._bucketIndex < rhs._bucketIndex; });

            //
			//		We create a new reordering for the vertices based on the bucket assigment.
			//		This will reorganize the vertex buffer so that buckets are sequential in the VB
			//
            //      \todo --    it would better if we tried to maintain the vertex ordering within
            //                  the bucket. That is, the relative positions of vertices within the
            //                  bucket should be the same as the relative positions of those vertices
            //                  as they were in the original
            //

        const size_t bucketCount = dimof(UnboundSkinController::_bucket) * controllers.size();
        std::vector<size_t> bucketStart(bucketCount, 0);
        std::vector<size_t> bucketEnd(bucketCount, 0);

		{
			uint32_t currentBucket = ~0u;
			for (size_t i=0; i<vertexMappingByFinalOrdering.size(); ++i) {
				auto thisBucket = (vertexMappingByFinalOrdering[i]._bucketIndex >> 16);
				if (thisBucket!=currentBucket) {
					if (currentBucket < bucketCount)
						bucketEnd[currentBucket] = i;
					assert(currentBucket == ~0u || thisBucket > currentBucket);
					currentBucket = thisBucket;
					bucketStart[currentBucket] = i;
				}
			}
			if (currentBucket < bucketCount)
				bucketEnd[currentBucket] = vertexMappingByFinalOrdering.size();
			for (unsigned b=currentBucket+1; b<bucketCount; ++b) {
				bucketStart[b] = bucketEnd[b] = vertexMappingByFinalOrdering.size();
			}
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

        unsigned unanimatedVertexStride  = CalculateVertexSize(MakeIteratorRange(unanimatedVertexLayout));
        unsigned animatedVertexStride    = CalculateVertexSize(MakeIteratorRange(animatedVertexLayout));

        if (!animatedVertexStride) {
            Throw(::Exceptions::BasicLabel("Could not find any animated vertex elements in skinning controller in node (%s). There must be a problem with vertex input semantics.", nodeName));
        }
                            
            //      Copy out those parts of the vertex buffer that are unanimated and animated
            //      (we also do the vertex reordering here)
		std::vector<uint8_t> unanimatedVertexBuffer(unanimatedVertexStride*unifiedVertexCount);
		std::vector<uint8_t> animatedVertexBuffer(animatedVertexStride*unifiedVertexCount);
		std::vector<uint8_t> newIndexBuffer(sourceGeo._indices.size());
		{
			std::vector<uint32_t> vertexOrdering;       // unifiedVertexReordering[oldIndex] = newIndex;
			vertexOrdering.resize(unifiedVertexCount, (uint32_t)~uint32_t(0x0));
			for (auto i=vertexMappingByFinalOrdering.cbegin(); i!=vertexMappingByFinalOrdering.cend(); ++i)
				vertexOrdering[i->_inputUnifiedVertexIndex] = (unsigned)std::distance(vertexMappingByFinalOrdering.cbegin(), i);
			
			CopyVertexElements(
				MakeIteratorRange(unanimatedVertexBuffer),	unanimatedVertexStride, 
				MakeIteratorRange(sourceGeo._vertices),		sourceGeo._mainDrawInputAssembly._vertexStride,
				MakeIteratorRange(unanimatedVertexLayout),
				MakeIteratorRange(sourceGeo._mainDrawInputAssembly._elements),
				MakeIteratorRange(vertexOrdering));

			CopyVertexElements(
				MakeIteratorRange(animatedVertexBuffer),	animatedVertexStride,
				MakeIteratorRange(sourceGeo._vertices),		sourceGeo._mainDrawInputAssembly._vertexStride,
				MakeIteratorRange(animatedVertexLayout),
				MakeIteratorRange(sourceGeo._mainDrawInputAssembly._elements),
				MakeIteratorRange(vertexOrdering));

				//      We have to remap the index buffer, also.
			if (sourceGeo._indexFormat == Format::R32_UINT) {
				std::transform(
					(const uint32_t*)AsPointer(sourceGeo._indices.begin()), (const uint32_t*)AsPointer(sourceGeo._indices.end()),
					(uint32_t*)newIndexBuffer.data(),
					[&vertexOrdering](uint32_t inputIndex) { return vertexOrdering[inputIndex]; });
			} else if (sourceGeo._indexFormat == Format::R16_UINT) {
				std::transform(
					(const uint16_t*)AsPointer(sourceGeo._indices.begin()), (const uint16_t*)AsPointer(sourceGeo._indices.end()),
					(uint16_t*)newIndexBuffer.data(),
					[&vertexOrdering](uint16_t inputIndex) -> uint16_t { auto result = vertexOrdering[inputIndex]; assert(result <= 0xffff); return (uint16_t)result; });
			} else if (sourceGeo._indexFormat == Format::R8_UINT) {
				std::transform(
					(const uint8*)AsPointer(sourceGeo._indices.begin()), (const uint8*)AsPointer(sourceGeo._indices.end()),
					(uint8_t*)newIndexBuffer.data(),
					[&vertexOrdering](uint8 inputIndex) -> uint8 { auto result = vertexOrdering[inputIndex]; assert(result <= 0xff); return (uint8)result; });
			} else {
				Throw(::Exceptions::BasicLabel("Unrecognised index format when instantiating skin controller in node (%s).", nodeName));
			}
		}

            //      Build the final vertex weights buffer (our weights are currently stored
            //      per vertex-position. So we need to expand to per-unified vertex -- blaggh!)
            //      This means the output weights vertex buffer is going to be larger than input ones combined.

        size_t destinationWeightVertexStride = 0;
        const std::vector<InputElementDesc>* finalWeightBufferFormat = nullptr;

        std::vector<unsigned> bucketVertexSizes(bucketCount);
        for (unsigned b=0; b<bucketCount; ++b) {
			auto& bucket = controllers[b>>2]._controller->_bucket[b&0x3];
            bucketVertexSizes[b] = CalculateVertexSize(MakeIteratorRange(bucket._vertexInputLayout));
            if (bucket._vertexBufferSize) {
                if (bucketVertexSizes[b] > destinationWeightVertexStride) {
                    destinationWeightVertexStride = bucketVertexSizes[b];
                    finalWeightBufferFormat = &bucket._vertexInputLayout;
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

		#if defined(_DEBUG)
			unsigned weightsOffset = 0;
            auto weightsFormat = Format::Unknown;
            for (auto i=finalWeightBufferFormat->cbegin(); i!=finalWeightBufferFormat->cend(); ++i) {
                if (!XlCompareStringI(i->_semanticName.c_str(), "WEIGHTS") && i->_semanticIndex == 0) {
                    weightsOffset = i->_alignedByteOffset;
                    weightsFormat = i->_nativeFormat;
                    break;
                }
            }
			unsigned indicesOffset = 0;
            auto indicesFormat = Format::Unknown;
            for (auto i=finalWeightBufferFormat->cbegin(); i!=finalWeightBufferFormat->cend(); ++i) {
                if (!XlCompareStringI(i->_semanticName.c_str(), "JOINTINDICES") && i->_semanticIndex == 0) {
                    indicesOffset = i->_alignedByteOffset;
                    indicesFormat = i->_nativeFormat;
                    break;
                }
            }
		#endif

        std::vector<uint8> skeletonBindingVertices;
        if (destinationWeightVertexStride && finalWeightBufferFormat) {
            skeletonBindingVertices = std::vector<uint8>(destinationWeightVertexStride*unifiedVertexCount, 0);

            for (size_t destinationVertexIndex=0; destinationVertexIndex<vertexMappingByFinalOrdering.size(); ++destinationVertexIndex) {
                unsigned sourceBucketIndex = vertexMappingByFinalOrdering[destinationVertexIndex]._bucketIndex;

				auto controllerIdx = sourceBucketIndex >> 18;
				auto bucketIdx = (sourceBucketIndex >> 16) & 0x3;
				auto sourceVertexInThisBucket = sourceBucketIndex & 0xffff;

				assert(controllerIdx < controllers.size());
				assert(bucketIdx < 4);
                                
				const auto& bucket = controllers[controllerIdx]._controller->_bucket[bucketIdx];

                    //
                    //      Note that sometimes we'll be expanding the vertex format in this process
                    //      If some buckets are using R8G8, and others are R8G8B8A8 (for example)
                    //      then they will all be expanded to the largest size
                    //

                auto sourceVertexStride = bucketVertexSizes[sourceBucketIndex>>16];
                void* destinationVertex = PtrAdd(skeletonBindingVertices.data(), destinationVertexIndex*destinationWeightVertexStride);
                assert((sourceVertexInThisBucket+1)*sourceVertexStride <= bucket._vertexBufferSize);
                const void* sourceVertex = PtrAdd(bucket._vertexBufferData.get(), sourceVertexInThisBucket*sourceVertexStride);

                if (sourceVertexStride == destinationWeightVertexStride) {
                    XlCopyMemory(destinationVertex, sourceVertex, sourceVertexStride);
                } else {
                    const InputElementDesc* dstElement = AsPointer(finalWeightBufferFormat->cbegin());
                    for (   auto srcElement=bucket._vertexInputLayout.cbegin(); 
                            srcElement!=bucket._vertexInputLayout.cend(); ++srcElement, ++dstElement) {

                            // (todo -- precalculate this min of element sizes)
						unsigned elementSize = std::min(BitsPerPixel(srcElement->_nativeFormat)/8, BitsPerPixel(dstElement->_nativeFormat)/8);
                        assert(PtrAdd(destinationVertex, dstElement->_alignedByteOffset+elementSize) <= PtrAdd(skeletonBindingVertices.data(), destinationWeightVertexStride*unifiedVertexCount));
                        assert(PtrAdd(sourceVertex, srcElement->_alignedByteOffset+elementSize) <= PtrAdd(bucket._vertexBufferData.get(), bucket._vertexBufferSize));
                        XlCopyMemory(   PtrAdd(destinationVertex, dstElement->_alignedByteOffset), 
                                        PtrAdd(sourceVertex, srcElement->_alignedByteOffset), 
                                        elementSize);
                    }
                }

				assert(destinationVertexIndex >= bucketStart[sourceBucketIndex>>16] && destinationVertexIndex < bucketEnd[sourceBucketIndex>>16]);

				#if defined(_DEBUG)
					for (unsigned c=0; c<GetComponentCount(GetComponents(indicesFormat)); ++c) {
						auto index = *(unsigned char*)PtrAdd(destinationVertex, indicesOffset+c);
						assert(index < (unsigned)controllers[controllerIdx]._jointMatrices.size());
					}
				#endif
            }
        }

        #if defined(_DEBUG)

            //  Double check that weights are normalized in the binding buffer
			/*{
                size_t stride = destinationWeightVertexStride;
                if (weightsFormat == Format::R8G8_UNORM) {
                    for (unsigned c=0; c<unifiedVertexCount; ++c) {
                        const void* p = PtrAdd(skeletonBindingVertices.get(), c*stride+weightsOffset);
                        unsigned char zero   = ((unsigned char*)p)[0];
                        unsigned char one    = ((unsigned char*)p)[1];
                        assert((zero+one) >= 0xfd);
                    }
                } else if (weightsFormat == Format::R8G8B8A8_UNORM) {
                    for (unsigned c=0; c<unifiedVertexCount; ++c) {
                        const void* p = PtrAdd(skeletonBindingVertices.get(), c*stride+weightsOffset);
                        unsigned char zero   = ((unsigned char*)p)[0];
                        unsigned char one    = ((unsigned char*)p)[1];
                        unsigned char two    = ((unsigned char*)p)[2];
                        unsigned char three  = ((unsigned char*)p)[3];
                        assert((zero+one+two+three) >= 0xfd);
                    }
                } else {
                    assert(weightsFormat == Format::R8_UNORM);
                }
            }*/

			// Ensure that the joint indices are never too large
			{
				size_t stride = destinationWeightVertexStride;

				for (unsigned controllerIdx=0; controllerIdx<controllers.size(); ++controllerIdx) {
					auto* bStart = &bucketStart[controllerIdx<<2];
					auto* bEnd = &bucketEnd[controllerIdx<<2];
					auto jointIndexMax = controllers[controllerIdx]._jointMatrices.size();
					for (unsigned b=0; b<4; ++b) {
						for (size_t v=bStart[b]; v<bEnd[b]; ++v) {

							const void* p = PtrAdd(skeletonBindingVertices.data(), v*stride+indicesOffset);
							if (indicesFormat == Format::R8G8_UINT) {
								for (unsigned c=0; c<unifiedVertexCount; ++c) {
									unsigned char zero   = ((unsigned char*)p)[0];
									unsigned char one    = ((unsigned char*)p)[1];
									assert(zero < jointIndexMax);
									assert(one < jointIndexMax);
								}
							} else if (indicesFormat == Format::R8G8B8A8_UINT) {
								for (unsigned c=0; c<unifiedVertexCount; ++c) {
									unsigned char zero   = ((unsigned char*)p)[0];
									unsigned char one    = ((unsigned char*)p)[1];
									unsigned char two    = ((unsigned char*)p)[2];
									unsigned char three  = ((unsigned char*)p)[3];
									assert(zero < jointIndexMax);
									assert(one < jointIndexMax);
									assert(two < jointIndexMax);
									assert(three < jointIndexMax);
								}
							} else {
								unsigned char zero   = ((unsigned char*)p)[0];
								assert(zero < jointIndexMax);
							}

						}
					}
				}
			}
                                
        #endif

            //      Calculate the local space bounding box for the input vertex buffer
            //      (assuming the position will appear in the animated vertex buffer)
        auto boundingBox = InvalidBoundingBox();
        auto positionDesc = FindPositionElement(AsPointer(animatedVertexLayout.begin()), animatedVertexLayout.size());
        if (positionDesc._nativeFormat != Format::Unknown) {
            AddToBoundingBox(
                boundingBox,
                animatedVertexBuffer.data(), animatedVertexStride, unifiedVertexCount,
                positionDesc, Identity<Float4x4>());
        }

            //      Build the final "BoundSkinnedGeometry" object
        NascentBoundSkinnedGeometry result;
		result._unanimatedVertexElements = std::move(unanimatedVertexBuffer);
		result._animatedVertexElements = std::move(animatedVertexBuffer);
		result._skeletonBinding = std::move(skeletonBindingVertices);
		result._indices = std::move(newIndexBuffer);

        result._skeletonBindingVertexStride = (unsigned)destinationWeightVertexStride;
        result._animatedVertexBufferSize = (unsigned)(animatedVertexStride*unifiedVertexCount);

        result._mainDrawCalls = sourceGeo._mainDrawCalls;
        result._mainDrawUnanimatedIA._vertexStride = unanimatedVertexStride;
        result._mainDrawUnanimatedIA._elements = std::move(unanimatedVertexLayout);
        result._indexFormat = sourceGeo._indexFormat;

        result._mainDrawAnimatedIA._vertexStride = animatedVertexStride;
        result._mainDrawAnimatedIA._elements = std::move(animatedVertexLayout);

		// Setup the per-section preskinning draw calls
		for (unsigned controllerIdx=0; controllerIdx<controllers.size(); ++controllerIdx) {
			std::vector<DrawCallDesc> preskinningDrawCalls;
			auto* bStart = &bucketStart[controllerIdx<<2];
			auto* bEnd = &bucketEnd[controllerIdx<<2];
			if (bEnd[0] > bStart[0]) {
				preskinningDrawCalls.push_back(
					DrawCallDesc{~unsigned(0x0), unsigned(bEnd[0] - bStart[0]), unsigned(bStart[0]), 4, Topology::PointList});
			}
			if (bEnd[1] > bStart[1]) {
				preskinningDrawCalls.push_back(
					DrawCallDesc{~unsigned(0x0), unsigned(bEnd[1] - bStart[1]), unsigned(bStart[1]), 2, Topology::PointList});
			}
			if (bEnd[2] > bStart[2]) {
				preskinningDrawCalls.push_back(
					DrawCallDesc{~unsigned(0x0), unsigned(bEnd[2] - bStart[2]), unsigned(bStart[2]), 1, Topology::PointList});
			}

			assert(bEnd[2] <= unifiedVertexCount);

			NascentBoundSkinnedGeometry::Section section;

			std::vector<Float4x4> bindShapeByInverseBindMatrices;
			bindShapeByInverseBindMatrices.reserve(controllers[controllerIdx]._controller->_inverseBindMatrices.size());
			for (const auto&ibm:controllers[controllerIdx]._controller->_inverseBindMatrices)
				bindShapeByInverseBindMatrices.push_back(Combine(controllers[controllerIdx]._controller->_bindShapeMatrix, ibm));
			section._bindShapeByInverseBindMatrices = std::move(bindShapeByInverseBindMatrices);

			section._preskinningDrawCalls = std::move(preskinningDrawCalls);
			section._jointMatrices = DynamicArray<uint16_t>(
				AsPointer(controllers[controllerIdx]._jointMatrices.begin()),
				AsPointer(controllers[controllerIdx]._jointMatrices.end()));

			result._preskinningSections.emplace_back(std::move(section));
		}

        if (finalWeightBufferFormat) {
            result._preskinningIA = RenderCore::Assets::CreateGeoInputAssembly(
                *finalWeightBufferFormat, (unsigned)destinationWeightVertexStride);
        }

        result._localBoundingBox = boundingBox;
        return std::move(result);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	static void Serialize(Serialization::NascentBlockSerializer& outputSerializer, const NascentBoundSkinnedGeometry::Section& section)
	{
		Serialize(outputSerializer, section._bindShapeByInverseBindMatrices);
		Serialize(outputSerializer, section._preskinningDrawCalls);
		outputSerializer.SerializeSubBlock(MakeIteratorRange(section._jointMatrices));
        outputSerializer.SerializeValue(section._jointMatrices.size());
	}

    void NascentBoundSkinnedGeometry::SerializeWithResourceBlock(
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

            // first part is just like "NascentRawGeometry::SerializeMethod"
        Serialize(
            outputSerializer, 
            RenderCore::Assets::VertexData 
                { _mainDrawUnanimatedIA, unsigned(vbOffset0), unsigned(vbSize0) });

        Serialize(
            outputSerializer, 
            RenderCore::Assets::IndexData 
                { _indexFormat, unsigned(ibOffset), unsigned(ibSize) });

        Serialize(outputSerializer, _mainDrawCalls);

            // append skinning related information
        Serialize(
            outputSerializer, 
            RenderCore::Assets::VertexData 
                { _mainDrawAnimatedIA, unsigned(vbOffset1), unsigned(vbSize1) });
        Serialize(
            outputSerializer, 
            RenderCore::Assets::VertexData 
                { _preskinningIA, unsigned(vbOffset2), unsigned(vbSize2) });
        
		Serialize(outputSerializer, _preskinningSections);

        Serialize(outputSerializer, _localBoundingBox.first);
        Serialize(outputSerializer, _localBoundingBox.second);
    }




        


	void UnboundSkinController::RemapJoints(IteratorRange<const unsigned*> newJointIndices)
	{
		// Remap all of the joints through the given remapping indices
		// Useful for removing redundant duplicates (etc)
		assert(newJointIndices.size() == _jointNames.size());
		assert(newJointIndices.size() == _inverseBindMatrices.size());

		for (unsigned b=0; b<4; ++b) {
			auto& bucket = _bucket[b];
			if (!bucket._vertexBufferSize) continue;
			auto ele = std::find_if(
				bucket._vertexInputLayout.begin(), bucket._vertexInputLayout.end(),
				[](const InputElementDesc& desc) { return desc._semanticName == "JOINTINDICES"; });
			auto range = MakeVertexIteratorRange(
				MakeIteratorRange(
					PtrAdd(bucket._vertexBufferData.get(), ele->_alignedByteOffset),
					PtrAdd(bucket._vertexBufferData.get(), bucket._vertexBufferSize)),
				CalculateVertexStrideForSlot(MakeIteratorRange(bucket._vertexInputLayout), ele->_inputSlot),
				ele->_nativeFormat);
			auto componentCount = GetComponentCount(RenderCore::GetComponents(ele->_nativeFormat));
			for (auto i=range.begin(); i<range.end(); ++i) {
				auto data = (uint8_t*)(*i)._data.begin();
				for (unsigned c=0; c<componentCount; ++c)
					data[c] = (uint8_t)newJointIndices[data[c]];
			}
		}

		unsigned maxOutputIndex = 0;
		for (auto i:newJointIndices) maxOutputIndex = std::max(maxOutputIndex, i);

		std::vector<std::string> newJointNames(maxOutputIndex+1);
		std::vector<Float4x4>  newInverseBindMatrices;
		newInverseBindMatrices.resize(size_t(maxOutputIndex+1), Identity<Float4x4>());

		for (unsigned c=0; c<_jointNames.size(); ++c) {
			assert(newJointNames[newJointIndices[c]].empty() || newJointNames[newJointIndices[c]] == _jointNames[c]);	// ensure that we haven't already mapped to this index
			newJointNames[newJointIndices[c]] = _jointNames[c];
			newInverseBindMatrices[newJointIndices[c]] = _inverseBindMatrices[c];
		}

		_jointNames = std::move(newJointNames);
		_inverseBindMatrices = std::move(newInverseBindMatrices);
	}

    UnboundSkinController::UnboundSkinController(   
        Bucket&& bucket4, Bucket&& bucket2, Bucket&& bucket1, Bucket&& bucket0,
        std::vector<Float4x4>&& inverseBindMatrices, const Float4x4& bindShapeMatrix,
        std::vector<std::string>&& jointNames,
        std::vector<uint32_t>&& vertexPositionToBucketIndex)
    :       _inverseBindMatrices(std::move(inverseBindMatrices))
    ,       _bindShapeMatrix(bindShapeMatrix)
    ,       _positionIndexToBucketIndex(vertexPositionToBucketIndex)
    ,       _jointNames(jointNames)
    {
        _bucket[0] = std::forward<Bucket>(bucket4);
        _bucket[1] = std::forward<Bucket>(bucket2);
        _bucket[2] = std::forward<Bucket>(bucket1);
        _bucket[3] = std::forward<Bucket>(bucket0);
    }

	template<unsigned WeightCount>
		void AccumulateJointUsage(
			const VertexWeightAttachmentBucket<WeightCount>& bucket,
			std::vector<unsigned>& accumulator)
	{
		assert(accumulator.size() >= 256);
		for (const auto&w:bucket._weightAttachments) {
			for (unsigned c=0; c<WeightCount; ++c) {
				++accumulator[w._jointIndex[c]];
			}
		}
	}

	template<unsigned WeightCount>
		void RemapJointIndices(
			VertexWeightAttachmentBucket<WeightCount>& bucket,
			IteratorRange<const unsigned*> remapping)
	{
		for (auto&w:bucket._weightAttachments) {
			for (unsigned c=0; c<WeightCount; ++c) {
				assert(w._jointIndex[c] < remapping.size());
				w._jointIndex[c] = (uint8)remapping[w._jointIndex[c]];
			}
		}
	}

	template void AccumulateJointUsage(const VertexWeightAttachmentBucket<1>&, std::vector<unsigned>&);
	template void AccumulateJointUsage(const VertexWeightAttachmentBucket<2>&, std::vector<unsigned>&);
	template void AccumulateJointUsage(const VertexWeightAttachmentBucket<4>&, std::vector<unsigned>&);
	template void RemapJointIndices(VertexWeightAttachmentBucket<1>&, IteratorRange<const unsigned*>);
	template void RemapJointIndices(VertexWeightAttachmentBucket<2>&, IteratorRange<const unsigned*>);
	template void RemapJointIndices(VertexWeightAttachmentBucket<4>&, IteratorRange<const unsigned*>);


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
		for(unsigned sectionIdx=0; sectionIdx<geo._preskinningSections.size(); ++sectionIdx) {
			c=0;
			auto& section = geo._preskinningSections[sectionIdx];
			for(const auto& dc:section._preskinningDrawCalls)
				stream << "Preskinning Section [" << sectionIdx << "] Draw [" << c++ << "] " << dc << std::endl;

			stream << "Section [" << sectionIdx << "] Joint matrices: ";
			for (size_t q=0; q<section._jointMatrices.size(); ++q) {
				if (q != 0) stream << ", ";
				stream << section._jointMatrices[q];
			}
			stream << std::endl;
		}
        
        stream << std::endl;
        return stream;
    }


}}}

