// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "RenderStep.h"

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

		RenderCore::IResourcePtr _resource;

		RenderStep_PrepareDMShadows(RenderCore::Format format, UInt2 dims, unsigned projectionCount);
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

		RenderCore::Techniques::DrawablesPacket* GetDrawablesPacket(RenderCore::Techniques::BatchFilter batch);
		ViewDelegate_Shadow(ShadowProjectionDesc shadowProjection);
		~ViewDelegate_Shadow();
	};
}

