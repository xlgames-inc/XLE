// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS   // warning C4996: 'std::_Copy_impl': Function call with parameters that may be unsafe

#include "ConversionObjects.h"
#include "ColladaConversion.h"
#include "../RenderCore/RenderUtils.h"
#include "../Assets/BlockSerializer.h"
#include "../Math/Transformations.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/IteratorUtils.h"
#include "../ConsoleRig/Log.h"

namespace RenderCore { namespace ColladaConversion
{
    using ::Assets::Exceptions::FormatError;

    void    NascentBoundSkinnedGeometry::Serialize( Serialization::NascentBlockSerializer& outputSerializer, 
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
        _mainDrawUnanimatedIA.Serialize(outputSerializer);
        outputSerializer.SerializeValue(unsigned(vbOffset0));
        outputSerializer.SerializeValue(unsigned(vbSize0));
        outputSerializer.SerializeValue(unsigned(_indexFormat));
        outputSerializer.SerializeValue(unsigned(ibOffset));
        outputSerializer.SerializeValue(unsigned(ibSize));
        
        outputSerializer.SerializeSubBlock(AsPointer(_mainDrawCalls.begin()), AsPointer(_mainDrawCalls.end()));
        outputSerializer.SerializeValue(_mainDrawCalls.size());

            // append skinning related information
        _mainDrawAnimatedIA.Serialize(outputSerializer);
        outputSerializer.SerializeValue(unsigned(vbOffset1));
        outputSerializer.SerializeValue(unsigned(vbSize1));
        _preskinningIA.Serialize(outputSerializer);
        outputSerializer.SerializeValue(unsigned(vbOffset2));
        outputSerializer.SerializeValue(unsigned(vbSize2));

        outputSerializer.SerializeSubBlock(_inverseBindMatrices.begin(), _inverseBindMatrices.end());
        outputSerializer.SerializeValue(_inverseBindMatrices.size());

        DynamicArray<Float4x4> inverseBindByBindShape = DynamicArray<Float4x4>::Copy(_inverseBindMatrices);
        for (unsigned c=0; c<inverseBindByBindShape.size(); ++c) {
            inverseBindByBindShape[c] = Combine(
                _bindShapeMatrix,
                inverseBindByBindShape[c]);
        }
        outputSerializer.SerializeSubBlock(inverseBindByBindShape.begin(), inverseBindByBindShape.end());
        outputSerializer.SerializeValue(inverseBindByBindShape.size());
        outputSerializer.SerializeSubBlock(_jointMatrices.begin(), _jointMatrices.end());
        outputSerializer.SerializeValue(_jointMatrices.size());
        
        Serialization::Serialize(outputSerializer, _bindShapeMatrix);

        outputSerializer.SerializeSubBlock(AsPointer(_preskinningDrawCalls.cbegin()), AsPointer(_preskinningDrawCalls.cend()));
        outputSerializer.SerializeValue(_preskinningDrawCalls.size());

        Serialization::Serialize(outputSerializer, _localBoundingBox.first);
        Serialization::Serialize(outputSerializer, _localBoundingBox.second);
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
    ,       _indexFormat(Metal::NativeFormat::Unknown)
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
    UnboundSkinController::Bucket::Bucket(Bucket&& moveFrom)
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

    UnboundSkinController::UnboundSkinController(   Bucket&& bucket4, Bucket&& bucket2, Bucket&& bucket1, Bucket&& bucket0,
                                                    DynamicArray<Float4x4>&& inverseBindMatrices, const Float4x4& bindShapeMatrix,
                                                    std::vector<uint32>&& vertexPositionToBucketIndex)
    :       _inverseBindMatrices(std::forward<DynamicArray<Float4x4>>(inverseBindMatrices))
    ,       _bindShapeMatrix(bindShapeMatrix)
    ,       _positionIndexToBucketIndex(vertexPositionToBucketIndex)
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
    {
        for (unsigned c=0; c<dimof(_bucket); ++c)
            _bucket[c] = std::move(moveFrom._bucket[c]);
    }

    UnboundSkinController& UnboundSkinController::operator=(UnboundSkinController&& moveFrom)
    {
        for (unsigned c=0; c<dimof(_bucket); ++c)
            _bucket[c] = std::move(moveFrom._bucket[c]);
        _inverseBindMatrices = std::move(moveFrom._inverseBindMatrices);
        _bindShapeMatrix = moveFrom._bindShapeMatrix;
        _positionIndexToBucketIndex = std::move(moveFrom._positionIndexToBucketIndex);
        return *this;
    }


    UnboundMorphController::UnboundMorphController()
    {}

    UnboundMorphController::UnboundMorphController(UnboundMorphController&& moveFrom) never_throws
    :       _source(std::move(moveFrom._source))
    {}

    UnboundMorphController& UnboundMorphController::operator=(UnboundMorphController&& moveFrom)
    {
        _source = std::move(moveFrom._source);
        return *this;
    }


    bool NodeReferences::IsImportant(ObjectGuid node) const
    {
        auto i = LowerBound(_nodeReferences, node);
        return i != _nodeReferences.end() && i->first == node;
    }

    const Float4x4* NodeReferences::GetInverseBindMatrix(ObjectGuid node) const
    {
        auto i = LowerBound(_inverseBindMatrics, node);
        if (i != _inverseBindMatrics.end() && i->first == node)
            return &i->second;
        return nullptr;
    }

    unsigned NodeReferences::GetOutputMatrixIndex(ObjectGuid node) const
    {
        auto i = LowerBound(_nodeReferences, node);
        if (i != _nodeReferences.end() && i->first == node)
            return i->second;
        return ~unsigned(0);
    }

    void NodeReferences::MarkImportant(ObjectGuid node)
    {
        auto i = LowerBound(_nodeReferences, node);
        if (i != _nodeReferences.end() && i->first == node) {
        } else {
            _nodeReferences.insert(i, std::make_pair(node, ~unsigned(0)));
        }
    }

    void NodeReferences::AttachInverseBindMatrix(ObjectGuid node, Float4x4 inverseBind)
    {
        auto i = LowerBound(_inverseBindMatrics, node);
        if (i != _inverseBindMatrics.end() && i->first == node) {
            i->second = inverseBind;
        } else {
            _inverseBindMatrics.insert(i, std::make_pair(node, inverseBind));
        }
    }

    void NodeReferences::SetOutputMatrix(ObjectGuid node, unsigned outputMatrixIndex)
    {
        auto i = LowerBound(_nodeReferences, node);
        if (i != _nodeReferences.end() && i->first == node) {
            i->second = outputMatrixIndex;
        } else {
            _nodeReferences.insert(i, std::make_pair(node, outputMatrixIndex));
        }
    }

    NodeReferences::NodeReferences() {}
    NodeReferences::~NodeReferences() {}

}}

