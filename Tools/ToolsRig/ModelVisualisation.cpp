// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ModelVisualisation.h"
#include "VisualisationUtils.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../PlatformRig/Screenshot.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/RayVsModel.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../SceneEngine/PreparedScene.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/LightingParserStandardPlugin.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/RenderStep.h"
#include "../../PlatformRig/BasicSceneParser.h"
#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../RenderOverlays/OverlayContext.h"
#include "../../RenderOverlays/HighlightEffects.h"
#include "../../FixedFunctionModel/SharedStateSet.h"
#include "../../FixedFunctionModel/ModelUtils.h"
#include "../../FixedFunctionModel/ModelCache.h"
#include "../../FixedFunctionModel/ModelRunTime.h"
#include "../../RenderCore/Assets/ModelImmutableData.h"
#include "../../RenderCore/Assets/ModelScaffold.h"
#include "../../RenderCore/Assets/MaterialScaffold.h"
#include "../../RenderCore/Assets/SimpleModelRenderer.h"
#include "../../RenderCore/Assets/SimpleModelDeform.h"
#include "../../RenderCore/Assets/SkinDeformer.h"
#include "../../RenderCore/IThreadContext.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/BasicDelegates.h"
#include "../../RenderCore/Techniques/RenderPassUtils.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/ObjectFactory.h"
#include "../../RenderCore/Format.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/Assets.h"
#include "../../Assets/AssetFuture.h"
#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/ResourceBox.h"
#include "../../Math/Transformations.h"
#include "../../Utility/HeapUtils.h"
#include "../../Utility/StringFormat.h"
#include <map>

#pragma warning(disable:4505)

namespace ToolsRig
{
	using RenderCore::Assets::ModelScaffold;
    using RenderCore::Assets::MaterialScaffold;
	using RenderCore::Assets::AnimationSetScaffold;
	using RenderCore::Assets::SkeletonScaffold;
    using RenderCore::Assets::SkeletonMachine;
	using RenderCore::Assets::SimpleModelRenderer;

    using FixedFunctionModel::ModelRenderer;
	using FixedFunctionModel::SharedStateSet;
    using FixedFunctionModel::ModelCache;
	using FixedFunctionModel::ModelCacheModel;
    using FixedFunctionModel::DelayedDrawCallSet;

///////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
    static void RenderWithEmbeddedSkeleton(
        const FixedFunctionModel::ModelRendererContext& context,
        const ModelRenderer& model,
        const SharedStateSet& sharedStateSet,
        const ModelScaffold* scaffold)
    {
        if (scaffold) {
            model.Render(
                context, sharedStateSet, Identity<Float4x4>(), 
                FixedFunctionModel::EmbeddedSkeletonPose(*scaffold).GetMeshToModel());
        } else {
            model.Render(context, sharedStateSet, Identity<Float4x4>());
        }
    }

    static void PrepareWithEmbeddedSkeleton(
        DelayedDrawCallSet& dest, 
        const ModelRenderer& model,
        const SharedStateSet& sharedStateSet,
        const ModelScaffold* scaffold)
    {
        if (scaffold) {
            model.Prepare(
                dest, sharedStateSet, Identity<Float4x4>(), 
                FixedFunctionModel::EmbeddedSkeletonPose(*scaffold).GetMeshToModel());
        } else {
            model.Prepare(dest, sharedStateSet, Identity<Float4x4>());
        }
    }
    
