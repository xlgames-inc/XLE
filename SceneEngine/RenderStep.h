// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "SceneParser.h"
#include "../RenderCore/Techniques/Drawables.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/LightingEngine/RenderStepFragments.h"
#include <memory>


namespace SceneEngine
{
	class IViewDelegate
	{
	public:
		virtual RenderCore::Techniques::DrawablesPacket* GetDrawablesPacket(RenderCore::Techniques::BatchFilter batch) = 0;
		virtual void Reset() = 0;
		virtual ~IViewDelegate();
	};

	class LightingParserContext;
	using RenderStepFragmentInterface = RenderCore::LightingEngine::RenderStepFragmentInterface;
	using RenderStepFragmentInstance = RenderCore::LightingEngine::RenderStepFragmentInstance;

	class IRenderStep
	{
	public:
		virtual std::shared_ptr<IViewDelegate> CreateViewDelegate();
		virtual const RenderStepFragmentInterface& GetInterface() const = 0;
		virtual void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			const RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
			LightingParserContext& lightingParserContext,
			RenderStepFragmentInstance& rpi,
			IViewDelegate* viewDelegate) = 0;
		virtual ~IRenderStep();
	};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

	std::shared_ptr<IRenderStep> CreateRenderStep_Forward(bool precisionTargets);
	std::shared_ptr<IRenderStep> CreateRenderStep_Direct(const std::shared_ptr<RenderCore::Techniques::ITechniqueDelegate>& customDelegate = nullptr);
	std::shared_ptr<IRenderStep> CreateRenderStep_GBuffer(unsigned gbufferType, bool precisionTargets);
	std::shared_ptr<IRenderStep> CreateRenderStep_LightingResolve(unsigned gbufferType, bool precisionTargets);
	std::shared_ptr<IRenderStep> CreateRenderStep_PostDeferredOpaque(bool precisionTargets);

	class BasicViewDelegate : public SceneEngine::IViewDelegate
	{
	public:
		RenderCore::Techniques::DrawablesPacket* GetDrawablesPacket(RenderCore::Techniques::BatchFilter batch) override
		{
			return (batch == RenderCore::Techniques::BatchFilter::General || batch == RenderCore::Techniques::BatchFilter::PostOpaque) ? &_pkt : nullptr;
		}

		void Reset() override
		{
			_pkt.Reset();
		}

		RenderCore::Techniques::DrawablesPacket _pkt;
	};
	
}
