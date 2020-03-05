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
#include "RenderStep_PrepareShadows.h"
#include "RenderStep_ResolveHDR.h"
#include "PreparedScene.h"

#include "RefractionsBuffer.h"
#include "Ocean.h"
#include "MetalStubs.h"

#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/DeferredShaderResource.h"
#include "../RenderCore/Techniques/PipelineAccelerator.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/ObjectFactory.h"
#include "../RenderCore/IThreadContext.h"
#include "../RenderCore/IAnnotator.h"
#include "../RenderCore/IDevice.h"
#include "../RenderOverlays/Font.h"
#include "../Assets/Assets.h"
#include "../Assets/AsyncMarkerGroup.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../ConsoleRig/Console.h"
#include "../xleres/FileList.h"

#include <set>

namespace SceneEngine
{
    using namespace RenderCore;

	std::vector<std::shared_ptr<IRenderStep>> CreateStandardRenderSteps(LightingModel lightingModel)
	{
		std::vector<std::shared_ptr<IRenderStep>> result;

		const bool enableParametersBuffer = Tweakable("EnableParametersBuffer", true);
		const bool precisionTargets = Tweakable("PrecisionTargets", false);
		unsigned gbufferType = enableParametersBuffer?1:2;

		std::shared_ptr<IRenderStep> mainSceneRenderStep;
		if (lightingModel == LightingModel::Deferred) {
			mainSceneRenderStep = CreateRenderStep_GBuffer(gbufferType, precisionTargets);
		} else if (lightingModel == LightingModel::Forward) {
			mainSceneRenderStep = CreateRenderStep_Forward(precisionTargets);
		} else {
			mainSceneRenderStep = CreateRenderStep_Direct();
		}

		result.push_back(mainSceneRenderStep);

		// In direct mode, we rendered directly to the presentation target, so we cannot resolve lighting or tonemap
		if (lightingModel != LightingModel::Direct) {
			if (lightingModel == LightingModel::Deferred)
				result.push_back(CreateRenderStep_LightingResolve(gbufferType, precisionTargets));
			auto resolveHDR = std::make_shared<RenderStep_ResolveHDR>();
			result.push_back(std::make_shared<RenderStep_SampleLuminance>(resolveHDR));
			result.push_back(resolveHDR);
		}

		return result;
	}

	class CompiledSceneTechnique
	{
	public:
		std::vector<std::shared_ptr<IViewDelegate>> _viewDelegates;
		std::vector<std::shared_ptr<ILightingParserPlugin>> _lightingPlugins;
		std::vector<std::shared_ptr<IRenderStep>> _renderSteps;

		class RenderPass
		{
		public:
			RenderCore::PipelineType _pipelineType;
			RenderCore::FrameBufferDesc _fbDesc;
			size_t _beginRenderStep = 0;
			size_t _endRenderStep = 0;

			std::vector<std::shared_ptr<RenderCore::Techniques::SequencerConfig>> _perSubpassSequencerConfigs;
			std::vector<Techniques::FrameBufferFragmentMapping> _perStepRemappings;
			std::vector<std::pair<uint64_t, AttachmentName>> _outputAttachments;
		};
		std::vector<RenderPass> _renderPasses;

		RenderCore::TextureSamples _sampling;
		std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool> _pipelineAccelerators;

		CompiledSceneTechnique(
			const SceneTechniqueDesc& techniqueDesc,
			const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
			const RenderCore::AttachmentDesc& targetAttachmentDesc);
		~CompiledSceneTechnique();
	};