    class FixedFunctionModelSceneParser : public SceneEngine::IScene
    {
    public:
        void ExecuteScene(
            RenderCore::IThreadContext& context,
			RenderCore::Techniques::ParsingContext& parserContext,
            SceneEngine::LightingParserContext& lightingParserContext, 
            RenderCore::Techniques::BatchFilter batchFilter,
            SceneEngine::PreparedScene& preparedPackets,
            unsigned techniqueIndex) const 
        {
            auto delaySteps = SceneEngine::AsDelaySteps(batchFilter);
            if (delaySteps.empty()) return;

            auto metalContext = RenderCore::Metal::DeviceContext::Get(context);

            FixedFunctionModel::SharedStateSet::CaptureMarker captureMarker;
            if (_sharedStateSet)
				captureMarker = _sharedStateSet->CaptureState(context, parserContext.GetRenderStateDelegate(), {});

            using namespace RenderCore;
            Metal::ConstantBuffer drawCallIndexBuffer(
				Metal::GetObjectFactory(), 
				CreateDesc(BindFlag::ConstantBuffer, CPUAccess::WriteDynamic, GPUAccess::Read, LinearBufferDesc::Create(sizeof(unsigned)*4), "drawCallIndex"));
            metalContext->GetNumericUniforms(ShaderStage::Geometry).Bind(MakeResourceList(drawCallIndexBuffer));

            if (Tweakable("RenderSkinned", false)) {
                if (delaySteps[0] == FixedFunctionModel::DelayStep::OpaqueRender) {
                    auto preparedAnimation = _model->CreatePreparedAnimation();
                    FixedFunctionModel::SkinPrepareMachine prepareMachine(
                        *_modelScaffold, _modelScaffold->EmbeddedSkeleton());
                    RenderCore::Assets::AnimationState animState = {0.f, 0u};
                    prepareMachine.PrepareAnimation(context, *preparedAnimation, animState);
                    _model->PrepareAnimation(context, *preparedAnimation, prepareMachine.GetSkeletonBinding());

                    FixedFunctionModel::MeshToModel meshToModel(
                        *preparedAnimation, &prepareMachine.GetSkeletonBinding());

                    _model->Render(
                        FixedFunctionModel::ModelRendererContext(*metalContext, parserContext, techniqueIndex),
                        *_sharedStateSet, Identity<Float4x4>(), 
                        meshToModel, preparedAnimation.get());

                    if (Tweakable("RenderSkeleton", false)) {
                        prepareMachine.RenderSkeleton(
                            context, parserContext, 
                            animState, Identity<Float4x4>());
                    }
                }
            } else {
                const bool fillInStencilInfo = (_settings->_colourByMaterial != 0);

                for (auto i:delaySteps)
                    ModelRenderer::RenderPrepared(
                        FixedFunctionModel::ModelRendererContext(*metalContext.get(), parserContext, techniqueIndex),
                        *_sharedStateSet, _delayedDrawCalls, i,
                        [&metalContext, &drawCallIndexBuffer, &fillInStencilInfo](ModelRenderer::DrawCallEvent evnt)
                        {
                            if (fillInStencilInfo) {
                                // hack -- we just adjust the depth stencil state to enable the stencil buffer
                                //          no way to do this currently without dropping back to low level API
                                #if GFXAPI_ACTIVE == GFXAPI_DX11
                                    Metal::DepthStencilState dss(*metalContext);
                                    D3D11_DEPTH_STENCIL_DESC desc;
                                    dss.GetUnderlying()->GetDesc(&desc);
                                    desc.StencilEnable = true;
                                    desc.StencilWriteMask = 0xff;
                                    desc.StencilReadMask = 0xff;
                                    desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
                                    desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
                                    desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
                                    desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
                                    desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
                                    desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
                                    desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
                                    desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
                                    auto newDSS = Metal::GetObjectFactory().CreateDepthStencilState(&desc);
                                    metalContext->GetUnderlying()->OMSetDepthStencilState(newDSS.get(), 1+evnt._drawCallIndex);
                                #endif
                            }

                            unsigned drawCallIndexB[4] = { evnt._drawCallIndex, 0, 0, 0 };
                            drawCallIndexBuffer.Update(*metalContext, drawCallIndexB, sizeof(drawCallIndexB));

                            metalContext->DrawIndexed(evnt._indexCount, evnt._firstIndex, evnt._firstVertex);
                        });
            }
        }

		virtual void ExecuteScene(
            RenderCore::IThreadContext& threadContext,
			SceneEngine::SceneExecuteContext& executeContext) const override
		{
		}

		VisCameraSettings AlignCamera()
		{
			return AlignCameraToBoundingBox(40.f, _boundingBox);
		}

		FixedFunctionModelSceneParser(
			const VisOverlaySettings& settings,
			ModelRenderer& model, const std::pair<Float3, Float3>& boundingBox, SharedStateSet& sharedStateSet,
			const ModelScaffold* modelScaffold = nullptr)
		: _model(&model), _boundingBox(boundingBox), _sharedStateSet(&sharedStateSet)
		, _settings(&settings), _modelScaffold(modelScaffold) 
		, _delayedDrawCalls(typeid(ModelRenderer).hash_code())
		{
			PrepareWithEmbeddedSkeleton(
				_delayedDrawCalls, *_model,
				*_sharedStateSet, modelScaffold);
			ModelRenderer::Sort(_delayedDrawCalls);
		}
	protected:
		ModelRenderer* _model;
		SharedStateSet* _sharedStateSet;
		std::pair<Float3, Float3> _boundingBox;

		const VisOverlaySettings* _settings;
		const ModelScaffold* _modelScaffold;
		DelayedDrawCallSet _delayedDrawCalls;
	};

	std::unique_ptr<SceneEngine::IScene> CreateModelScene(const ModelCacheModel& model)
    {
        ModelVisSettings settings;
        return std::make_unique<FixedFunctionModelSceneParser>(
            settings,
            *model._renderer, model._boundingBox, *model._sharedStateSet);
    }

#endif

	class StencilRefDelegate : public SimpleModelRenderer::IPreDrawDelegate
	{
	public:
		virtual bool OnDraw( 
			RenderCore::Metal::DeviceContext& metalContext, RenderCore::Techniques::ParsingContext&,
			const RenderCore::Techniques::Drawable&,
			uint64_t materialGuid, unsigned drawCallIdx) override
		{
			using namespace RenderCore;
			metalContext.Bind(_dss, drawCallIdx+1);
			return true;
		}

		StencilRefDelegate()
		: _dss(
			true, true,
			0xff, 0xff,
			RenderCore::Metal::StencilMode::AlwaysWrite,
			RenderCore::Metal::StencilMode::NoEffect)
		{}
	private:
		RenderCore::Metal::DepthStencilState _dss;
	};

