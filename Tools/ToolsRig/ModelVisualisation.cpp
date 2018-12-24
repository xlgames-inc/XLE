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
#include "../../RenderCore/IThreadContext.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/BasicDelegates.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/ObjectFactory.h"
#include "../../RenderCore/Format.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/Assets.h"
#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/Log.h"
#include "../../Math/Transformations.h"
#include "../../Utility/HeapUtils.h"
#include "../../Utility/StringFormat.h"
#include <map>

namespace ToolsRig
{
	using RenderCore::Assets::ModelScaffold;
    using RenderCore::Assets::MaterialScaffold;
    using RenderCore::Assets::SkeletonMachine;
	using RenderCore::Assets::SimpleModelRenderer;

    using FixedFunctionModel::ModelRenderer;
	using FixedFunctionModel::SharedStateSet;
    using FixedFunctionModel::ModelCache;
	using FixedFunctionModel::ModelCacheModel;
    using FixedFunctionModel::DelayedDrawCallSet;

///////////////////////////////////////////////////////////////////////////////////////////////////

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

		FixedFunctionModelSceneParser(
			const ModelVisSettings& settings,
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

		const ModelVisSettings* _settings;
		const ModelScaffold* _modelScaffold;
		DelayedDrawCallSet _delayedDrawCalls;
	};

	std::unique_ptr<SceneEngine::IScene> CreateModelScene(const ModelCacheModel& model)
    {
        ModelVisSettings settings;
        *settings._camera = AlignCameraToBoundingBox(40.f, model._boundingBox);
        return std::make_unique<FixedFunctionModelSceneParser>(
            settings,
            *model._renderer, model._boundingBox, *model._sharedStateSet);
    }

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

	class ModelSceneParser : public SceneEngine::IScene
    {
    public:
		virtual void ExecuteScene(
            RenderCore::IThreadContext& threadContext,
			SceneEngine::SceneExecuteContext& executeContext) const override
		{
			for (unsigned v=0; v<executeContext.GetViews().size(); ++v) {
				RenderCore::Techniques::DrawablesPacket* pkts[unsigned(RenderCore::Techniques::BatchFilter::Max)];
				for (unsigned c=0; c<unsigned(RenderCore::Techniques::BatchFilter::Max); ++c)
					pkts[c] = executeContext.GetDrawablesPacket(v, RenderCore::Techniques::BatchFilter(c));

				_renderer->BuildDrawables(MakeIteratorRange(pkts), Identity<Float4x4>(), _preDrawDelegate);
			}
		}

		ModelSceneParser(
            const std::shared_ptr<ModelScaffold>& modelScaffold,
			const std::shared_ptr<MaterialScaffold>& materialScaffold)
        {
			_renderer = std::make_shared<SimpleModelRenderer>(modelScaffold, materialScaffold);
			_preDrawDelegate = std::make_shared<StencilRefDelegate>();
        }

        ~ModelSceneParser() {}

    protected:
		std::shared_ptr<SimpleModelRenderer> _renderer;
		std::shared_ptr<StencilRefDelegate> _preDrawDelegate;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ModelVisLayer::Pimpl
    {
    public:
        std::shared_ptr<ModelVisSettings> _settings;
        std::shared_ptr<VisEnvSettings> _envSettings;
    };

    void ModelVisLayer::Render(
        RenderCore::IThreadContext& context,
		const RenderCore::IResourcePtr& renderTarget,
        RenderCore::Techniques::ParsingContext& parserContext)
    {
        using namespace SceneEngine;

		auto modelFuture = ::Assets::MakeAsset<ModelScaffold>(_pimpl->_settings->_modelName);
		modelFuture->StallWhilePending();
		auto model = modelFuture->TryActualize();
		auto materialFuture = ::Assets::MakeAsset<MaterialScaffold>(_pimpl->_settings->_materialName, _pimpl->_settings->_modelName);
		materialFuture->StallWhilePending();
		auto material = materialFuture->TryActualize();
		if (!model || !material)
			return;

        if (_pimpl->_settings->_pendingCameraAlignToModel) {
                // After the model is loaded, if we have a pending camera align,
                // we should reset the camera to the match the model.
                // We also need to trigger the change event after we make a change...
            *_pimpl->_settings->_camera = AlignCameraToBoundingBox(
                _pimpl->_settings->_camera->_verticalFieldOfView,
                model->GetStaticBoundingBox());
            _pimpl->_settings->_pendingCameraAlignToModel = false;
            _pimpl->_settings->_changeEvent.Trigger();
        }

        auto envSettings = _pimpl->_envSettings;
        if (!envSettings)
            envSettings = ::Assets::MakeAsset<VisEnvSettings>(
                MakeStringSection(_pimpl->_settings->_envSettingsFile))->TryActualize();
		if (!envSettings)
			envSettings = std::make_shared<VisEnvSettings>();

        ModelSceneParser sceneParser(model, material);
		VisLightingParserDelegate lightingParserDelegate(envSettings);

		std::shared_ptr<SceneEngine::ILightingParserPlugin> lightingPlugins[] = {
			std::make_shared<SceneEngine::LightingParserStandardPlugin>()
		};
        auto qualSettings = SceneEngine::RenderSceneSettings{
			SceneEngine::RenderSceneSettings::LightingModel::Deferred,
			&lightingParserDelegate,
			MakeIteratorRange(lightingPlugins)};

        auto& screenshot = Tweakable("Screenshot", 0);
        if (screenshot) {
            PlatformRig::TiledScreenshot(
                context, parserContext,
                sceneParser, AsCameraDesc(*_pimpl->_settings->_camera),
                qualSettings, UInt2(screenshot, screenshot));
            screenshot = 0;
        }

        LightingParser_ExecuteScene(
            context, renderTarget, parserContext, 
			sceneParser, AsCameraDesc(*_pimpl->_settings->_camera),
            qualSettings);
    }

