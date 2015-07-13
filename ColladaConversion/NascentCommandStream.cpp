// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentCommandStream.h"
#include "../RenderCore/Assets/RawAnimationCurve.h"
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
        unsigned curveId, 
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
        const std::vector<Assets::RawAnimationCurve>& sourceCurves, 
        std::vector<Assets::RawAnimationCurve>& destinationCurves)
    {
            //
            //      Merge the animation drivers in the given input animation, and give 
            //      them the supplied name
            //
        float minTime = FLT_MAX, maxTime = -FLT_MAX;
        size_t startIndex = _animationDrivers.size();
        size_t constantStartIndex = _constantDrivers.size();
        for (auto i=animation._animationDrivers.cbegin(); i!=animation._animationDrivers.end(); ++i) {
            if (i->_curveIndex >= sourceCurves.size()) continue;
            const auto* animCurve = &sourceCurves[i->_curveIndex];
            if (animCurve) {
                float curveStart = animCurve->StartTime();
                float curveEnd = animCurve->EndTime();
                minTime = std::min(minTime, curveStart);
                maxTime = std::max(maxTime, curveEnd);

                const std::string& name = animation._parameterInterfaceDefinition[i->_parameterIndex];
                destinationCurves.push_back(Assets::RawAnimationCurve(*animCurve));
                AddAnimationDriver(
                    name, unsigned(destinationCurves.size()-1), 
                    i->_samplerType, i->_samplerOffset);
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
        ::Serialize(serializer, unsigned(_curveIndex));
        ::Serialize(serializer, _parameterIndex);
        ::Serialize(serializer, _samplerOffset);
        ::Serialize(serializer, unsigned(_samplerType));
    }

    void NascentAnimationSet::ConstantDriver::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        ::Serialize(serializer, _dataOffset);
        ::Serialize(serializer, _parameterIndex);
        ::Serialize(serializer, _samplerOffset);
        ::Serialize(serializer, unsigned(_samplerType));
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
            ::Serialize(serializer, _name);
            ::Serialize(serializer, _beginDriver);
            ::Serialize(serializer, _endDriver);
            ::Serialize(serializer, _beginConstantDriver);
            ::Serialize(serializer, _endConstantDriver);
            ::Serialize(serializer, _beginTime);
            ::Serialize(serializer, _endTime);
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
        ::Serialize(serializer, _transformationMachine);
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

    void NascentModelCommandStream::RegisterTransformationMachineOutput(const std::string& bindingName, ObjectGuid id, unsigned transformMarker)
    {
        // auto existing = FindTransformationMachineOutput(id);
        // if (existing != ~unsigned(0)) return existing;
        // 
        // auto result = (unsigned)_transformationMachineOutputs.size();
        // _transformationMachineOutputs.push_back(
        //     TransformationMachineOutput{id, bindingName});
        // return result;
        if (_transformationMachineOutputs.size() < transformMarker+1)
            _transformationMachineOutputs.resize(transformMarker+1);
        _transformationMachineOutputs[transformMarker] = TransformationMachineOutput{id, bindingName};
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

    void NascentModelCommandStream::Add(GeometryInstance&& geoInstance)
    {
        _geometryInstances.emplace_back(std::move(geoInstance));
    }

    void NascentModelCommandStream::Add(CameraInstance&& camInstance)
    {
        _cameraInstances.emplace_back(std::move(camInstance));
    }

    void NascentModelCommandStream::Add(SkinControllerInstance&& skinControllerInstance)
    {
        _skinControllerInstances.emplace_back(std::move(skinControllerInstance));
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
        ::Serialize(serializer, _id);
        ::Serialize(serializer, _localToWorldId);
        serializer.SerializeSubBlock(AsPointer(_materials.begin()), AsPointer(_materials.end()));
        ::Serialize(serializer, _materials.size());
        ::Serialize(serializer, _levelOfDetail);
    }

    void NascentModelCommandStream::SkinControllerInstance::Serialize(Serialization::NascentBlockSerializer& serializer) const
    {
        ::Serialize(serializer, _id);
        ::Serialize(serializer, _localToWorldId);
        serializer.SerializeSubBlock(AsPointer(_materials.begin()), AsPointer(_materials.end()));
        ::Serialize(serializer, _materials.size());
        ::Serialize(serializer, _levelOfDetail);
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
        auto inputInterface = GetInputInterface();
        serializer.SerializeSubBlock(AsPointer(inputInterface.cbegin()), AsPointer(inputInterface.cend()));
        serializer.SerializeValue(_transformationMachineOutputs.size());
    }

    std::vector<uint64> NascentModelCommandStream::GetInputInterface() const
    {
        std::vector<uint64> inputInterface(_transformationMachineOutputs.size());
        unsigned c=0;
        for (auto i=_transformationMachineOutputs.begin(); i!=_transformationMachineOutputs.end(); ++i, ++c)
            inputInterface[c] = Hash64(AsPointer(i->_name.begin()), AsPointer(i->_name.end()));
        return std::move(inputInterface);
    }

    std::ostream& operator<<(std::ostream& stream, const NascentModelCommandStream& cmdStream)
    {
        stream << "Command stream input interface:" << std::endl;
        unsigned c=0;
        for (auto i=cmdStream._transformationMachineOutputs.begin(); i!=cmdStream._transformationMachineOutputs.end(); ++i, ++c)
            stream << "  [" << c << "] " << i->_name << std::endl;
        return stream;
    }


}}