	class ModelScene : public SceneEngine::IScene, public IVisContent
    {
    public:
		virtual void ExecuteScene(
            RenderCore::IThreadContext& threadContext,
			SceneEngine::SceneExecuteContext& executeContext) const override
		{
			auto skeletonMachine = GetSkeletonMachine();
			assert(skeletonMachine);

			std::vector<Float4x4> skeletonMachineOutput(skeletonMachine->GetOutputMatrixCount());

			if (_animationScaffold) {
				auto& animData = _animationScaffold->ImmutableData();
				auto foundAnimation = animData._animationSet.GetAnimations()[0];

				static float time = 0.f;
				time += 1.0f / 60.f;

				RenderCore::Assets::AnimationState animState(
					std::fmod(time, foundAnimation._endTime), foundAnimation._name);
				auto params = animData._animationSet.BuildTransformationParameterSet(
					animState,
					*skeletonMachine, _animSetBinding,
					animData._curves, animData._curvesCount);

				skeletonMachine->GenerateOutputTransforms(
					MakeIteratorRange(skeletonMachineOutput),
					&params);
			} else {
				skeletonMachine->GenerateOutputTransforms(
					MakeIteratorRange(skeletonMachineOutput),
					&skeletonMachine->GetDefaultParameters());
			}

			for (unsigned c=0; c<_renderer->DeformOperationCount(); ++c) {
				auto* skinDeformOp = dynamic_cast<RenderCore::Assets::SkinDeformer*>(&_renderer->DeformOperation(c));
				if (!skinDeformOp) continue;
				skinDeformOp->FeedInSkeletonMachineResults(
					MakeIteratorRange(skeletonMachineOutput),
					skeletonMachine->GetOutputInterface());
			}
			_renderer->GenerateDeformBuffer(threadContext);

			for (unsigned v=0; v<executeContext.GetViews().size(); ++v) {
				RenderCore::Techniques::DrawablesPacket* pkts[unsigned(RenderCore::Techniques::BatchFilter::Max)];
				for (unsigned c=0; c<unsigned(RenderCore::Techniques::BatchFilter::Max); ++c)
					pkts[c] = executeContext.GetDrawablesPacket(v, RenderCore::Techniques::BatchFilter(c));

				if (_preDrawDelegate) {
					_renderer->BuildDrawables(MakeIteratorRange(pkts), Identity<Float4x4>(), _preDrawDelegate);
				} else {
					_renderer->BuildDrawables(MakeIteratorRange(pkts), Identity<Float4x4>());
				}
			}
		}

		std::string GetModelName() const { return _modelName; }
		std::string GetMaterialName() const { return _materialName; }
		std::pair<Float3, Float3> GetBoundingBox() const { return _renderer->GetModelScaffold()->GetStaticBoundingBox(); }

		std::shared_ptr<SimpleModelRenderer::IPreDrawDelegate> SetPreDrawDelegate(const std::shared_ptr<SimpleModelRenderer::IPreDrawDelegate>& delegate)
		{
			auto oldDelegate = delegate;
			std::swap(_preDrawDelegate, oldDelegate);
			return oldDelegate;
		}

		static void ConstructToFuture(
			::Assets::AssetFuture<ModelScene>& future,
			const ModelVisSettings& settings)
		{
			auto rendererFuture = ::Assets::MakeAsset<SimpleModelRenderer>(settings._modelName, settings._materialName, "skin");

			::Assets::FuturePtr<AnimationSetScaffold> animationSetFuture;
			::Assets::FuturePtr<SkeletonScaffold> skeletonFuture;

			if (!settings._animationFileName.empty())
				animationSetFuture = ::Assets::MakeAsset<AnimationSetScaffold>(settings._animationFileName);

			if (!settings._skeletonFileName.empty())
				skeletonFuture = ::Assets::MakeAsset<SkeletonScaffold>(settings._skeletonFileName);

			future.SetPollingFunction(
				[rendererFuture, animationSetFuture, skeletonFuture](::Assets::AssetFuture<ModelScene>& thatFuture) -> bool {

					bool stillPending = false;
					auto rendererActual = rendererFuture->TryActualize();
					if (!rendererActual) {
						auto state = rendererFuture->GetAssetState();
						if (state == ::Assets::AssetState::Invalid) {
							std::stringstream str;
							str << "SimpleModelRenderer failed to actualize: ";
							const auto& actLog = rendererFuture->GetActualizationLog();
							str << (actLog ? ::Assets::AsString(actLog) : std::string("<<no log>>"));
							thatFuture.SetInvalidAsset(rendererFuture->GetDependencyValidation(), ::Assets::AsBlob(str.str()));
							return false;
						}
						stillPending = true;
					}

					std::shared_ptr<AnimationSetScaffold> animationSetActual;
					std::shared_ptr<SkeletonScaffold> skeletonActual;

					if (animationSetFuture) {
						animationSetActual = animationSetFuture->TryActualize();
						if (!animationSetActual) {
							auto state = animationSetFuture->GetAssetState();
							if (state == ::Assets::AssetState::Invalid) {
								std::stringstream str;
								str << "AnimationSet failed to actualize: ";
								const auto& actLog = animationSetFuture->GetActualizationLog();
								str << (actLog ? ::Assets::AsString(actLog) : std::string("<<no log>>"));
								thatFuture.SetInvalidAsset(animationSetFuture->GetDependencyValidation(), ::Assets::AsBlob(str.str()));
								return false;
							}
							stillPending = true;
						}
					}

					if (skeletonFuture) {
						skeletonActual = skeletonFuture->TryActualize();
						if (!skeletonFuture) {
							auto state = skeletonFuture->GetAssetState();
							if (state == ::Assets::AssetState::Invalid) {
								std::stringstream str;
								str << "Skeleton failed to actualize: ";
								const auto& actLog = skeletonFuture->GetActualizationLog();
								str << (actLog ? ::Assets::AsString(actLog) : std::string("<<no log>>"));
								thatFuture.SetInvalidAsset(skeletonFuture->GetDependencyValidation(), ::Assets::AsBlob(str.str()));
								return false;
							}
							stillPending = true;
						}
					}

					if (stillPending)
						return true;

					auto newModel = std::make_shared<ModelScene>(rendererActual, animationSetActual, skeletonActual);
					thatFuture.SetAsset(std::move(newModel), {});
					return false;
				});
		}

