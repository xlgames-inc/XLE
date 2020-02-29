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
#include "../../RenderCore/Techniques/TechniqueDelegates.h"
#include "../../RenderCore/Techniques/PipelineAccelerator.h"
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
#include <iomanip>

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
        result._farClip = 5.25f * Magnitude(result._focus - result._position);
        result._nearClip = result._farClip / 10000.f;
        return result;
    }

	VisEnvSettings::VisEnvSettings() : _envConfigFile("defaultenv.txt:environment"), _lightingType(LightingType::Deferred) {}
	VisEnvSettings::VisEnvSettings(const std::string& envConfigFile) : _envConfigFile(envConfigFile) {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class SimpleSceneLayer::Pimpl
    {
    public:
		std::shared_ptr<SceneEngine::IScene> _scene;

		std::shared_ptr<PlatformRig::EnvironmentSettings> _envSettings;
		::Assets::FuturePtr<PlatformRig::EnvironmentSettings> _envSettingsFuture;
		
		std::string _envSettingsErrorMessage;
		unsigned _loadingIndicatorCounter = 0;

		bool _preparingScene = false;
		bool _preparingEnvSettings = false;
		bool _preparingPipelineAccelerators = false;

		std::shared_ptr<VisCameraSettings> _camera;

		std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool> _pipelineAccelerators;
		std::shared_ptr<::Assets::IAsyncMarker> _pendingPipelines;

		std::vector<std::shared_ptr<SceneEngine::ILightingParserPlugin>> _lightingPlugins;
		std::vector<std::shared_ptr<SceneEngine::IRenderStep>> _renderSteps;
    };

	static ::Assets::AssetState GetAsyncSceneState(SceneEngine::IScene& scene)
	{
		auto* asyncScene = dynamic_cast<::Assets::IAsyncMarker*>(&scene);
		if (asyncScene)
			return asyncScene->GetAssetState();
		return ::Assets::AssetState::Ready;
	}

	static void DrawDiamond(RenderOverlays::IOverlayContext* context, const RenderOverlays::DebuggingDisplay::Rect& rect, RenderOverlays::ColorB colour)
    {
        if (rect._bottomRight[0] <= rect._topLeft[0] || rect._bottomRight[1] <= rect._topLeft[1]) {
            return;
        }

		using namespace RenderOverlays;
		using namespace RenderOverlays::DebuggingDisplay;
        context->DrawTriangle(
            ProjectionMode::P2D, 
            AsPixelCoords(Coord2(rect._bottomRight[0],								0.5f * (rect._topLeft[1] + rect._bottomRight[1]))), colour,
            AsPixelCoords(Coord2(0.5f * (rect._topLeft[0] + rect._bottomRight[0]),	rect._topLeft[1])), colour,
            AsPixelCoords(Coord2(rect._topLeft[0],									0.5f * (rect._topLeft[1] + rect._bottomRight[1]))), colour);

        context->DrawTriangle(
            ProjectionMode::P2D, 
            AsPixelCoords(Coord2(rect._topLeft[0],									0.5f * (rect._topLeft[1] + rect._bottomRight[1]))), colour,
            AsPixelCoords(Coord2(0.5f * (rect._topLeft[0] + rect._bottomRight[0]),	rect._bottomRight[1])), colour,
            AsPixelCoords(Coord2(rect._bottomRight[0],								0.5f * (rect._topLeft[1] + rect._bottomRight[1]))), colour);
    }

	static void RenderLoadingIndicator(
		RenderOverlays::IOverlayContext& context,
		const RenderOverlays::DebuggingDisplay::Rect& viewport,
		unsigned animationCounter)
    {
        using namespace RenderOverlays::DebuggingDisplay;

		const unsigned indicatorWidth = 80;
		const unsigned indicatorHeight = 120;
		RenderOverlays::DebuggingDisplay::Rect outerRect;
		outerRect._topLeft[0] = std::max(viewport._topLeft[0]+12u, viewport._bottomRight[0]-indicatorWidth-12u);
		outerRect._topLeft[1] = std::max(viewport._topLeft[1]+12u, viewport._bottomRight[1]-indicatorHeight-12u);
		outerRect._bottomRight[0] = viewport._bottomRight[0]-12u;
		outerRect._bottomRight[1] = viewport._bottomRight[1]-12u;

		Float2 center {
			(outerRect._bottomRight[0] + outerRect._topLeft[0]) / 2.0f,
			(outerRect._bottomRight[1] + outerRect._topLeft[1]) / 2.0f };

		const unsigned cycleCount = 1080;
		// there are always 3 diamonds, distributed evenly throughout the animation....
		unsigned oldestIdx = (unsigned)std::ceil(animationCounter / float(cycleCount/3));
		int oldestStartPoint = -int(animationCounter % (cycleCount/3));
		float phase = -oldestStartPoint / float(cycleCount/3);
		for (unsigned c=0; c<3; ++c) {
			unsigned idx = oldestIdx+c;

			float a = (phase + (2-c)) / 3.0f;
			float a2 = std::fmodf(idx / 10.f, 1.0f);
			a2 = 0.5f + 0.5f * a2;

			Rect r;
			r._topLeft[0] = unsigned(center[0] - a * 0.5f * (outerRect._bottomRight[0] - outerRect._topLeft[0]));
			r._topLeft[1] = unsigned(center[1] - a * 0.5f * (outerRect._bottomRight[1] - outerRect._topLeft[1]));
			r._bottomRight[0] = unsigned(center[0] + a * 0.5f * (outerRect._bottomRight[0] - outerRect._topLeft[0]));
			r._bottomRight[1] = unsigned(center[1] + a * 0.5f * (outerRect._bottomRight[1] - outerRect._topLeft[1]));

			using namespace RenderOverlays::DebuggingDisplay;
			float fadeOff = std::min((1.0f - a) * 10.f, 1.0f);
			DrawDiamond(&context, r, RenderOverlays::ColorB { uint8_t(0xff * fadeOff * a2), uint8_t(0xff * fadeOff * a2), uint8_t(0xff * fadeOff * a2), 0xff });
		}
	}

    void SimpleSceneLayer::Render(
        RenderCore::IThreadContext& threadContext,
		const RenderCore::IResourcePtr& renderTarget,
        RenderCore::Techniques::ParsingContext& parserContext)
    {
        using namespace SceneEngine;

		if (_pimpl->_preparingEnvSettings) {
			auto newActualized = _pimpl->_envSettingsFuture->TryActualize();
			if (newActualized) {
				_pimpl->_envSettings = newActualized;
				_pimpl->_envSettingsFuture = nullptr;
				_pimpl->_envSettingsErrorMessage = {};
				_pimpl->_preparingEnvSettings = false;
			} else if (_pimpl->_envSettingsFuture->GetAssetState() == ::Assets::AssetState::Invalid) {
				_pimpl->_envSettingsErrorMessage = ::Assets::AsString(_pimpl->_envSettingsFuture->GetActualizationLog());
				_pimpl->_envSettings = nullptr;
				_pimpl->_envSettingsFuture = nullptr;
				_pimpl->_preparingEnvSettings = false;
			}
		}

		auto compiledTechnique = SceneEngine::CreateCompiledSceneTechnique(
			{
				MakeIteratorRange(_pimpl->_renderSteps),
				MakeIteratorRange(_pimpl->_lightingPlugins)
			},
			_pimpl->_pipelineAccelerators,
			RenderCore::AsAttachmentDesc(renderTarget->GetDesc()));

		PlatformRig::BasicLightingParserDelegate lightingParserDelegate(_pimpl->_envSettings);

		static float time = 0.f;
		time += 1.0f / 60.f;
		lightingParserDelegate.SetTimeValue(time);

		if (_pimpl->_preparingScene && !_pimpl->_preparingEnvSettings) {
			auto stillPending = GetAsyncSceneState(*_pimpl->_scene) == ::Assets::AssetState::Pending;
			if (!stillPending) {
				_pimpl->_preparingScene = false;
				ResetCamera();

				_pimpl->_preparingPipelineAccelerators = true;
				_pimpl->_pendingPipelines = SceneEngine::PreparePipelines(
					threadContext, *compiledTechnique, lightingParserDelegate, *_pimpl->_scene);
			}
		}

		if (_pimpl->_preparingPipelineAccelerators) {
			if (!_pimpl->_pendingPipelines || _pimpl->_pendingPipelines->GetAssetState() != ::Assets::AssetState::Pending) {
				_pimpl->_preparingPipelineAccelerators = false;
				_pimpl->_pendingPipelines.reset();
			}
		}

		if (!_pimpl->_preparingEnvSettings && !_pimpl->_preparingScene && !_pimpl->_preparingPipelineAccelerators) {
			auto& screenshot = Tweakable("Screenshot", 0);
			if (screenshot) {
				PlatformRig::TiledScreenshot(
					threadContext, parserContext,
					*_pimpl->_scene, AsCameraDesc(*_pimpl->_camera),
					*compiledTechnique, UInt2(screenshot, screenshot));
				screenshot = 0;
			}

			auto lightingParserContext = LightingParser_ExecuteScene(
				threadContext, renderTarget, parserContext, 
				*compiledTechnique, lightingParserDelegate,
				*_pimpl->_scene, AsCameraDesc(*_pimpl->_camera));

			// Draw debugging overlays -- 
			{
				auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, renderTarget, parserContext);
				SceneEngine::LightingParser_Overlays(threadContext, parserContext, lightingParserContext);
			}
		} else {
			// Draw a loading indicator, 
			auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, renderTarget, parserContext, RenderCore::LoadStore::Clear);
			using namespace RenderOverlays::DebuggingDisplay;
			RenderOverlays::ImmediateOverlayContext overlays(
				threadContext, &parserContext.GetNamedResources(),
				parserContext.GetProjectionDesc());
			overlays.CaptureState();
			auto viewportDims = threadContext.GetStateDesc()._viewportDimensions;
			Rect rect { Coord2{0, 0}, Coord2(viewportDims[0], viewportDims[1]) };
			RenderLoadingIndicator(overlays, rect, _pimpl->_loadingIndicatorCounter++);
			overlays.ReleaseState();
		}

		if (!_pimpl->_envSettingsErrorMessage.empty()) {
			auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, renderTarget, parserContext);
			if (!_pimpl->_envSettingsErrorMessage.empty())
				SceneEngine::DrawString(threadContext, RenderOverlays::GetDefaultFont(), _pimpl->_envSettingsErrorMessage);
		}
    }

    void SimpleSceneLayer::Set(const VisEnvSettings& envSettings)
    {
		_pimpl->_envSettingsFuture = std::make_shared<::Assets::AssetFuture<PlatformRig::EnvironmentSettings>>("VisualizationEnvironment");
		::Assets::AutoConstructToFuture(*_pimpl->_envSettingsFuture, envSettings._envConfigFile);
		_pimpl->_preparingEnvSettings = true;
    }

	void SimpleSceneLayer::Set(const std::shared_ptr<SceneEngine::IScene>& scene)
	{
		_pimpl->_scene = scene;
		_pimpl->_preparingScene = true;
	}

	const std::shared_ptr<VisCameraSettings>& SimpleSceneLayer::GetCamera()
	{
		return _pimpl->_camera;
	}

	void SimpleSceneLayer::ResetCamera()
	{
		auto* scene = dynamic_cast<ToolsRig::IVisContent*>(_pimpl->_scene.get());
		if (scene) {
			auto boundingBox = scene->GetBoundingBox();
			*_pimpl->_camera = ToolsRig::AlignCameraToBoundingBox(_pimpl->_camera->_verticalFieldOfView, boundingBox);
		}
	}

	auto SimpleSceneLayer::GetOverlayState() const -> OverlayState
	{
		RefreshMode refreshMode = RefreshMode::EventBased;

		if (!_pimpl->_envSettings && _pimpl->_envSettingsFuture->GetAssetState() == ::Assets::AssetState::Pending)
			return { RefreshMode::RegularAnimation };

		if (_pimpl->_preparingEnvSettings || _pimpl->_preparingScene || _pimpl->_preparingPipelineAccelerators)
			return { RefreshMode::RegularAnimation };

		// Need regular updates if the scene future hasn't been fully loaded yet
		// Or if there's active animation playing in the scene
		if (_pimpl->_scene) {
			if (GetAsyncSceneState(*_pimpl->_scene) == ::Assets::AssetState::Pending) { 	
				refreshMode = RefreshMode::RegularAnimation;
			} else {
				auto* visContext = dynamic_cast<IVisContent*>(_pimpl->_scene.get());
				if (visContext && visContext->HasActiveAnimation())
					refreshMode = RefreshMode::RegularAnimation;
			}
		}
		
		return { refreshMode };
	}
	
    SimpleSceneLayer::SimpleSceneLayer(const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAccelerators)
    {
        _pimpl = std::make_unique<Pimpl>();
		_pimpl->_camera = std::make_shared<VisCameraSettings>();
		_pimpl->_pipelineAccelerators = pipelineAccelerators;

		_pimpl->_lightingPlugins.push_back(std::make_shared<SceneEngine::LightingParserStandardPlugin>());
		_pimpl->_renderSteps = SceneEngine::CreateStandardRenderSteps(SceneEngine::LightingModel::Deferred);
    }

    SimpleSceneLayer::~SimpleSceneLayer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	static SceneEngine::LightingModel AsLightingModel(VisEnvSettings::LightingType lightingType)
	{
		switch (lightingType) {
		case VisEnvSettings::LightingType::Deferred:
			return SceneEngine::LightingModel::Deferred;
		case VisEnvSettings::LightingType::Forward:
			return SceneEngine::LightingModel::Forward;
		default:
		case VisEnvSettings::LightingType::Direct:
			return SceneEngine::LightingModel::Direct;
		}
	}

	std::pair<DrawPreviewResult, std::string> DrawPreview(
        RenderCore::IThreadContext& context,
		const RenderCore::IResourcePtr& renderTarget,
        RenderCore::Techniques::ParsingContext& parserContext,
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		VisCameraSettings& cameraSettings,
		VisEnvSettings& envSettings,
		SceneEngine::IScene& scene,
		const std::shared_ptr<SceneEngine::IRenderStep>& renderStep)
    {
		try
        {
			auto future = ::Assets::MakeAsset<PlatformRig::EnvironmentSettings>(envSettings._envConfigFile);
			future->StallWhilePending();
			PlatformRig::BasicLightingParserDelegate lightingParserDelegate(future->Actualize());

			auto renderSteps = SceneEngine::CreateStandardRenderSteps(AsLightingModel(envSettings._lightingType));
			if (renderStep) {		// if we've got a custom render step, override the default
				if (renderSteps.size() > 0) {
					renderSteps[0] = renderStep;
				} else {
					renderSteps.push_back(renderStep);
				}
			}

			std::shared_ptr<SceneEngine::ILightingParserPlugin> lightingPlugins[] = {
				std::make_shared<SceneEngine::LightingParserStandardPlugin>()
			};
			SceneEngine::SceneTechniqueDesc techniqueDesc{
				MakeIteratorRange(renderSteps),
				MakeIteratorRange(lightingPlugins)};

			auto compiledTechnique = CreateCompiledSceneTechnique(
				techniqueDesc,
				pipelineAccelerators,
				RenderCore::AsAttachmentDesc(renderTarget->GetDesc()));

			SceneEngine::LightingParser_ExecuteScene(
				context, renderTarget, parserContext,
				*compiledTechnique,
				lightingParserDelegate,
				scene, AsCameraDesc(cameraSettings));

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

	class StencilRefDelegate : public RenderCore::Techniques::SimpleModelRenderer::IPreDrawDelegate
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

		std::shared_ptr<RenderCore::Techniques::SimpleModelRenderer::IPreDrawDelegate> _stencilPrimeDelegate;

		Pimpl()
		{
			_stencilPrimeDelegate = std::make_shared<StencilRefDelegate>();
		}
    };

	static void RenderTrackingOverlay(
        RenderOverlays::IOverlayContext& context,
		const RenderOverlays::DebuggingDisplay::Rect& viewport,
		const ToolsRig::VisMouseOver& mouseOver, 
		const SceneEngine::IScene& scene)
    {
        using namespace RenderOverlays::DebuggingDisplay;

        auto textHeight = (int)RenderOverlays::GetDefaultFont()->GetFontProperties()._lineHeight;
        std::string matName;
		auto* visContent = dynamic_cast<const ToolsRig::IVisContent*>(&scene);
		if (visContent)
			matName = visContent->GetDrawCallDetails(mouseOver._drawCallIndex, mouseOver._materialGuid)._materialName;
        DrawText(
            &context,
            Rect(Coord2(viewport._topLeft[0]+3, viewport._bottomRight[1]-textHeight-8), Coord2(viewport._bottomRight[0]-6, viewport._bottomRight[1]-8)),
            nullptr, RenderOverlays::ColorB(0xffafafaf),
            StringMeld<512>() 
                << "Material: {Color:7f3faf}" << matName
                << "{Color:afafaf}, Draw call: " << mouseOver._drawCallIndex
                << std::setprecision(4)
                << ", (" << mouseOver._intersectionPt[0]
                << ", "  << mouseOver._intersectionPt[1]
                << ", "  << mouseOver._intersectionPt[2]
                << ")");
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

		if (!_pimpl->_scene || !_pimpl->_cameraSettings || GetAsyncSceneState(*_pimpl->_scene) == ::Assets::AssetState::Pending) return;

		RenderCore::Techniques::SequencerContext sequencerTechnique;

		auto& techUSI = RenderCore::Techniques::TechniqueContext::GetGlobalUniformsStreamInterface();
		for (unsigned c=0; c<techUSI._cbBindings.size(); ++c)
			sequencerTechnique._sequencerUniforms.emplace_back(std::make_pair(techUSI._cbBindings[c]._hashName, std::make_shared<RenderCore::Techniques::GlobalCBDelegate>(c)));

		auto cam = AsCameraDesc(*_pimpl->_cameraSettings);
		SceneEngine::SceneView sceneView {
			SceneEngine::SceneView::Type::Normal,
			Techniques::BuildProjectionDesc(cam, UInt2(threadContext.GetStateDesc()._viewportDimensions[0], threadContext.GetStateDesc()._viewportDimensions[1])),
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

			static auto visWireframeDelegate =
				RenderCore::Techniques::CreateTechniqueDelegateLegacy(
					Techniques::TechniqueIndex::VisWireframe, {}, {}, {});
			static auto visNormals =
				RenderCore::Techniques::CreateTechniqueDelegateLegacy(
					Techniques::TechniqueIndex::VisNormals, {}, {}, {});

			DepthStencilDesc ds;
			ds._stencilEnable = true;
			ds._stencilWriteMask = 0xff;
			ds._frontFaceStencil = StencilDesc::AlwaysWrite;
			static auto primeStencilBuffer =
				RenderCore::Techniques::CreateTechniqueDelegateLegacy(
					Techniques::TechniqueIndex::DepthOnly, {}, {}, ds);

			if (_pimpl->_settings._drawWireframe) {
				CATCH_ASSETS_BEGIN
					auto sequencerConfig = parserContext._pipelineAcceleratorPool->CreateSequencerConfig(visWireframeDelegate, ParameterBox{}, fbDesc);
					sequencerTechnique._sequencerConfig = sequencerConfig.get();
					SceneEngine::ExecuteSceneRaw(
						threadContext, parserContext, 
						sequencerTechnique,
						sceneView,
						*_pimpl->_scene);
				CATCH_ASSETS_END(parserContext)
			}

			if (_pimpl->_settings._drawNormals) {
				CATCH_ASSETS_BEGIN
					auto sequencerConfig = parserContext._pipelineAcceleratorPool->CreateSequencerConfig(visNormals, ParameterBox{}, fbDesc);
					sequencerTechnique._sequencerConfig = sequencerConfig.get();
					SceneEngine::ExecuteSceneRaw(
						threadContext, parserContext, 
						sequencerTechnique,
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
				std::shared_ptr<RenderCore::Techniques::SimpleModelRenderer::IPreDrawDelegate> oldDelegate;
				if (visContent)
					oldDelegate = visContent->SetPreDrawDelegate(_pimpl->_stencilPrimeDelegate);
				CATCH_ASSETS_BEGIN
					// Prime the stencil buffer with draw call indices
					auto sequencerCfg = parserContext._pipelineAcceleratorPool->CreateSequencerConfig(primeStencilBuffer, ParameterBox{}, fbDesc);
					sequencerTechnique._sequencerConfig = sequencerCfg.get();
					SceneEngine::ExecuteSceneRaw(
						threadContext, parserContext, 
						sequencerTechnique,
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

		bool writeMaterialName = 
			(_pimpl->_settings._colourByMaterial == 2 && _pimpl->_mouseOver->_hasMouseOver);

		if (writeMaterialName) {

			CATCH_ASSETS_BEGIN

				{
					auto rpi = RenderCore::Techniques::RenderPassToPresentationTarget(threadContext, renderTarget, parserContext);
					using namespace RenderOverlays::DebuggingDisplay;
					RenderOverlays::ImmediateOverlayContext overlays(
						threadContext, &parserContext.GetNamedResources(),
						parserContext.GetProjectionDesc());
					overlays.CaptureState();
					auto viewportDims = threadContext.GetStateDesc()._viewportDimensions;
					Rect rect { Coord2{0, 0}, Coord2(viewportDims[0], viewportDims[1]) };
					RenderTrackingOverlay(overlays, rect, *_pimpl->_mouseOver, *_pimpl->_scene);
					overlays.ReleaseState();
				}

			CATCH_ASSETS_END(parserContext)
		}
    }

	void VisualisationOverlay::Set(const std::shared_ptr<SceneEngine::IScene>& scene)
	{
		_pimpl->_scene = scene;

		if (_pimpl->_animState) {
			auto* visContext = dynamic_cast<IVisContent*>(_pimpl->_scene.get());
			if (visContext)
				visContext->BindAnimationState(_pimpl->_animState);
		}
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
		if (_pimpl->_scene) {
			if (GetAsyncSceneState(*_pimpl->_scene) == ::Assets::AssetState::Pending) { 	
				refreshMode = RefreshMode::RegularAnimation;
			} else {
				auto* visContext = dynamic_cast<IVisContent*>(_pimpl->_scene.get());
				if (visContext && visContext->HasActiveAnimation())
					refreshMode = RefreshMode::RegularAnimation;
			}
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
		RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
        std::pair<Float3, Float3> worldSpaceRay,
		SceneEngine::IScene& scene)
	{
		using namespace RenderCore;

		Techniques::ParsingContext parserContext { techniqueContext };
		parserContext._pipelineAcceleratorPool = &pipelineAccelerators;
		
		SceneEngine::ModelIntersectionStateContext stateContext {
            SceneEngine::ModelIntersectionStateContext::RayTest,
            threadContext, parserContext };
        stateContext.SetRay(worldSpaceRay);
		
		CATCH_ASSETS_BEGIN
		
			auto sequencerTechnique = stateContext.MakeRayTestSequencerTechnique();
			SceneEngine::ExecuteSceneRaw(
				threadContext, parserContext, 
				sequencerTechnique,
				{SceneEngine::SceneView::Type::Other},
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

            if (_scene) {
				auto intr = FirstRayIntersection(*RenderCore::Techniques::GetThreadContext(), *_techniqueContext, *_pipelineAccelerators, worldSpaceRay, *_scene);
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

		void Set(const std::shared_ptr<SceneEngine::IScene>& scene) { _scene = scene; }
		const std::shared_ptr<SceneEngine::IScene>& GetScene() { return _scene; }

        MouseOverTrackingListener(
            const std::shared_ptr<VisMouseOver>& mouseOver,
            const std::shared_ptr<RenderCore::Techniques::TechniqueContext>& techniqueContext,
			const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
            const std::shared_ptr<VisCameraSettings>& camera)
        : _mouseOver(mouseOver)
        , _techniqueContext(techniqueContext)
		, _pipelineAccelerators(pipelineAccelerators)
        , _camera(camera)
        {}
        MouseOverTrackingListener::~MouseOverTrackingListener() {}

    protected:
        std::shared_ptr<VisMouseOver> _mouseOver;
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> _techniqueContext;
		std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool> _pipelineAccelerators;
        std::shared_ptr<VisCameraSettings> _camera;
        
        std::shared_ptr<SceneEngine::IScene> _scene;
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
    }

	void MouseOverTrackingOverlay::Set(const std::shared_ptr<SceneEngine::IScene>& scene)
	{
		_inputListener->Set(scene);
	}

    MouseOverTrackingOverlay::MouseOverTrackingOverlay(
        const std::shared_ptr<VisMouseOver>& mouseOver,
        const std::shared_ptr<RenderCore::Techniques::TechniqueContext>& techniqueContext,
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
        const std::shared_ptr<VisCameraSettings>& camera)
    {
        _mouseOver = mouseOver;
        _inputListener = std::make_shared<MouseOverTrackingListener>(
            mouseOver,
            techniqueContext, 
			pipelineAccelerators,
            camera);
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

	void StallWhilePending(SceneEngine::IScene& scene)
	{
		auto* marker = dynamic_cast<::Assets::IAsyncMarker*>(&scene);
		if (marker)
			marker->StallWhilePending();
	}
	
	/*const std::shared_ptr<SceneEngine::IScene>& TryActualize(const ::Assets::AssetFuture<SceneEngine::IScene>& future)
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
	}*/

    void ChangeEvent::Invoke() 
    {
        for (auto i=_callbacks.begin(); i!=_callbacks.end(); ++i) {
            (*i)->OnChange();
        }
    }
    ChangeEvent::~ChangeEvent() {}

	IVisContent::~IVisContent() {}

}

