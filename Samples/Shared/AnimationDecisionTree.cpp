// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "AnimationDecisionTree.h"
#include "../../RenderCore/Assets/AnimationScaffoldInternal.h"
#include "../../RenderCore/Assets/RawAnimationCurve.h"
#include "../../Utility/MemoryUtils.h"

namespace Sample
{

    void                AnimationDecisionTree::SelectIdleAnimation(AnimationState& state)
    {
        if (rand() < RAND_MAX/2 || _extraIdles.empty()) {
            state._animation          = _mainAnimations[AnimationType::Idle]._name;
            state._animationDuration  = _mainAnimations[AnimationType::Idle]._duration;
        } else {
            unsigned extra = rand() % unsigned(_extraIdles.size());
            state._animation          = _extraIdles[extra]._name;
            state._animationDuration  = _extraIdles[extra]._duration;
        }
    }

    AnimationState      AnimationDecisionTree::PlayAnimation(uint64 animation, const AnimationState& prevState, const RenderCore::Assets::AnimationSet& animSet)
    {
        AnimationState result = prevState;
        result._animation           = animation;
        result._animationDuration   = BuildAnimationDesc(animSet, animation)._duration;
        result._motionCompensation  = Float3(0.f, 0.f, 0.f);
        result._time                = 0.f;
        result._type                = AnimationType::Idle;
        return result;
    }

    AnimationState      AnimationDecisionTree::Update(  float deltaTime, 
                                                        const AnimationState& prevState, 
                                                        const Float3& localTranslation, 
                                                        float rotation)
    {
            //
            //      First, calculate if we want to move forwards, backwards, left or right -- or just
            //      go to idle
            //
        auto newType = AnimationType::Idle;
        Float3 motionCompensation(0.f, 0.f, 0.f);
        const float deadZone = 20.f * deltaTime / _characterScale;
        if (XlAbs(localTranslation[1]) > XlAbs(localTranslation[0])) {
            if (localTranslation[1] > deadZone) {
                newType = AnimationType::RunForward;
                motionCompensation = -_runForwardVelocity;
            } else if (localTranslation[1] < -deadZone) {
                newType = AnimationType::RunBack;
                motionCompensation = -_runBackVelocity;
            }
        } else {
            if (localTranslation[0] > deadZone) {
                newType = AnimationType::RunRight;
                motionCompensation = -_runRightVelocity;
            } else if (localTranslation[0] < -deadZone) {
                newType = AnimationType::RunLeft;
                motionCompensation = -_runLeftVelocity;
            }
        }

            //
            //      Now, either transition to the new animation, or update the
            //      current one
            //
        AnimationState newState = prevState;
        if (prevState._type != newType) {

            newState._type               = newType;
            newState._time               = 0.f;
            newState._animation          = _mainAnimations[newType]._name;
            newState._animationDuration  = _mainAnimations[newType]._duration;
            newState._motionCompensation = motionCompensation;

            if (newType == AnimationType::Idle) {

                    //
                    //      When transitioning to an idle animation, we always
                    //      go through one of our intermediate animations
                    //

                if (prevState._type == AnimationType::RunForward) {
                    newState._animation          = _runForwardToIdle._name;
                    newState._animationDuration  = _runForwardToIdle._duration;
                } else if (prevState._type == AnimationType::RunBack) {
                    newState._animation          = _runBackToIdle._name;
                    newState._animationDuration  = _runBackToIdle._duration;
                } else if (prevState._type == AnimationType::RunLeft) {
                    newState._animation          = _runLeftToIdle._name;
                    newState._animationDuration  = _runLeftToIdle._duration;
                } else if (prevState._type == AnimationType::RunRight) {
                    newState._animation          = _runRightToIdle._name;
                    newState._animationDuration  = _runRightToIdle._duration;
                }

            }
        
        } else {

                //
                //      How far through the animation do we need to go to
                //      get the movement we need?
                //      Calculate incremental animation (and add in motion compensation)
                //
            Float3 animVel = Float3(0.f, 0.f, 0.f);
            if (newState._type == AnimationType::RunForward) {
                float animationTime = (_runForwardVelocity[1] != 0.f) ? localTranslation[1] / _runForwardVelocity[1] : deltaTime;
                animationTime = std::max(0.f, animationTime);
                newState._time += animationTime;    // update the animation to match
                animVel = _runForwardVelocity;
            } else if (newState._type == AnimationType::RunBack) {
                float animationTime = (_runBackVelocity[1] != 0.f) ? localTranslation[1] / _runBackVelocity[1] : deltaTime;
                animationTime = std::max(0.f, animationTime);
                newState._time += animationTime;
                animVel = _runBackVelocity;
            } else if (newState._type == AnimationType::RunLeft) {
                float animationTime = (_runLeftVelocity[0] != 0.f) ? localTranslation[0] / _runLeftVelocity[0] : deltaTime;
                animationTime = std::max(0.f, animationTime);
                newState._time += animationTime;
                animVel = _runLeftVelocity;
            } else if (newState._type == AnimationType::RunRight) {
                float animationTime = (_runRightVelocity[0] != 0.f) ? localTranslation[0] / _runRightVelocity[0] : deltaTime;
                animationTime = std::max(0.f, animationTime);
                newState._time += animationTime;
                animVel = _runRightVelocity;
            } else {
                newState._time += deltaTime;        // update idle, etc
            }

            assert(std::isfinite(newState._time) && !std::isinf(newState._time));

            if (newState._time > newState._animationDuration) {
                if (newState._animationDuration > 0.f) {
                    newState._time = XlFMod(newState._time, newState._animationDuration);
                } else
                    newState._time = 0.f;

                if (newState._type == AnimationType::Idle) {
                        //
                        //      The idle animations aren't just a basic looping animation.
                        //      We don't want to just wrap around. Sometimes we'll swap
                        //      to another idle animation.
                        //      (   And if we're coming from a run->idle animation, we will
                        //          definitely need to change to a new idle animation)
                        //
                    SelectIdleAnimation(newState);
                }
            }
        }

        return newState;
    }