	CompiledSceneTechnique::CompiledSceneTechnique(
		const SceneTechniqueDesc& techniqueDesc,
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const RenderCore::AttachmentDesc& targetAttachmentDesc)
	: _pipelineAccelerators(pipelineAccelerators)
	{
		_renderSteps.insert(_renderSteps.end(), techniqueDesc._renderSteps.begin(), techniqueDesc._renderSteps.end());
		_lightingPlugins.insert(_lightingPlugins.begin(), techniqueDesc._lightingPlugins.begin(), techniqueDesc._lightingPlugins.end());
		
		//
		//		Figure out the frame buffers and render passes required
		//		Iterate through each render step and check it's input/output interface
		//		Where we can merge multiple render steps into a single renderpass, do so
		//			-- however, render passes can use different pipeline types, so (for example
		//			a compute pass can break up a graphics pipeline pass
		//

		std::vector<Techniques::PreregisteredAttachment> workingAttachments = {
			Techniques::PreregisteredAttachment { 
				Techniques::AttachmentSemantics::ColorLDR,
				targetAttachmentDesc,
				Techniques::PreregisteredAttachment::State::Uninitialized
			}
		};

		auto renderStepIterator = _renderSteps.begin();
		while (renderStepIterator != _renderSteps.end()) {
			std::vector<Techniques::FrameBufferDescFragment> fragments;
			std::vector<RenderStepFragmentInterface::SubpassExtension> subpassExtensions;

			auto renderStepEnd = renderStepIterator;
			auto& firstInterface = (*renderStepEnd)->GetInterface();
			auto pipelineType = firstInterface.GetFrameBufferDescFragment()._pipelineType;
			fragments.push_back(firstInterface.GetFrameBufferDescFragment());
			subpassExtensions.insert(subpassExtensions.end(), firstInterface.GetSubpassAddendums().begin(), firstInterface.GetSubpassAddendums().end());
			assert(firstInterface.GetSubpassAddendums().size() == firstInterface.GetFrameBufferDescFragment()._subpasses.size());
			++renderStepEnd;
			for (; renderStepEnd != _renderSteps.end(); ++renderStepEnd) {
				auto& interf = (*renderStepEnd)->GetInterface();
				if (interf.GetFrameBufferDescFragment()._pipelineType != pipelineType) break;
				fragments.push_back(interf.GetFrameBufferDescFragment());
				subpassExtensions.insert(subpassExtensions.end(), interf.GetSubpassAddendums().begin(), interf.GetSubpassAddendums().end());
				assert(interf.GetSubpassAddendums().size() == interf.GetFrameBufferDescFragment()._subpasses.size());
			}
		
			auto merged = Techniques::MergeFragments(
				MakeIteratorRange(workingAttachments),
				MakeIteratorRange(fragments));
			auto fbDesc = Techniques::BuildFrameBufferDesc(std::move(merged._mergedFragment));

			workingAttachments.reserve(merged._outputAttachments.size());
			for (const auto&o:merged._outputAttachments) {
				auto i = std::find_if(
					workingAttachments.begin(), workingAttachments.end(),
					[&o](const Techniques::PreregisteredAttachment& p) { return p._semantic == o.first; });
				if (i != workingAttachments.end()) {
					assert(merged._mergedFragment._attachments[o.second]._outputSemanticBinding == o.first);
					i->_desc = merged._mergedFragment._attachments[o.second]._desc;
					i->_state = i->_stencilState = Techniques::PreregisteredAttachment::State::Initialized;
				} else {
					assert(merged._mergedFragment._attachments[o.second]._outputSemanticBinding == o.first);
					workingAttachments.push_back(
						Techniques::PreregisteredAttachment { 
							o.first,
							merged._mergedFragment._attachments[o.second]._desc,
							Techniques::PreregisteredAttachment::State::Initialized,
							Techniques::PreregisteredAttachment::State::Initialized
						});
				}
			}

			RenderPass newRenderPass;
			newRenderPass._fbDesc = std::move(fbDesc);
			newRenderPass._pipelineType = pipelineType;
			newRenderPass._beginRenderStep = std::distance(_renderSteps.begin(), renderStepIterator);
			newRenderPass._endRenderStep = std::distance(_renderSteps.begin(), renderStepEnd);
			newRenderPass._perStepRemappings = std::move(merged._remapping);
			newRenderPass._outputAttachments = std::move(merged._outputAttachments);

			// Fill in the SequencerConfig for each render step
			auto subpassIterator = subpassExtensions.begin();
			for (auto step=renderStepIterator; step!=renderStepEnd; ++step) {
				for (unsigned subpass=0; subpass<unsigned(fragments[step-renderStepIterator]._subpasses.size()); ++subpass, ++subpassIterator) {
					if (subpassIterator->_techniqueDelegate) {
						auto sequencerConfig = pipelineAccelerators->CreateSequencerConfig(
							subpassIterator->_techniqueDelegate,
							subpassIterator->_sequencerSelectors,
							newRenderPass._fbDesc,
							(unsigned)(subpassIterator - subpassExtensions.begin()));
						newRenderPass._perSubpassSequencerConfigs.push_back(sequencerConfig);
					} else {
						newRenderPass._perSubpassSequencerConfigs.push_back({});
					}
				}
			}

			_renderPasses.emplace_back(std::move(newRenderPass));
			renderStepIterator = renderStepEnd;
		}

		// 
		//		Other configuration & state
		//

		for (const auto&step:_renderSteps) {
			auto viewDelegate = step->CreateViewDelegate();
			if (viewDelegate)
				_viewDelegates.push_back(viewDelegate);
		}

		// _gbufferType = enableParametersBuffer?1:2;
		// _precisionTargets = precisionTargets;
		_sampling = techniqueDesc._sampling;
	}

