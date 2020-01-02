// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LightingParser.h"
#include "LightingParserContext.h"
#include "SceneParser.h"
#include "SceneEngineUtils.h"
#include "RenderStepUtils.h"
#include "MetricsBox.h"
#include "LightingTargets.h"
#include "LightInternal.h"
#include "RenderStep.h"
#include "PreparedScene.h"

#include "RefractionsBuffer.h"
#include "Ocean.h"
#include "MetalStubs.h"

#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/DeferredShaderResource.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/ObjectFactory.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/IAnnotator.h"
#include "../RenderCore/IDevice.h"
#include "../RenderOverlays/Font.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../ConsoleRig/Console.h"

namespace SceneEngine
{
    using namespace RenderCore;

	class CompiledSceneTechnique
	{
	public:

		std::vector<std::shared_ptr<IViewDelegate>> _viewDelegates;
		std::vector<std::shared_ptr<ILightingParserPlugin>> _lightingPlugins;

		std::shared_ptr<IRenderStep> _mainSceneRenderStep;
		std::vector<std::shared_ptr<IRenderStep>> _renderSteps;

		unsigned _gbufferType;
		bool _precisionTargets;
		RenderCore::TextureSamples _sampling;

		CompiledSceneTechnique(
			const SceneTechniqueDesc& techniqueDesc,
			const std::shared_ptr<RenderCore::Techniques::PipelineAcceleratorPool>& pipelineAccelerators);
		~CompiledSceneTechnique();
	
	};

	CompiledSceneTechnique::CompiledSceneTechnique(
		const SceneTechniqueDesc& techniqueDesc,
		const std::shared_ptr<RenderCore::Techniques::PipelineAcceleratorPool>& pipelineAccelerators)
	{
		const bool enableParametersBuffer = Tweakable("EnableParametersBuffer", true);
		const bool precisionTargets = Tweakable("PrecisionTargets", false);

		// 
		//		Setup render steps
		//
		if (techniqueDesc._lightingModel == SceneTechniqueDesc::LightingModel::Deferred) {
			_mainSceneRenderStep = std::make_shared<RenderStep_GBuffer>(enableParametersBuffer?1:2, precisionTargets);
		} else if (techniqueDesc._lightingModel == SceneTechniqueDesc::LightingModel::Forward) {
			_mainSceneRenderStep = std::make_shared<RenderStep_Forward>(precisionTargets);
		} else {
			_mainSceneRenderStep = std::make_shared<RenderStep_Direct>();
		}

		_renderSteps.push_back(_mainSceneRenderStep);

		// In direct mode, we rendered directly to the presentation target, so we cannot resolve lighting or tonemap
		if (techniqueDesc._lightingModel != SceneTechniqueDesc::LightingModel::Direct) {
			if (techniqueDesc._lightingModel == SceneTechniqueDesc::LightingModel::Deferred)
				_renderSteps.push_back(std::make_shared<RenderStep_LightingResolve>(precisionTargets));
			auto resolveHDR = std::make_shared<RenderStep_ResolveHDR>();
			_renderSteps.push_back(std::make_shared<RenderStep_SampleLuminance>(resolveHDR));
			_renderSteps.push_back(resolveHDR);
		}

		// 
		//		Other configuration & state
		//
		_lightingPlugins.insert(
			_lightingPlugins.begin(),
			techniqueDesc._lightingPlugins.begin(), techniqueDesc._lightingPlugins.end());

		_gbufferType = enableParametersBuffer?1:2;
		_precisionTargets = precisionTargets;
		_sampling = techniqueDesc._sampling;
	}

	CompiledSceneTechnique::~CompiledSceneTechnique()
	{
	}

	std::shared_ptr<CompiledSceneTechnique> CreateCompiledSceneTechnique(
		const SceneTechniqueDesc& techniqueDesc,
		const std::shared_ptr<RenderCore::Techniques::PipelineAcceleratorPool>& pipelineAccelerators)
	{
		return std::make_shared<CompiledSceneTechnique>(techniqueDesc, pipelineAccelerators);
	}

    void LightingParser_InitBasicLightEnv(
        RenderCore::IThreadContext& context,
        Techniques::ParsingContext& parsingContext,
		LightingParserContext& lightingParserContext);

    static LightingParserContext LightingParser_SetupContext(
        RenderCore::IThreadContext& context, 
		RenderCore::Techniques::ParsingContext& parsingContext,
        const ILightingParserDelegate& delegate,
		const CompiledSceneTechnique& technique,
        unsigned samplingPassIndex = 0, unsigned samplingPassCount = 1);

