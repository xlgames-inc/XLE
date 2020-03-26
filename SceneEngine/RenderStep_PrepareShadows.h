// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "RenderStep.h"

namespace RenderCore { namespace Techniques { class IPipelineAcceleratorPool; }}

namespace SceneEngine
{
	class RenderStep_PrepareDMShadows : public IRenderStep
	{
	public:
		const RenderStepFragmentInterface& GetInterface() const override { return _fragment; }
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderStepFragmentInstance& rpi,
			IViewDelegate* viewDelegate) override;

		RenderStep_PrepareDMShadows(const ShadowGeneratorDesc& desc);
		~RenderStep_PrepareDMShadows();
	private:
		RenderStepFragmentInterface _fragment;
	};

	class RenderStep_PrepareRTShadows : public IRenderStep
	{
	public:
		const RenderStepFragmentInterface& GetInterface() const override { return _fragment; }
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderStepFragmentInstance& rpi,
			IViewDelegate* viewDelegate) override;

		RenderStep_PrepareRTShadows();
		~RenderStep_PrepareRTShadows();
	private:
		RenderStepFragmentInterface _fragment;
	};

	class ViewDelegate_Shadow : public IViewDelegate
	{
	public:
		RenderCore::Techniques::DrawablesPacket _general;
		ShadowProjectionDesc _shadowProj;

		RenderCore::Techniques::DrawablesPacket* GetDrawablesPacket(RenderCore::Techniques::BatchFilter batch) override;
		void Reset() override;
		ViewDelegate_Shadow(ShadowProjectionDesc shadowProjection);
		~ViewDelegate_Shadow();
	};

	class ICompiledShadowGenerator
	{
	public:
		virtual void ExecutePrepare(
			RenderCore::IThreadContext& threadContext, 
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			ViewDelegate_Shadow& shadowDelegate,
			RenderCore::Techniques::FrameBufferPool& shadowGenFrameBufferPool,
			RenderCore::Techniques::AttachmentPool& shadowGenAttachmentPool) = 0;
		~ICompiledShadowGenerator();
	};

	class ShadowGeneratorDesc;
	std::shared_ptr<ICompiledShadowGenerator> CreateCompiledShadowGenerator(
		const ShadowGeneratorDesc& desc, 
		const std::shared_ptr<RenderCore::Techniques::IPipelineAcceleratorPool>& pipelineAccelerator);

	class MainTargets;
	void ShadowGen_DrawShadowFrustums(
        RenderCore::Metal::DeviceContext& devContext, 
		RenderCore::Techniques::ParsingContext& parserContext,
        MainTargets mainTargets,
		const ShadowProjectionDesc& projectionDesc);
}
