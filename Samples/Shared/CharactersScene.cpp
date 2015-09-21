// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CharactersScene.h"
#include "Character.h"
#include "SampleGlobals.h"
#include "../../RenderCore/Assets/ModelRunTime.h"
#include "../../RenderCore/Assets/SharedStateSet.h"
#include "../../RenderCore/Assets/AssetUtils.h"
#include "../../RenderCore/Assets/AnimationRunTime.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/GPUProfiler.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../ConsoleRig/Console.h"
#include "../../Math/Transformations.h"
#include "../../Math/ProjectionMath.h"
#include "../../Utility/Mixins.h"
#include "../../Utility/Profiling/CPUProfiler.h"

namespace Sample
{
    class StateBin : noncopyable
    {
    public:
        const CharacterModel*       _model;
        float                       _time;
        uint64                      _animation;
        std::vector<Float4x4>       _instances;
    
        StateBin();
        StateBin(StateBin&& moveFrom);
        StateBin& operator=(StateBin&& moveFrom);
    };

    StateBin::StateBin()
    {
        _model = nullptr; _time = 0.f; _animation = 0;
    }

    StateBin::StateBin(StateBin&& moveFrom)
    :       _model    (std::move(moveFrom._model))
    ,       _time     (moveFrom._time)
    ,       _animation(moveFrom._animation)
    ,       _instances(std::move(moveFrom._instances))
    {
    }

    StateBin& StateBin::operator=(StateBin&& moveFrom)
    {
        _model       = std::move(moveFrom._model);
        _time        = moveFrom._time;
        _animation   = moveFrom._animation;
        _instances   = std::move(moveFrom._instances);
        return *this;
    }

//////////////////////////////////////////////////////////////////////////////////////////////

    class CharactersScene::Pimpl
    {
    public:
        typedef RenderCore::Assets::ModelRenderer::PreparedAnimation PreparedAnimation;

        std::shared_ptr<AnimationDecisionTree>      _mainAnimDecisionTree;
        std::unique_ptr<CharacterModel>             _characterModel;

        mutable std::vector<PreparedAnimation>      _preallocatedState;
        mutable RenderCore::Assets::SharedStateSet  _charactersSharedStateSet;

        std::shared_ptr<PlayerCharacter>    _playerCharacter;
        std::vector<NPCCharacter>           _characters;
        std::vector<NetworkCharacter>       _networkCharacters;
        std::vector<StateBin>               _stateCache;

    };

//////////////////////////////////////////////////////////////////////////////////////////////

    static const std::basic_string<utf8> StringAutoCotangent((const utf8*)"AUTO_COTANGENT");

