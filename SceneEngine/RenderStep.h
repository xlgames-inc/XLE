// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "SceneParser.h"
#include "LightDesc.h"
#include "Tonemap.h"		// (for LuminanceResult)
#include "../RenderCore/Techniques/Drawables.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include <memory>

namespace RenderCore { namespace Techniques 
{
	class FrameBufferDescFragment;
	class RenderPassFragment;
	class ITechniqueDelegate_New;
	using SequencerConfigId = uint64_t;
}}

namespace SceneEngine
{
	class IViewDelegate
	{
	public:
		virtual RenderCore::Techniques::DrawablesPacket* GetDrawablesPacket(RenderCore::Techniques::BatchFilter batch) = 0;
		virtual ~IViewDelegate();
	};

	class LightingParserContext;

	class IRenderStep
	{
	public:
		virtual std::shared_ptr<IViewDelegate> CreateViewDelegate();
		virtual const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const = 0;
		struct TechniqueDelegate
		{
			std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate_New> _techniqueDelegate;
			ParameterBox _sequencerSelectors;
		};
		virtual TechniqueDelegate GetTechniqueDelegate(unsigned subpassIdx) const { return {}; }
		virtual void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IteratorRange<const RenderCore::Techniques::SequencerConfigId*> sequencerConfigs,
			IViewDelegate* viewDelegate) = 0;
		virtual ~IRenderStep();
	};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class RenderStep_Forward : public IRenderStep
	{
	public:
		std::shared_ptr<IViewDelegate> CreateViewDelegate() override;
		const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const override;
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IteratorRange<const RenderCore::Techniques::SequencerConfigId*> sequencerConfigs,
			IViewDelegate* viewDelegate) override;

		RenderStep_Forward(bool precisionTargets);
		~RenderStep_Forward();
	private:
		RenderCore::Techniques::FrameBufferDescFragment _forward;
	};

	class RenderStep_Direct : public IRenderStep
	{
	public:
		std::shared_ptr<IViewDelegate> CreateViewDelegate() override;
		const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const override;
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IteratorRange<const RenderCore::Techniques::SequencerConfigId*> sequencerConfigs,
			IViewDelegate* viewDelegate) override;

		RenderStep_Direct();
		~RenderStep_Direct();
	private:
		RenderCore::Techniques::FrameBufferDescFragment _direct;
	};

	class RenderStep_GBuffer : public IRenderStep
	{
	public:
		std::shared_ptr<IViewDelegate> CreateViewDelegate() override;
		TechniqueDelegate GetTechniqueDelegate(unsigned subpassIdx) const override;
		const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const override;
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IteratorRange<const RenderCore::Techniques::SequencerConfigId*> sequencerConfigs,
			IViewDelegate* viewDelegate) override;

		RenderStep_GBuffer(unsigned gbufferType, bool precisionTargets);
		~RenderStep_GBuffer();
	private:
		RenderCore::Techniques::FrameBufferDescFragment _createGBuffer;
		unsigned _gbufferType;
		std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate_New> _deferredIllumDelegate;
	};

	class RenderStep_PrepareDMShadows : public IRenderStep
	{
	public:
		const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const override { return _fragment; }
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IteratorRange<const RenderCore::Techniques::SequencerConfigId*> sequencerConfigs,
			IViewDelegate* viewDelegate) override;

		RenderCore::IResourcePtr _resource;

		RenderStep_PrepareDMShadows(RenderCore::Format format, UInt2 dims, unsigned projectionCount);
		~RenderStep_PrepareDMShadows();
	private:
		RenderCore::Techniques::FrameBufferDescFragment _fragment;
	};

	class RenderStep_PrepareRTShadows : public IRenderStep
	{
	public:
		const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const override { return _fragment; }
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IteratorRange<const RenderCore::Techniques::SequencerConfigId*> sequencerConfigs,
			IViewDelegate* viewDelegate) override;

		RenderStep_PrepareRTShadows();
		~RenderStep_PrepareRTShadows();
	private:
		RenderCore::Techniques::FrameBufferDescFragment _fragment;
	};

	class ViewDelegate_Shadow : public IViewDelegate
	{
	public:
		RenderCore::Techniques::DrawablesPacket _general;
		ShadowProjectionDesc _shadowProj;

		RenderCore::Techniques::DrawablesPacket* GetDrawablesPacket(RenderCore::Techniques::BatchFilter batch);
		ViewDelegate_Shadow(ShadowProjectionDesc shadowProjection);
		~ViewDelegate_Shadow();
	};

	class RenderStep_LightingResolve : public IRenderStep
	{
	public:
		virtual const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const override { return _fragment; }
		virtual void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IteratorRange<const RenderCore::Techniques::SequencerConfigId*> sequencerConfigs,
			IViewDelegate* viewDelegate) override;

		RenderStep_LightingResolve(bool precisionTargets);
	private:
		RenderCore::Techniques::FrameBufferDescFragment _fragment;
	};

	class RenderStep_ResolveHDR : public IRenderStep
	{
	public:
		const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const override { return _fragment; }
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IteratorRange<const RenderCore::Techniques::SequencerConfigId*> sequencerConfigs,
			IViewDelegate* viewDelegate);

		void SetLuminanceResult(LuminanceResult&&);

		RenderStep_ResolveHDR();
		~RenderStep_ResolveHDR();
	private:
		RenderCore::Techniques::FrameBufferDescFragment _fragment;
		LuminanceResult _luminanceResult;
	};

	class RenderStep_SampleLuminance : public IRenderStep
	{
	public:
		const RenderCore::Techniques::FrameBufferDescFragment& GetInterface() const override { return _fragment; }
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderCore::Techniques::RenderPassFragment& rpi,
			IteratorRange<const RenderCore::Techniques::SequencerConfigId*> sequencerConfigs,
			IViewDelegate* viewDelegate) override;

		RenderStep_SampleLuminance(const std::shared_ptr<RenderStep_ResolveHDR>& downstream);
		~RenderStep_SampleLuminance();
	private:
		RenderCore::Techniques::FrameBufferDescFragment _fragment;
		std::shared_ptr<RenderStep_ResolveHDR> _downstream;
	};


}
