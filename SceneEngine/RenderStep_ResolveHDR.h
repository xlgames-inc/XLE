// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderStep.h"

namespace SceneEngine
{
	class RenderStep_ResolveHDR : public IRenderStep
	{
	public:
		const RenderStepFragmentInterface& GetInterface() const override { return _fragment; }
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderStepFragmentInstance& rpi,
			IViewDelegate* viewDelegate);

		void SetLuminanceResult(LuminanceResult&&);

		RenderStep_ResolveHDR();
		~RenderStep_ResolveHDR();
	private:
		RenderStepFragmentInterface _fragment;
		LuminanceResult _luminanceResult;
	};

	class RenderStep_SampleLuminance : public IRenderStep
	{
	public:
		const RenderStepFragmentInterface& GetInterface() const override { return _fragment; }
		void Execute(
			RenderCore::IThreadContext& threadContext,
			RenderCore::Techniques::ParsingContext& parsingContext,
			LightingParserContext& lightingParserContext,
			RenderStepFragmentInstance& rpi,
			IViewDelegate* viewDelegate) override;

		RenderStep_SampleLuminance(const std::shared_ptr<RenderStep_ResolveHDR>& downstream);
		~RenderStep_SampleLuminance();
	private:
		RenderStepFragmentInterface _fragment;
		std::shared_ptr<RenderStep_ResolveHDR> _downstream;
	};
}
