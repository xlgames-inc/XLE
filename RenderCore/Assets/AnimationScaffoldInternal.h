// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TransformationCommands.h"

namespace RenderCore { namespace Assets
{
    class RawAnimationCurve;
    class AnimationSetBinding;
    typedef TransformationParameterSet::Type::Enum AnimSamplerType;
    class AnimationState;
    class TransformationMachine;

    #pragma pack(push)
    #pragma pack(1)

    class AnimationSet
    {
    public:
            /////   A N I M A T I O N   D R I V E R   /////
        class AnimationDriver
        {
        public:
            unsigned        _curveId;
            unsigned        _parameterIndex;
            unsigned        _samplerOffset;
            AnimSamplerType _samplerType;
        };

        class ConstantDriver
        {
        public:
            unsigned            _dataOffset;
            unsigned            _parameterIndex;
            unsigned            _samplerOffset;
            AnimSamplerType     _samplerType;
        };

        class OutputInterface
        {
        public:
            uint64*     _parameterInterfaceDefinition;
            size_t      _parameterInterfaceCount;
        };

        class Animation
        {
        public:
            uint64      _name;
            unsigned    _beginDriver, _endDriver;
            unsigned    _beginConstantDriver, _endConstantDriver;
            float       _beginTime, _endTime;
        };

        TransformationParameterSet  BuildTransformationParameterSet(
            const AnimationState&           animState,
            const TransformationMachine&    transformationMachine,
            const AnimationSetBinding&      binding,
            const RawAnimationCurve*        curves,
            size_t                          curvesCount) const;

        const AnimationDriver&  GetAnimationDriver(size_t index) const;
        size_t                  GetAnimationDriverCount() const;

        Animation               FindAnimation(uint64 animation) const;
        unsigned                FindParameter(uint64 parameterName) const;

        const OutputInterface&  GetOutputInterface() const { return _outputInterface; }

        ~AnimationSet();

		AnimationSet(const AnimationSet&) = delete;
		AnimationSet& operator=(const AnimationSet&) = delete;
    private:
        AnimationDriver*    _animationDrivers;
        size_t              _animationDriverCount;
        ConstantDriver*     _constantDrivers;
        size_t              _constantDriverCount;
        void*               _constantData;
        Animation*          _animations;
        size_t              _animationCount;
        OutputInterface     _outputInterface;
    };

    inline auto         AnimationSet::GetAnimationDriver(size_t index) const -> const AnimationDriver&            { return _animationDrivers[index]; }
    inline size_t       AnimationSet::GetAnimationDriverCount() const                                             { return _animationDriverCount; }

    class AnimationImmutableData
    {
    public:
        AnimationSet        _animationSet;
        RawAnimationCurve*  _curves;
        size_t              _curvesCount;

        ~AnimationImmutableData();
    };

    #pragma pack(pop)

}}