	CompiledSceneTechnique::~CompiledSceneTechnique()
	{
	}

	std::shared_ptr<CompiledSceneTechnique> CreateCompiledSceneTechnique(
		const SceneTechniqueDesc& techniqueDesc,
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const RenderCore::AttachmentDesc& targetAttachmentDesc)
	{
		return std::make_shared<CompiledSceneTechnique>(techniqueDesc, pipelineAccelerators, targetAttachmentDesc);
	}

	RenderCore::AttachmentName RenderStepFragmentInterface::DefineAttachment(uint64_t semantic, const RenderCore::AttachmentDesc& request)
	{
		return _frameBufferDescFragment.DefineAttachment(semantic, request);
	}

	RenderCore::AttachmentName RenderStepFragmentInterface::DefineTemporaryAttachment(const RenderCore::AttachmentDesc& request)
	{
		return _frameBufferDescFragment.DefineTemporaryAttachment(request);
	}

    void RenderStepFragmentInterface::AddSubpass(
		RenderCore::SubpassDesc&& subpass,
		const std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate>& techniqueDelegate,
		ParameterBox&& sequencerSelectors)
	{
		_frameBufferDescFragment.AddSubpass(std::move(subpass));
		_subpassExtensions.emplace_back(
			SubpassExtension {
				techniqueDelegate, std::move(sequencerSelectors)
			});
	}

	RenderStepFragmentInterface::RenderStepFragmentInterface(RenderCore::PipelineType pipelineType)
	{
		_frameBufferDescFragment._pipelineType = pipelineType;
	}

	RenderStepFragmentInterface::~RenderStepFragmentInterface() {}


	const RenderCore::Techniques::SequencerConfig* RenderStepFragmentInstance::GetSequencerConfig() const
	{
		if (_currentPassIndex >= _sequencerConfigs.size())
			return nullptr;
		return _sequencerConfigs[_currentPassIndex].get();
	}

	RenderStepFragmentInstance::RenderStepFragmentInstance(
		RenderCore::Techniques::RenderPassInstance& rpi,
        const RenderCore::Techniques::FrameBufferFragmentMapping& mapping,
		IteratorRange<const std::shared_ptr<RenderCore::Techniques::SequencerConfig>*> sequencerConfigs)
	: RenderCore::Techniques::RenderPassFragment(rpi, mapping)
	{
		_sequencerConfigs = sequencerConfigs;
	}

	RenderStepFragmentInstance::RenderStepFragmentInstance() {}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
		for (auto&step:technique._viewDelegates) {
			step->Reset();
			executeContext.AddView(SceneView{SceneView::Type::Normal, mainSceneProjection}, step);
		}

		for (unsigned s=0; s<lightingDelegate.GetShadowProjectionCount(); ++s) {
			auto proj = lightingDelegate.GetShadowProjectionDesc(s, mainSceneProjection);
			// todo -- what's the correct projection to give to this view?
			executeContext.AddView(
				SceneView{SceneView::Type::Shadow, /*proj*/mainSceneProjection},
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
							{}, MakeIteratorRange(&interf.GetFrameBufferDescFragment(), &interf.GetFrameBufferDescFragment()+1));

						Techniques::AttachmentPool shadowsAttachmentPool;
						auto fbDesc = Techniques::BuildFrameBufferDesc(std::move(merged._mergedFragment));

						Techniques::RenderPassInstance rpi(threadContext, fbDesc, parsingContext.GetFrameBufferPool(), shadowsAttachmentPool);
						RenderStepFragmentInstance rpf(rpi, merged._remapping[0], {});
						Metal::DeviceContext::Get(threadContext)->Bind(
							Metal::ViewportDesc(0.f, 0.f, float(shadowDelegate._shadowProj._width), float(shadowDelegate._shadowProj._height)));

						renderStep._resource = shadowsAttachmentPool.GetResource(0);
						renderStep.Execute(threadContext, parsingContext, lightingParserContext, rpf, &shadowDelegate);
					} else {
						RenderStep_PrepareRTShadows renderStep;
						RenderStepFragmentInstance rpf;
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
			Techniques::AttachmentSemantics::ColorLDR, renderTarget);

