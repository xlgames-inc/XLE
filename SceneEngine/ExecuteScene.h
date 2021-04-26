// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once 

#include "IScene.h"

namespace RenderCore { namespace Techniques 
{
    class IPipelineAcceleratorPool;
    class SequencerContext;
    class PreregisteredAttachment;
}}

namespace RenderCore { namespace LightingEngine 
{
    class CompiledLightingTechnique;
    class LightingTechniqueInstance;
}}

namespace SceneEngine
{
	void ExecuteSceneRaw(
		RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parserContext,
		const RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
		const RenderCore::Techniques::SequencerContext& sequencerTechnique,
		const SceneView& view,
		IScene& scene);

    RenderCore::LightingEngine::LightingTechniqueInstance BeginLightingTechnique(
		RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parsingContext,
		RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
		IteratorRange<const RenderCore::Techniques::PreregisteredAttachment*> preregisteredAttachment,
		SceneEngine::ILightingStateDelegate& lightingState,
		RenderCore::LightingEngine::CompiledLightingTechnique& compiledTechnique);
}
