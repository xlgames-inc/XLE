// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShadowPreparer.h"
#include "LightDesc.h"
#include "ShadowUniforms.h"
#include "RenderStepFragments.h"
#include "SharedTechniqueDelegates.h"
#include "../Techniques/PipelineAccelerator.h"
#include "../Techniques/ParsingContext.h"
#include "../Techniques/RenderPass.h"
#include "../Techniques/TechniqueDelegates.h"
#include "../Techniques/RenderStateResolver.h"
#include <vector>

namespace RenderCore { namespace LightingEngine
{
	class CompiledShadowPreparer : public ICompiledShadowPreparer
	{
	public:
		Techniques::RenderPassInstance Begin(
			IThreadContext& threadContext, 
			Techniques::ParsingContext& parsingContext,
			const ShadowProjectionDesc& frustum,
			Techniques::FrameBufferPool& shadowGenFrameBufferPool,
			Techniques::AttachmentPool& shadowGenAttachmentPool) override;

		PreparedShadowFrustum End(
			IThreadContext& threadContext, 
			Techniques::ParsingContext& parsingContext,
			Techniques::RenderPassInstance& rpi) override;

		std::shared_ptr<Techniques::SequencerConfig> GetSequencerConfig() override;

		CompiledShadowPreparer(
			const ShadowGeneratorDesc& desc,
			const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
			const std::shared_ptr<SharedTechniqueDelegateBox>& delegatesBox);
		~CompiledShadowPreparer();

	private:
		std::shared_ptr<Techniques::IPipelineAcceleratorPool> _pipelineAccelerators;

		FrameBufferDesc _fbDesc;
		std::vector<std::shared_ptr<Techniques::SequencerConfig>> _sequencerConfigs;

		PreparedDMShadowFrustum _workingDMFrustum;
	};

	ICompiledShadowPreparer::~ICompiledShadowPreparer() {}

	Techniques::RenderPassInstance CompiledShadowPreparer::Begin(
		IThreadContext& threadContext, 
		Techniques::ParsingContext& parsingContext,
		const ShadowProjectionDesc& frustum,
		Techniques::FrameBufferPool& shadowGenFrameBufferPool,
		Techniques::AttachmentPool& shadowGenAttachmentPool)
	{
		_workingDMFrustum = SetupPreparedDMShadowFrustum(frustum);
		assert(_workingDMFrustum.IsReady());
		assert(!_fbDesc.GetSubpasses().empty());
		parsingContext.GetProjectionDesc()._worldToProjection = frustum._worldToClip;
		return Techniques::RenderPassInstance{threadContext, _fbDesc, shadowGenFrameBufferPool, shadowGenAttachmentPool};
	}

	PreparedShadowFrustum CompiledShadowPreparer::End(
		IThreadContext& threadContext, 
		Techniques::ParsingContext& parsingContext,
		Techniques::RenderPassInstance& rpi)
	{
		// todo -- create a uniform delegate and attach it to the parsing context
		/*
		if (lightingParserContext._preparedDMShadows.size() == Tweakable("ShadowGenDebugging", 0)) {
			auto srvForDebugging = *rpi.GetRenderPassInstance().GetDepthStencilAttachmentSRV(TextureViewDesc{TextureViewDesc::Aspect::ColorLinear});
			parsingContext._pendingOverlays.push_back(
				std::bind(
					&ShadowGen_DrawDebugging, 
					std::placeholders::_1, std::placeholders::_2,
					srvForDebugging));
		}

		if (lightingParserContext._preparedDMShadows.size() == Tweakable("ShadowGenFrustumDebugging", 0)) {
			parsingContext._pendingOverlays.push_back(
				std::bind(
					&ShadowGen_DrawShadowFrustums, 
					std::placeholders::_1, std::placeholders::_2,
					lightingParserContext.GetMainTargets(),
					shadowDelegate._shadowProj));
		}
		*/

		return std::move(_workingDMFrustum);
	}

	std::shared_ptr<Techniques::SequencerConfig> CompiledShadowPreparer::GetSequencerConfig()
	{
		return _sequencerConfigs[0];
	}

	static const auto s_shadowCascadeModeString = "SHADOW_CASCADE_MODE";
    static const auto s_shadowEnableNearCascadeString = "SHADOW_ENABLE_NEAR_CASCADE";

	CompiledShadowPreparer::CompiledShadowPreparer(
		const ShadowGeneratorDesc& desc,
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const std::shared_ptr<SharedTechniqueDelegateBox>& delegatesBox)
	: _pipelineAccelerators(pipelineAccelerators)
	{
		assert(desc._resolveType == ShadowResolveType::DepthTexture);

		///////////////////////////////
		RenderStepFragmentInterface fragment{PipelineType::Graphics};
		{
			Techniques::RSDepthBias singleSidedBias {
				desc._rasterDepthBias, desc._depthBiasClamp, desc._slopeScaledBias };
			Techniques::RSDepthBias doubleSidedBias {
				desc._dsRasterDepthBias, desc._dsDepthBiasClamp, desc._dsSlopeScaledBias };

			auto output = fragment.DefineAttachment(
				Techniques::AttachmentSemantics::ShadowDepthMap, 
				{
					AsTypelessFormat(desc._format),
					float(desc._width), float(desc._height),
					desc._arrayCount 
				});
			
			auto shadowGenDelegate = delegatesBox->GetShadowGenTechniqueDelegate(singleSidedBias, doubleSidedBias, desc._cullMode);

			ParameterBox box;
			box.SetParameter(s_shadowCascadeModeString, desc._projectionMode == ShadowProjectionMode::Ortho?2:1);
			box.SetParameter(s_shadowEnableNearCascadeString, desc._enableNearCascade?1:0);

			SubpassDesc subpass;
			subpass.SetDepthStencil(output, LoadStore::Clear, LoadStore::Retain);
			fragment.AddSubpass(
				std::move(subpass),
				shadowGenDelegate,
				Techniques::BatchFilter::Max,
				std::move(box));
		}
		///////////////////////////////
		
		auto merged = Techniques::MergeFragments(
			{}, MakeIteratorRange(&fragment.GetFrameBufferDescFragment(), &fragment.GetFrameBufferDescFragment()+1));
		_fbDesc = Techniques::BuildFrameBufferDesc(std::move(merged._mergedFragment), FrameBufferProperties{});

		auto sequencerConfig = pipelineAccelerators->CreateSequencerConfig(
			fragment.GetSubpassAddendums()[0]._techniqueDelegate,
			fragment.GetSubpassAddendums()[0]._sequencerSelectors,
			_fbDesc,
			0);

		_sequencerConfigs.push_back(std::move(sequencerConfig));
	}

	CompiledShadowPreparer::~CompiledShadowPreparer() {}

	std::shared_ptr<ICompiledShadowPreparer> CreateCompiledShadowPreparer(
		const ShadowGeneratorDesc& desc, 
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerator,
		const std::shared_ptr<SharedTechniqueDelegateBox>& delegatesBox)
	{
		return std::make_shared<CompiledShadowPreparer>(desc, pipelineAccelerator, delegatesBox);
	}


}}