		parsingContext.GetNamedResources().Bind(
			RenderCore::FrameBufferProperties {
				targetTextureDesc._width, targetTextureDesc._height, 
				technique._sampling });

		auto* prevPipelineAccelerator = parsingContext._pipelineAcceleratorPool;
		parsingContext._pipelineAcceleratorPool = technique._pipelineAccelerators.get();

		technique._pipelineAccelerators->RebuildAllOutOfDatePipelines();		// (check for pipelines that need hot reloading)

		for (const auto&rp:technique._renderPasses) {
			Techniques::RenderPassInstance rpi;
			if (rp._pipelineType == PipelineType::Graphics) {
				rpi = Techniques::RenderPassInstance { threadContext, rp._fbDesc, parsingContext.GetFrameBufferPool(), parsingContext.GetNamedResources() };
				// metalContext.Bind(Metal::ViewportDesc { 0.f, 0.f, float(targetTextureDesc._width), float(targetTextureDesc._height) });
			} else {
				// construct a "non-metal" render pass instance. This just handles attachment remapping logic, but doesn't create an renderpass
				// in the underlying graphics API
				rpi = Techniques::RenderPassInstance { rp._fbDesc, parsingContext.GetNamedResources() };
			}

			unsigned subpassCounter = 0;
			auto stepRemappingIterator = rp._perStepRemappings.begin();
			for (size_t step=rp._beginRenderStep; step!=rp._endRenderStep; ++step, ++stepRemappingIterator) {
				CATCH_ASSETS_BEGIN
					auto range = MakeIteratorRange(AsPointer(rp._perSubpassSequencerConfigs.begin() + subpassCounter), AsPointer(rp._perSubpassSequencerConfigs.begin() + subpassCounter + stepRemappingIterator->_subpassCount));
					RenderStepFragmentInstance rpf(rpi, *stepRemappingIterator, range);
					IViewDelegate* viewDelegate = nullptr;
					if (step==0) viewDelegate = executeContext.GetViewDelegates()[0].get();

					technique._renderSteps[step]->Execute(threadContext, parsingContext, lightingParserContext, rpf, viewDelegate);
					subpassCounter += (unsigned)range.size();
				CATCH_ASSETS_END(parsingContext)
			}

			// Bind the output attachments from the render pass
			// Note -- we never unbind any "exhausted" attachments. All attachments
			//		that are written to end up here.
			for (const auto&w:rp._outputAttachments)
				parsingContext.GetNamedResources().Bind(w.first, rpi.GetResource(w.second));
		}

		parsingContext._pipelineAcceleratorPool = prevPipelineAccelerator;

		// Bind depth to NamedResources(), so we can find it later with RenderPassToPresentationTargetWithDepthStencil()
		/*if (merged._mergedFragment._attachments[c].GetOutputSemanticBinding() == Techniques::AttachmentSemantics::MultisampleDepth)
			parsingContext.GetNamedResources().Bind(
				RenderCore::Techniques::AttachmentSemantics::MultisampleDepth,
				rpi.GetResource(c));*/

