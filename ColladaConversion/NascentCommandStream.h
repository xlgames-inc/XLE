// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ConversionCore.h"  // for ObjectGuid
#include "../RenderCore/Assets/TransformationCommands.h"
#include "../RenderCore/Assets/NascentTransformationMachine.h"
#include "../Utility/Mixins.h"
#include <vector>

namespace Serialization { class NascentBlockSerializer; }
namespace RenderCore { namespace Assets { class NascentTransformationMachine; class RawAnimationCurve; }}
namespace Utility { class OutputStream; }

namespace RenderCore { namespace ColladaConversion
{
    class NascentSkeleton;

        //
        //      "NascentAnimationSet" is a set of animations
        //      and some information to bind these animations to
        //      a skeleton
        //

    typedef Assets::TransformationParameterSet::Type::Enum AnimSamplerType;

    class NascentAnimationSet : noncopyable
    {
    public:
                    /////   A N I M A T I O N   D R I V E R   /////
        class AnimationDriver
        {
        public:
            unsigned            _curveIndex;
            unsigned            _parameterIndex;
            unsigned            _samplerOffset;
            AnimSamplerType     _samplerType;

            AnimationDriver(
                unsigned            curveIndex, 
                unsigned            parameterIndex, 
                AnimSamplerType     samplerType, 
                unsigned            samplerOffset)
            : _curveIndex(curveIndex)
            , _parameterIndex(parameterIndex)
            , _samplerType(samplerType)
            , _samplerOffset(samplerOffset) {}

            void Serialize(Serialization::NascentBlockSerializer& serializer) const;
        };

                    /////   C O N S T A N T   D R I V E R   /////
        class ConstantDriver
        {
        public:
            unsigned            _dataOffset;
            unsigned            _parameterIndex;
            unsigned            _samplerOffset;
            AnimSamplerType     _samplerType;

            ConstantDriver(
                unsigned            dataOffset,
                unsigned            parameterIndex,
                AnimSamplerType     samplerType,
                unsigned            samplerOffset)
            : _dataOffset(dataOffset)
            , _parameterIndex(parameterIndex)
            , _samplerType(samplerType)
            , _samplerOffset(samplerOffset) {}

            void Serialize(Serialization::NascentBlockSerializer& serializer) const;
        };

        void    AddAnimationDriver(
            const std::string&  parameterName, 
            unsigned            curveId, 
            AnimSamplerType     samplerType, 
            unsigned            samplerOffset);

        void    AddConstantDriver(  
            const std::string&  parameterName, 
            const void*         constantValue, 
            AnimSamplerType     samplerType, 
            unsigned            samplerOffset);

        bool    HasAnimationDriver(const std::string&  parameterName) const;

        void    MergeAnimation(
            const NascentAnimationSet& animation, const char name[],
            const std::vector<Assets::RawAnimationCurve>& sourceCurves, 
            std::vector<Assets::RawAnimationCurve>& destinationCurves);

        NascentAnimationSet();
        ~NascentAnimationSet();
        NascentAnimationSet(NascentAnimationSet&& moveFrom);
        NascentAnimationSet& operator=(NascentAnimationSet&& moveFrom);

        void            Serialize(Serialization::NascentBlockSerializer& serializer) const;
    private:
        std::vector<AnimationDriver>    _animationDrivers;
        std::vector<ConstantDriver>     _constantDrivers;
        class Animation;
        std::vector<Animation>          _animations;
        std::vector<std::string>        _parameterInterfaceDefinition;
        std::vector<uint8>              _constantData;
    };

    AnimSamplerType     SamplerWidthToType(unsigned samplerWidth);
    size_t              SamplerSize(AnimSamplerType samplerType);

        //
        //      "NascentSkeleton" represents the skeleton information for an 
        //      object. Usually this is mostly just the transformation machine.
        //      But we also need some binding information for binding the output
        //      matrices of the transformation machine to joints.
        //

    class NascentSkeleton : noncopyable
    {
    public:
        using NascentTransformationMachine = RenderCore::Assets::NascentTransformationMachine;