    void CharactersScene::Render(
        RenderCore::Metal::DeviceContext* context,
        SceneEngine::LightingParserContext& parserContext,
        int techniqueIndex) const
    {
        using RenderCore::Metal::ConstantBuffer;
        using namespace SceneEngine;

        RenderCore::Metal::GPUProfiler::DebugAnnotation anno(*context, L"Characters");
        CPUProfileEvent pEvnt("CharactersSceneRender", g_cpuProfiler);

            //  Turn on auto cotangents for character rendering
            //  This prevents us from having to transform the tangent frame through the skinning
            //  transforms (and avoids having to store those tangents in the prepared animation buffer)
        auto& techEnv = parserContext.GetTechniqueContext()._globalEnvironmentState;
        techEnv.SetParameter(StringAutoCotangent.c_str(), 1);

        _pimpl->_charactersSharedStateSet.CaptureState(context);

        RenderCore::Assets::ModelRendererContext modelContext(
            context, parserContext, techniqueIndex);

        if (!_pimpl->_preallocatedState.empty()) {

            const bool interleavedRender = Tweakable("InterleavedRender", false);
            if (interleavedRender) {

                auto si = _pimpl->_preallocatedState.begin();
                for (auto i=_pimpl->_stateCache.begin(); i!=_pimpl->_stateCache.end(); ++i, ++si) {
                    TRY {
                        const auto& model = *i->_model;

                        si->_animState = RenderCore::Assets::AnimationState(i->_time, i->_animation);
                        model.GetPrepareMachine().PrepareAnimation(context, *si);
                        model.GetRenderer().PrepareAnimation(context, *si, model.GetPrepareMachine().GetSkeletonBinding());

                        for (auto i2=i->_instances.cbegin(); i2!=i->_instances.cend(); ++i2) {
                            RenderCore::Assets::MeshToModel meshToModel(
                                si->_finalMatrices.get(),
                                model.GetPrepareMachine().GetSkeletonOutputCount(),
                                &model.GetPrepareMachine().GetSkeletonBinding());
                            model.GetRenderer().Render(modelContext, _pimpl->_charactersSharedStateSet, *i2, meshToModel, AsPointer(si));
                        }
                    } CATCH(const std::exception&) {
                    } CATCH_END
                }

            } else {

                    //
                    //  Use the skinned characters information calculated in PrepareCharacters
                    //  This way, we can do the skinning once, but re-use the results while
                    //  rendering the characters multiple times (for example, for shadows or
                    //  other passes).
                    //

                namespace GPUProfiler = RenderCore::Metal::GPUProfiler;

                context->Bind(RenderCore::Techniques::CommonResources()._dssReadWrite);
                context->Bind(RenderCore::Techniques::CommonResources()._blendOpaque);

                GPUProfiler::TriggerEvent(*context, g_gpuProfiler.get(), "RenderCharacters", GPUProfiler::Begin);

                auto si = _pimpl->_preallocatedState.begin();
                for (auto i=_pimpl->_stateCache.begin(); i!=_pimpl->_stateCache.end(); ++i, ++si) {
                    TRY {
                        const auto& model = *i->_model;
                        for (auto i2=i->_instances.cbegin(); i2!=i->_instances.cend(); ++i2) {
                            // RenderCore::Assets::ModelRenderer::MeshToModel meshToModel(
                            //     si->_finalMatrices.get(),
                            //     model.GetPrepareMachine().GetSkeletonOutputCount(),
                            //     &model.GetPrepareMachine().GetSkeletonBinding());

                            CPUProfileEvent pEvnt("CharacterModelRender", g_cpuProfiler);
                            model.GetRenderer().Render(modelContext, _pimpl->_charactersSharedStateSet, *i2, 
                                /*&meshToModel*/ RenderCore::Assets::MeshToModel(), AsPointer(si));
                        }
                    } CATCH(const std::exception&) {
                    } CATCH_END
                }

                GPUProfiler::TriggerEvent(*context, g_gpuProfiler.get(), "RenderCharacters", GPUProfiler::End);

            }

        } else {

                //
                //      In some cases we can't allocate preallocated state... This will happen
                //      on hardware without geometry shader support. Here, we have to render
                //      without geometry shaders / stream output
                //
                //      (this will currently render without any animation applied)
                //

            for (auto i=_pimpl->_stateCache.begin(); i!=_pimpl->_stateCache.end(); ++i) {
                TRY {
                    const auto& model = *i->_model;
                    for (auto i2=i->_instances.cbegin(); i2!=i->_instances.cend(); ++i2) {
                        model.GetRenderer().Render(modelContext, _pimpl->_charactersSharedStateSet, *i2);
                    }
                } CATCH(const std::exception&) {
                } CATCH_END
            }

        }

        _pimpl->_charactersSharedStateSet.ReleaseState(context);

        techEnv.SetParameter(StringAutoCotangent.c_str(), 0);
    }

    void CharactersScene::Prepare(
        RenderCore::Metal::DeviceContext* context)
    {
        CPUProfileEvent pEvnt("CharactersScenePrepare", g_cpuProfiler);

            //  We need to prepare the animation state for all of the visible characters for
            //  this frame. Build the animation state before we do any rendering -- so 
            //  usually this is one of the first steps in rendering any given frame.

        if (RenderCore::Assets::ModelRenderer::CanDoPrepareAnimation(context)) {
            while (_pimpl->_preallocatedState.size() < _pimpl->_stateCache.size()) {
                _pimpl->_preallocatedState.push_back(_pimpl->_stateCache[0]._model->GetRenderer().CreatePreparedAnimation());
            }
        }

        const bool interleavedRender = Tweakable("InterleavedRender", false);
        if (interleavedRender) 
            return;

        namespace GPUProfiler = RenderCore::Metal::GPUProfiler;

        GPUProfiler::TriggerEvent(*context, g_gpuProfiler.get(), "PrepareAnimation", GPUProfiler::Begin);
        RenderCore::Metal::GPUProfiler::DebugAnnotation anno(*context, L"PrepareCharacters");

            //
            //      Separate state preparation from rendering, so we can profile
            //      them both separately
            //  
        auto si = _pimpl->_preallocatedState.begin();
        for (auto i=_pimpl->_stateCache.begin(); i!=_pimpl->_stateCache.end(); ++i, ++si) {
            TRY {
                const auto& model = *i->_model;
                    // 2 prepare steps
                    //      * first, we need to generate the transform matrices
                    //      * second, we generate the animated vertex positions
                si->_animState = RenderCore::Assets::AnimationState(i->_time, i->_animation);
                model.GetPrepareMachine().PrepareAnimation(context, *si);
                model.GetRenderer().PrepareAnimation(context, *si, model.GetPrepareMachine().GetSkeletonBinding());
            } CATCH(const std::exception&) {
            } CATCH_END
        }

        GPUProfiler::TriggerEvent(*context, g_gpuProfiler.get(), "PrepareAnimation", GPUProfiler::End);
    }