    auto AnimationDecisionTree::BuildAnimationDesc(const RenderCore::Assets::AnimationSet& animSet, uint64 animation) const -> AnimationDesc
    {
        auto i = animSet.FindAnimation(animation);
        AnimationDesc result;
        result._duration = i._endTime - i._beginTime;
        result._name     = animation;
        return result;
    }

    static RenderCore::Assets::RawAnimationCurve* FindCurve(const RenderCore::Assets::AnimationImmutableData& animSet, unsigned id)
    {
        if (id < animSet._curvesCount)
            return &animSet._curves[id];
        return nullptr;
    }

    Float3 AnimationDecisionTree::ExtractMotionVelocity(
        const RenderCore::Assets::AnimationImmutableData& animSet,
        uint64 animation, uint32 rootNodeParameter)
    {
            //
            //      Part of the animation is just a fixed offset to the root node of the skeleton.
            //      This should have constant velocity, and we need to compensate for this translation
            //      when playing the animation back! Calculate the movement by finding the start and
            //      end points of the right animation driver, and taking the overall translation.
            //
        auto anim = animSet._animationSet.FindAnimation(animation);
        for (unsigned c=anim._beginDriver; c<anim._endDriver; ++c) {
            auto driver = animSet._animationSet.GetAnimationDriver(c);
            if (rootNodeParameter == ~uint32(0x0) || driver._parameterIndex == rootNodeParameter) {
                using namespace RenderCore;

                auto curve = FindCurve(animSet, driver._curveId);
                if (curve) {
                    float startTime = curve->StartTime();
                    float endTime = curve->EndTime();
                    if (driver._samplerType == RenderCore::Assets::TransformationParameterSet::Type::Float3) {
                        Float3 start    = curve->Calculate<Float3>(startTime);
                        Float3 end      = curve->Calculate<Float3>(endTime);
                        return (end - start) / (endTime - startTime) / _characterScale;
                    } else if (driver._samplerType == RenderCore::Assets::TransformationParameterSet::Type::Float4x4) {
                        Float4x4 start    = curve->Calculate<Float4x4>(startTime);
                        Float4x4 end      = curve->Calculate<Float4x4>(endTime);
                        return (ExtractTranslation(end) - ExtractTranslation(start)) / (endTime - startTime) / _characterScale;
                    }
                }
            }
        }

        return Float3(0.f, 0.f, 0.f);
    }

