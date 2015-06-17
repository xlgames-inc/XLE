// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelCommandStream.h"
#include "RawGeometry.h"
#include "ConversionObjects.h"

#include "../RenderCore/Assets/RawAnimationCurve.h"
#include "../RenderCore/Metal/InputLayout.h"

#include "../Assets/BlockSerializer.h"
#include "../ConsoleRig/OutputStream.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/IteratorUtils.h"

namespace RenderCore { namespace ColladaConversion
{ 
    using ::Assets::Exceptions::FormatError;

    class NascentAnimationSet::Animation
    {
    public:
        std::string     _name;
        unsigned        _begin, _end;
        unsigned        _constantBegin, _constantEnd;
        float           _startTime, _endTime;
        Animation() : _begin(0), _end(0), _constantBegin(0), _constantEnd(0), _startTime(0.f), _endTime(0.f) {}
        Animation(const std::string& name, unsigned begin, unsigned end, unsigned constantBegin, unsigned constantEnd, float startTime, float endTime) 
            : _name(name), _begin(begin), _end(end), _constantBegin(constantBegin), _constantEnd(constantEnd), _startTime(startTime), _endTime(endTime) {}
    };

    Assets::TransformationParameterSet      NascentAnimationSet::BuildTransformationParameterSet(
        float time, 
        const char animationName[],
        const NascentSkeleton& skeleton,
        const TableOfObjects& accessableObjects) const
    {
        Assets::TransformationParameterSet result(skeleton.GetTransformationMachine().GetDefaultParameters());
        float* float1s      = result.GetFloat1Parameters();
        Float3* float3s     = result.GetFloat3Parameters();
        Float4* float4s     = result.GetFloat4Parameters();
        Float4x4* float4x4s = result.GetFloat4x4Parameters();

        size_t driverStartIndex = 0, driverEndIndex = _animationDrivers.size();
        size_t constantDriverStartIndex = 0, constantDriverEndIndex = _constantDrivers.size();
        if (animationName) {
            for (auto i=_animations.begin(); i!=_animations.end(); ++i) {
                if (i->_name == animationName) {
                    driverStartIndex = i->_begin;
                    driverEndIndex = i->_end;
                    constantDriverStartIndex = i->_constantBegin;
                    constantDriverEndIndex = i->_constantEnd;
                    time += i->_startTime;
                }
            }
        }

        for (   auto i=_animationDrivers.begin() + driverStartIndex;
                i!=_animationDrivers.begin() + driverEndIndex; ++i) {
            auto colladaId      = skeleton.GetTransformationMachine().StringIdToHashedId(_parameterInterfaceDefinition[i->_parameterIndex]);
            auto typeAndIndex   = skeleton.GetTransformationMachine().GetParameterIndex(colladaId);
            if (i->_samplerType == Assets::TransformationParameterSet::Type::Float4x4) {
                const Assets::RawAnimationCurve* curve = accessableObjects.Get<Assets::RawAnimationCurve>(i->_curveId);
                if (curve) {
                    assert(typeAndIndex.first == Assets::TransformationParameterSet::Type::Float4x4);
                    // assert(i->_index < float4x4s.size());
                    float4x4s[typeAndIndex.second] = curve->Calculate<Float4x4>(time);
                }
            } else if (i->_samplerType == Assets::TransformationParameterSet::Type::Float4) {
                const Assets::RawAnimationCurve* curve = accessableObjects.Get<Assets::RawAnimationCurve>(i->_curveId);
                if (curve) {
                    if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float4) {
                        float4s[typeAndIndex.second] = curve->Calculate<Float4>(time);
                    } else if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float3) {
                        float3s[typeAndIndex.second] = Truncate(curve->Calculate<Float4>(time));
                    }
                }
            } else if (i->_samplerType == Assets::TransformationParameterSet::Type::Float3) {
                const Assets::RawAnimationCurve* curve = accessableObjects.Get<Assets::RawAnimationCurve>(i->_curveId);
                if (curve) {
                    assert(typeAndIndex.first == Assets::TransformationParameterSet::Type::Float3);
                    // assert(i->_index < float3s.size());
                    float3s[typeAndIndex.second] = curve->Calculate<Float3>(time);
                }
            } else if (i->_samplerType == Assets::TransformationParameterSet::Type::Float1) {
                const Assets::RawAnimationCurve* curve = accessableObjects.Get<Assets::RawAnimationCurve>(i->_curveId);
                if (curve) {
                    float result = curve->Calculate<float>(time);
                    if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float1) {
                        // assert(i->_index < float1s.size());
                        float1s[typeAndIndex.second] = result;
                    } else if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float3) {
                        assert(i->_samplerOffset < 3);
                        float3s[typeAndIndex.second][i->_samplerOffset] = result;
                    } else if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float4) {
                        assert(i->_samplerOffset < 4);
                        float4s[typeAndIndex.second][i->_samplerOffset] = result;
                    }
                }
            }
        }

        for (   auto i=_constantDrivers.begin() + constantDriverStartIndex;
                i!=_constantDrivers.begin() + constantDriverEndIndex; ++i) {
            auto colladaId      = skeleton.GetTransformationMachine().StringIdToHashedId(_parameterInterfaceDefinition[i->_parameterIndex]);
            auto typeAndIndex   = skeleton.GetTransformationMachine().GetParameterIndex(colladaId);
            const void* data    = PtrAdd(AsPointer(_constantData.begin()), i->_dataOffset);
            if (i->_samplerType == Assets::TransformationParameterSet::Type::Float4x4) {
                assert(typeAndIndex.first == Assets::TransformationParameterSet::Type::Float4x4);
                float4x4s[typeAndIndex.second] = *(const Float4x4*)data;
            } else if (i->_samplerType == Assets::TransformationParameterSet::Type::Float4) {
                if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float4) {
                    float4s[typeAndIndex.second] = *(const Float4*)data;
                } else if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float3) {
                    float3s[typeAndIndex.second] = Truncate(*(const Float4*)data);
                }
            } else if (i->_samplerType == Assets::TransformationParameterSet::Type::Float3) {
                assert(typeAndIndex.first == Assets::TransformationParameterSet::Type::Float3);
                float3s[typeAndIndex.second] = *(Float3*)data;
            } else if (i->_samplerType == Assets::TransformationParameterSet::Type::Float1) {
                if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float1) {
                    float1s[typeAndIndex.second] = *(float*)data;
                } else if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float3) {
                    assert(i->_samplerOffset < 3);
                    float3s[typeAndIndex.second][i->_samplerOffset] = *(const float*)data;
                } else if (typeAndIndex.first == Assets::TransformationParameterSet::Type::Float4) {
                    assert(i->_samplerOffset < 4);
                    float4s[typeAndIndex.second][i->_samplerOffset] = *(const float*)data;
                }
            }
        }

        return result;
    }

    AnimSamplerType      SamplerWidthToType(unsigned samplerWidth)
    {
        Assets::TransformationParameterSet::Type::Enum samplerType = Assets::TransformationParameterSet::Type::Float1;
        switch (samplerWidth) {
        case  1:    samplerType = Assets::TransformationParameterSet::Type::Float1;     break;
        case  3:    samplerType = Assets::TransformationParameterSet::Type::Float3;     break;
        case  4:    samplerType = Assets::TransformationParameterSet::Type::Float4;     break;
        case 16:    samplerType = Assets::TransformationParameterSet::Type::Float4x4;   break;
        default: 
            ThrowException(
                FormatError(
                    "Strange sampler width encountered when adding animation driver!"));
        }

        return samplerType;
    }

    size_t      SamplerSize(AnimSamplerType samplerType)
    {
        switch (samplerType) {
        case Assets::TransformationParameterSet::Type::Float1:      return sizeof(float);
        case Assets::TransformationParameterSet::Type::Float3:      return sizeof(Float3);
        case Assets::TransformationParameterSet::Type::Float4:      return sizeof(Float4);
        case Assets::TransformationParameterSet::Type::Float4x4:    return sizeof(Float4x4);
        }
        return 0;
    }

    void    NascentAnimationSet::AddConstantDriver( 
                                    const std::string&  parameterName, 
                                    const void*         constantValue, 
                                    AnimSamplerType     samplerType, 
                                    unsigned            samplerOffset)
    {
        size_t parameterIndex = _parameterInterfaceDefinition.size();
        auto i = std::find( _parameterInterfaceDefinition.cbegin(), 
                            _parameterInterfaceDefinition.cend(), parameterName);
        if (i!=_parameterInterfaceDefinition.end()) {
            parameterIndex = (unsigned)std::distance(_parameterInterfaceDefinition.cbegin(), i);
        } else {
            _parameterInterfaceDefinition.push_back(parameterName);
        }

        unsigned dataOffset = unsigned(_constantData.size());
        std::copy(
            (uint8*)constantValue, PtrAdd((uint8*)constantValue, SamplerSize(samplerType)),
            std::back_inserter(_constantData));

        _constantDrivers.push_back(
            ConstantDriver(dataOffset, (unsigned)parameterIndex, samplerType, samplerOffset));
    }

    void    NascentAnimationSet::AddAnimationDriver( 
        const std::string& parameterName, 
        ObjectGuid curveId, 
        AnimSamplerType samplerType, unsigned samplerOffset)
    {
        size_t parameterIndex = _parameterInterfaceDefinition.size();
        auto i = std::find( _parameterInterfaceDefinition.cbegin(), 
                            _parameterInterfaceDefinition.cend(), parameterName);
        if (i!=_parameterInterfaceDefinition.end()) {
            parameterIndex = (unsigned)std::distance(_parameterInterfaceDefinition.cbegin(), i);
        } else {
            _parameterInterfaceDefinition.push_back(parameterName);
        }

        _animationDrivers.push_back(
            AnimationDriver(curveId, (unsigned)parameterIndex, samplerType, samplerOffset));
    }

    bool    NascentAnimationSet::HasAnimationDriver(const std::string&  parameterName) const
    {
        auto i = std::find( _parameterInterfaceDefinition.cbegin(), 
                            _parameterInterfaceDefinition.cend(), parameterName);
        if (i==_parameterInterfaceDefinition.end()) 
            return false;

        auto parameterIndex = (unsigned)std::distance(_parameterInterfaceDefinition.cbegin(), i);

        for (auto i=_animationDrivers.begin(); i!=_animationDrivers.end(); ++i) {
            if (i->_parameterIndex == parameterIndex)
                return true;
        }

        for (auto i=_constantDrivers.begin(); i!=_constantDrivers.end(); ++i) {
            if (i->_parameterIndex == parameterIndex)
                return true;
        }
        return false;
    }

    void    NascentAnimationSet::MergeAnimation(
                const NascentAnimationSet& animation, const char name[],
                const TableOfObjects& sourceObjects, TableOfObjects& destinationObjects)
    {
            //
            //      Merge the animation drivers in the given input animation, and give 
            //      them the supplied name
            //
        float minTime = FLT_MAX, maxTime = -FLT_MAX;
        size_t startIndex = _animationDrivers.size();
        size_t constantStartIndex = _constantDrivers.size();
        for (auto i=animation._animationDrivers.cbegin(); i!=animation._animationDrivers.end(); ++i) {
            const Assets::RawAnimationCurve* animCurve = sourceObjects.Get<Assets::RawAnimationCurve>(i->_curveId);
            if (animCurve) {
                float curveStart = animCurve->StartTime();
                float curveEnd = animCurve->EndTime();
                minTime = std::min(minTime, curveStart);
                maxTime = std::max(maxTime, curveEnd);

                auto desc = sourceObjects.GetDesc<Assets::RawAnimationCurve>(i->_curveId);
                
                const std::string& name = animation._parameterInterfaceDefinition[i->_parameterIndex];
                Assets::RawAnimationCurve duplicate(*animCurve);
                destinationObjects.Add(
                    i->_curveId,
                    std::get<0>(desc), std::get<1>(desc), 
                    std::move(duplicate));
                AddAnimationDriver(name, i->_curveId, i->_samplerType, i->_samplerOffset);
            }
        }

        for (auto i=animation._constantDrivers.cbegin(); i!=animation._constantDrivers.end(); ++i) {
            const std::string& name = animation._parameterInterfaceDefinition[i->_parameterIndex];
            AddConstantDriver(name, PtrAdd(AsPointer(animation._constantData.begin()), i->_dataOffset), i->_samplerType, i->_samplerOffset);
        }

        _animations.push_back(Animation(name, 
            (unsigned)startIndex, (unsigned)_animationDrivers.size(), 
            (unsigned)constantStartIndex, (unsigned)_constantDrivers.size(),
            minTime, maxTime));
    }

    void NascentAnimationSet::AnimationDriver::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        Serialization::Serialize(serializer, unsigned(_curveId._objectId));
        Serialization::Serialize(serializer, _parameterIndex);
        Serialization::Serialize(serializer, _samplerOffset);
        Serialization::Serialize(serializer, unsigned(_samplerType));
    }

    void NascentAnimationSet::ConstantDriver::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        Serialization::Serialize(serializer, _dataOffset);
        Serialization::Serialize(serializer, _parameterIndex);
        Serialization::Serialize(serializer, _samplerOffset);
        Serialization::Serialize(serializer, unsigned(_samplerType));
    }

    struct AnimationDesc        // matches AnimationSet::Animation
    {
        uint64      _name;
        unsigned    _beginDriver, _endDriver;
        unsigned    _beginConstantDriver, _endConstantDriver;
        float       _beginTime, _endTime; 

        AnimationDesc() {}
        void Serialize(Serialization::NascentBlockSerializer& serializer) const
        {
            Serialization::Serialize(serializer, _name);
            Serialization::Serialize(serializer, _beginDriver);
            Serialization::Serialize(serializer, _endDriver);
            Serialization::Serialize(serializer, _beginConstantDriver);
            Serialization::Serialize(serializer, _endConstantDriver);
            Serialization::Serialize(serializer, _beginTime);
            Serialization::Serialize(serializer, _endTime);
        }
    };

    struct CompareAnimationName
    {
        bool operator()(const AnimationDesc& lhs, const AnimationDesc& rhs) const   { return lhs._name < rhs._name; }
        bool operator()(const AnimationDesc& lhs, uint64 rhs) const                 { return lhs._name < rhs; }
        bool operator()(uint64 lhs, const AnimationDesc& rhs) const                 { return lhs < rhs._name; }
    };

    void NascentAnimationSet::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        serializer.SerializeSubBlock(AsPointer(_animationDrivers.begin()), AsPointer(_animationDrivers.end()));
        serializer.SerializeValue(_animationDrivers.size());
        serializer.SerializeSubBlock(AsPointer(_constantDrivers.begin()), AsPointer(_constantDrivers.end()));
        serializer.SerializeValue(_constantDrivers.size());
        serializer.SerializeSubBlock(AsPointer(_constantData.begin()), AsPointer(_constantData.end()));

            //      List of animations...

        auto outputAnimations = std::make_unique<AnimationDesc[]>(_animations.size());
        for (size_t c=0; c<_animations.size(); ++c) {
            AnimationDesc&o = outputAnimations[c];
            const Animation&i = _animations[c];
            o._name = Hash64(AsPointer(i._name.begin()), AsPointer(i._name.end()));
            o._beginDriver = i._begin; o._endDriver = i._end;
            o._beginConstantDriver = i._constantBegin; o._endConstantDriver = i._constantEnd;
            o._beginTime = i._startTime; o._endTime = i._endTime;
        }
        std::sort(outputAnimations.get(), &outputAnimations[_animations.size()], CompareAnimationName());
        serializer.SerializeSubBlock(outputAnimations.get(), &outputAnimations[_animations.size()]);
        serializer.SerializeValue(_animations.size());

            //      Output interface...

        ConsoleRig::DebuggerOnlyWarning("Animation set output interface:\n");
        auto parameterNameHashes = std::make_unique<uint64[]>(_parameterInterfaceDefinition.size());
        for (size_t c=0; c<_parameterInterfaceDefinition.size(); ++c) {
            ConsoleRig::DebuggerOnlyWarning("  [%i] %s\n", c, _parameterInterfaceDefinition[c].c_str());
            parameterNameHashes[c] = Hash64(AsPointer(_parameterInterfaceDefinition[c].begin()), AsPointer(_parameterInterfaceDefinition[c].end()));
        }
        serializer.SerializeSubBlock(parameterNameHashes.get(), &parameterNameHashes[_parameterInterfaceDefinition.size()]);
        serializer.SerializeValue(_parameterInterfaceDefinition.size());
    }

    NascentAnimationSet::NascentAnimationSet() {}
    NascentAnimationSet::~NascentAnimationSet() {}
    NascentAnimationSet::NascentAnimationSet(NascentAnimationSet&& moveFrom)
    :   _animationDrivers(std::move(moveFrom._animationDrivers))
    ,   _constantDrivers(std::move(moveFrom._constantDrivers))
    ,   _animations(std::move(moveFrom._animations))
    ,   _parameterInterfaceDefinition(std::move(moveFrom._parameterInterfaceDefinition))
    ,   _constantData(std::move(moveFrom._constantData))
    {}
    NascentAnimationSet& NascentAnimationSet::operator=(NascentAnimationSet&& moveFrom)
    {
        _animationDrivers = std::move(moveFrom._animationDrivers);
        _constantDrivers = std::move(moveFrom._constantDrivers);
        _animations = std::move(moveFrom._animations);
        _parameterInterfaceDefinition = std::move(moveFrom._parameterInterfaceDefinition);
        _constantData = std::move(moveFrom._constantData);
        return *this;
    }





    void NascentSkeleton::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        Serialization::Serialize(serializer, _transformationMachine);
    }

    NascentSkeleton::NascentSkeleton() {}
    NascentSkeleton::~NascentSkeleton() {}
    NascentSkeleton::NascentSkeleton(NascentSkeleton&& moveFrom)
    :   _transformationMachine(std::move(moveFrom._transformationMachine))
    {}
    NascentSkeleton& NascentSkeleton::operator=(NascentSkeleton&& moveFrom)
    {
        _transformationMachine = std::move(moveFrom._transformationMachine);
        return *this;
    }




    class NascentModelCommandStream::TransformationMachineOutput
    {
    public:
        ObjectGuid _colladaId;
        std::string _name;
    };

    unsigned NascentModelCommandStream::RegisterTransformationMachineOutput(const std::string& bindingName, ObjectGuid id)
    {
        auto existing = FindTransformationMachineOutput(id);
        if (existing != ~unsigned(0)) return existing;

        auto result = (unsigned)_transformationMachineOutputs.size();
        _transformationMachineOutputs.push_back(
            TransformationMachineOutput{id, bindingName});
        return result;
    }

    unsigned    NascentModelCommandStream::FindTransformationMachineOutput(ObjectGuid nodeId) const
    {
        auto i2=_transformationMachineOutputs.cbegin();
        for (;i2!=_transformationMachineOutputs.cend(); ++i2) {
            if (i2->_colladaId == nodeId) {
                return (unsigned)std::distance(_transformationMachineOutputs.cbegin(), i2);
            }
        }
        return ~unsigned(0x0);
    }

    NascentModelCommandStream::NascentModelCommandStream()
    {
    }

    NascentModelCommandStream::~NascentModelCommandStream()
    {

    }

    NascentModelCommandStream::NascentModelCommandStream(NascentModelCommandStream&& moveFrom)
    :       _transformationMachineOutputs(std::move(moveFrom._transformationMachineOutputs))
    ,       _geometryInstances(std::move(moveFrom._geometryInstances))
    // ,       _modelInstances(std::move(moveFrom._modelInstances))
    ,       _cameraInstances(std::move(moveFrom._cameraInstances))
    ,       _skinControllerInstances(std::move(moveFrom._skinControllerInstances))
    {
    }

    NascentModelCommandStream& NascentModelCommandStream::operator=(NascentModelCommandStream&& moveFrom) never_throws
    {
        _transformationMachineOutputs = std::move(moveFrom._transformationMachineOutputs);
        _geometryInstances = std::move(moveFrom._geometryInstances);
        // _modelInstances = std::move(moveFrom._modelInstances);
        _cameraInstances = std::move(moveFrom._cameraInstances);
        _skinControllerInstances = std::move(moveFrom._skinControllerInstances);
        return *this;
    }

    void NascentModelCommandStream::GeometryInstance::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        Serialization::Serialize(serializer, _id);
        Serialization::Serialize(serializer, _localToWorldId);
        serializer.SerializeSubBlock(AsPointer(_materials.begin()), AsPointer(_materials.end()));
        Serialization::Serialize(serializer, _materials.size());
        Serialization::Serialize(serializer, _levelOfDetail);
    }

    void NascentModelCommandStream::SkinControllerInstance::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        Serialization::Serialize(serializer, _id);
        Serialization::Serialize(serializer, _localToWorldId);
        serializer.SerializeSubBlock(AsPointer(_materials.begin()), AsPointer(_materials.end()));
        Serialization::Serialize(serializer, _materials.size());
        Serialization::Serialize(serializer, _levelOfDetail);
    }

    void NascentModelCommandStream::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        serializer.SerializeSubBlock(AsPointer(_geometryInstances.begin()), AsPointer(_geometryInstances.end()));
        serializer.SerializeValue(_geometryInstances.size());
        serializer.SerializeSubBlock(AsPointer(_skinControllerInstances.begin()), AsPointer(_skinControllerInstances.end()));
        serializer.SerializeValue(_skinControllerInstances.size());
            
            //
            //      Turn our list of input matrices into hash values, and write out the
            //      run-time input interface definition...
            //
        ConsoleRig::DebuggerOnlyWarning("Command stream input interface:\n");
        auto inputInterface = std::make_unique<uint64[]>(_transformationMachineOutputs.size());
        unsigned c=0;
        for (auto i=_transformationMachineOutputs.begin(); i!=_transformationMachineOutputs.end(); ++i, ++c) {
            ConsoleRig::DebuggerOnlyWarning("  [%i] %s\n", std::distance(_transformationMachineOutputs.begin(), i), i->_name.c_str());
            inputInterface[c] = Hash64(AsPointer(i->_name.begin()), AsPointer(i->_name.end()));
        }
        serializer.SerializeSubBlock(inputInterface.get(), &inputInterface[_transformationMachineOutputs.size()]);
        serializer.SerializeValue(_transformationMachineOutputs.size());
    }

    



