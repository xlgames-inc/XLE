// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NascentRawGeometry.h"
#include "NascentObjectGuid.h"
#include "../RenderCore/Types.h"
#include "../Math/Matrix.h"
#include "../Math/Vector.h"
#include "../Utility/UTFUtils.h"
#include "../Utility/PtrUtils.h"
#include <vector>

namespace RenderCore { namespace Assets { namespace GeoProc
{
	std::pair<Float3, Float3>   InvalidBoundingBox();

        ////////////////////////////////////////////////////////

    class NascentBoundSkinnedGeometry
    {
    public:
        DynamicArray<uint8_t>		_unanimatedVertexElements;
        DynamicArray<uint8_t>		_indices;

        Format                      _indexFormat = (Format)0;
        GeoInputAssembly            _mainDrawUnanimatedIA;
        GeoInputAssembly            _mainDrawAnimatedIA;

        std::vector<DrawCallDesc>	_mainDrawCalls;

        DynamicArray<uint8_t>		_animatedVertexElements;
        DynamicArray<uint8_t>		_skeletonBinding;
        unsigned                    _skeletonBindingVertexStride = 0;
        unsigned                    _animatedVertexBufferSize = 0;

        std::vector<Float4x4>		_bindShapeByInverseBindMatrices;
        DynamicArray<uint16_t>		_jointMatrices;         // (uint16 or uint8 for this array)
            
        std::vector<DrawCallDesc>   _preskinningDrawCalls;
        GeoInputAssembly            _preskinningIA;

        std::pair<Float3, Float3>	_localBoundingBox = InvalidBoundingBox();

        void    Serialize(Serialization::NascentBlockSerializer& outputSerializer, std::vector<uint8>& largeResourcesBlock) const;
        friend std::ostream& StreamOperator(std::ostream&, const NascentBoundSkinnedGeometry&);
    };

        ////////////////////////////////////////////////////////

    class UnboundSkinController 
    {
    public:
        struct Bucket
        {
            std::vector<InputElementDesc>    _vertexInputLayout;
            unsigned                         _weightCount = 0;

            std::unique_ptr<uint8[]>         _vertexBufferData;
            size_t                           _vertexBufferSize = 0;

            std::vector<uint16>              _vertexBindings;
        };

        Float4x4					_bindShapeMatrix;

        Bucket						_bucket[4];      // 4, 2, 1, 0
        std::vector<uint32>			_positionIndexToBucketIndex;

        std::vector<std::string>	_jointNames;
        std::vector<Float4x4>		_inverseBindMatrices;

        NascentObjectGuid			_sourceRef;

		void RemapJoints(IteratorRange<const unsigned*> newIndices);

        UnboundSkinController(
            Bucket&& bucket4, Bucket&& bucket2, Bucket&& bucket1, Bucket&& bucket0, 
            std::vector<Float4x4>&& inverseBindMatrices, const Float4x4& bindShapeMatrix,
            std::vector<std::string>&& jointNames,
            NascentObjectGuid sourceRef,
            std::vector<uint32>&& vertexPositionToBucketIndex);
        UnboundSkinController(UnboundSkinController&& moveFrom) = default;
        UnboundSkinController& operator=(UnboundSkinController&& moveFrom) = default;
    };

	template <int WeightCount>
        class VertexWeightAttachment
    {
    public:
        uint8       _weights[WeightCount];            // go straight to compressed 8 bit value
        uint8       _jointIndex[WeightCount];
    };

    template <>
        class VertexWeightAttachment<0>
    {
    };

    template <int WeightCount>
        class VertexWeightAttachmentBucket
    {
    public:
        std::vector<uint16>                                 _vertexBindings;
        std::vector<VertexWeightAttachment<WeightCount>>    _weightAttachments;
    };

    template<unsigned WeightCount> 
        VertexWeightAttachment<WeightCount> BuildWeightAttachment(const uint8 weights[], const unsigned joints[], unsigned jointCount)
    {
        VertexWeightAttachment<WeightCount> attachment;
        std::fill(attachment._weights, &attachment._weights[dimof(attachment._weights)], 0);
        std::fill(attachment._jointIndex, &attachment._jointIndex[dimof(attachment._jointIndex)], 0);
		for (unsigned c=0; c<std::min(WeightCount, jointCount); ++c) {
			attachment._weights[c] = weights[c];
			attachment._jointIndex[c] = (uint8)joints[c];
		}
        return attachment;
    }

    template<> inline VertexWeightAttachment<0> BuildWeightAttachment(const uint8 weights[], const unsigned joints[], unsigned jointCount)
    {
        return VertexWeightAttachment<0>();
    }

	template<unsigned WeightCount>
		void AccumulateJointUsage(
			const VertexWeightAttachmentBucket<WeightCount>& bucket,
			std::vector<unsigned>& accumulator);
	template<unsigned WeightCount>
		void RemapJointIndices(
			VertexWeightAttachmentBucket<WeightCount>& bucket,
			IteratorRange<const unsigned*> remapping);

        ////////////////////////////////////////////////////////

    class UnboundMorphController
    {
    public:
        NascentObjectGuid   _source;

        UnboundMorphController();
        UnboundMorphController(UnboundMorphController&& moveFrom) never_throws;
        UnboundMorphController& operator=(UnboundMorphController&& moveFrom) never_throws;
    private:
        UnboundMorphController& operator=(const UnboundMorphController& copyFrom);
    };

        ////////////////////////////////////////////////////////

    class UnboundSkinControllerAndAttachedSkeleton
    {
    public:
        NascentObjectGuid                  _unboundControllerId;
        NascentObjectGuid                  _source;
        std::vector<NascentObjectGuid>     _jointIds;
    };


        ////////////////////////////////////////////////////////

    NascentBoundSkinnedGeometry BindController(
        const NascentRawGeometry& sourceGeo,
        const UnboundSkinController& controller,
        DynamicArray<uint16>&& jointMatrices,
        const char nodeName[]);
}}}