		ModelScene(
			const std::shared_ptr<SimpleModelRenderer>& renderer,
			const std::shared_ptr<AnimationSetScaffold>& animationScaffold,
			const std::shared_ptr<SkeletonScaffold>& skeletonScaffold)
		: _renderer(renderer), _animationScaffold(animationScaffold), _skeletonScaffold(skeletonScaffold)
		{
			if (!_skeletonScaffold)
				_modelScaffoldForEmbeddedSkeleton = _renderer->GetModelScaffold();

			if (_animationScaffold) {
				_animSetBinding = RenderCore::Assets::AnimationSetBinding(
					_animationScaffold->ImmutableData()._animationSet.GetOutputInterface(), 
					GetSkeletonMachine()->GetInputInterface());
			}

			_depVal = std::make_shared<::Assets::DependencyValidation>();
			::Assets::RegisterAssetDependency(_depVal, _renderer->GetDependencyValidation());
			if (_animationScaffold)
				::Assets::RegisterAssetDependency(_depVal, _animationScaffold->GetDependencyValidation());
			if (_skeletonScaffold)
				::Assets::RegisterAssetDependency(_depVal, _skeletonScaffold->GetDependencyValidation());
        }

        ~ModelScene() {}

		const ::Assets::DepValPtr& GetDependencyValidation() { return _depVal; }

    protected:
		std::shared_ptr<SimpleModelRenderer>		_renderer;
		std::shared_ptr<SimpleModelRenderer::IPreDrawDelegate>			_preDrawDelegate;
		std::shared_ptr<AnimationSetScaffold>		_animationScaffold;

		std::shared_ptr<ModelScaffold>				_modelScaffoldForEmbeddedSkeleton;
		std::shared_ptr<SkeletonScaffold>			_skeletonScaffold;
		RenderCore::Assets::AnimationSetBinding		_animSetBinding;

		::Assets::DepValPtr					_depVal;
		std::string _modelName, _materialName;

		const SkeletonMachine* GetSkeletonMachine() const
		{
			const SkeletonMachine* skeletonMachine = nullptr;
			if (_skeletonScaffold) {
				skeletonMachine = &_skeletonScaffold->GetTransformationMachine();
			} else if (_modelScaffoldForEmbeddedSkeleton)
				skeletonMachine = &_modelScaffoldForEmbeddedSkeleton->EmbeddedSkeleton();
			return skeletonMachine;
		}
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

	::Assets::FuturePtr<SceneEngine::IScene> MakeScene(const ModelVisSettings& settings)
	{
		auto modelScene = std::make_shared<::Assets::AssetFuture<ModelScene>>("ModelVisualization");
		::Assets::AutoConstructToFuture(*modelScene, settings);
		return std::reinterpret_pointer_cast<::Assets::AssetFuture<SceneEngine::IScene>>(modelScene);
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ModelVisLayer::Pimpl
    {
    public:
		std::shared_ptr<SceneEngine::IScene> _scene;
        ::Assets::FuturePtr<SceneEngine::IScene> _sceneFuture;

		std::shared_ptr<PlatformRig::EnvironmentSettings> _envSettings;
		::Assets::FuturePtr<PlatformRig::EnvironmentSettings> _envSettingsFuture;
		
		std::string _sceneErrorMessage;
		std::string _envSettingsErrorMessage;

		std::shared_ptr<VisCameraSettings> _camera;
    };

	class UsefulFonts
    {
    public:
        class Desc {};

		std::shared_ptr<RenderOverlays::Font> _defaultFont0;
		std::shared_ptr<RenderOverlays::Font> _defaultFont1;

        UsefulFonts(const Desc&)
        {
            _defaultFont0 = RenderOverlays::GetX2Font("Raleway", 18);
            _defaultFont1 = RenderOverlays::GetX2Font("Vera", 18);
        }
    };

    void ModelVisLayer::Render(
        RenderCore::IThreadContext& threadContext,
		const RenderCore::IResourcePtr& renderTarget,
        RenderCore::Techniques::ParsingContext& parserContext)
    {
        using namespace SceneEngine;

		if (_pimpl->_sceneFuture) {
			auto newActualized = _pimpl->_sceneFuture->TryActualize();
			if (newActualized) {
				// After the model is loaded, if we have a pending camera align,
                // we should reset the camera to the match the model.
                // We also need to trigger the change event after we make a change...
				auto* visContent = dynamic_cast<IVisContent*>(newActualized.get());
				if (visContent) {
					*_pimpl->_camera = AlignCameraToBoundingBox(
						_pimpl->_camera->_verticalFieldOfView,
						visContent->GetBoundingBox());
					// _pimpl->_settings->_changeEvent.Trigger();
				}

				_pimpl->_scene = newActualized;
				_pimpl->_sceneFuture = nullptr;
				_pimpl->_sceneErrorMessage = {};
			} else if (_pimpl->_sceneFuture->GetAssetState() == ::Assets::AssetState::Invalid) {
				_pimpl->_sceneErrorMessage = ::Assets::AsString(_pimpl->_sceneFuture->GetActualizationLog());
				_pimpl->_scene = nullptr;
				_pimpl->_sceneFuture = nullptr;
			}
		}

		if (_pimpl->_envSettingsFuture) {
			auto newActualized = _pimpl->_envSettingsFuture->TryActualize();
			if (newActualized) {
				_pimpl->_envSettings = newActualized;
				_pimpl->_envSettingsFuture = nullptr;
				_pimpl->_envSettingsErrorMessage = {};
			} else if (_pimpl->_sceneFuture->GetAssetState() == ::Assets::AssetState::Invalid) {
				_pimpl->_envSettingsErrorMessage = ::Assets::AsString(_pimpl->_envSettingsFuture->GetActualizationLog());
				_pimpl->_envSettings = nullptr;
				_pimpl->_envSettingsFuture = nullptr;
			}
		}

		if (_pimpl->_envSettings) {
			PlatformRig::BasicLightingParserDelegate lightingParserDelegate(_pimpl->_envSettings);

			std::shared_ptr<SceneEngine::ILightingParserPlugin> lightingPlugins[] = {
				std::make_shared<SceneEngine::LightingParserStandardPlugin>()
			};
			auto qualSettings = SceneEngine::RenderSceneSettings{
				SceneEngine::RenderSceneSettings::LightingModel::Deferred,
				&lightingParserDelegate,
				MakeIteratorRange(lightingPlugins)};

			if (_pimpl->_scene) {
				auto& screenshot = Tweakable("Screenshot", 0);
				if (screenshot) {
					PlatformRig::TiledScreenshot(
						threadContext, parserContext,
						*_pimpl->_scene, AsCameraDesc(*_pimpl->_camera),
						qualSettings, UInt2(screenshot, screenshot));
					screenshot = 0;
				}

				auto lightingParserContext = LightingParser_ExecuteScene(
					threadContext, renderTarget, parserContext, 
					*_pimpl->_scene, AsCameraDesc(*_pimpl->_camera),
					qualSettings);

				// Draw debugging overlays -- 
				{
					auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, renderTarget, parserContext);
					SceneEngine::LightingParser_Overlays(threadContext, parserContext, lightingParserContext);
				}
			}
		}

		if (!_pimpl->_sceneErrorMessage.empty() || !_pimpl->_envSettingsErrorMessage.empty()) {
			auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, renderTarget, parserContext);
			if (!_pimpl->_sceneErrorMessage.empty())
				SceneEngine::DrawString(threadContext, ConsoleRig::FindCachedBox<UsefulFonts>(UsefulFonts::Desc{})._defaultFont1, _pimpl->_sceneErrorMessage);
			if (!_pimpl->_envSettingsErrorMessage.empty())
				SceneEngine::DrawString(threadContext, ConsoleRig::FindCachedBox<UsefulFonts>(UsefulFonts::Desc{})._defaultFont1, _pimpl->_envSettingsErrorMessage);
		}
    }

