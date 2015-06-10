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
#include "../ConsoleRig/Log.h"

#pragma warning(push)
#pragma warning(disable:4201)       // nonstandard extension used : nameless struct/union
#pragma warning(disable:4245)       // conversion from 'int' to 'const COLLADAFW::SamplerID', signed/unsigned mismatch
#pragma warning(disable:4512)       // assignment operator could not be generated
    #include <COLLADAFWUniqueId.h>
    #include <COLLADAFWImage.h>
    #include <COLLADAFWSkinController.h>
    #include <COLLADAFWSkinControllerData.h>
#pragma warning(pop)

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




        

    ReferencedTexture       Convert(const COLLADAFW::Image* image)
    {
        if (image->getSourceType() != COLLADAFW::Image::SOURCE_TYPE_URI) {
            ThrowException(FormatError("Cannot load image (%s). Only URI type textures are supported.", image->getName().c_str()));
        }

        const COLLADABU::URI& originalURI = image->getImageURI();
        std::string nativePath = originalURI.toNativePath();
        if (nativePath.size() >= 2 && (nativePath[0] == '\\' || nativePath[0] == '/') && (nativePath[1] == '\\' || nativePath[1] == '/')) {
                // skip over the first 2 characters if we begin with \\ or //
            nativePath = nativePath.substr(2);  
        }
        return ReferencedTexture(nativePath);
    }


    static const std::string DefaultSemantic_Weights         = "WEIGHTS";
    static const std::string DefaultSemantic_JointIndices    = "JOINTINDICES";

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

    template<typename FloatType>
        const COLLADAFW::ArrayPrimitiveType<FloatType>&         GetValues(const COLLADAFW::FloatOrDoubleArray& input);

    template<> const COLLADAFW::ArrayPrimitiveType<float>&      GetValues<float>(const COLLADAFW::FloatOrDoubleArray& input)     { return *input.getFloatValues(); }
    template<> const COLLADAFW::ArrayPrimitiveType<double>&     GetValues<double>(const COLLADAFW::FloatOrDoubleArray& input)    { return *input.getDoubleValues(); }

    static const unsigned AbsoluteMaxJointInfluenceCount = 256;

    template<typename FloatType, typename OutputFloatType>
        void            GetWeights(             const COLLADAFW::SkinControllerData& controller, OutputFloatType outputWeights[], 
                                                unsigned weightCount, size_t baseIndex)
    {
        using namespace COLLADAFW;
        const FloatOrDoubleArray& weightArray = controller.getWeights();
        for (unsigned c=0; c<weightCount; ++c) {
            unsigned weightIndex = controller.getWeightIndices()[baseIndex+c];
            if (weightIndex < weightArray.getValuesCount()) {
                outputWeights[c] = OutputFloatType(GetValues<FloatType>(weightArray)[weightIndex]);
            } else 
                outputWeights[c] = OutputFloatType(0);
        }
    }

    template<typename FloatType>
        void            GetNormalizedWeights(   const COLLADAFW::SkinControllerData& controller, uint8 outputWeights[], 
                                                unsigned weightCount, size_t baseIndex)
    {
        using namespace COLLADAFW;
        const FloatOrDoubleArray& weightArray = controller.getWeights();

            //      We have to re-normalize the weights from the input data    //
        FloatType weights[AbsoluteMaxJointInfluenceCount], totalWeightValue = FloatType(0);
        unsigned c;
        for (c=0; c<std::min(weightCount, dimof(weights)); ++c) {
            unsigned weightIndex = controller.getWeightIndices()[baseIndex+c];
            if (weightIndex < weightArray.getValuesCount()) {
                weights[c] = GetValues<FloatType>(weightArray)[weightIndex];
                totalWeightValue += weights[c];
            } else 
                weights[c] = 0.;
        }

        if (totalWeightValue != 0.) {
            for (c=0; c<std::min(weightCount, dimof(weights)); ++c) {
                outputWeights[c] = (uint8)(Clamp(weights[c] / totalWeightValue, FloatType(0), FloatType(1)) * FloatType(255) + FloatType(.5));
            }
        } else {
            Warning("Warning -- found vertex with no active vertex weights in controller (%s)\n", controller.getName().c_str());
        }
    }

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
        VertexWeightAttachment<WeightCount> BuildWeightAttachment(const COLLADAFW::SkinControllerData& controller, unsigned jointCount, size_t indexOffset)
    {
        VertexWeightAttachment<WeightCount> attachment;
        std::fill(attachment._weights, &attachment._weights[dimof(attachment._weights)], 0);
        std::fill(attachment._jointIndex, &attachment._jointIndex[dimof(attachment._jointIndex)], 0);

        using namespace COLLADAFW;
        const FloatOrDoubleArray& weightArray = controller.getWeights();
        if (weightArray.getType() == FloatOrDoubleArray::DATA_TYPE_DOUBLE) {
            GetNormalizedWeights<double>(controller, attachment._weights, std::min(jointCount, unsigned(WeightCount)), indexOffset);
        } else {
            GetNormalizedWeights<float>(controller, attachment._weights, std::min(jointCount, unsigned(WeightCount)), indexOffset);
        }

        for (unsigned c=0; c<std::min(jointCount, unsigned(WeightCount)); ++c) {
            uint32 index = controller.getJointIndices()[indexOffset+c];
            if (index > 0xff) {
                ThrowException(Exceptions::FormatError("Joint index out of range in controller (%s). There may be too many joints in this model.", controller.getName().c_str()));
            }
            attachment._jointIndex[c] = (uint8)index;        // (has to be re-mapped in binding stage)
        }
        return attachment;
    }

    template<> VertexWeightAttachment<0> BuildWeightAttachment(const COLLADAFW::SkinControllerData& controller, unsigned jointCount, size_t indexOffset)
    {
        return VertexWeightAttachment<0>();
    }

    template<unsigned WeightCount> 
        VertexWeightAttachment<WeightCount> BuildWeightAttachment(const uint8 weights[], const uint8 joints[], unsigned jointCount)
    {
        VertexWeightAttachment<WeightCount> attachment;
        std::fill(attachment._weights, &attachment._weights[dimof(attachment._weights)], 0);
        std::fill(attachment._jointIndex, &attachment._jointIndex[dimof(attachment._jointIndex)], 0);
        std::copy(weights, &weights[std::min(WeightCount,jointCount)], attachment._weights);
        std::copy(joints, &joints[std::min(WeightCount,jointCount)], attachment._jointIndex);
        return attachment;
    }

    template<> VertexWeightAttachment<0> BuildWeightAttachment(const uint8 weights[], const uint8 joints[], unsigned jointCount)
    {
        return VertexWeightAttachment<0>();
    }

    UnboundSkinController Convert(const COLLADAFW::SkinControllerData* input)
    {
        using namespace COLLADAFW;

        if (!input->getJointsCount()) {
            ThrowException(FormatError("Skin controller object found with 0 joint count. This would be better as static geometry (%s)", input->getName().c_str()));
        }

        Float4x4 bindShapeMatrix = AsFloat4x4(input->getBindShapeMatrix());
        std::unique_ptr<Float4x4[]> inverseBindMatrices = std::make_unique<Float4x4[]>(input->getJointsCount());
        for (size_t c=0; c<input->getJointsCount(); ++c) {
            inverseBindMatrices[c] = AsFloat4x4(input->getInverseBindMatrices()[c]);
        }

            //
            //      Practically speaking, it's only useful to
            //      have 1, 2 or 4 weights per vertex (unless we were doing skinning via compute shaders). 
            //
            //      If we have a mesh where there are many vertices with only 1 or 2
            //      weights, but others with 4, we could potentially split the
            //      skinning operation up into multiple parts.
            //
            //      This could be done in 2 ways:
            //          1. split the geometry into multiple meshes, with extra draw calls
            //          2. do skinning in a preprocessing step before Draw
            //                  Ie; multiple skin -> to vertex buffer steps
            //                  then a single draw call...
            //              That would be efficient if writing to a GPU vertex buffer was
            //              very fast (it would also help reduce shader explosion).
            //
            //      If we were using type 2, it might be best to separate the animated parts
            //      of the vertex from the main vertex buffer. So texture coordinates and vertex
            //      colors will be static, so left in a separate buffer. But positions (and possibly
            //      normals and tangent frames) might be animated. So they could be held separately.
            //
            //      It might be handy to have a vertex buffer with just the positions. This could
            //      be used for pre-depth passes, etc.
            //
            //      Option 2 would be an efficient mode in multiple-pass rendering (because we
            //      apply the skinning only once, even if the object is rendered multiple times).
            //      But we need lots of temporary buffer space. Apparently in D3D11.1, we can 
            //      output to a buffer as a side-effect of vertex transformations, also. So a 
            //      first pass vertex shader could do depth-prepass and generate skinned
            //      positions for the second pass. (but there are some complications... might be
            //      worth experimenting with...)
            //
            //
            //      Let's create a number of buckets based on the number of weights attached
            //      to that vertex. Ideally we want a lot of vertices in the smaller buckets, 
            //      and few in the high buckets.
            //

        VertexWeightAttachmentBucket<4> bucket4;
        VertexWeightAttachmentBucket<2> bucket2;
        VertexWeightAttachmentBucket<1> bucket1;
        VertexWeightAttachmentBucket<0> bucket0;

        size_t vertexCount = input->getVertexCount();
        if (vertexCount >= std::numeric_limits<uint16>::max()) {
            ThrowException(FormatError("Exceeded maximum number of vertices supported by skinning controller (%s)", input->getName().c_str()));
        }

        std::vector<uint32> vertexPositionToBucketIndex;
        vertexPositionToBucketIndex.reserve(vertexCount);

        size_t indexIterator = 0;
        for (size_t vertex=0; vertex<vertexCount; ++vertex) {
            size_t basicJointCount = input->getJointsPerVertex()[vertex];

                //
                //      Sometimes the input data has joints attached at very low weight
                //      values. In these cases it's better to just ignore the influence.
                //
                //      So we need to calculate the normalized weights for all of the
                //      influences, first -- and then strip out the unimportant ones.
                //
            float weights[AbsoluteMaxJointInfluenceCount];
            uint8 jointIndices[AbsoluteMaxJointInfluenceCount];
            size_t jointCount = std::min(size_t(AbsoluteMaxJointInfluenceCount), basicJointCount);
            const FloatOrDoubleArray& weightArray = input->getWeights();
            if (weightArray.getType() == FloatOrDoubleArray::DATA_TYPE_DOUBLE) {
                GetWeights<double>(*input, weights, (int)jointCount, indexIterator);
            } else {
                GetWeights<float> (*input, weights, (int)jointCount, indexIterator);
            }

            for (size_t c=0; c<jointCount; ++c) {
                uint32 index = input->getJointIndices()[indexIterator+c];
                if (index > 0xff) {
                    ThrowException(FormatError("Joint index out of range in controller (%s). There may be too many joints in this model.", input->getName().c_str()));
                }
                assert(std::find(jointIndices, &jointIndices[c], index) == &jointIndices[c]);
                jointIndices[c] = (uint8)index;        // (has to be re-mapped in binding stage)
            }

            const float minWeightThreshold = 8.f / 255.f;
            float totalWeightValue = 0.f;
            for (size_t c=0; c<jointCount;) {
                if (weights[c] < minWeightThreshold) {
                    std::move(&weights[c+1],        &weights[jointCount],       &weights[c]);
                    std::move(&jointIndices[c+1],   &jointIndices[jointCount],  &jointIndices[c]);
                    --jointCount;
                } else {
                    totalWeightValue += weights[c];
                    ++c;
                }
            }

            uint8 normalizedWeights[AbsoluteMaxJointInfluenceCount];
            for (size_t c=0; c<jointCount; ++c) {
                normalizedWeights[c] = (uint8)(Clamp(weights[c] / totalWeightValue, 0.f, 1.f) * 255.f + .5f);
            }

                //
                // \todo -- should we sort influcences by the strength of the influence, or by the joint
                //          index?
                //
                //          Sorting by weight could allow us to decrease the number of influences
                //          calculated smoothly.
                //
                //          Sorting by joint index might mean that adjacent vertices are more frequently
                //          calculating the same joint.
                //

            #if defined(_DEBUG)     // double check to make sure no joint is referenced twice!
                for (size_t c=1; c<jointCount; ++c) {
                    assert(std::find(jointIndices, &jointIndices[c], jointIndices[c]) == &jointIndices[c]);
                }
            #endif

            if (jointCount >= 3) {
                if (jointCount > 4) {
                    LogAlwaysWarningF(    "Warning -- Exceeded maximum number of joints affecting a single vertex in skinning controller " 
                                "(%s). Only 4 joints can affect any given single vertex.\n",
                                input->getName().c_str());
                        // (When this happens, only use the first 4, and ignore the others)
                    LogAlwaysWarningF("Original weights:\n");
                    for (size_t c=0; c<basicJointCount; ++c) {
                        int weightIndex = input->getWeightIndices()[indexIterator+c];
                        int jointIndex = input->getJointIndices()[indexIterator+c];
                        float weight = input->getWeights().getType() == FloatOrDoubleArray::DATA_TYPE_FLOAT ? (*input->getWeights().getFloatValues())[weightIndex] : float((*input->getWeights().getDoubleValues())[weightIndex]);
                        LogAlwaysWarningF("  [%i] Weight: %f Joint: %i\n", c, weight, jointIndex);
                    }
                    LogAlwaysWarningF("After filtering:\n");
                    for (size_t c=0; c<jointCount; ++c) {
                        LogAlwaysWarningF("  [%i] Weight: %i Joint: %i\n", c, normalizedWeights[c], jointIndices[c]);
                    }
                }

                    // (we could do a separate bucket for 3, if it was useful)
                vertexPositionToBucketIndex.push_back((0<<16) | (uint32(bucket4._vertexBindings.size())&0xffff));
                bucket4._vertexBindings.push_back(uint16(vertex));
                bucket4._weightAttachments.push_back(BuildWeightAttachment<4>(normalizedWeights, jointIndices, (unsigned)jointCount));
            } else if (jointCount == 2) {
                vertexPositionToBucketIndex.push_back((1<<16) | (uint32(bucket2._vertexBindings.size())&0xffff));
                bucket2._vertexBindings.push_back(uint16(vertex));
                bucket2._weightAttachments.push_back(BuildWeightAttachment<2>(normalizedWeights, jointIndices, (unsigned)jointCount));
            } else if (jointCount == 1) {
                vertexPositionToBucketIndex.push_back((2<<16) | (uint32(bucket1._vertexBindings.size())&0xffff));
                bucket1._vertexBindings.push_back(uint16(vertex));
                bucket1._weightAttachments.push_back(BuildWeightAttachment<1>(normalizedWeights, jointIndices, (unsigned)jointCount));
            } else {
                vertexPositionToBucketIndex.push_back((3<<16) | (uint32(bucket0._vertexBindings.size())&0xffff));
                bucket0._vertexBindings.push_back(uint16(vertex));
                bucket0._weightAttachments.push_back(BuildWeightAttachment<0>(normalizedWeights, jointIndices, (unsigned)jointCount));
            }

            indexIterator += basicJointCount;
        }
            
        UnboundSkinController::Bucket b4;
        b4._vertexBindings = std::move(bucket4._vertexBindings);
        b4._weightCount = 4;
        b4._vertexBufferSize = bucket4._weightAttachments.size() * sizeof(VertexWeightAttachment<4>);
        b4._vertexBufferData = std::make_unique<uint8[]>(b4._vertexBufferSize);
        XlCopyMemory(b4._vertexBufferData.get(), AsPointer(bucket4._weightAttachments.begin()), b4._vertexBufferSize);
        b4._vertexInputLayout.push_back(Metal::InputElementDesc(DefaultSemantic_Weights, 0, Metal::NativeFormat::R8G8B8A8_UNORM, 1, 0));
        b4._vertexInputLayout.push_back(Metal::InputElementDesc(DefaultSemantic_JointIndices, 0, Metal::NativeFormat::R8G8B8A8_UINT, 1, 4));

        UnboundSkinController::Bucket b2;
        b2._vertexBindings = std::move(bucket2._vertexBindings);
        b2._weightCount = 2;
        b2._vertexBufferSize = bucket2._weightAttachments.size() * sizeof(VertexWeightAttachment<2>);
        b2._vertexBufferData = std::make_unique<uint8[]>(b2._vertexBufferSize);
        XlCopyMemory(b2._vertexBufferData.get(), AsPointer(bucket2._weightAttachments.begin()), b2._vertexBufferSize);
        b2._vertexInputLayout.push_back(Metal::InputElementDesc(DefaultSemantic_Weights, 0, Metal::NativeFormat::R8G8_UNORM, 1, 0));
        b2._vertexInputLayout.push_back(Metal::InputElementDesc(DefaultSemantic_JointIndices, 0, Metal::NativeFormat::R8G8_UINT, 1, 2));

        UnboundSkinController::Bucket b1;
        b1._vertexBindings = std::move(bucket1._vertexBindings);
        b1._weightCount = 1;
        b1._vertexBufferSize = bucket1._weightAttachments.size() * sizeof(VertexWeightAttachment<1>);
        b1._vertexBufferData = std::make_unique<uint8[]>(b1._vertexBufferSize);
        XlCopyMemory(b1._vertexBufferData.get(), AsPointer(bucket1._weightAttachments.begin()), b1._vertexBufferSize);
        b1._vertexInputLayout.push_back(Metal::InputElementDesc(DefaultSemantic_Weights, 0, Metal::NativeFormat::R8_UNORM, 1, 0));
        b1._vertexInputLayout.push_back(Metal::InputElementDesc(DefaultSemantic_JointIndices, 0, Metal::NativeFormat::R8_UINT, 1, 1));

        UnboundSkinController::Bucket b0;
        b0._vertexBindings = std::move(bucket0._vertexBindings);
        b0._weightCount = 0;
        b0._vertexBufferSize = bucket0._weightAttachments.size() * sizeof(VertexWeightAttachment<0>);
        if (b0._vertexBufferSize) {
            b0._vertexBufferData = std::make_unique<uint8[]>(b0._vertexBufferSize);
            XlCopyMemory(b1._vertexBufferData.get(), AsPointer(bucket0._weightAttachments.begin()), b0._vertexBufferSize);
        }

        return UnboundSkinController(
            std::move(b4), std::move(b2), std::move(b1), std::move(b0),
            DynamicArray<Float4x4>(std::move(inverseBindMatrices), input->getJointsCount()), bindShapeMatrix, std::move(vertexPositionToBucketIndex));
    }




    bool JointReferences::HasJoint(ObjectGuid joint) const
    {
        return std::find_if(_references.begin(), _references.end(), [=](const Reference& ref) { return ref._joint == joint; }) != _references.end();
    }


    ObjectGuid Convert(const COLLADAFW::UniqueId& input)
    {
        return ObjectGuid(input.getObjectId(), input.getFileId());
    }

    RenderCore::Assets::AnimationParameterId BuildAnimParameterId(const COLLADAFW::UniqueId& input)
    {
        return (RenderCore::Assets::AnimationParameterId)input.getObjectId();
    }

}}

