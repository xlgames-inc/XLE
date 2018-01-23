// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SkeletonScaffoldInternal.h"
#include "ModelRunTime.h"
#include "ModelImmutableData.h"
#include "RawAnimationCurve.h"
#include "AssetUtils.h"
#include "../../Assets/ChunkFileContainer.h"
#include "../../Assets/DeferredConstruction.h"
#include "../../ConsoleRig/Log.h"

namespace RenderCore { namespace Assets
{
    template <typename Type>
        void DestroyArray(const Type* begin, const Type* end)
        {
            for (auto i=begin; i!=end; ++i) { i->~Type(); }
        }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    struct CompareAnimationName
    {
        bool operator()(const AnimationSet::Animation& lhs, const AnimationSet::Animation& rhs) const { return lhs._name < rhs._name; }
        bool operator()(const AnimationSet::Animation& lhs, uint64 rhs) const { return lhs._name < rhs; }
        bool operator()(uint64 lhs, const AnimationSet::Animation& rhs) const { return lhs < rhs._name; }
    };

    TransformationParameterSet      AnimationSet::BuildTransformationParameterSet(
        const AnimationState&           animState__,
        const SkeletonMachine&    transformationMachine,
        const AnimationSetBinding&      binding,
        const RawAnimationCurve*        curves,
        size_t                          curvesCount) const
    {
        TransformationParameterSet result(transformationMachine.GetDefaultParameters());
        auto float1s	= result.GetFloat1Parameters();
        auto float3s	= result.GetFloat3Parameters();
        auto float4s	= result.GetFloat4Parameters();
        auto float4x4s	= result.GetFloat4x4Parameters();

        AnimationState animState = animState__;

        size_t driverStart = 0, driverEnd = GetAnimationDriverCount();
        size_t constantDriverStartIndex = 0, constantDriverEndIndex = _constantDriverCount;
        if (animState._animation!=0x0) {
            auto end = &_animations[_animationCount];
            auto i = std::lower_bound(_animations, end, animState._animation, CompareAnimationName());
            if (i!=end && i->_name == animState._animation) {
                driverStart = i->_beginDriver;
                driverEnd = i->_endDriver;
                constantDriverStartIndex = i->_beginConstantDriver;
                constantDriverEndIndex = i->_endConstantDriver;
                animState._time += i->_beginTime;
            }
        }

        const SkeletonMachine::InputInterface& inputInterface 
            = transformationMachine.GetInputInterface();
        for (size_t c=driverStart; c<driverEnd; ++c) {
            const AnimationDriver& driver = _animationDrivers[c];
            unsigned transInputIndex = binding.AnimDriverToMachineParameter(driver._parameterIndex);
            if (transInputIndex == ~unsigned(0x0)) {
                continue;   // (unbound output)
            }

            assert(transInputIndex < inputInterface._parameterCount);
            const SkeletonMachine::InputInterface::Parameter& p 
                = inputInterface._parameters[transInputIndex];

            if (driver._samplerType == AnimSamplerType::Float4x4) {
                if (driver._curveId < curvesCount) {
                    const RawAnimationCurve& curve = curves[driver._curveId];
                    assert(p._type == AnimSamplerType::Float4x4);
                    // assert(i->_index < float4x4s.size());
                    float4x4s[p._index] = curve.Calculate<Float4x4>(animState._time);
                }
            } else if (driver._samplerType == AnimSamplerType::Float4) {
                if (driver._curveId < curvesCount) {
                    const RawAnimationCurve& curve = curves[driver._curveId];
                    if (p._type == AnimSamplerType::Float4) {
                        float4s[p._index] = curve.Calculate<Float4>(animState._time);
                    } else if (p._type == AnimSamplerType::Float3) {
                        float3s[p._index] = Truncate(curve.Calculate<Float4>(animState._time));
                    } else {
                        assert(p._type == AnimSamplerType::Float1);
                        float1s[p._index] = curve.Calculate<Float4>(animState._time)[0];
                    }
                }
            } else if (driver._samplerType == AnimSamplerType::Float3) {
                if (driver._curveId < curvesCount) {
                    const RawAnimationCurve& curve = curves[driver._curveId];
                    if (p._type == AnimSamplerType::Float3) {
                        float3s[p._index] = curve.Calculate<Float3>(animState._time);
                    } else {
                        assert(p._type == AnimSamplerType::Float1);
                        float1s[p._index] = curve.Calculate<Float3>(animState._time)[0];
                    }
                }
            } else if (driver._samplerType == AnimSamplerType::Float1) {
                if (driver._curveId < curvesCount) {
                    const RawAnimationCurve& curve = curves[driver._curveId];
                    float curveresult = curve.Calculate<float>(animState._time);
                    if (p._type == AnimSamplerType::Float1) {
                        float1s[p._index] = curveresult;
                    } else if (p._type == AnimSamplerType::Float3) {
                        assert(driver._samplerOffset < 3);
                        float3s[p._index][driver._samplerOffset] = curveresult;
                    } else if (p._type == AnimSamplerType::Float4) {
                        assert(driver._samplerOffset < 4);
                        float4s[p._index][driver._samplerOffset] = curveresult;
                    }
                }
            }
        }

        for (   size_t c=constantDriverStartIndex; c<constantDriverEndIndex; ++c) {
            const ConstantDriver& driver = _constantDrivers[c];
            unsigned transInputIndex = binding.AnimDriverToMachineParameter(driver._parameterIndex);
            if (transInputIndex == ~unsigned(0x0)) {
                continue;   // (unbound output)
            }

            assert(transInputIndex < inputInterface._parameterCount);
            const SkeletonMachine::InputInterface::Parameter& p 
                = inputInterface._parameters[transInputIndex];

            const void* data    = PtrAdd(_constantData, driver._dataOffset);
            if (driver._samplerType == AnimSamplerType::Float4x4) {
                assert(p._type == AnimSamplerType::Float4x4);
                float4x4s[p._index] = *(const Float4x4*)data;
            } else if (driver._samplerType == AnimSamplerType::Float4) {
                if (p._type == AnimSamplerType::Float4) {
                    float4s[p._index] = *(const Float4*)data;
                } else if (p._type == AnimSamplerType::Float3) {
                    float3s[p._index] = Truncate(*(const Float4*)data);
                }
            } else if (driver._samplerType == AnimSamplerType::Float3) {
                assert(p._type == AnimSamplerType::Float3);
                float3s[p._index] = *(Float3*)data;
            } else if (driver._samplerType == AnimSamplerType::Float1) {
                if (p._type == AnimSamplerType::Float1) {
                    float1s[p._index] = *(float*)data;
                } else if (p._type == AnimSamplerType::Float3) {
                    assert(driver._samplerOffset < 3);
                    float3s[p._index][driver._samplerOffset] = *(const float*)data;
                } else if (p._type == AnimSamplerType::Float4) {
                    assert(driver._samplerOffset < 4);
                    float4s[p._index][driver._samplerOffset] = *(const float*)data;
                }
            }
        }

        return result;
    }