		return lightingParserContext;
    }

	std::shared_ptr<::Assets::IAsyncMarker> PreparePipelines(
		RenderCore::IThreadContext& threadContext,
		const CompiledSceneTechnique& technique,
		const ILightingParserDelegate& lightingDelegate,
		IScene& scene)
	{
		SceneExecuteContext executeContext;
		for (auto&step:technique._viewDelegates) {
			step->Reset();
			executeContext.AddView(SceneView{SceneView::Type::Other}, step);
		}

		for (unsigned s=0; s<lightingDelegate.GetShadowProjectionCount(); ++s) {
			executeContext.AddView(
				SceneView{SceneView::Type::Other},
				std::make_shared<ViewDelegate_Shadow>(ShadowProjectionDesc{}));
		}

		scene.ExecuteScene(threadContext, executeContext);

		auto pipelineAcceleratorPool = technique._pipelineAccelerators.get();
		pipelineAcceleratorPool->RebuildAllOutOfDatePipelines();

		std::set<::Assets::FuturePtr<Metal::GraphicsPipeline>> pendingPipelines;

		for (const auto&rp:technique._renderPasses) {
			unsigned subpassCounter = 0;
			auto stepRemappingIterator = rp._perStepRemappings.begin();
			for (size_t step=rp._beginRenderStep; step!=rp._endRenderStep; ++step, ++stepRemappingIterator) {
				IViewDelegate* viewDelegate = nullptr;
				if (step==0) viewDelegate = executeContext.GetViewDelegates()[0].get();

				for (unsigned b=0; b<(unsigned)RenderCore::Techniques::BatchFilter::Max; ++b) {
					auto *drawablesPacket = viewDelegate ? viewDelegate->GetDrawablesPacket(RenderCore::Techniques::BatchFilter(b)) : nullptr;
					if (!drawablesPacket) continue;

					for (auto d=drawablesPacket->_drawables.begin(); d!=drawablesPacket->_drawables.end(); ++d) {
						const auto& drawable = *(RenderCore::Techniques::Drawable*)d.get();
						auto range = MakeIteratorRange(
							AsPointer(rp._perSubpassSequencerConfigs.begin() + subpassCounter), 
							AsPointer(rp._perSubpassSequencerConfigs.begin() + subpassCounter + stepRemappingIterator->_subpassCount));
						for (auto r:range) {
							auto pipelineAccelerator = pipelineAcceleratorPool->GetPipeline(*drawable._pipeline, *r);
							if (pipelineAccelerator->GetAssetState() == ::Assets::AssetState::Pending)
								pendingPipelines.insert(pipelineAccelerator);
						}
					}
				}
			}
		}

		if (pendingPipelines.empty())
			return nullptr;

		std::string markerName{"SceneEngine PreparePipelines"};
		auto result = std::make_shared<::Assets::AsyncMarkerGroup>();
		for (const auto&pp:pendingPipelines)
			result->Add(pp, markerName);
		
		return result;
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

	// note -- explicitly implemented because of use of PreparedDMShadowFrustum, PreparedRTShadowFrustum in a vector
	LightingParserContext::LightingParserContext(LightingParserContext&& moveFrom)
	: _preparedDMShadows(std::move(moveFrom._preparedDMShadows))
	, _preparedRTShadows(std::move(moveFrom._preparedRTShadows))
	, _plugins(std::move(moveFrom._plugins))
	, _metricsBox(moveFrom._metricsBox)
	, _mainTargets(std::move(moveFrom._mainTargets))
	, _preparedScene(moveFrom._preparedScene)
	, _sampleCount(moveFrom._sampleCount)
	, _delegate(moveFrom._delegate)
	{
		moveFrom._preparedScene = nullptr;
		moveFrom._sampleCount = 0;
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
		_metricsBox = moveFrom._metricsBox;
		_delegate = moveFrom._delegate;
		moveFrom._preparedScene = nullptr;
		moveFrom._sampleCount = 0;
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

		// lightingParserContext._gbufferType = technique._gbufferType;

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
                        METRICS_RENDER_VERTEX_HLSL ":main:vs_*", 
                        METRICS_RENDER_GEO_HLSL ":main:gs_*",
                        METRICS_RENDER_PIXEL_HLSL ":main:ps_*",
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

	void ExecuteSceneRaw(
		RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parserContext,
		const RenderCore::Techniques::SequencerContext& sequencerTechnique,
		const SceneView& view,
		IScene& scene)
	{
		using namespace RenderCore;
		SceneExecuteContext sceneExeContext;
		auto viewDelegate = std::make_shared<BasicViewDelegate>();
		sceneExeContext.AddView(view, viewDelegate);
		scene.ExecuteScene(threadContext, sceneExeContext);

		Techniques::Draw(
			threadContext, parserContext, 
			sequencerTechnique, 
			viewDelegate->_pkt);
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
