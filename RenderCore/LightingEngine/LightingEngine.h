// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Techniques/Drawables.h"		// for Techniques::BatchFilter

namespace RenderCore { class IDevice; class IThreadContext; }
namespace RenderCore { namespace Techniques { class ParsingContext; class AttachmentPool; class FrameBufferPool; } }

namespace RenderCore { namespace LightingEngine
{
	class CompiledLightingTechnique;
	class SceneLightingDesc;
	class LightingEngineApparatus;
	class SharedTechniqueDelegateBox;

	enum class StepType { ParseScene, None, Abort };

	class LightingTechniqueIterator;
	class LightingTechniqueInstance
	{
	public:
		struct Step
		{
			StepType _type = StepType::Abort;
			Techniques::BatchFilter _batch = Techniques::BatchFilter::Max;
			Techniques::DrawablesPacket* _pkt = nullptr;
		};
		Step GetNextStep();
		
		LightingTechniqueInstance(
			IThreadContext&,
			Techniques::ParsingContext&,
			Techniques::IPipelineAcceleratorPool&,
			const SceneLightingDesc&,
			CompiledLightingTechnique&);
		~LightingTechniqueInstance();
	private:
		std::unique_ptr<LightingTechniqueIterator> _iterator;
	};

	std::shared_ptr<CompiledLightingTechnique> CreateForwardLightingTechnique(
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const std::shared_ptr<LightingEngineApparatus>& apparatus);

	std::shared_ptr<CompiledLightingTechnique> CreateForwardLightingTechnique(
		const std::shared_ptr<IDevice>& device,
		const std::shared_ptr<Techniques::IPipelineAcceleratorPool>& pipelineAccelerators,
		const std::shared_ptr<SharedTechniqueDelegateBox>& techDelBox);
}}
