// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NascentTransformationMachine.h"
#include "../RenderCore/Assets/TransformationCommands.h"		// (for TransformationParameterSet)
#include <vector>

namespace Serialization { class NascentBlockSerializer; }
namespace RenderCore { namespace Assets { class RawAnimationCurve; }}
namespace RenderCore { enum class Format : int; }
namespace Utility { class OutputStream; }

namespace RenderCore { namespace Assets { namespace GeoProc
{
    class NascentSkeleton;
	class NascentSkeletonMachine;

        //
        //      "NascentAnimationSet" is a set of animations
        //      and some information to bind these animations to
        //      a skeleton
        //
	
    class NascentAnimationSet
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
			Format				_format;
            unsigned            _samplerOffset;
            AnimSamplerType     _samplerType;

            ConstantDriver(
                unsigned            dataOffset,
                unsigned            parameterIndex,
				Format				format,
                AnimSamplerType     samplerType,
                unsigned            samplerOffset)
            : _dataOffset(dataOffset)
            , _parameterIndex(parameterIndex)
			, _format(format)
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
			size_t				constantValueSize,
			Format				format,
            AnimSamplerType     samplerType, 
            unsigned            samplerOffset);

        bool    HasAnimationDriver(const std::string&  parameterName) const;

        void    MergeAnimation(
            const NascentAnimationSet& animation, const std::string& name,
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

    size_t              SamplerSize(AnimSamplerType samplerType);

        //
        //      "NascentSkeleton" represents the skeleton information for an 
        //      object. Usually this is mostly just the transformation machine.
        //      But we also need some binding information for binding the output
        //      matrices of the transformation machine to joints.
        //

    class NascentSkeleton
    {
    public:
        NascentSkeletonMachine&				GetSkeletonMachine()			{ return _skeletonMachine; }
        const NascentSkeletonMachine&		GetSkeletonMachine() const		{ return _skeletonMachine; }
		NascentSkeletonInterface&			GetInterface()					{ return _interface; }
		const NascentSkeletonInterface&		GetInterface() const			{ return _interface; }
		TransformationParameterSet&         GetDefaultParameters()			{ return _defaultParameters; }
		const TransformationParameterSet&   GetDefaultParameters() const	{ return _defaultParameters; }

		void        Serialize(Serialization::NascentBlockSerializer& serializer) const;

        NascentSkeleton();
        ~NascentSkeleton();
        NascentSkeleton(NascentSkeleton&& moveFrom) = default;
        NascentSkeleton& operator=(NascentSkeleton&& moveFrom) = default;
    private:
        NascentSkeletonMachine		_skeletonMachine;
		NascentSkeletonInterface	_interface;
		TransformationParameterSet	_defaultParameters;
    };

        //
        //      "Model Command Stream" is the list of model elements
        //      from a single export. Usually it will be mostly geometry
        //      and skin controller elements.
        //

    class NascentModelCommandStream
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

        unsigned RegisterInputInterfaceMarker(const std::string& name);

        std::vector<uint64> GetInputInterface() const;
        unsigned GetMaxLOD() const;

        NascentModelCommandStream();
        ~NascentModelCommandStream();
        NascentModelCommandStream(NascentModelCommandStream&& moveFrom) = default;
        NascentModelCommandStream& operator=(NascentModelCommandStream&& moveFrom) = default;

        std::vector<GeometryInstance>               _geometryInstances;
        std::vector<CameraInstance>                 _cameraInstances;
        std::vector<SkinControllerInstance>         _skinControllerInstances;

        friend std::ostream& operator<<(std::ostream&, const NascentModelCommandStream&);

    private:
        std::vector<uint64>			_inputInterface;
		std::vector<std::string>	_inputInterfaceNames;

        NascentModelCommandStream& operator=(const NascentModelCommandStream& copyFrom) never_throws;
        NascentModelCommandStream(const NascentModelCommandStream& copyFrom);
    };
    
	class NascentSkeleton;
	class SkeletonRegistry;
	void RegisterNodeBindingNames(NascentModelCommandStream& stream, const SkeletonRegistry& registry);
	void RegisterNodeBindingNames(NascentSkeleton& skeleton, const SkeletonRegistry& registry);
}}}