    void ModelVisLayer::Set(const VisEnvSettings& envSettings)
    {
		_pimpl->_envSettingsFuture = std::make_shared<::Assets::AssetFuture<PlatformRig::EnvironmentSettings>>("VisualizationEnvironment");
		::Assets::AutoConstructToFuture(*_pimpl->_envSettingsFuture, envSettings._envConfigFile);
    }

	void ModelVisLayer::Set(const ::Assets::FuturePtr<SceneEngine::IScene>& scene)
	{
		_pimpl->_sceneFuture = scene;
	}

	const std::shared_ptr<VisCameraSettings>& ModelVisLayer::GetCamera()
	{
		return _pimpl->_camera;
	}
	
    ModelVisLayer::ModelVisLayer()
    {
        _pimpl = std::make_unique<Pimpl>();
		_pimpl->_camera = std::make_shared<VisCameraSettings>();
    }

    ModelVisLayer::~ModelVisLayer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class VisualisationOverlay::Pimpl
    {
    public:
		std::shared_ptr<VisOverlaySettings> _settings;
        std::shared_ptr<VisMouseOver> _mouseOver;
		std::shared_ptr<VisCameraSettings> _cameraSettings;

		std::shared_ptr<SceneEngine::IScene> _scene;
        ::Assets::FuturePtr<SceneEngine::IScene> _sceneFuture;

		std::shared_ptr<RenderCore::Techniques::IMaterialDelegate>		_materialDelegate;
		std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate>		_techniqueDelegate;
		std::shared_ptr<RenderCore::Assets::SimpleModelRenderer::IPreDrawDelegate> _stencilPrimeDelegate;

		Pimpl()
		{
			_techniqueDelegate = std::make_shared<RenderCore::Techniques::TechniqueDelegate_Basic>();
			_materialDelegate = std::make_shared<RenderCore::Techniques::MaterialDelegate_Basic>();
			_stencilPrimeDelegate = std::make_shared<StencilRefDelegate>();
		}
    };

	static ModelCacheModel GetModel(
		ModelCache& cache,
		ModelVisSettings& settings)
	{
		std::vector<ModelCache::SupplementGUID> supplements;
		{
			const auto& s = settings._supplements;
			size_t offset = 0;
			for (;;) {
				auto comma = s.find_first_of(',', offset);
				if (comma == std::string::npos) comma = s.size();
				if (offset == comma) break;
				auto hash = ConstHash64FromString(AsPointer(s.begin()) + offset, AsPointer(s.begin()) + comma);
				supplements.push_back(hash);
				offset = comma;
			}
		}

		return cache.GetModel(
			MakeStringSection(settings._modelName), 
			MakeStringSection(settings._materialName),
			MakeIteratorRange(supplements),
			settings._levelOfDetail);
	}
    
