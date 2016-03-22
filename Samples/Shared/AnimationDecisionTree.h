// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Vector.h"
#include "../../Core/Types.h"
#include <vector>

namespace RenderCore { namespace Assets { class AnimationSet; class AnimationImmutableData; } }

namespace Sample
{
    /// Container for AnimationType::Enum
    namespace AnimationType
    {
        enum Enum 
        {
            Idle,

            RunForward,
            RunLeft,
            RunRight,
            RunBack,

            Max
        };
    }

    class AnimationState
    {
    public:
        uint64              _animation;
        float               _time;
        AnimationType::Enum _type;
        float               _animationDuration;
        Float3              _motionCompensation;

        AnimationState()
        {
            _animation = 0;
            _time = 0.;
            _type = AnimationType::Max;
            _animationDuration = 0.f;
            _motionCompensation = Float3(0.f, 0.f, 0.f);
        }
    };

    class AnimationNames
    {
    public:
        std::string _runForward, _runBack;
        std::string _runLeft, _runRight;

        std::string _runForward_ToIdle;
        std::string _runBack_ToIdle;
        std::string _runLeft_ToIdle;
        std::string _runRight_ToIdle;

        std::string _idle, _idle1, _idle2;
        std::string _idle3, _idle4, _idle5;

        std::string _rootTransform;

        uint64 MakeHash() const;
    };
    
    /// <summary>Simple logic for character animation</summary>
    /// Typically when animating characters we need some structure to
    /// decide what animation to play at a given time (given player or
    /// AI inputs). 
    /// Here is a simple (and limited) implementation for selecting 
    /// whole-body movement animations and smoothly transitioning between them.
    class AnimationDecisionTree
    {
    public:
        AnimationState      Update(float deltaTime, const AnimationState& prevState, const Float3& localTranslation, float rotation);
        AnimationState      PlayAnimation(uint64 animation, const AnimationState& prevState, const RenderCore::Assets::AnimationSet& animSet);

        AnimationDecisionTree(
            const AnimationNames& cfg,
            const RenderCore::Assets::AnimationImmutableData& animSet, 
            float characterScale);
        ~AnimationDecisionTree();

    private:
        struct AnimationDesc
        { 
            uint64  _name;
            float   _duration;
        };
        AnimationDesc   _mainAnimations[AnimationType::Max];
        AnimationDesc   _runForwardToIdle;
        AnimationDesc   _runBackToIdle;
        AnimationDesc   _runLeftToIdle;
        AnimationDesc   _runRightToIdle;
        std::vector<AnimationDesc> _extraIdles;

        Float3          _runForwardVelocity;
        Float3          _runBackVelocity;
        Float3          _runLeftVelocity;
        Float3          _runRightVelocity;

        float           _characterScale;

        AnimationDesc   BuildAnimationDesc(const RenderCore::Assets::AnimationSet& animSet, uint64 animation) const;
        Float3          ExtractMotionVelocity(const RenderCore::Assets::AnimationImmutableData& animSet, uint64 animation, uint32 rootNodeParameter);
        void            SelectIdleAnimation(AnimationState& state);
    };
}