    AnimationDecisionTree::AnimationDecisionTree(
        const AnimationNames& cfg,
        const RenderCore::Assets::AnimationImmutableData& animSet, float characterScale)
    {
            //      Let's get the critical animation for each animation from the animSet
        _characterScale = characterScale;

        _mainAnimations[AnimationType::RunForward]  = BuildAnimationDesc(animSet._animationSet, Hash64(cfg._runForward));
        _mainAnimations[AnimationType::RunBack]     = BuildAnimationDesc(animSet._animationSet, Hash64(cfg._runBack));
        _mainAnimations[AnimationType::RunLeft]     = BuildAnimationDesc(animSet._animationSet, Hash64(cfg._runLeft));
        _mainAnimations[AnimationType::RunRight]    = BuildAnimationDesc(animSet._animationSet, Hash64(cfg._runRight));

        _runForwardToIdle   = BuildAnimationDesc(animSet._animationSet, Hash64(cfg._runForward_ToIdle));
        _runBackToIdle      = BuildAnimationDesc(animSet._animationSet, Hash64(cfg._runBack_ToIdle));
        _runLeftToIdle      = BuildAnimationDesc(animSet._animationSet, Hash64(cfg._runLeft_ToIdle));
        _runRightToIdle     = BuildAnimationDesc(animSet._animationSet, Hash64(cfg._runRight_ToIdle));

        _mainAnimations[AnimationType::Idle] = BuildAnimationDesc(animSet._animationSet, Hash64(cfg._idle));

        const std::string* extraIdles[] = { &cfg._idle1, &cfg._idle2, &cfg._idle3, &cfg._idle4, &cfg._idle5 };
        for (unsigned c=0; c<dimof(extraIdles); ++c) {
            auto animDesc = BuildAnimationDesc(animSet._animationSet, Hash64(*extraIdles[c]));
            if (animDesc._duration > 0.f)
                _extraIdles.push_back(animDesc);
        }

        auto rootNodeHash = Hash64(cfg._rootTransform);
        auto rootNodeParameter = animSet._animationSet.FindParameter(rootNodeHash);

        if (rootNodeParameter != ~0x0u) {
            _runForwardVelocity = ExtractMotionVelocity(animSet, Hash64(cfg._runForward), rootNodeParameter);
            _runBackVelocity    = ExtractMotionVelocity(animSet, Hash64(cfg._runBack), rootNodeParameter);
            _runLeftVelocity    = ExtractMotionVelocity(animSet, Hash64(cfg._runLeft), rootNodeParameter);
            _runRightVelocity   = ExtractMotionVelocity(animSet, Hash64(cfg._runRight), rootNodeParameter);
        } else {
            _runForwardVelocity = Zero<Float3>();
            _runBackVelocity = Zero<Float3>();
            _runLeftVelocity = Zero<Float3>();
            _runRightVelocity = Zero<Float3>();
        }
    }

    AnimationDecisionTree::~AnimationDecisionTree() {}


    uint64 AnimationNames::MakeHash() const
    {
        auto result = Hash64(_runForward);
        result = HashCombine(result, Hash64(_runBack));
        result = HashCombine(result, Hash64(_runLeft));
        result = HashCombine(result, Hash64(_runRight));

        result = HashCombine(result, Hash64(_runForward_ToIdle));
        result = HashCombine(result, Hash64(_runBack_ToIdle));
        result = HashCombine(result, Hash64(_runLeft_ToIdle));
        result = HashCombine(result, Hash64(_runRight_ToIdle));

        result = HashCombine(result, Hash64(_idle));
        result = HashCombine(result, Hash64(_idle1));
        result = HashCombine(result, Hash64(_idle2));
        result = HashCombine(result, Hash64(_idle3));
        result = HashCombine(result, Hash64(_idle4));
        result = HashCombine(result, Hash64(_idle5));

        result = HashCombine(result, Hash64(_rootTransform));
        return result;
    }

}


#include "../../Utility/Meta/ClassAccessors.h"
#include "../../Utility/Meta/ClassAccessorsImpl.h"

template<> const ClassAccessors& GetAccessors<Sample::AnimationNames>()
{
    using Obj = Sample::AnimationNames;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add(u("RunForward"),          DefaultGet(Obj, _runForward),       DefaultSet(Obj, _runForward));
        props.Add(u("RunBack"),             DefaultGet(Obj, _runBack),          DefaultSet(Obj, _runBack));
        props.Add(u("RunLeft"),             DefaultGet(Obj, _runLeft),          DefaultSet(Obj, _runLeft));
        props.Add(u("RunRight"),            DefaultGet(Obj, _runRight),         DefaultSet(Obj, _runRight));

        props.Add(u("RunForward_ToIdle"),       DefaultGet(Obj, _runForward_ToIdle),    DefaultSet(Obj, _runForward_ToIdle));
        props.Add(u("RunBack_ToIdle"),          DefaultGet(Obj, _runBack_ToIdle),       DefaultSet(Obj, _runBack_ToIdle));
        props.Add(u("RunLeft_ToIdle"),          DefaultGet(Obj, _runLeft_ToIdle),       DefaultSet(Obj, _runLeft_ToIdle));
        props.Add(u("RunRight_ToIdle"),         DefaultGet(Obj, _runRight_ToIdle),      DefaultSet(Obj, _runRight_ToIdle));

        props.Add(u("Idle"),            DefaultGet(Obj, _idle),     DefaultSet(Obj, _idle));
        props.Add(u("Idle1"),           DefaultGet(Obj, _idle1),    DefaultSet(Obj, _idle1));
        props.Add(u("Idle2"),           DefaultGet(Obj, _idle2),    DefaultSet(Obj, _idle2));
        props.Add(u("Idle3"),           DefaultGet(Obj, _idle3),    DefaultSet(Obj, _idle3));
        props.Add(u("Idle4"),           DefaultGet(Obj, _idle4),    DefaultSet(Obj, _idle4));
        props.Add(u("Idle5"),           DefaultGet(Obj, _idle5),    DefaultSet(Obj, _idle5));

        props.Add(u("RootTransform"),   DefaultGet(Obj, _rootTransform),    DefaultSet(Obj, _rootTransform));

        init = true;
    }
    return props;
}


