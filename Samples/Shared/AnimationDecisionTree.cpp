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
        if (rand() < RAND_MAX/2) {
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
                float animationTime = localTranslation[1] / _runForwardVelocity[1];
                animationTime = std::max(0.f, animationTime);
                newState._time += animationTime;    // update the animation to match
                animVel = _runForwardVelocity;
            } else if (newState._type == AnimationType::RunBack) {
                float animationTime = localTranslation[1] / _runBackVelocity[1];
                animationTime = std::max(0.f, animationTime);
                newState._time += animationTime;
                animVel = _runBackVelocity;
            } else if (newState._type == AnimationType::RunLeft) {
                float animationTime = localTranslation[0] / _runLeftVelocity[0];
                animationTime = std::max(0.f, animationTime);
                newState._time += animationTime;
                animVel = _runLeftVelocity;
            } else if (newState._type == AnimationType::RunRight) {
                float animationTime = localTranslation[0] / _runRightVelocity[0];
                animationTime = std::max(0.f, animationTime);
                newState._time += animationTime;
                animVel = _runRightVelocity;
            } else {
                newState._time += deltaTime;        // update idle, etc
            }

            if (newState._time > newState._animationDuration) {
                newState._time = XlFMod(newState._time, newState._animationDuration);

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

    namespace AnimationNames
    {
        static const std::string RunForward         = "onehand_mo_combat_run_f";
        static const std::string RunBack            = "onehand_mo_combat_run_b";
        static const std::string RunLeft            = "onehand_mo_combat_run_l";
        static const std::string RunRight           = "onehand_mo_combat_run_r";

        static const std::string RunForward_ToIdle  = "onehand_mo_combat_runtoidle_f";
        static const std::string RunBack_ToIdle     = "onehand_mo_combat_runtoidle_b";
        static const std::string RunLeft_ToIdle     = "onehand_mo_combat_runtoidle_l";
        static const std::string RunRight_ToIdle    = "onehand_mo_combat_runtoidle_r";

        static const std::string Idle               = "onehand_ba_combat_idle";
        static const std::string Idle1              = "onehand_ba_combat_idle_rand_1";
        static const std::string Idle2              = "onehand_ba_combat_idle_rand_2";
        static const std::string Idle3              = "onehand_ba_combat_idle_rand_3";
        static const std::string Idle4              = "onehand_ba_combat_idle_rand_4";
        static const std::string Idle5              = "onehand_ba_combat_idle_rand_5";
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

    Float3 AnimationDecisionTree::ExtractMotionVelocity(const RenderCore::Assets::AnimationImmutableData& animSet, uint64 animation)
    {
            //
            //      Part of the animation is just a fixed offset to the root node of the skeleton.
            //      This should have constant velocity, and we need to compensate for this translation
            //      when playing the animation back! Calculate the movement by finding the start and
            //      end points of the right animation driver, and taking the overall translation.
            //
        uint64 rootNodeHash = Hash64("Bip01/matrix"); // Hash64("Bip01");
        uint32 parameter = animSet._animationSet.FindParameter(rootNodeHash);

        auto anim = animSet._animationSet.FindAnimation(animation);
        for (unsigned c=anim._beginDriver; c<anim._endDriver; ++c) {
            auto driver = animSet._animationSet.GetAnimationDriver(c);
            if (parameter == ~uint32(0x0) || driver._parameterIndex == parameter) {
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

    AnimationDecisionTree::AnimationDecisionTree(const RenderCore::Assets::AnimationImmutableData& animSet, float characterScale)
    {
            //      Let's get the critical animation for each animation from the animSet
        _characterScale = characterScale;

        _mainAnimations[AnimationType::RunForward]  = BuildAnimationDesc(animSet._animationSet, Hash64(AnimationNames::RunForward));
        _mainAnimations[AnimationType::RunBack]     = BuildAnimationDesc(animSet._animationSet, Hash64(AnimationNames::RunBack));
        _mainAnimations[AnimationType::RunLeft]     = BuildAnimationDesc(animSet._animationSet, Hash64(AnimationNames::RunLeft));
        _mainAnimations[AnimationType::RunRight]    = BuildAnimationDesc(animSet._animationSet, Hash64(AnimationNames::RunRight));

        _runForwardToIdle   = BuildAnimationDesc(animSet._animationSet, Hash64(AnimationNames::RunForward_ToIdle));
        _runBackToIdle      = BuildAnimationDesc(animSet._animationSet, Hash64(AnimationNames::RunBack_ToIdle));
        _runLeftToIdle      = BuildAnimationDesc(animSet._animationSet, Hash64(AnimationNames::RunLeft_ToIdle));
        _runRightToIdle     = BuildAnimationDesc(animSet._animationSet, Hash64(AnimationNames::RunRight_ToIdle));

        _mainAnimations[AnimationType::Idle] = BuildAnimationDesc(animSet._animationSet, Hash64(AnimationNames::Idle));
        _extraIdles.push_back(BuildAnimationDesc(animSet._animationSet, Hash64(AnimationNames::Idle1)));
        _extraIdles.push_back(BuildAnimationDesc(animSet._animationSet, Hash64(AnimationNames::Idle2)));
        _extraIdles.push_back(BuildAnimationDesc(animSet._animationSet, Hash64(AnimationNames::Idle3)));
        _extraIdles.push_back(BuildAnimationDesc(animSet._animationSet, Hash64(AnimationNames::Idle4)));
        _extraIdles.push_back(BuildAnimationDesc(animSet._animationSet, Hash64(AnimationNames::Idle5)));

        _runForwardVelocity = ExtractMotionVelocity(animSet, Hash64(AnimationNames::RunForward));
        _runBackVelocity    = ExtractMotionVelocity(animSet, Hash64(AnimationNames::RunBack));
        _runLeftVelocity    = ExtractMotionVelocity(animSet, Hash64(AnimationNames::RunLeft));
        _runRightVelocity   = ExtractMotionVelocity(animSet, Hash64(AnimationNames::RunRight));
    }


}