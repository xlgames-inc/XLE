// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "VisualisationUtils.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/LightingParserStandardPlugin.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/RenderStep.h"
#include "../../SceneEngine/RayVsModel.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../PlatformRig/BasicSceneParser.h"
#include "../../PlatformRig/Screenshot.h"
#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../RenderOverlays/OverlayContext.h"
#include "../../RenderOverlays/HighlightEffects.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/BasicDelegates.h"
#include "../../RenderCore/Techniques/RenderPassUtils.h"
#include "../../RenderCore/IThreadContext.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/Assets.h"
#include "../../Math/Transformations.h"
#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/TimeUtils.h"
#include "../../Utility/FunctionUtils.h"

#pragma warning(disable:4505) // unreferenced local function has been removed

namespace ToolsRig
{
    RenderCore::Techniques::CameraDesc AsCameraDesc(const VisCameraSettings& camSettings)
    {
        RenderCore::Techniques::CameraDesc result;
        result._cameraToWorld = MakeCameraToWorld(
            Normalize(camSettings._focus - camSettings._position),
            Float3(0.f, 0.f, 1.f), camSettings._position);
        result._farClip = camSettings._farClip;
        result._nearClip = camSettings._nearClip;
        result._verticalFieldOfView = Deg2Rad(camSettings._verticalFieldOfView);
        result._left = camSettings._left;
        result._top = camSettings._top;
        result._right = camSettings._right;
        result._bottom = camSettings._bottom;
        result._projection = 
            (camSettings._projection == VisCameraSettings::Projection::Orthogonal)
             ? RenderCore::Techniques::CameraDesc::Projection::Orthogonal
             : RenderCore::Techniques::CameraDesc::Projection::Perspective;
        assert(std::isfinite(result._cameraToWorld(0,0)) && !std::isnan(result._cameraToWorld(0,0)));
        return result;
    }

    VisCameraSettings AlignCameraToBoundingBox(
        float verticalFieldOfView, 
        const std::pair<Float3, Float3>& boxIn)
    {
        auto box = boxIn;

            // convert empty/inverted boxes into something rational...
        if (    box.first[0] >= box.second[0] 
            ||  box.first[1] >= box.second[1] 
            ||  box.first[2] >= box.second[2]) {
            box.first = Float3(-10.f, -10.f, -10.f);
            box.second = Float3( 10.f,  10.f,  10.f);
        }

        const float border = 0.0f;
        Float3 position = .5f * (box.first + box.second);

            // push back to attempt to fill the viewport with the bounding box
        float verticalHalfDimension = .5f * box.second[2] - box.first[2];
        position[0] = box.first[0] - (verticalHalfDimension * (1.f + border)) / XlTan(.5f * Deg2Rad(verticalFieldOfView));

        VisCameraSettings result;
        result._position = position;
        result._focus = .5f * (box.first + box.second);
        result._verticalFieldOfView = verticalFieldOfView;
        result._farClip = 1.25f * (box.second[0] - position[0]);
        result._nearClip = result._farClip / 10000.f;
        return result;
    }

	VisEnvSettings::VisEnvSettings() : _envConfigFile("defaultenv.txt:environment"), _lightingType(LightingType::Deferred) {}
	VisEnvSettings::VisEnvSettings(const std::string& envConfigFile) : _envConfigFile(envConfigFile) {}

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