    void CharactersScene::Cull(const Float4x4& worldToProjection)
    {
        CPUProfileEvent pEvnt("CharactersSceneCull", g_cpuProfiler);

            //  Prepare the list of visible characters
            //  Here we do culling against the edge of the screen. We could do occlusion
            //  and/or distance culling also... This implementation is very primitive. 
            //  There is a lot of excessive allocations and some inefficiency. 
            //  But it's ok for a sample.
            //
            //  We actually need to do the culling for the main scene and shadows at the same time.
            //  This is because we need to collect a list of all of the characters that need to
            //  have their animation prepared for a frame. The animation needs to be built if
            //  that character is visible in the main scene, or in the shadows -- and only once
            //  if it is visible in both.
            //  
            //  So an ideal implementation of this would check each character against multiple
            //  frustums, and also calculate the correct shadow frustum flags required.
        auto roughBoundingBox = std::make_pair(Float3(-2.f, -2.f, -2.f), Float3(2.f, 2.f, 2.f));
        _pimpl->_stateCache.clear();

        std::sort(_pimpl->_characters.begin(), _pimpl->_characters.end());
        if (!_pimpl->_characters.empty() && Tweakable("DrawNPCs", true)) {
            const float epsilon = 1.0f / (1.5f*60.f);
            auto blockStart = _pimpl->_characters.begin();
            for (auto i=_pimpl->_characters.begin();;++i) {
                if  (   i==_pimpl->_characters.end() 
                    ||  i->_model != blockStart->_model 
                    ||  i->_animState._animation != blockStart->_animState._animation 
                    || (i->_animState._time - blockStart->_animState._time) > epsilon) {

                    StateBin newState;
                    newState._model     =  blockStart->_model;
                    newState._animation =  blockStart->_animState._animation;
                    newState._time      = (blockStart->_animState._time + (i-1)->_animState._time) * .5f;
                    for (auto i2=blockStart; i2<i; ++i2) {
                        Float4x4 final = i2->_localToWorld;
                        Combine_InPlace(i2->_animState._motionCompensation * newState._time, final);
                        Combine_InPlace(UniformScale(1.f/CharactersScale), final);

                        __declspec(align(16)) auto localToCulling = Combine(i2->_localToWorld, worldToProjection);
                        if (!CullAABB_Aligned(AsFloatArray(localToCulling), roughBoundingBox.first, roughBoundingBox.second)) {
                            newState._instances.push_back(final);
                        }
                    }

                    if (!newState._instances.empty()) {
                        _pimpl->_stateCache.push_back(std::move(newState));
                    }

                    if (i==_pimpl->_characters.end()) break;
                    blockStart = i;

                }
            }
        }

        for (auto i=_pimpl->_networkCharacters.cbegin();i!=_pimpl->_networkCharacters.cend();++i) {
            StateBin newState;
            newState._model     = i->_model;
            newState._animation = i->_animState._animation;
            newState._time      = i->_animState._time;
    
            Float4x4 final      = i->_localToWorld;
            Combine_InPlace(i->_animState._motionCompensation * i->_animState._time, final);
            Combine_InPlace(UniformScale(1.0f/CharactersScale), final);

            __declspec(align(16)) auto localToCulling = Combine(i->_localToWorld, worldToProjection);
            if (!CullAABB_Aligned(AsFloatArray(localToCulling), roughBoundingBox.first, roughBoundingBox.second)) {
                newState._instances.push_back(final);
                _pimpl->_stateCache.push_back(std::move(newState));
            }
        }

        StateBin newState;
        newState._model = _pimpl->_playerCharacter->_model;
        newState._animation = _pimpl->_playerCharacter->_animState._animation;
        newState._time = _pimpl->_playerCharacter->_animState._time;
    
        Float4x4 final = _pimpl->_playerCharacter->_localToWorld;
        Combine_InPlace(_pimpl->_playerCharacter->_animState._motionCompensation * _pimpl->_playerCharacter->_animState._time, final);
        Combine_InPlace(UniformScale(1.0f/CharactersScale), final);
    
        __declspec(align(16)) auto localToCulling = Combine(_pimpl->_playerCharacter->_localToWorld, worldToProjection);
        if (!CullAABB_Aligned(AsFloatArray(localToCulling), roughBoundingBox.first, roughBoundingBox.second)) {
            newState._instances.push_back(final);
            _pimpl->_stateCache.push_back(std::move(newState));
        }
    }