    void ModelVisLayer::SetEnvironment(std::shared_ptr<VisEnvSettings> envSettings)
    {
        _pimpl->_envSettings = envSettings;
    }

    ModelVisLayer::ModelVisLayer(std::shared_ptr<ModelVisSettings> settings)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_settings = std::move(settings);
    }

    ModelVisLayer::~ModelVisLayer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class VisualisationOverlay::Pimpl
    {
    public:
        std::shared_ptr<ModelCache> _cache;
        std::shared_ptr<ModelVisSettings> _settings;
        std::shared_ptr<VisMouseOver> _mouseOver;
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
        RenderCore::IThreadContext& context, 
		const RenderCore::IResourcePtr& renderTarget,
        RenderCore::Techniques::ParsingContext& parserContext)
    {
        using namespace RenderCore;
		parserContext.GetNamedResources().Bind(RenderCore::Techniques::AttachmentSemantics::ColorLDR, renderTarget);
		if (_pimpl->_settings->_drawWireframe || !_pimpl->_settings->_drawNormals) {
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
				context, fbDesc, 
				parserContext.GetFrameBufferPool(),
				parserContext.GetNamedResources() };

			if (_pimpl->_settings->_drawWireframe) {

				CATCH_ASSETS_BEGIN
					auto model = GetModel(*_pimpl->_cache, *_pimpl->_settings);
					assert(model._renderer && model._sharedStateSet);

					FixedFunctionModel::SharedStateSet::CaptureMarker captureMarker;
					if (model._sharedStateSet)
						captureMarker = model._sharedStateSet->CaptureState(context, parserContext.GetRenderStateDelegate(), {});

					const auto techniqueIndex = Techniques::TechniqueIndex::VisWireframe;

					RenderWithEmbeddedSkeleton(
						FixedFunctionModel::ModelRendererContext(context, parserContext, techniqueIndex),
						*model._renderer, *model._sharedStateSet, model._model);
				CATCH_ASSETS_END(parserContext)
			}

			if (_pimpl->_settings->_drawNormals) {

				CATCH_ASSETS_BEGIN
					auto model = GetModel(*_pimpl->_cache, *_pimpl->_settings);
					assert(model._renderer && model._sharedStateSet);

					FixedFunctionModel::SharedStateSet::CaptureMarker captureMarker;
					if (model._sharedStateSet)
						captureMarker = model._sharedStateSet->CaptureState(context, parserContext.GetRenderStateDelegate(), {});

					const auto techniqueIndex = Techniques::TechniqueIndex::VisNormals;

					RenderWithEmbeddedSkeleton(
						FixedFunctionModel::ModelRendererContext(context, parserContext, techniqueIndex),
						*model._renderer, *model._sharedStateSet, model._model);
				CATCH_ASSETS_END(parserContext)
			}
		}

		bool doColorByMaterial = 
			(_pimpl->_settings->_colourByMaterial == 1)
			|| (_pimpl->_settings->_colourByMaterial == 2 && !_pimpl->_mouseOver->_hasMouseOver);

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
				for (unsigned c=0; c<dimof(settings._stencilToMarkerMap); c++)
					settings._stencilToMarkerMap[c] = UInt4(marker, marker, marker, marker);

