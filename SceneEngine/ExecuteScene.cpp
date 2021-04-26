// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ExecuteScene.h"
#include "../RenderCore/LightingEngine/LightingEngine.h"
#include "../RenderCore/LightingEngine/LightDesc.h"
#include "../RenderCore/Techniques/PipelineAccelerator.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/RenderPass.h"

namespace SceneEngine
{
	void ExecuteSceneRaw(
		RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parserContext,
		const RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
		const RenderCore::Techniques::SequencerContext& sequencerTechnique,
		const SceneView& view,
		IScene& scene)
    {
        assert(0);
    }

    RenderCore::LightingEngine::LightingTechniqueInstance BeginLightingTechnique(
		RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parsingContext,
		RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
		IteratorRange<const RenderCore::Techniques::PreregisteredAttachment*> preregisteredAttachment,
		SceneEngine::ILightingStateDelegate& lightingState,
		RenderCore::LightingEngine::CompiledLightingTechnique& compiledTechnique)
	{
		return RenderCore::LightingEngine::LightingTechniqueInstance{
			threadContext, parsingContext, pipelineAccelerators,
			RenderCore::LightingEngine::SceneLightingDesc{}, compiledTechnique};
	}

	std::shared_ptr<::Assets::IAsyncMarker> PrepareResources(
		const RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
		RenderCore::LightingEngine::CompiledLightingTechnique& compiledTechnique,
		IScene& scene)
	{
		return nullptr;
	}
}