    AnimationSet::Animation AnimationSet::FindAnimation(uint64 animation) const
    {
        for (size_t c=0; c<_animationCount; ++c) {
            if (_animations[c]._name == animation) {
                return _animations[c];
            }
        }
        Animation result;
        result._name = 0ull;
        result._beginDriver = result._endDriver = 0;
        result._beginTime = result._endTime = 0.f;
        return result;
    }

    unsigned                AnimationSet::FindParameter(uint64 parameterName) const
    {
        for (size_t c=0; c<_outputInterface._parameterInterfaceCount; ++c) {
            if (_outputInterface._parameterInterfaceDefinition[c] == parameterName) {
                return unsigned(c);
            }
        }
        return ~unsigned(0x0);
    }

    AnimationSet::AnimationSet() {}
    AnimationSet::~AnimationSet()
    {
        DestroyArray(_animationDrivers,         &_animationDrivers[_animationDriverCount]);
    }

    AnimationImmutableData::AnimationImmutableData() {}
    AnimationImmutableData::~AnimationImmutableData()
    {
        DestroyArray(_curves, &_curves[_curvesCount]);
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    AnimationSetBinding::AnimationSetBinding(
            const AnimationSet::OutputInterface&            output,
            const SkeletonMachine::InputInterface&    input)
    {
            //
            //      for each animation set output value, match it with a 
            //      value in the transformation machine interface
            //      The interfaces are not sorted, we we just have to 
            //      do brute force searches. But there shouldn't be many
            //      parameters (so it should be fairly quick)
            //
        std::vector<unsigned> result;
        result.resize(output._parameterInterfaceCount);
        for (size_t c=0; c<output._parameterInterfaceCount; ++c) {
            uint64 parameterName = output._parameterInterfaceDefinition[c];
            result[c] = ~unsigned(0x0);

            for (size_t c2=0; c2<input._parameterCount; ++c2) {
                if (input._parameters[c2]._name == parameterName) {
                    result[c] = unsigned(c2);
                    break;
                }
            }

            #if defined(_DEBUG)
                if (result[c] == ~unsigned(0x0)) {
                    LogWarning << "Animation driver output cannot be bound to transformation machine input";
                }
            #endif
        }

        _animDriverToMachineParameter = std::move(result);
    }

    AnimationSetBinding::AnimationSetBinding() {}
    AnimationSetBinding::AnimationSetBinding(AnimationSetBinding&& moveFrom) never_throws
    : _animDriverToMachineParameter(std::move(moveFrom._animDriverToMachineParameter))
    {}
    AnimationSetBinding& AnimationSetBinding::operator=(AnimationSetBinding&& moveFrom) never_throws
    {
        _animDriverToMachineParameter = std::move(moveFrom._animDriverToMachineParameter);
        return *this;
    }
    AnimationSetBinding::~AnimationSetBinding() {}

    SkeletonBinding::SkeletonBinding(   const SkeletonMachine::OutputInterface&		output,
                                        const ModelCommandStream::InputInterface&   input)
    {
        std::vector<unsigned> result(input._jointCount, ~0u);

        for (size_t c=0; c<input._jointCount; ++c) {
            uint64 name = input._jointNames[c];
            for (size_t c2=0; c2<output._outputMatrixNameCount; ++c2) {
                if (output._outputMatrixNames[c2] == name) {
                    result[c] = unsigned(c2);
                    break;
                }
            }

            #if defined(_DEBUG)
                // if (result[c] == ~unsigned(0x0)) {
                //     LogWarning << "Couldn't bind skin matrix to transformation machine output.";
                // }
            #endif
        }
            
        _modelJointIndexToMachineOutput = std::move(result);
    }

    SkeletonBinding::SkeletonBinding() {}
    SkeletonBinding::SkeletonBinding(SkeletonBinding&& moveFrom) never_throws
    : _modelJointIndexToMachineOutput(std::move(moveFrom._modelJointIndexToMachineOutput))
    {}
    SkeletonBinding& SkeletonBinding::operator=(SkeletonBinding&& moveFrom) never_throws
    {
        _modelJointIndexToMachineOutput = std::move(moveFrom._modelJointIndexToMachineOutput);
        return *this;
    }
    SkeletonBinding::~SkeletonBinding() {}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void SkeletonMachine::GenerateOutputTransforms(   
        Float4x4 output[], unsigned outputCount,
        const TransformationParameterSet*   parameterSet) const
    {
        if (outputCount < _outputMatrixCount)
            Throw(::Exceptions::BasicLabel("Output buffer to SkeletonMachine::GenerateOutputTransforms is too small"));
        GenerateOutputTransformsFree(
            output, outputCount, parameterSet, 
            MakeIteratorRange(_commandStream, _commandStream + _commandStreamSize));
    }

    void SkeletonMachine::GenerateOutputTransforms(   
        Float4x4 output[], unsigned outputCount,
        const TransformationParameterSet*   parameterSet,
        const DebugIterator& debugIterator) const
    {
        if (outputCount < _outputMatrixCount)
            Throw(::Exceptions::BasicLabel("Output buffer to SkeletonMachine::GenerateOutputTransforms is too small"));
        GenerateOutputTransformsFree(
            output, outputCount, parameterSet, 
            MakeIteratorRange(_commandStream, _commandStream + _commandStreamSize), 
            debugIterator);
    }

    SkeletonMachine::SkeletonMachine()
    {
        _commandStream = nullptr;
        _commandStreamSize = 0;
        _outputMatrixCount = 0;
    }

    SkeletonMachine::~SkeletonMachine()
    {
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    const SkeletonMachine&   SkeletonScaffold::GetTransformationMachine() const                
    {
        return *(const SkeletonMachine*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
    }

    const ::Assets::AssetChunkRequest SkeletonScaffold::ChunkRequests[]
    {
        ::Assets::AssetChunkRequest { "Scaffold", ChunkType_Skeleton, 0, ::Assets::AssetChunkRequest::DataType::BlockSerializer },
    };
    
    SkeletonScaffold::SkeletonScaffold(IteratorRange<::Assets::AssetChunkResult*> chunks, const ::Assets::DepValPtr& depVal)
	: _depVal(depVal)
    {
		assert(chunks.size() == 1);
		_rawMemoryBlock = std::move(chunks[0]._buffer);
    }

    SkeletonScaffold::SkeletonScaffold(SkeletonScaffold&& moveFrom) never_throws
    : _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
	, _depVal(std::move(moveFrom._depVal))
    {}

    SkeletonScaffold& SkeletonScaffold::operator=(SkeletonScaffold&& moveFrom) never_throws
    {
		GetTransformationMachine().~SkeletonMachine();
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
		_depVal = std::move(moveFrom._depVal);
        return *this;
    }

    SkeletonScaffold::~SkeletonScaffold()
    {
		GetTransformationMachine().~SkeletonMachine();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    const AnimationImmutableData&   AnimationSetScaffold::ImmutableData() const                
    {
        return *(const AnimationImmutableData*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
    }

    const ::Assets::AssetChunkRequest AnimationSetScaffold::ChunkRequests[]
    {
        ::Assets::AssetChunkRequest { "Scaffold", ChunkType_AnimationSet, 0, ::Assets::AssetChunkRequest::DataType::BlockSerializer },
    };
    
    AnimationSetScaffold::AnimationSetScaffold(IteratorRange<::Assets::AssetChunkResult*> chunks, const ::Assets::DepValPtr& depVal)
	: _depVal(depVal)
    {
		assert(chunks.size() == 1);
		_rawMemoryBlock = std::move(chunks[0]._buffer);
    }

    AnimationSetScaffold::AnimationSetScaffold(AnimationSetScaffold&& moveFrom) never_throws
    : _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
	, _depVal(std::move(moveFrom._depVal))
    {}

    AnimationSetScaffold& AnimationSetScaffold::operator=(AnimationSetScaffold&& moveFrom) never_throws
    {
		ImmutableData().~AnimationImmutableData();
        _rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
		_depVal = std::move(moveFrom._depVal);
        return *this;
    }

    AnimationSetScaffold::~AnimationSetScaffold()
    {
        ImmutableData().~AnimationImmutableData();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	void TransformationParameterSet::Set(uint32 index, float p) 
	{
		if (_float1Parameters.size() < (index+1)) _float1Parameters.resize(index+1, 0.f);
		_float1Parameters[index] = p;
	}

	void TransformationParameterSet::Set(uint32 index, Float3 p)
	{
		if (_float3Parameters.size() < (index+1)) _float3Parameters.resize(index+1, Zero<Float3>());
		_float3Parameters[index] = p;
	}

	void TransformationParameterSet::Set(uint32 index, Float4 p)
	{
		if (_float4Parameters.size() < (index+1)) _float4Parameters.resize(index+1, Zero<Float4>());
		_float4Parameters[index] = p;
	}

	void TransformationParameterSet::Set(uint32 index, const Float4x4& p)
	{
		if (_float4x4Parameters.size() < (index+1)) _float4x4Parameters.resize(index+1, Zero<Float4x4>());
		_float4x4Parameters[index] = p;
	}

    TransformationParameterSet::TransformationParameterSet() {}

    TransformationParameterSet::TransformationParameterSet(TransformationParameterSet&& moveFrom)
    :       _float4x4Parameters(    std::move(moveFrom._float4x4Parameters))
    ,       _float4Parameters(      std::move(moveFrom._float4Parameters))
    ,       _float3Parameters(      std::move(moveFrom._float3Parameters))
    ,       _float1Parameters(      std::move(moveFrom._float1Parameters))
    {}

    TransformationParameterSet& TransformationParameterSet::operator=(TransformationParameterSet&& moveFrom)
    {
        _float4x4Parameters = std::move(moveFrom._float4x4Parameters);
        _float4Parameters   = std::move(moveFrom._float4Parameters);
        _float3Parameters   = std::move(moveFrom._float3Parameters);
        _float1Parameters   = std::move(moveFrom._float1Parameters);
        return *this;
    }

    TransformationParameterSet::TransformationParameterSet(const TransformationParameterSet& copyFrom)
    :       _float4x4Parameters(copyFrom._float4x4Parameters)
    ,       _float4Parameters(copyFrom._float4Parameters)
    ,       _float3Parameters(copyFrom._float3Parameters)
    ,       _float1Parameters(copyFrom._float1Parameters)
    {
    }

    TransformationParameterSet&  TransformationParameterSet::operator=(const TransformationParameterSet& copyFrom)
    {
        _float4x4Parameters = copyFrom._float4x4Parameters;
        _float4Parameters = copyFrom._float4Parameters;
        _float3Parameters = copyFrom._float3Parameters;
        _float1Parameters = copyFrom._float1Parameters;
        return *this;
    }

    void    TransformationParameterSet::Serialize(Serialization::NascentBlockSerializer& outputSerializer) const
    {
        ::Serialize(outputSerializer, _float4x4Parameters);
        ::Serialize(outputSerializer, _float4Parameters);
        ::Serialize(outputSerializer, _float3Parameters);
        ::Serialize(outputSerializer, _float1Parameters);
    }
}}