    void CharactersScene::Update(float deltaTime)
    {
        CPUProfileEvent pEvnt("CharactersSceneUpdate", g_cpuProfiler);

            // update the simulations state for all characters
        for (auto i = _pimpl->_characters.begin(); i!=_pimpl->_characters.end(); ++i) {
            i->Update(deltaTime);
        }
        for (auto i = _pimpl->_networkCharacters.begin(); i!=_pimpl->_networkCharacters.end(); ++i) {
            i->Update(deltaTime);
        }
        _pimpl->_playerCharacter->Update(deltaTime);
    }

    std::shared_ptr<PlayerCharacter>  CharactersScene::GetPlayerCharacter()
    {
        return _pimpl->_playerCharacter;
    }

    Float4x4 CharactersScene::DefaultCameraToWorld() const
    {
        std::pair<Float3, Float3> boundingBox(Float3(0.f, 0.f, 0.f), Float3(0.f, 0.f, 0.f));
        if (_pimpl->_characterModel) {
            boundingBox = _pimpl->_characterModel->GetModelScaffold().GetStaticBoundingBox();
        }
        auto cameraFocusPoint = LinearInterpolate(boundingBox.first, boundingBox.second, 0.5f);

        const Float3 defaultForwardVector(0.f, -1.f, 0.f);
        const Float3 defaultUpVector(0.f, 0.f, 1.f);
        const float boundingBoxDimension = Magnitude(boundingBox.second - boundingBox.first);
        return
            MakeCameraToWorld(
                defaultForwardVector, defaultUpVector,
                cameraFocusPoint - boundingBoxDimension * defaultForwardVector);
    }

    class CharacterInputFiles
    {
    public:
        std::string     _skin;
        std::string     _animationSet;
        std::string     _skeleton;

        CharacterInputFiles(const std::string& basePath);
    };

    CharacterInputFiles::CharacterInputFiles(const std::string& basePath)
    {
        _skin           = basePath + "/skin/dragon003";
        _animationSet   = basePath + "/animation";
        _skeleton       = basePath + "/skeleton/all_co_sk_whirlwind_launch_mub";
    }

    CharactersScene::CharactersScene()
    {
        auto pimpl = std::make_unique<Pimpl>();

        CharacterInputFiles inputFiles("game/chr/nu_f");
        pimpl->_characterModel = std::make_unique<CharacterModel>(
            inputFiles._skin.c_str(), inputFiles._skeleton.c_str(), inputFiles._animationSet.c_str(), 
            std::ref(pimpl->_charactersSharedStateSet));

        pimpl->_mainAnimDecisionTree = std::make_shared<AnimationDecisionTree>(
            std::ref(pimpl->_characterModel->GetAnimationData()), CharactersScale);

        #if defined(_DEBUG)
            const unsigned npcCount = 5;
        #else
            const unsigned npcCount = 5;
        #endif
        for (unsigned c=0; c<npcCount; ++c) {
            pimpl->_characters.push_back(NPCCharacter(*pimpl->_characterModel, pimpl->_mainAnimDecisionTree));
        }

        pimpl->_playerCharacter = std::make_shared<PlayerCharacter>(std::ref(*pimpl->_characterModel), pimpl->_mainAnimDecisionTree);

        _pimpl = std::move(pimpl);
    }

    CharactersScene::~CharactersScene()
    {

    }

}
