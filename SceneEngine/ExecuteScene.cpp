// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ExecuteScene.h"
#include "../RenderCore/LightingEngine/LightingEngine.h"
#include "../RenderCore/LightingEngine/LightDesc.h"
#include "../RenderCore/Techniques/PipelineAccelerator.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/RenderPass.h"
#include "../RenderCore/Techniques/ParsingContext.h"

namespace SceneEngine
{
	void ExecuteSceneRaw(
		RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parserContext,
		const RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
		const RenderCore::Techniques::SequencerContext& sequencerTechnique,
		const SceneView& view, RenderCore::Techniques::BatchFilter batchFilter,
		IScene& scene)
    {
		RenderCore::Techniques::DrawablesPacket pkt;
        scene.ExecuteScene(threadContext, ExecuteSceneContext{view, batchFilter, &pkt});
		RenderCore::Techniques::Draw(threadContext, parserContext, pipelineAccelerators, sequencerTechnique, pkt);
    }

    RenderCore::LightingEngine::LightingTechniqueInstance BeginLightingTechnique(
		RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parsingContext,
		RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
		SceneEngine::ILightingStateDelegate& lightingState,
		RenderCore::LightingEngine::CompiledLightingTechnique& compiledTechnique)
	{
		RenderCore::LightingEngine::SceneLightingDesc lightingDesc;
		lightingDesc._env = lightingState.GetEnvironmentalLightingDesc();
		auto lightCount = lightingState.GetLightCount();
		auto shadowProjCount = lightingState.GetShadowProjectionCount();
		lightingDesc._lights.reserve(lightCount);
		lightingDesc._shadowProjections.reserve(shadowProjCount);
		for (unsigned c=0; c<lightCount; ++c) lightingDesc._lights.push_back(lightingState.GetLightDesc(c));
		for (unsigned c=0; c<shadowProjCount; ++c) lightingDesc._shadowProjections.push_back(lightingState.GetShadowProjectionDesc(c, parsingContext.GetProjectionDesc()));
		return RenderCore::LightingEngine::LightingTechniqueInstance{
			threadContext, parsingContext, pipelineAccelerators,
			lightingDesc, compiledTechnique};
	}

	std::shared_ptr<::Assets::IAsyncMarker> PrepareResources(
		const RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
		RenderCore::LightingEngine::CompiledLightingTechnique& compiledTechnique,
		IScene& scene)
	{
		return nullptr;
	}
}