///////////////////////////////////////////////////////////////////////////////////////////////////

    void CopyVertexElements(     
        void* destinationBuffer,            size_t destinationVertexStride,
        const void* sourceBuffer,           size_t sourceVertexStride,
        const Metal::InputElementDesc* destinationLayoutBegin,  const Metal::InputElementDesc* destinationLayoutEnd,
        const Metal::InputElementDesc* sourceLayoutBegin,       const Metal::InputElementDesc* sourceLayoutEnd,
        const uint16* reorderingBegin,      const uint16* reorderingEnd )
    {
        uint32      elementReordering[32];
        signed      maxSourceLayout = -1;
        for (auto source=sourceLayoutBegin; source!=sourceLayoutEnd; ++source) {
                //      look for the same element in the destination layout (or put ~uint16(0x0) if it's not there)
            elementReordering[source-sourceLayoutBegin] = ~uint32(0x0);
            for (auto destination=destinationLayoutBegin; destination!=destinationLayoutEnd; ++destination) {
                if (    destination->_semanticName   == source->_semanticName 
                    &&  destination->_semanticIndex  == source->_semanticIndex
                    &&  destination->_nativeFormat   == source->_nativeFormat) {

                    elementReordering[source-sourceLayoutBegin] = uint32(destination-destinationLayoutBegin);
                    maxSourceLayout = std::max(maxSourceLayout, signed(source-sourceLayoutBegin));
                    break;
                }
            }
        }

        if (maxSourceLayout<0) return;

        size_t vertexCount = reorderingEnd - reorderingBegin; (void)vertexCount;

        #if defined(_DEBUG)
                    //  fill in some dummy values
            std::fill((uint8*)destinationBuffer, (uint8*)PtrAdd(destinationBuffer, vertexCount*destinationVertexStride), 0xaf);
        #endif

            ////////////////     copy each vertex (slowly) piece by piece       ////////////////
        for (auto reordering = reorderingBegin; reordering!=reorderingEnd; ++reordering) {
            size_t sourceIndex               = reordering-reorderingBegin, destinationIndex = *reordering;
            void* destinationVertexStart     = PtrAdd(destinationBuffer, destinationIndex*destinationVertexStride);
            const void* sourceVertexStart    = PtrAdd(sourceBuffer, sourceIndex*sourceVertexStride);
            for (unsigned c=0; c<=(unsigned)maxSourceLayout; ++c) {
                if (elementReordering[c] != ~uint16(0x0)) {
                    const Metal::InputElementDesc& destinationElement = destinationLayoutBegin[elementReordering[c]]; assert(&destinationElement < destinationLayoutEnd);
                    const Metal::InputElementDesc& sourceElement = sourceLayoutBegin[c]; assert(&sourceElement < sourceLayoutEnd);
                    size_t elementSize = Metal::BitsPerPixel(destinationElement._nativeFormat)/8;
                    assert(elementSize == Metal::BitsPerPixel(sourceElement._nativeFormat)/8);
                    assert(destinationElement._alignedByteOffset + elementSize <= destinationVertexStride);
                    assert(sourceElement._alignedByteOffset + elementSize <= sourceVertexStride);
                    assert(PtrAdd(destinationVertexStart, destinationElement._alignedByteOffset+elementSize) <= PtrAdd(destinationVertexStart, vertexCount*destinationVertexStride));
                    assert(PtrAdd(sourceVertexStart, sourceElement._alignedByteOffset+elementSize) <= PtrAdd(sourceVertexStart, vertexCount*sourceVertexStride));

                    XlCopyMemory(
                        PtrAdd(destinationVertexStart, destinationElement._alignedByteOffset),
                        PtrAdd(sourceVertexStart, sourceElement._alignedByteOffset),
                        elementSize);
                }
            }
        }
    }

    unsigned CalculateVertexSize(
        const Metal::InputElementDesc* layoutBegin,  
        const Metal::InputElementDesc* layoutEnd)
    {
        unsigned result = 0;
        for (auto l=layoutBegin; l!=layoutEnd; ++l)
            result += Metal::BitsPerPixel(l->_nativeFormat);
        return result/8;
    }

}}