                ExecuteHighlightByStencil(
                    context, parserContext, 
                    settings, _pimpl->_settings->_colourByMaterial==2);
            CATCH_ASSETS_END(parserContext)
        }
    }

    auto VisualisationOverlay::GetInputListener() -> std::shared_ptr<IInputListener>
    { return nullptr; }

    void VisualisationOverlay::SetActivationState(bool) {}

    VisualisationOverlay::VisualisationOverlay(
        std::shared_ptr<ModelVisSettings> settings,
        std::shared_ptr<ModelCache> cache,
        std::shared_ptr<VisMouseOver> mouseOver)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_settings = std::move(settings);
        _pimpl->_cache = std::move(cache);
        _pimpl->_mouseOver = std::move(mouseOver);
    }

    VisualisationOverlay::~VisualisationOverlay() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class SingleModelIntersectionResolver : public SceneEngine::IIntersectionTester
    {
    public:
        virtual Result FirstRayIntersection(
            const SceneEngine::IntersectionTestContext& context,
            std::pair<Float3, Float3> worldSpaceRay) const;

        virtual void FrustumIntersection(
            std::vector<Result>& results,
            const SceneEngine::IntersectionTestContext& context,
            const Float4x4& worldToProjection) const;

        SingleModelIntersectionResolver(
            const std::shared_ptr<ModelScaffold>& modelScaffold,
			const std::shared_ptr<MaterialScaffold>& materialScaffold,
			const std::string& modelName, const std::string& materialName);
        ~SingleModelIntersectionResolver();
    protected:
        std::shared_ptr<SimpleModelRenderer> _renderer;
		std::string _modelName, _materialName;
    };

    auto SingleModelIntersectionResolver::FirstRayIntersection(
        const SceneEngine::IntersectionTestContext& context,
        std::pair<Float3, Float3> worldSpaceRay) const -> Result
    {
        using namespace SceneEngine;
		using namespace RenderCore;

		Techniques::DrawablesPacket pkt;
		Techniques::DrawablesPacket* pkts[] = { &pkt };
		_renderer->BuildDrawables(MakeIteratorRange(pkts), Identity<Float4x4>());

		Techniques::ParsingContext parserContext{context.GetTechniqueContext()};
		auto cam = context.GetCameraDesc();
        ModelIntersectionStateContext stateContext(
            ModelIntersectionStateContext::RayTest,
            *context.GetThreadContext(), parserContext, 
            &cam);
        stateContext.SetRay(worldSpaceRay);

		Techniques::SequencerTechnique sequencer;
		sequencer._techniqueDelegate = SceneEngine::CreateRayTestTechniqueDelegate();
		sequencer._materialDelegate = std::make_shared<Techniques::MaterialDelegate_Basic>();
		// sequencer._renderStateDelegate = parserContext.GetRenderStateDelegate();

		auto& techUSI = Techniques::TechniqueContext::GetGlobalUniformsStreamInterface();
		for (unsigned c=0; c<techUSI._cbBindings.size(); ++c)
			sequencer._sequencerUniforms.emplace_back(std::make_pair(techUSI._cbBindings[c]._hashName, std::make_shared<Techniques::GlobalCBDelegate>(c)));

		for (auto d=pkt._drawables.begin(); d!=pkt._drawables.end(); ++d)
			Techniques::Draw(
				*context.GetThreadContext(), parserContext, 
				Techniques::TechniqueIndex::DepthOnly,
				sequencer, 
				*(Techniques::Drawable*)d.get());

        auto results = stateContext.GetResults();
        if (!results.empty()) {
            const auto& r = results[0];

            Result result;
            result._type = IntersectionTestScene::Type::Extra;
            result._worldSpaceCollision = 
                worldSpaceRay.first + r._intersectionDepth * Normalize(worldSpaceRay.second - worldSpaceRay.first);
            result._distance = r._intersectionDepth;
            result._drawCallIndex = r._drawCallIndex;
            result._materialGuid = r._materialGuid;
            result._materialName = _materialName;
            result._modelName = _modelName;

            return result;
        }

        return Result();
    }

    void SingleModelIntersectionResolver::FrustumIntersection(
        std::vector<Result>& results,
        const SceneEngine::IntersectionTestContext& context,
        const Float4x4& worldToProjection) const
    {}

    SingleModelIntersectionResolver::SingleModelIntersectionResolver(
        const std::shared_ptr<ModelScaffold>& modelScaffold,
		const std::shared_ptr<MaterialScaffold>& materialScaffold,
		const std::string& modelName, const std::string& materialName)
    {
		_renderer = std::make_shared<SimpleModelRenderer>(modelScaffold, materialScaffold);
		_modelName = modelName;
		_materialName = materialName;
	}

    SingleModelIntersectionResolver::~SingleModelIntersectionResolver()
    {}

    std::shared_ptr<SceneEngine::IntersectionTestScene> CreateModelIntersectionScene(
		StringSection<> modelName, StringSection<> materialName)
    {
		auto modelFuture = ::Assets::MakeAsset<ModelScaffold>(modelName);
		modelFuture->StallWhilePending();
		auto model = modelFuture->TryActualize();
		auto materialFuture = ::Assets::MakeAsset<MaterialScaffold>(materialName, modelName);
		materialFuture->StallWhilePending();
		auto material = materialFuture->TryActualize();
		if (!model || !material)
			return nullptr;

        auto resolver = std::make_shared<SingleModelIntersectionResolver>(model, material, modelName.AsString(), materialName.AsString());
        return std::shared_ptr<SceneEngine::IntersectionTestScene>(
            new SceneEngine::IntersectionTestScene(nullptr, nullptr, nullptr, { resolver }));
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class MouseOverTrackingListener : public RenderOverlays::DebuggingDisplay::IInputListener
    {
    public:
        bool OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
        {
            using namespace SceneEngine;

            auto cam = AsCameraDesc(*_camera);
            IntersectionTestContext testContext(
                _threadContext, cam, 
                std::make_shared<RenderCore::PresentationChainDesc>(
					PlatformRig::InputTranslator::s_hackWindowSize[0],
					PlatformRig::InputTranslator::s_hackWindowSize[1]),
                _techniqueContext);
            auto worldSpaceRay = testContext.CalculateWorldSpaceRay(
                evnt._mousePosition);
                
            auto intr = _scene->FirstRayIntersection(testContext, worldSpaceRay);
            if (intr._type != 0) {
                if (        intr._drawCallIndex != _mouseOver->_drawCallIndex
                        ||  intr._materialGuid != _mouseOver->_materialGuid
                        ||  !_mouseOver->_hasMouseOver) {

                    _mouseOver->_hasMouseOver = true;
                    _mouseOver->_drawCallIndex = intr._drawCallIndex;
                    _mouseOver->_materialGuid = intr._materialGuid;
                    _mouseOver->_changeEvent.Trigger();
                }
            } else {
                if (_mouseOver->_hasMouseOver) {
                    _mouseOver->_hasMouseOver = false;
                    _mouseOver->_changeEvent.Trigger();
                }
            }

            return false;
        }

        MouseOverTrackingListener(
            std::shared_ptr<VisMouseOver> mouseOver,
            std::shared_ptr<RenderCore::IThreadContext> threadContext,
            std::shared_ptr<RenderCore::Techniques::TechniqueContext> techniqueContext,
            std::shared_ptr<VisCameraSettings> camera,
            std::shared_ptr<SceneEngine::IntersectionTestScene> scene)
            : _mouseOver(std::move(mouseOver))
            , _threadContext(std::move(threadContext))
            , _techniqueContext(std::move(techniqueContext))
            , _camera(std::move(camera))
            , _scene(std::move(scene))
        {}
        MouseOverTrackingListener::~MouseOverTrackingListener() {}

    protected:
        std::shared_ptr<VisMouseOver> _mouseOver;
        std::shared_ptr<RenderCore::IThreadContext> _threadContext;
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> _techniqueContext;
        std::shared_ptr<VisCameraSettings> _camera;
        std::shared_ptr<SceneEngine::IntersectionTestScene> _scene;
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

        using namespace RenderOverlays::DebuggingDisplay;
        RenderOverlays::ImmediateOverlayContext overlays(
            threadContext, &parsingContext.GetNamedResources(),
            parsingContext.GetProjectionDesc());
        _overlayFn(overlays, *_mouseOver);
    }

    MouseOverTrackingOverlay::MouseOverTrackingOverlay(
        std::shared_ptr<VisMouseOver> mouseOver,
        std::shared_ptr<RenderCore::IThreadContext> threadContext,
        std::shared_ptr<RenderCore::Techniques::TechniqueContext> techniqueContext,
        std::shared_ptr<VisCameraSettings> camera,
        std::shared_ptr<SceneEngine::IntersectionTestScene> scene,
        OverlayFn&& overlayFn)
    : _overlayFn(std::move(overlayFn))
    {
        _mouseOver = mouseOver;
        _inputListener = std::make_shared<MouseOverTrackingListener>(
            std::move(mouseOver),
            std::move(threadContext), std::move(techniqueContext), 
            std::move(camera), std::move(scene));
    }

    MouseOverTrackingOverlay::~MouseOverTrackingOverlay() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void ChangeEvent::Trigger() 
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
        _envSettingsFile = "defaultenv.txt:environment";
        _levelOfDetail = 0;
        _pendingCameraAlignToModel = true;
        _doHighlightWireframe = false;
        _highlightRay = std::make_pair(Zero<Float3>(), Zero<Float3>());
        _highlightRayWidth = 0.f;
        _colourByMaterial = 0;
        _camera = std::make_shared<VisCameraSettings>();
        _drawNormals = false;
        _drawWireframe = false;
    }

}