    void VisualisationOverlay::Render(
        RenderCore::IThreadContext& threadContext, 
		const RenderCore::IResourcePtr& renderTarget,
        RenderCore::Techniques::ParsingContext& parserContext)
    {
        using namespace RenderCore;
		parserContext.GetNamedResources().Bind(RenderCore::Techniques::AttachmentSemantics::ColorLDR, renderTarget);
		
		if (!parserContext.GetNamedResources().GetBoundResource(Techniques::AttachmentSemantics::MultisampleDepth))		// we need this attachment to continue
			return;

		if (_pimpl->_sceneFuture) {
			auto newActualized = _pimpl->_sceneFuture->TryActualize();
			if (newActualized) {
				_pimpl->_scene = newActualized;
				_pimpl->_sceneFuture = nullptr;
			} else if (_pimpl->_sceneFuture->GetAssetState() == ::Assets::AssetState::Invalid) {
				_pimpl->_scene = nullptr;
				_pimpl->_sceneFuture = nullptr;
			}
		}

		if (!_pimpl->_scene || !_pimpl->_cameraSettings) return;

		RenderCore::Techniques::SequencerTechnique sequencerTechnique;
		sequencerTechnique._techniqueDelegate = _pimpl->_techniqueDelegate;
		sequencerTechnique._materialDelegate = _pimpl->_materialDelegate;
		sequencerTechnique._renderStateDelegate = parserContext.GetRenderStateDelegate();

		auto cam = AsCameraDesc(*_pimpl->_cameraSettings);
		SceneEngine::SceneView sceneView {
			Techniques::BuildProjectionDesc(cam, UInt2(threadContext.GetStateDesc()._viewportDimensions[0], threadContext.GetStateDesc()._viewportDimensions[1])),
			SceneEngine::SceneView::Type::Normal
		};

		bool doColorByMaterial = 
			(_pimpl->_settings->_colourByMaterial == 1)
			|| (_pimpl->_settings->_colourByMaterial == 2 && _pimpl->_mouseOver->_hasMouseOver);

		if (_pimpl->_settings->_drawWireframe || _pimpl->_settings->_drawNormals || doColorByMaterial) {
			AttachmentDesc colorLDRDesc = AsAttachmentDesc(renderTarget->GetDesc());
			std::vector<FrameBufferDesc::Attachment> attachments {
				{ Techniques::AttachmentSemantics::ColorLDR, colorLDRDesc },
				{ Techniques::AttachmentSemantics::MultisampleDepth, Format::D24_UNORM_S8_UINT }
			};
			SubpassDesc mainPass;
			mainPass.SetName("VisualisationOverlay");
			mainPass.AppendOutput(0);
			mainPass.SetDepthStencil(1);
			FrameBufferDesc fbDesc{ std::move(attachments), {mainPass} };
			Techniques::RenderPassInstance rpi {
				threadContext, fbDesc, 
				parserContext.GetFrameBufferPool(),
				parserContext.GetNamedResources() };

			if (_pimpl->_settings->_drawWireframe) {

				CATCH_ASSETS_BEGIN
					/*auto model = GetModel(*_pimpl->_cache, *_pimpl->_modelSettings);
					assert(model._renderer && model._sharedStateSet);

					FixedFunctionModel::SharedStateSet::CaptureMarker captureMarker;
					if (model._sharedStateSet)
						captureMarker = model._sharedStateSet->CaptureState(context, parserContext.GetRenderStateDelegate(), {});

					const auto techniqueIndex = Techniques::TechniqueIndex::VisWireframe;

					RenderWithEmbeddedSkeleton(
						FixedFunctionModel::ModelRendererContext(context, parserContext, techniqueIndex),
						*model._renderer, *model._sharedStateSet, model._model);*/

					SceneEngine::ExecuteSceneRaw(
						threadContext, parserContext, 
						sequencerTechnique,
						Techniques::TechniqueIndex::VisWireframe,
						sceneView,
						*_pimpl->_scene);

				CATCH_ASSETS_END(parserContext)
			}

			if (_pimpl->_settings->_drawNormals) {

				CATCH_ASSETS_BEGIN
					/*auto model = GetModel(*_pimpl->_cache, *_pimpl->_modelSettings);
					assert(model._renderer && model._sharedStateSet);

					FixedFunctionModel::SharedStateSet::CaptureMarker captureMarker;
					if (model._sharedStateSet)
						captureMarker = model._sharedStateSet->CaptureState(context, parserContext.GetRenderStateDelegate(), {});

					const auto techniqueIndex = Techniques::TechniqueIndex::VisNormals;

					RenderWithEmbeddedSkeleton(
						FixedFunctionModel::ModelRendererContext(context, parserContext, techniqueIndex),
						*model._renderer, *model._sharedStateSet, model._model);*/

					SceneEngine::ExecuteSceneRaw(
						threadContext, parserContext, 
						sequencerTechnique,
						Techniques::TechniqueIndex::VisNormals,
						sceneView,
						*_pimpl->_scene);
				CATCH_ASSETS_END(parserContext)
			}

			if (doColorByMaterial) {
				auto *visContent = dynamic_cast<IVisContent*>(_pimpl->_scene.get());
				std::shared_ptr<RenderCore::Assets::SimpleModelRenderer::IPreDrawDelegate> oldDelegate;
				if (visContent)
					oldDelegate = visContent->SetPreDrawDelegate(_pimpl->_stencilPrimeDelegate);
				CATCH_ASSETS_BEGIN
					// Prime the stencil buffer with draw call indices
					SceneEngine::ExecuteSceneRaw(
						threadContext, parserContext, 
						sequencerTechnique,
						Techniques::TechniqueIndex::DepthOnly,
						sceneView,
						*_pimpl->_scene);
				CATCH_ASSETS_END(parserContext)
				if (visContent)
					visContent->SetPreDrawDelegate(oldDelegate);
			}
		}

		
            //  Draw an overlay over the scene, 
            //  containing debugging / profiling information
        if (doColorByMaterial) {

			CATCH_ASSETS_BEGIN
                RenderOverlays::HighlightByStencilSettings settings;

				// The highlight shader supports remapping the 8 bit stencil value to through an array
				// to some other value. This is useful for ignoring bits or just making 2 different stencil
				// buffer values mean the same thing. We don't need it right now though, we can just do a
				// direct mapping here --
				auto marker = _pimpl->_mouseOver->_drawCallIndex;
				settings._highlightedMarker = UInt4(marker, marker, marker, marker);
				settings._stencilToMarkerMap[0] = UInt4(~0u, ~0u, ~0u, ~0u);
				for (unsigned c=1; c<dimof(settings._stencilToMarkerMap); c++)
					settings._stencilToMarkerMap[c] = UInt4(c-1, c-1, c-1, c-1);

                ExecuteHighlightByStencil(
                    threadContext, parserContext, 
                    settings, _pimpl->_settings->_colourByMaterial==2);
            CATCH_ASSETS_END(parserContext)
        }
    }