	LightingParserContext LightingParser_ExecuteScene(
        RenderCore::IThreadContext& threadContext, 
		const RenderCore::IResourcePtr& renderTarget,
        Techniques::ParsingContext& parsingContext,
		const CompiledSceneTechnique& technique,
		const ILightingParserDelegate& lightingDelegate,
        IScene& scene,
        const RenderCore::Techniques::CameraDesc& camera)
    {
		auto targetTextureDesc = renderTarget->GetDesc()._textureDesc;

		// First, setup the views so we can do the "ExecuteScene" step
		auto mainSceneProjection = RenderCore::Techniques::BuildProjectionDesc(camera, UInt2{targetTextureDesc._width, targetTextureDesc._height});

		SceneExecuteContext executeContext;
		executeContext.AddView(
			SceneView{mainSceneProjection},
			technique._mainSceneRenderStep->CreateViewDelegate());

		for (unsigned s=0; s<lightingDelegate.GetShadowProjectionCount(); ++s) {
			auto proj = lightingDelegate.GetShadowProjectionDesc(s, mainSceneProjection);
			// todo -- what's the correct projection to give to this view?
			executeContext.AddView(
				SceneView{/*proj*/mainSceneProjection, SceneView::Type::Shadow},
				std::make_shared<ViewDelegate_Shadow>(proj));
		}

		// No, go ahead and execute the scene, which should generate a lot of Drawables (and potentially other scene preparation elements)
        CATCH_ASSETS_BEGIN
			scene.ExecuteScene(threadContext, executeContext);
		CATCH_ASSETS_END(parsingContext)

        // Throw in a "frame priority barrier" here, right after the prepare scene. This will
        // force all uploads started during PrepareScene to be completed when we next call
        // bufferUploads.Update(). Normally we want a gap between FramePriority_Barrier() and
        // Update(), because this gives some time for the background thread to run.
        GetBufferUploads().FramePriority_Barrier();

		auto lightingParserContext = LightingParser_SetupContext(threadContext, parsingContext, lightingDelegate, technique);
        LightingParser_SetGlobalTransform(threadContext, parsingContext, mainSceneProjection);

		PreparedScene preparedScene;
		lightingParserContext._preparedScene = &preparedScene;

		auto& metalContext = *Metal::DeviceContext::Get(threadContext);
		ReturnToSteadyState(metalContext);
		CATCH_ASSETS_BEGIN
			SetFrameGlobalStates(metalContext);
		CATCH_ASSETS_END(parsingContext)

		// Preparation steps (including shadows prepare)
        {
            GPUAnnotation anno(threadContext, "Prepare");
            for (auto i=lightingParserContext._plugins.cbegin(); i!=lightingParserContext._plugins.cend(); ++i) {
                CATCH_ASSETS_BEGIN
                    (*i)->OnPreScenePrepare(threadContext, parsingContext, lightingParserContext);
                CATCH_ASSETS_END(parsingContext)
            }

			auto viewDelegates = executeContext.GetViewDelegates();
			for (unsigned c=1; c<viewDelegates.size(); ++c) {
				CATCH_ASSETS_BEGIN
					auto& shadowDelegate = *checked_cast<ViewDelegate_Shadow*>(viewDelegates[c].get());
					if (shadowDelegate._general._drawables.empty())
						continue;

					if (shadowDelegate._shadowProj._resolveType == ShadowProjectionDesc::ResolveType::DepthTexture) {
						RenderStep_PrepareDMShadows renderStep(
							shadowDelegate._shadowProj._format, 
							UInt2(shadowDelegate._shadowProj._width, shadowDelegate._shadowProj._height),
							shadowDelegate._shadowProj._projections.Count());
						auto interf = renderStep.GetInterface();

						auto merged = Techniques::MergeFragments(
							{}, MakeIteratorRange(&interf, &interf+1));

						Techniques::AttachmentPool shadowsAttachmentPool;
						auto fbDesc = Techniques::BuildFrameBufferDesc(std::move(merged._mergedFragment));

						Techniques::RenderPassInstance rpi(threadContext, fbDesc, parsingContext.GetFrameBufferPool(), shadowsAttachmentPool);
						Techniques::RenderPassFragment rpf(rpi, merged._remapping[0]);
						Metal::DeviceContext::Get(threadContext)->Bind(
							Metal::ViewportDesc(0.f, 0.f, float(shadowDelegate._shadowProj._width), float(shadowDelegate._shadowProj._height)));

						renderStep._resource = shadowsAttachmentPool.GetResource(0);
						renderStep.Execute(threadContext, parsingContext, lightingParserContext, rpf, &shadowDelegate);
					} else {
						RenderStep_PrepareRTShadows renderStep;
						Techniques::RenderPassFragment rpf;
						renderStep.Execute(threadContext, parsingContext, lightingParserContext, rpf, &shadowDelegate);
					}
				CATCH_ASSETS_END(parsingContext)
			}
        }

        GetBufferUploads().Update(threadContext, true);

		std::vector<std::pair<uint64_t, RenderCore::IResourcePtr>> workingAttachments = {
			{ Techniques::AttachmentSemantics::ColorLDR, renderTarget }
		};

		parsingContext.GetNamedResources().Bind(
			RenderCore::FrameBufferProperties {
				targetTextureDesc._width, targetTextureDesc._height, 
				technique._sampling });

		auto renderStepIterator = technique._renderSteps.begin();
		while (renderStepIterator != technique._renderSteps.end()) {
			std::vector<Techniques::FrameBufferDescFragment> fragments;
			auto renderStepEnd = renderStepIterator;
			auto& firstInterface = (*renderStepEnd)->GetInterface();
			auto pipelineType = firstInterface._pipelineType;
			fragments.push_back(firstInterface);
			++renderStepEnd;
			for (; renderStepEnd != technique._renderSteps.end(); ++renderStepEnd) {
				auto& interf = (*renderStepEnd)->GetInterface();
				if (interf._pipelineType != pipelineType) break;
				fragments.push_back(interf);
			}
		
			std::vector<Techniques::PreregisteredAttachment> preregistered;
			preregistered.reserve(workingAttachments.size());
			for (const auto&w:workingAttachments)	// todo -- set "initialized / uninitialized" flags correctly
				preregistered.push_back({w.first, AsAttachmentDesc(w.second->GetDesc()), RenderCore::Techniques::PreregisteredAttachment::State::Initialized});
			auto merged = Techniques::MergeFragments(
				MakeIteratorRange(preregistered),
				MakeIteratorRange(fragments),
				UInt2(targetTextureDesc._width, targetTextureDesc._height));
			auto fbDesc = Techniques::BuildFrameBufferDesc(std::move(merged._mergedFragment));

			for (const auto&w:workingAttachments)
				parsingContext.GetNamedResources().Bind(w.first, w.second);

			Techniques::RenderPassInstance rpi;
			if (pipelineType == PipelineType::Graphics) {
				rpi = Techniques::RenderPassInstance { threadContext, fbDesc, parsingContext.GetFrameBufferPool(), parsingContext.GetNamedResources() };
				metalContext.Bind(Metal::ViewportDesc { 0.f, 0.f, float(targetTextureDesc._width), float(targetTextureDesc._height) });
			} else {
				// construct a "non-metal" render pass instance. This just handles attachment remapping logic, but doesn't create an renderpass
				// in the underlying graphics API
				rpi = Techniques::RenderPassInstance { fbDesc, parsingContext.GetNamedResources() };
			}

			for (unsigned c=0; c<std::distance(renderStepIterator, renderStepEnd); ++c) {
				CATCH_ASSETS_BEGIN
					Techniques::RenderPassFragment rpf(rpi, merged._remapping[c]);
					IViewDelegate* viewDelegate = nullptr;
					if (c==0 && renderStepIterator == technique._renderSteps.begin()) viewDelegate = executeContext.GetViewDelegates()[0].get();
					renderStepIterator[c]->Execute(threadContext, parsingContext, lightingParserContext, rpf, viewDelegate);
				CATCH_ASSETS_END(parsingContext)
			}

			// todo -- erase resources that are "exhausted" by this render pass -- ie, input but not output
			/*for (const auto&w:workingAttachments)
				parsingContext.GetNamedResources().Unbind(*w.second);*/

			// workingAttachments.clear();
			workingAttachments.reserve(merged._outputAttachments.size());
			for (const auto&o:merged._outputAttachments) {
				auto i = std::find_if(
					workingAttachments.begin(), workingAttachments.end(),
					[&o](const std::pair<uint64_t, RenderCore::IResourcePtr>& p) { return p.first == o.first; });
				if (i != workingAttachments.end()) {
					i->second = rpi.GetResource(o.second);
				} else {
					workingAttachments.push_back(std::make_pair(o.first, rpi.GetResource(o.second)));
				}
			}

			renderStepIterator = renderStepEnd;
		}

		// Bind the final outputs
		for (const auto&w:workingAttachments)
			parsingContext.GetNamedResources().Bind(w.first, w.second);

		// Bind depth to NamedResources(), so we can find it later with RenderPassToPresentationTargetWithDepthStencil()
		/*if (merged._mergedFragment._attachments[c].GetOutputSemanticBinding() == Techniques::AttachmentSemantics::MultisampleDepth)
			parsingContext.GetNamedResources().Bind(
				RenderCore::Techniques::AttachmentSemantics::MultisampleDepth,
				rpi.GetResource(c));*/

		return lightingParserContext;
    }