		std::shared_ptr<RenderCore::Techniques::IMaterialDelegate> _materialDelegate;
		std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate> _techniqueDelegate;
		std::shared_ptr<RenderCore::Techniques::IRenderStateDelegate> _renderStateDelegate;
		
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
			} else if (_pimpl->_envSettingsFuture->GetAssetState() == ::Assets::AssetState::Invalid) {
				_pimpl->_envSettingsErrorMessage = ::Assets::AsString(_pimpl->_envSettingsFuture->GetActualizationLog());
				_pimpl->_envSettings = nullptr;
				_pimpl->_envSettingsFuture = nullptr;
			}
		}

		if (_pimpl->_envSettings && _pimpl->_scene) {
			PlatformRig::BasicLightingParserDelegate lightingParserDelegate(_pimpl->_envSettings);

			std::shared_ptr<SceneEngine::ILightingParserPlugin> lightingPlugins[] = {
				std::make_shared<SceneEngine::LightingParserStandardPlugin>()
			};
			auto qualSettings = SceneEngine::RenderSceneSettings{
				SceneEngine::RenderSceneSettings::LightingModel::Deferred,
				&lightingParserDelegate,
				MakeIteratorRange(lightingPlugins)};

			LightingParserContext lightingParserContext;
			{
				// Setup delegates to override the material & technique values (if we've been given overrides for them)
				AutoCleanup materialDelegateCleanup, techniqueDelegateCleanup;
				if (_pimpl->_materialDelegate) {
					auto oldDelegate = parserContext.SetMaterialDelegate(_pimpl->_materialDelegate);
					materialDelegateCleanup = AutoCleanup{[oldDelegate, &parserContext]() {
						parserContext.SetMaterialDelegate(oldDelegate);
					}};
				}

				if (_pimpl->_techniqueDelegate) {
					auto oldDelegate = parserContext.SetTechniqueDelegate(_pimpl->_techniqueDelegate);
					techniqueDelegateCleanup = AutoCleanup{[oldDelegate, &parserContext]() {
						parserContext.SetTechniqueDelegate(oldDelegate);
					}};
				}

				auto& screenshot = Tweakable("Screenshot", 0);
				if (screenshot) {
					PlatformRig::TiledScreenshot(
						threadContext, parserContext,
						*_pimpl->_scene, AsCameraDesc(*_pimpl->_camera),
						qualSettings, UInt2(screenshot, screenshot));
					screenshot = 0;
				}

				lightingParserContext = LightingParser_ExecuteScene(
					threadContext, renderTarget, parserContext, 
					*_pimpl->_scene, AsCameraDesc(*_pimpl->_camera),
					qualSettings);
			}

			// Draw debugging overlays -- 
			{
				auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, renderTarget, parserContext);
				SceneEngine::LightingParser_Overlays(threadContext, parserContext, lightingParserContext);
			}
		}

		if (!_pimpl->_sceneErrorMessage.empty() || !_pimpl->_envSettingsErrorMessage.empty()) {
			auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, renderTarget, parserContext);
			if (!_pimpl->_sceneErrorMessage.empty())
				SceneEngine::DrawString(threadContext, RenderOverlays::GetDefaultFont(), _pimpl->_sceneErrorMessage);
			if (!_pimpl->_envSettingsErrorMessage.empty())
				SceneEngine::DrawString(threadContext, RenderOverlays::GetDefaultFont(), _pimpl->_envSettingsErrorMessage);
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

	void ModelVisLayer::SetOverrides(const std::shared_ptr<RenderCore::Techniques::IMaterialDelegate>& delegate)
	{
		_pimpl->_materialDelegate = delegate;
	}

	void ModelVisLayer::SetOverrides(const std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate>& delegate)
	{
		_pimpl->_techniqueDelegate = delegate;
	}

	void ModelVisLayer::SetOverrides(const std::shared_ptr<RenderCore::Techniques::IRenderStateDelegate>& delegate)
	{
		_pimpl->_renderStateDelegate = delegate;
	}

	const std::shared_ptr<VisCameraSettings>& ModelVisLayer::GetCamera()
	{
		return _pimpl->_camera;
	}

	void ModelVisLayer::ResetCamera()
	{
		auto* scene = dynamic_cast<ToolsRig::IVisContent*>(_pimpl->_scene.get());
		if (scene) {
			auto boundingBox = scene->GetBoundingBox();
			*_pimpl->_camera = ToolsRig::AlignCameraToBoundingBox(_pimpl->_camera->_verticalFieldOfView, boundingBox);
		}
	}
	
    ModelVisLayer::ModelVisLayer()
    {
        _pimpl = std::make_unique<Pimpl>();
		_pimpl->_camera = std::make_shared<VisCameraSettings>();
    }

    ModelVisLayer::~ModelVisLayer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	static SceneEngine::RenderSceneSettings::LightingModel AsLightingModel(VisEnvSettings::LightingType lightingType)
	{
		switch (lightingType) {
		case VisEnvSettings::LightingType::Deferred:
			return SceneEngine::RenderSceneSettings::LightingModel::Deferred;
		case VisEnvSettings::LightingType::Forward:
			return SceneEngine::RenderSceneSettings::LightingModel::Forward;
		default:
		case VisEnvSettings::LightingType::Direct:
			return SceneEngine::RenderSceneSettings::LightingModel::Direct;
		}
	}

	std::pair<DrawPreviewResult, std::string> DrawPreview(
        RenderCore::IThreadContext& context,
		const RenderCore::IResourcePtr& renderTarget,
        RenderCore::Techniques::ParsingContext& parserContext,
		VisCameraSettings& cameraSettings,
		VisEnvSettings& envSettings,
		SceneEngine::IScene& scene)
    {
        try
        {
			auto future = ::Assets::MakeAsset<PlatformRig::EnvironmentSettings>(envSettings._envConfigFile);
			future->StallWhilePending();
			PlatformRig::BasicLightingParserDelegate lightingParserDelegate(future->Actualize());

			std::shared_ptr<SceneEngine::ILightingParserPlugin> lightingPlugins[] = {
				std::make_shared<SceneEngine::LightingParserStandardPlugin>()
			};
			SceneEngine::RenderSceneSettings qualSettings{
				AsLightingModel(envSettings._lightingType),
				&lightingParserDelegate,
				MakeIteratorRange(lightingPlugins)};

			SceneEngine::LightingParser_ExecuteScene(
				context, renderTarget, parserContext,
				scene, AsCameraDesc(cameraSettings), qualSettings);

            if (parserContext.HasErrorString())
				return std::make_pair(DrawPreviewResult::Error, parserContext._stringHelpers->_errorString);
			if (parserContext.HasInvalidAssets())
				return std::make_pair(DrawPreviewResult::Error, "Invalid assets encountered");
            if (parserContext.HasPendingAssets())
				return std::make_pair(DrawPreviewResult::Pending, std::string());

            return std::make_pair(DrawPreviewResult::Success, std::string());
        }
        catch (::Assets::Exceptions::InvalidAsset& e) { return std::make_pair(DrawPreviewResult::Error, e.what()); }
        catch (::Assets::Exceptions::PendingAsset& e) { return std::make_pair(DrawPreviewResult::Pending, e.Initializer()); }

        return std::make_pair(DrawPreviewResult::Error, std::string());
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	class StencilRefDelegate : public RenderCore::Assets::SimpleModelRenderer::IPreDrawDelegate
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

    class VisualisationOverlay::Pimpl
    {
    public:
		VisOverlaySettings _settings;
        std::shared_ptr<VisMouseOver> _mouseOver;
		std::shared_ptr<VisCameraSettings> _cameraSettings;
		std::shared_ptr<VisAnimationState> _animState;

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

				auto* visContext = dynamic_cast<IVisContent*>(_pimpl->_scene.get());
				if (visContext)
					visContext->BindAnimationState(_pimpl->_animState);
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

		auto& techUSI = RenderCore::Techniques::TechniqueContext::GetGlobalUniformsStreamInterface();
		for (unsigned c=0; c<techUSI._cbBindings.size(); ++c)
			sequencerTechnique._sequencerUniforms.emplace_back(std::make_pair(techUSI._cbBindings[c]._hashName, std::make_shared<RenderCore::Techniques::GlobalCBDelegate>(c)));

		auto cam = AsCameraDesc(*_pimpl->_cameraSettings);
		SceneEngine::SceneView sceneView {
			Techniques::BuildProjectionDesc(cam, UInt2(threadContext.GetStateDesc()._viewportDimensions[0], threadContext.GetStateDesc()._viewportDimensions[1])),
			SceneEngine::SceneView::Type::Normal
		};

		bool doColorByMaterial = 
			(_pimpl->_settings._colourByMaterial == 1)
			|| (_pimpl->_settings._colourByMaterial == 2 && _pimpl->_mouseOver->_hasMouseOver);

		if (_pimpl->_settings._drawWireframe || _pimpl->_settings._drawNormals || _pimpl->_settings._skeletonMode || doColorByMaterial) {
			AttachmentDesc colorLDRDesc = AsAttachmentDesc(renderTarget->GetDesc());
			std::vector<FrameBufferDesc::Attachment> attachments {
				{ Techniques::AttachmentSemantics::ColorLDR, colorLDRDesc },
				{ Techniques::AttachmentSemantics::MultisampleDepth, Format::D24_UNORM_S8_UINT }
			};
			SubpassDesc mainPass;
			mainPass.SetName("VisualisationOverlay");
			mainPass.AppendOutput(0);
			mainPass.SetDepthStencil(1, LoadStore::Retain_ClearStencil);		// ensure stencil is cleared (but ok to keep depth)
			FrameBufferDesc fbDesc{ std::move(attachments), {mainPass} };
			Techniques::RenderPassInstance rpi {
				threadContext, fbDesc, 
				parserContext.GetFrameBufferPool(),
				parserContext.GetNamedResources() };

			if (_pimpl->_settings._drawWireframe) {
				CATCH_ASSETS_BEGIN
					SceneEngine::ExecuteSceneRaw(
						threadContext, parserContext, 
						sequencerTechnique,
						Techniques::TechniqueIndex::VisWireframe,
						sceneView,
						*_pimpl->_scene);
				CATCH_ASSETS_END(parserContext)
			}

			if (_pimpl->_settings._drawNormals) {
				CATCH_ASSETS_BEGIN
					SceneEngine::ExecuteSceneRaw(
						threadContext, parserContext, 
						sequencerTechnique,
						Techniques::TechniqueIndex::VisNormals,
						sceneView,
						*_pimpl->_scene);
				CATCH_ASSETS_END(parserContext)
			}

			if (_pimpl->_settings._skeletonMode) {
				auto* visContent = dynamic_cast<IVisContent*>(_pimpl->_scene.get());
				if (visContent) {
					CATCH_ASSETS_BEGIN
						visContent->RenderSkeleton(
							threadContext, parserContext,
							_pimpl->_settings._skeletonMode == 2);
					CATCH_ASSETS_END(parserContext)
				}
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
                    settings, _pimpl->_settings._colourByMaterial==2);
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

	void VisualisationOverlay::Set(const VisOverlaySettings& overlaySettings)
	{
		_pimpl->_settings = overlaySettings;
	}

	const VisOverlaySettings& VisualisationOverlay::GetOverlaySettings() const
	{
		return _pimpl->_settings;
	}

	void VisualisationOverlay::Set(const std::shared_ptr<VisAnimationState>& animState)
	{
		_pimpl->_animState = animState;
		if (_pimpl->_scene) {
			auto* visContext = dynamic_cast<IVisContent*>(_pimpl->_scene.get());
			if (visContext)
				visContext->BindAnimationState(animState);
		}
	}

    auto VisualisationOverlay::GetOverlayState() const -> OverlayState
	{
		RefreshMode refreshMode = RefreshMode::EventBased;

		// Need regular updates if the scene future hasn't been fully loaded yet
		// Or if there's active animation playing in the scene
		if (_pimpl->_sceneFuture) { 	
			refreshMode = RefreshMode::RegularAnimation;
		} else {
			auto* visContext = dynamic_cast<IVisContent*>(_pimpl->_scene.get());
			if (visContext && visContext->HasActiveAnimation())
				refreshMode = RefreshMode::RegularAnimation;
		}
		
		return { refreshMode };
	}

    VisualisationOverlay::VisualisationOverlay(
		const VisOverlaySettings& overlaySettings,
        std::shared_ptr<VisMouseOver> mouseOver)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_settings = overlaySettings;
        _pimpl->_mouseOver = std::move(mouseOver);
    }

    VisualisationOverlay::~VisualisationOverlay() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	static SceneEngine::IIntersectionTester::Result FirstRayIntersection(
		RenderCore::IThreadContext& threadContext,
		const RenderCore::Techniques::TechniqueContext& techniqueContext,
        std::pair<Float3, Float3> worldSpaceRay,
		SceneEngine::IScene& scene)
	{
		using namespace RenderCore;

		Techniques::ParsingContext parserContext { techniqueContext };
		
		SceneEngine::ModelIntersectionStateContext stateContext {
            SceneEngine::ModelIntersectionStateContext::RayTest,
            threadContext, parserContext };
        stateContext.SetRay(worldSpaceRay);
		
		CATCH_ASSETS_BEGIN
		
			SceneEngine::ExecuteSceneRaw(
				threadContext, parserContext, 
				stateContext.MakeRayTestSequencerTechnique(),
				Techniques::TechniqueIndex::DepthOnly,
				{RenderCore::Techniques::ProjectionDesc{}, SceneEngine::SceneView::Type::Other},
				scene);

		CATCH_ASSETS_END(parserContext)	// we can get pending/invalid assets here, which we can suppress

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

			result._modelName = "Model";
			result._materialName = "Material";
			auto* visContent = dynamic_cast<IVisContent*>(&scene);
			if (visContent) {
				auto details = visContent->GetDrawCallDetails(result._drawCallIndex, result._materialGuid);
				result._modelName = details._modelName;
				result._materialName = details._materialName;
			}

            return result;
        }

		

		return {};
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class MouseOverTrackingListener : public PlatformRig::IInputListener, public std::enable_shared_from_this<MouseOverTrackingListener>
    {
    public:
        bool OnInputEvent(
			const PlatformRig::InputContext& context,
			const PlatformRig::InputSnapshot& evnt)
        {
			if (evnt._mouseDelta == PlatformRig::Coord2 { 0, 0 })
				return false;

			// Limit the update frequency by ensuring that enough time has
			// passed since the last time we did an update. If there hasn't
			// been enough time, we should schedule a timeout event to trigger.
			//
			// If there has already been a timeout event scheduled, we have 2 options.
			// Either we reschedule it, or we just allow the previous timeout to 
			// finish as normal.
			//			
			// If we rescheduled the event, it would mean that fast movement of the 
			// mouse would disable all update events, and we would only get new information
			// after the mouse has come to rest for the timeout period.
			//
			// The preferred option may depend on the particular use case.
			auto time = Millisecond_Now();
			const auto timePeriod = 200u;
			_timeoutContext = context;
			_timeoutMousePosition = evnt._mousePosition;
			if ((time - _timeOfLastCalculate) < timePeriod) {
				auto* osRunLoop = PlatformRig::GetOSRunLoop();
				if (_timeoutEvent == ~0u && osRunLoop) {
					std::weak_ptr<MouseOverTrackingListener> weakThis = shared_from_this();
					_timeoutEvent = osRunLoop->ScheduleTimeoutEvent(
						time + timePeriod,
						[weakThis]() {
							auto l = weakThis.lock();
							if (l) {
								l->_timeOfLastCalculate = Millisecond_Now();
								l->CalculateForMousePosition(
									l->_timeoutContext,
									l->_timeoutMousePosition);
								l->_timeoutEvent = ~0u;								
							}
						});
				}
			} else {
				auto* osRunLoop = PlatformRig::GetOSRunLoop();
				if (_timeoutEvent != ~0u && osRunLoop) {
					osRunLoop->RemoveEvent(_timeoutEvent);
					_timeoutEvent = ~0u;
				}

				CalculateForMousePosition(context, evnt._mousePosition);
				_timeOfLastCalculate = time;
			}

			return false;
		}

		void CalculateForMousePosition(
			const PlatformRig::InputContext& context,
			PlatformRig::Coord2 mousePosition)
		{
            auto worldSpaceRay = SceneEngine::IntersectionTestContext::CalculateWorldSpaceRay(
				AsCameraDesc(*_camera), mousePosition, context._viewMins, context._viewMaxs);

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
		unsigned _timeOfLastCalculate = 0;

		PlatformRig::InputContext _timeoutContext;
		PlatformRig::Coord2 _timeoutMousePosition;
		unsigned _timeoutEvent = ~0u;
    };

    auto MouseOverTrackingOverlay::GetInputListener() -> std::shared_ptr<PlatformRig::IInputListener>
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

	class InputLayer : public PlatformRig::IOverlaySystem
    {
    public:
        std::shared_ptr<PlatformRig::IInputListener> GetInputListener();

        void Render(
            RenderCore::IThreadContext& context,
			const RenderCore::IResourcePtr& renderTarget,
            RenderCore::Techniques::ParsingContext& parserContext); 

        InputLayer(std::shared_ptr<PlatformRig::IInputListener> listener);
        ~InputLayer();
    protected:
        std::shared_ptr<PlatformRig::IInputListener> _listener;
    };

    auto InputLayer::GetInputListener() -> std::shared_ptr<PlatformRig::IInputListener>
    {
        return _listener;
    }

    void InputLayer::Render(
        RenderCore::IThreadContext&,
		const RenderCore::IResourcePtr&,
		RenderCore::Techniques::ParsingContext&) {}

    InputLayer::InputLayer(std::shared_ptr<PlatformRig::IInputListener> listener) : _listener(listener) {}
    InputLayer::~InputLayer() {}

	std::shared_ptr<PlatformRig::IOverlaySystem> MakeLayerForInput(const std::shared_ptr<PlatformRig::IInputListener>& listener)
	{
		return std::make_shared<InputLayer>(listener);
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	const std::shared_ptr<SceneEngine::IScene>& TryActualize(const ::Assets::AssetFuture<SceneEngine::IScene>& future)
	{
		// This function exists because we can't call TryActualize() from a C++/CLR source file because
		// of the problem related to including <mutex>
		return future.TryActualize();
	}

	std::optional<std::string> GetActualizationError(const ::Assets::AssetFuture<SceneEngine::IScene>& future)
	{
		auto state = future.GetAssetState();
		if (state != ::Assets::AssetState::Invalid)
			return {};
		return ::Assets::AsString(future.GetActualizationLog());
	}

    void ChangeEvent::Invoke() 
    {
        for (auto i=_callbacks.begin(); i!=_callbacks.end(); ++i) {
            (*i)->OnChange();
        }
    }
    ChangeEvent::~ChangeEvent() {}

	IVisContent::~IVisContent() {}

}