	void VisualisationOverlay::Set(const ::Assets::FuturePtr<SceneEngine::IScene>& scene)
	{
		_pimpl->_sceneFuture = scene;
	}

	void VisualisationOverlay::Set(const std::shared_ptr<VisCameraSettings>& camera)
	{
		_pimpl->_cameraSettings = camera;
	}

    auto VisualisationOverlay::GetInputListener() -> std::shared_ptr<IInputListener>
    { return nullptr; }

    void VisualisationOverlay::SetActivationState(bool) {}

    VisualisationOverlay::VisualisationOverlay(
		std::shared_ptr<VisOverlaySettings> overlaySettings,
        std::shared_ptr<VisMouseOver> mouseOver)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_settings = std::move(overlaySettings);
        _pimpl->_mouseOver = std::move(mouseOver);
    }

    VisualisationOverlay::~VisualisationOverlay() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	static RenderCore::Techniques::SequencerTechnique MakeRayTestSequencerTechnique()
	{
		using namespace RenderCore;
		Techniques::SequencerTechnique sequencer;
		sequencer._techniqueDelegate = SceneEngine::CreateRayTestTechniqueDelegate();
		sequencer._materialDelegate = std::make_shared<Techniques::MaterialDelegate_Basic>();

		auto& techUSI = Techniques::TechniqueContext::GetGlobalUniformsStreamInterface();
		for (unsigned c=0; c<techUSI._cbBindings.size(); ++c)
			sequencer._sequencerUniforms.emplace_back(std::make_pair(techUSI._cbBindings[c]._hashName, std::make_shared<Techniques::GlobalCBDelegate>(c)));
		return sequencer;
	}

	static SceneEngine::IIntersectionTester::Result FirstRayIntersection(
		RenderCore::IThreadContext& threadContext,
		const RenderCore::Techniques::TechniqueContext& techniqueContext,
        std::pair<Float3, Float3> worldSpaceRay,
		SceneEngine::IScene& scene,
		const std::string& modelName, const std::string& materialName)
	{
		using namespace RenderCore;

		Techniques::ParsingContext parserContext { techniqueContext };
		
		CATCH_ASSETS_BEGIN
		
        SceneEngine::ModelIntersectionStateContext stateContext {
            SceneEngine::ModelIntersectionStateContext::RayTest,
            threadContext, parserContext };
        stateContext.SetRay(worldSpaceRay);

		SceneEngine::ExecuteSceneRaw(
			threadContext, parserContext, 
			MakeRayTestSequencerTechnique(),
			Techniques::TechniqueIndex::DepthOnly,
			{RenderCore::Techniques::ProjectionDesc{}, SceneEngine::SceneView::Type::Other},
			scene);

        auto results = stateContext.GetResults();
        if (!results.empty()) {
            const auto& r = results[0];

            SceneEngine::IIntersectionTester::Result result;
            result._type = SceneEngine::IntersectionTestScene::Type::Extra;
            result._worldSpaceCollision = 
                worldSpaceRay.first + r._intersectionDepth * Normalize(worldSpaceRay.second - worldSpaceRay.first);
            result._distance = r._intersectionDepth;
            result._drawCallIndex = r._drawCallIndex;
            result._materialGuid = r._materialGuid;
            result._materialName = materialName;
            result._modelName = modelName;

            return result;
        }

		CATCH_ASSETS_END(parserContext)	// we can get pending/invalid assets here, which we can suppress

		return {};
    }

	static SceneEngine::IIntersectionTester::Result FirstRayIntersection(
        RenderCore::IThreadContext& threadContext,
		const RenderCore::Techniques::TechniqueContext& techniqueContext,
        std::pair<Float3, Float3> worldSpaceRay,
		SceneEngine::IScene& scene)
    {
		std::string modelName = "Model", materialName = "Material";
		auto* visContent = dynamic_cast<IVisContent*>(&scene);
		if (visContent) {
			modelName = visContent->GetModelName();
			materialName = visContent->GetMaterialName();
		}

		return FirstRayIntersection(
			threadContext, techniqueContext, worldSpaceRay,
			scene,
			modelName, materialName);
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class MouseOverTrackingListener : public RenderOverlays::DebuggingDisplay::IInputListener
    {
    public:
        bool OnInputEvent(
			const RenderOverlays::DebuggingDisplay::InputContext& context,
			const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
        {
            using namespace SceneEngine;

            auto worldSpaceRay = IntersectionTestContext::CalculateWorldSpaceRay(
				AsCameraDesc(*_camera), evnt._mousePosition, context._viewMins, context._viewMaxs);

			SceneEngine::IScene* scene = nullptr;
			if (_sceneFuture)
				scene = _sceneFuture->TryActualize().get();
			    
            if (scene) {
				auto intr = FirstRayIntersection(*RenderCore::Techniques::GetThreadContext(), *_techniqueContext, worldSpaceRay, *scene);
				if (intr._type != 0) {
					if (        intr._drawCallIndex != _mouseOver->_drawCallIndex
							||  intr._materialGuid != _mouseOver->_materialGuid
							||  !_mouseOver->_hasMouseOver) {

						_mouseOver->_hasMouseOver = true;
						_mouseOver->_drawCallIndex = intr._drawCallIndex;
						_mouseOver->_materialGuid = intr._materialGuid;
						_mouseOver->_changeEvent.Invoke();
					}
				} else {
					if (_mouseOver->_hasMouseOver) {
						_mouseOver->_hasMouseOver = false;
						_mouseOver->_changeEvent.Invoke();
					}
				}
			}

            return false;
        }

		void Set(const ::Assets::FuturePtr<SceneEngine::IScene>& scene)
		{
			_sceneFuture = scene;
		}

		const std::shared_ptr<SceneEngine::IScene>& GetScene()
		{
			return _sceneFuture->Actualize();
		}

        MouseOverTrackingListener(
            const std::shared_ptr<VisMouseOver>& mouseOver,
            const std::shared_ptr<RenderCore::Techniques::TechniqueContext>& techniqueContext,
            const std::shared_ptr<VisCameraSettings>& camera)
            : _mouseOver(std::move(mouseOver))
            , _techniqueContext(std::move(techniqueContext))
            , _camera(std::move(camera))
        {}
        MouseOverTrackingListener::~MouseOverTrackingListener() {}

    protected:
        std::shared_ptr<VisMouseOver> _mouseOver;
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> _techniqueContext;
        std::shared_ptr<VisCameraSettings> _camera;
        
        ::Assets::FuturePtr<SceneEngine::IScene> _sceneFuture;
    };

    auto MouseOverTrackingOverlay::GetInputListener() -> std::shared_ptr<IInputListener>
    {
        return _inputListener;
    }

    void MouseOverTrackingOverlay::Render(
        RenderCore::IThreadContext& threadContext,
		const RenderCore::IResourcePtr& renderTarget,
        RenderCore::Techniques::ParsingContext& parsingContext) 
    {
        if (!_mouseOver->_hasMouseOver || !_overlayFn) return;
		auto scene = _inputListener->GetScene();
		if (!scene) return;

		auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, renderTarget, parsingContext);
        using namespace RenderOverlays::DebuggingDisplay;
        RenderOverlays::ImmediateOverlayContext overlays(
            threadContext, &parsingContext.GetNamedResources(),
            parsingContext.GetProjectionDesc());
		overlays.CaptureState();
		auto viewportDims = threadContext.GetStateDesc()._viewportDimensions;
		Rect rect { Coord2{0, 0}, Coord2(viewportDims[0], viewportDims[1]) };
        _overlayFn(overlays, rect, *_mouseOver, *scene);
		overlays.ReleaseState();
    }

	void MouseOverTrackingOverlay::Set(const ::Assets::FuturePtr<SceneEngine::IScene>& scene)
	{
		_inputListener->Set(scene);
	}

    MouseOverTrackingOverlay::MouseOverTrackingOverlay(
        const std::shared_ptr<VisMouseOver>& mouseOver,
        const std::shared_ptr<RenderCore::Techniques::TechniqueContext>& techniqueContext,
        const std::shared_ptr<VisCameraSettings>& camera,
        OverlayFn&& overlayFn)
    : _overlayFn(std::move(overlayFn))
    {
        _mouseOver = mouseOver;
        _inputListener = std::make_shared<MouseOverTrackingListener>(
            std::move(mouseOver),
            std::move(techniqueContext), 
            std::move(camera));
    }

    MouseOverTrackingOverlay::~MouseOverTrackingOverlay() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void ChangeEvent::Invoke() 
    {
        for (auto i=_callbacks.begin(); i!=_callbacks.end(); ++i) {
            (*i)->OnChange();
        }
    }
    ChangeEvent::~ChangeEvent() {}

    ModelVisSettings::ModelVisSettings()
    {
        _modelName = "game/model/galleon/galleon.dae";
        _materialName = "game/model/galleon/galleon.material";

		// _modelName = "data/meshes/actors/dragon/character assets/alduin.nif";
		// _materialName = "data/meshes/actors/dragon/character assets/alduin.nif";
		// _animationFileName = "data/meshes/actors/dragon/animations/special_alduindeathagony.hkx";

        // _envSettingsFile = "defaultenv.txt:environment";
    }

	IVisContent::~IVisContent() {}

}