        NascentTransformationMachine&           GetTransformationMachine()          { return _transformationMachine; }
        const NascentTransformationMachine&     GetTransformationMachine() const    { return _transformationMachine; }

        void        Serialize(Serialization::NascentBlockSerializer& serializer) const;

        NascentSkeleton();
        ~NascentSkeleton();
        NascentSkeleton(NascentSkeleton&& moveFrom);
        NascentSkeleton& operator=(NascentSkeleton&& moveFrom);
    private:
        NascentTransformationMachine    _transformationMachine;
    };

        //
        //      "Model Command Stream" is the list of model elements
        //      from a single export. Usually it will be mostly geometry
        //      and skin controller elements.
        //

    class NascentModelCommandStream : noncopyable
    {
    public:
        typedef uint64 MaterialGuid;
        static const MaterialGuid s_materialGuid_Invalid = ~0x0ull;

            /////   G E O M E T R Y   I N S T A N C E   /////
        class GeometryInstance
        {
        public:
            unsigned                    _id;
            unsigned                    _localToWorldId;
            std::vector<MaterialGuid>   _materials;
            unsigned                    _levelOfDetail;
            GeometryInstance(
                unsigned id, unsigned localToWorldId, 
                std::vector<MaterialGuid>&& materials, unsigned levelOfDetail) 
            :   _id(id), _localToWorldId(localToWorldId)
            ,   _materials(std::forward<std::vector<MaterialGuid>>(materials))
            ,   _levelOfDetail(levelOfDetail) {}

            void Serialize(Serialization::NascentBlockSerializer& serializer) const;
        };

            /////   S K I N   C O N T R O L L E R   I N S T A N C E   /////
        class SkinControllerInstance
        {
        public:
            unsigned                    _id;
            unsigned                    _localToWorldId;
            std::vector<MaterialGuid>   _materials;
            unsigned                    _levelOfDetail;
            SkinControllerInstance(
                unsigned id, unsigned localToWorldId, 
                std::vector<MaterialGuid>&& materials, unsigned levelOfDetail)
            :   _id(id), _localToWorldId(localToWorldId), _materials(std::forward<std::vector<MaterialGuid>>(materials))
            ,   _levelOfDetail(levelOfDetail) {}

            void Serialize(Serialization::NascentBlockSerializer& serializer) const;
        };

            /////   C A M E R A   I N S T A N C E   /////
        class CameraInstance
        {
        public:
            unsigned    _localToWorldId;
            CameraInstance(unsigned localToWorldId) : _localToWorldId(localToWorldId) {}
        };

        void Add(GeometryInstance&& geoInstance);
        void Add(CameraInstance&& geoInstance);
        void Add(SkinControllerInstance&& geoInstance);

        bool IsEmpty() const { return _geometryInstances.empty() && _cameraInstances.empty() && _skinControllerInstances.empty(); }
        void Serialize(Serialization::NascentBlockSerializer& serializer) const;

        void RegisterTransformationMachineOutput(const std::string& bindingName, ObjectGuid id, unsigned transformMarker);
        unsigned FindTransformationMachineOutput(ObjectGuid nodeId) const;

        std::vector<uint64> GetInputInterface() const;
        unsigned GetMaxLOD() const;

        NascentModelCommandStream();
        ~NascentModelCommandStream();
        NascentModelCommandStream(NascentModelCommandStream&& moveFrom);
        NascentModelCommandStream& operator=(NascentModelCommandStream&& moveFrom);

        std::vector<GeometryInstance>               _geometryInstances;
        std::vector<CameraInstance>                 _cameraInstances;
        std::vector<SkinControllerInstance>         _skinControllerInstances;

        friend std::ostream& operator<<(std::ostream&, const NascentModelCommandStream&);

    private:
        class TransformationMachineOutput;
        std::vector<TransformationMachineOutput>    _transformationMachineOutputs;

        NascentModelCommandStream& operator=(const NascentModelCommandStream& copyFrom) never_throws;
        NascentModelCommandStream(const NascentModelCommandStream& copyFrom);
    };

    
    
}}