    void LightingParserContext::SetMetricsBox(MetricsBox* box)
    {
        _metricsBox = box;
    }

    void LightingParserContext::Reset()
    {
        _preparedDMShadows.clear();
        _preparedRTShadows.clear();
    }

	LightingParserContext::LightingParserContext() {}
	LightingParserContext::~LightingParserContext() {}
	LightingParserContext::LightingParserContext(LightingParserContext&& moveFrom)
	: _preparedDMShadows(std::move(moveFrom._preparedDMShadows))
	, _preparedRTShadows(std::move(moveFrom._preparedRTShadows))
	, _plugins(std::move(moveFrom._plugins))
	, _metricsBox(moveFrom._metricsBox)
	, _mainTargets(std::move(moveFrom._mainTargets))
	, _preparedScene(moveFrom._preparedScene)
	, _sampleCount(moveFrom._sampleCount)
	, _gbufferType(moveFrom._gbufferType)
	, _delegate(moveFrom._delegate)
	{
		moveFrom._preparedScene = nullptr;
		moveFrom._sampleCount = 0;
		moveFrom._gbufferType = 0;
		moveFrom._metricsBox = nullptr;
		moveFrom._delegate = nullptr;
	}

	LightingParserContext& LightingParserContext::operator=(LightingParserContext&& moveFrom)
	{
		_preparedDMShadows = std::move(moveFrom._preparedDMShadows);
		_preparedRTShadows = std::move(moveFrom._preparedRTShadows);
		_plugins = std::move(moveFrom._plugins);
		_mainTargets = std::move(moveFrom._mainTargets);
		_preparedScene = moveFrom._preparedScene;
		_sampleCount = moveFrom._sampleCount;
		_gbufferType = moveFrom._gbufferType;
		_metricsBox = moveFrom._metricsBox;
		_delegate = moveFrom._delegate;
		moveFrom._preparedScene = nullptr;
		moveFrom._sampleCount = 0;
		moveFrom._gbufferType = 0;
		moveFrom._metricsBox = nullptr;
		moveFrom._delegate = nullptr;
		return *this;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	static LightingParserContext LightingParser_SetupContext(
        RenderCore::IThreadContext& context, 
		RenderCore::Techniques::ParsingContext& parsingContext,
        const ILightingParserDelegate& delegate,
		const CompiledSceneTechnique& technique,
        unsigned samplingPassIndex, unsigned samplingPassCount)
    {
        struct GlobalCBuffer
        {
            float _time; unsigned _samplingPassIndex; 
            unsigned _samplingPassCount; unsigned _dummy;
        } time { delegate.GetTimeValue(), samplingPassIndex, samplingPassCount, 0 };
		auto& metalContext = *Metal::DeviceContext::Get(context);
        parsingContext.SetGlobalCB(
            metalContext, Techniques::TechniqueContext::CB_GlobalState,
            &time, sizeof(time));

		LightingParserContext lightingParserContext;
		lightingParserContext._delegate = &delegate;
		lightingParserContext._plugins.insert(
			lightingParserContext._plugins.end(),
			technique._lightingPlugins.begin(), technique._lightingPlugins.end());
        LightingParser_InitBasicLightEnv(context, parsingContext, lightingParserContext);

        auto& metricsBox = ConsoleRig::FindCachedBox2<MetricsBox>();
        metalContext.ClearUInt(metricsBox._metricsBufferUAV, { 0,0,0,0 });
        lightingParserContext.SetMetricsBox(&metricsBox);

		lightingParserContext._gbufferType = technique._gbufferType;

        return lightingParserContext;
    }

	void LightingParser_Overlays(   IThreadContext& context,
									Techniques::ParsingContext& parsingContext,
                                    LightingParserContext& lightingParserContext)
    {
        GPUAnnotation anno(context, "Overlays");

		auto metalContext = Metal::DeviceContext::Get(context);
        Metal::ViewportDesc mainViewportDesc = metalContext->GetBoundViewport();
        auto& refractionBox = ConsoleRig::FindCachedBox2<RefractionsBuffer>(unsigned(mainViewportDesc.Width/2), unsigned(mainViewportDesc.Height/2));
        refractionBox.Build(*metalContext, parsingContext, 4.f);
        MetalStubs::GetGlobalNumericUniforms(*metalContext, ShaderStage::Pixel).Bind(MakeResourceList(12, refractionBox.GetSRV()));

		auto defaultFont0 = RenderOverlays::GetX2Font("Raleway", 16);
		SceneEngine::DrawPendingResources(context, parsingContext, defaultFont0);

        for (auto i=parsingContext._pendingOverlays.cbegin(); i!=parsingContext._pendingOverlays.cend(); ++i) {
            CATCH_ASSETS_BEGIN
                (*i)(*metalContext, parsingContext);
            CATCH_ASSETS_END(parsingContext)
        }
                    
        if (Tweakable("FFTDebugging", false)) {
            FFT_DoDebugging(metalContext.get());
        }

        if (Tweakable("MetricsRender", false) && lightingParserContext.GetMetricsBox()) {
            CATCH_ASSETS_BEGIN

                using namespace RenderCore;
                using namespace RenderCore::Metal;
                auto& metricsShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                        "xleres/utility/metricsrender.vsh:main:vs_*", 
                        "xleres/utility/metricsrender.gsh:main:gs_*",
                        "xleres/utility/metricsrender.psh:main:ps_*",
                        "");
                metalContext->Bind(metricsShader);
                metalContext->GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(
                    3, ::Assets::MakeAsset<RenderCore::Techniques::DeferredShaderResource>("xleres/DefaultResources/metricsdigits.dds:T")->Actualize()->GetShaderResource()));
                metalContext->Bind(BlendState(BlendOp::Add, Blend::One, Blend::InvSrcAlpha));
                metalContext->Bind(DepthStencilState(false));
                metalContext->GetNumericUniforms(ShaderStage::Vertex).Bind(MakeResourceList(lightingParserContext.GetMetricsBox()->_metricsBufferSRV));
                unsigned dimensions[4] = { unsigned(mainViewportDesc.Width), unsigned(mainViewportDesc.Height), 0, 0 };
                metalContext->GetNumericUniforms(ShaderStage::Vertex).Bind(MakeResourceList(MakeMetalCB(dimensions, sizeof(dimensions))));
                metalContext->GetNumericUniforms(ShaderStage::Geometry).Bind(MakeResourceList(MakeMetalCB(dimensions, sizeof(dimensions))));
                SetupVertexGeneratorShader(*metalContext);
                metalContext->Bind(Topology::PointList);
                metalContext->Draw(9);

                MetalStubs::UnbindPS<ShaderResourceView>(*metalContext, 3, 1);
                MetalStubs::UnbindVS<ShaderResourceView>(*metalContext, 0, 1);

            CATCH_ASSETS_END(parsingContext)
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	auto MainTargets::GetSRV(Techniques::ParsingContext& context, uint64_t semantic, const RenderCore::TextureViewDesc& window) const -> SRV
	{
		auto& namedResources = context.GetNamedResources();
		RenderCore::FrameBufferDesc::Attachment requestAttachments[1];
		requestAttachments[0]._semantic = semantic;
		auto result = namedResources.Request(MakeIteratorRange(requestAttachments));
		if (!result.empty())
			return *namedResources.GetSRV(result[0]);
		return SRV {};
	}

	RenderCore::IResourcePtr MainTargets::GetResource(Techniques::ParsingContext& context, uint64_t semantic) const
	{
		return context.GetNamedResources().GetBoundResource(semantic);
	}

	UInt2		MainTargets::GetDimensions(Techniques::ParsingContext& context) const
	{
		auto& fbProps = context.GetNamedResources().GetFrameBufferProperties();
		return { fbProps._outputWidth, fbProps._outputHeight };
	}

	unsigned    MainTargets::GetSamplingCount(Techniques::ParsingContext& context) const
	{
		auto& fbProps = context.GetNamedResources().GetFrameBufferProperties();
		return fbProps._samples._sampleCount;
	}


	class SceneExecuteContext::Pimpl
	{
	public:
		std::vector<std::shared_ptr<IViewDelegate>> _viewDelegates;
		PreparedScene _preparedScene;
	};

	IteratorRange<const std::shared_ptr<IViewDelegate>*> SceneExecuteContext::GetViewDelegates()
	{
		return MakeIteratorRange(_pimpl->_viewDelegates);
	}

	RenderCore::Techniques::DrawablesPacket* SceneExecuteContext::GetDrawablesPacket(unsigned viewIndex, RenderCore::Techniques::BatchFilter batch)
	{
		return _pimpl->_viewDelegates[viewIndex]->GetDrawablesPacket(batch);
	}

	PreparedScene& SceneExecuteContext::GetPreparedScene()
	{
		return _pimpl->_preparedScene;
	}

	void SceneExecuteContext::AddView(
		const SceneView& view,
		const std::shared_ptr<IViewDelegate>& delegate)
	{
		_views.push_back(view);
		_pimpl->_viewDelegates.push_back(delegate);
		assert(_views.size() == _pimpl->_viewDelegates.size());
	}

	SceneExecuteContext::SceneExecuteContext()
	{
		_pimpl = std::make_unique<Pimpl>();
	}
	
	SceneExecuteContext::~SceneExecuteContext()
	{
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class BasicViewDelegate : public SceneEngine::IViewDelegate
	{
	public:
		RenderCore::Techniques::DrawablesPacket* GetDrawablesPacket(RenderCore::Techniques::BatchFilter batch)
		{
			return (batch == RenderCore::Techniques::BatchFilter::General) ? &_pkt : nullptr;
		}

		RenderCore::Techniques::DrawablesPacket _pkt;
	};

	void ExecuteSceneRaw(
		RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parserContext,
		const RenderCore::Techniques::SequencerTechnique& sequencerTechnique,
		unsigned techniqueIndex,
		const SceneView& view,
		IScene& scene)
	{
		using namespace RenderCore;
		SceneExecuteContext sceneExeContext;
		auto viewDelegate = std::make_shared<BasicViewDelegate>();
		sceneExeContext.AddView(view, viewDelegate);
		scene.ExecuteScene(threadContext, sceneExeContext);

		auto begin = viewDelegate->_pkt._drawables.begin();
		auto end = viewDelegate->_pkt._drawables.end();

		for (auto d=begin; d!=end; ++d)
			Techniques::Draw(
				threadContext, parserContext, 
				techniqueIndex,
				sequencerTechnique, 
				*(Techniques::Drawable*)d.get());
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    IScene::~IScene() {}
	ILightingParserDelegate::~ILightingParserDelegate() {}
	IViewDelegate::~IViewDelegate() {}
	std::shared_ptr<IViewDelegate> IRenderStep::CreateViewDelegate() { return nullptr; }
	IRenderStep::~IRenderStep() {}


	void ILightingParserPlugin::OnPreScenePrepare(
		RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&) const {}
    void ILightingParserPlugin::OnLightingResolvePrepare(
        RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&, 
		LightingResolveContext&) const {}
    void ILightingParserPlugin::OnPostSceneRender(
        RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&, 
		RenderCore::Techniques::BatchFilter filter, unsigned techniqueIndex) const {}
    void ILightingParserPlugin::InitBasicLightEnvironment(
        RenderCore::IThreadContext&, RenderCore::Techniques::ParsingContext&, LightingParserContext&, 
		ShaderLightDesc::BasicEnvironment& env) const {}
	ILightingParserPlugin::~ILightingParserPlugin() {}

}

            ////////////////////////////////////////////////////////////////////
                //      1. Resolve lighting
                //          -> outputs  1. postLightingResolveTexture (HDR colour)
                //                      2. depth buffer (NDC depths)
                //                      3. secondary depth buffer (contains per 
                //                          pixel/per sample stencil)
                //
            ////////............................................................
                //      2. Resolve MSAA (if needed)
                //          -> outputs  1. single sample HDR colour
                //                      2. single sample depth buffer
                //
            ////////............................................................
                //      3. Post processing operations
                //
            ////////............................................................
                //      4. Resolve HDR (tone mapping, bloom, etc)
                //          -> outputs  1. LDR SRGB colour
                //
            ////////............................................................
                //      5. Debugging / overlays
                //
            ////////////////////////////////////////////////////////////////////
