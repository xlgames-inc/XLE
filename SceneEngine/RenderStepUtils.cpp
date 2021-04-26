// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderStepUtils.h"
#include "../RenderCore/IAnnotator.h"
#include "../RenderCore/Techniques/Drawables.h"
#include "../RenderCore/Techniques/RenderStateResolver.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/BasicDelegates.h"
#include "../RenderCore/Techniques/TechniqueDelegates.h"
#include "../RenderCore/Techniques/SystemUniformsDelegate.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../Assets/AssetsCore.h"
#include "../ConsoleRig/ResourceBox.h"

namespace SceneEngine
{
	using namespace RenderCore;

	void ExecuteDrawables(
        IThreadContext& threadContext,
		Techniques::ParsingContext& parserContext,
		const RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
		const Techniques::SequencerContext& sequencerContext,
		const Techniques::DrawablesPacket& drawables,
		const char name[])
    {
		using namespace RenderCore;
        CATCH_ASSETS_BEGIN
            GPUAnnotation anno(threadContext, name);
			RenderCore::Techniques::Draw(
				threadContext, 
				parserContext,
				pipelineAccelerators,
				sequencerContext,
				drawables);
        CATCH_ASSETS_END(parserContext)
    }

	bool BatchHasContent(const RenderCore::Techniques::DrawablesPacket& drawables)
    {
        return !drawables._drawables.empty();
    }

	void LightingParser_SetGlobalTransform(
        RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext, 
        const RenderCore::Techniques::ProjectionDesc& projDesc)
    {
        parserContext.GetProjectionDesc() = projDesc;
    }

	RenderCore::Techniques::SequencerContext MakeSequencerContext(
		RenderCore::Techniques::ParsingContext& parserContext,
		RenderCore::Techniques::SequencerConfig& sequencerConfig,
		unsigned techniqueIndex)
	{
		RenderCore::Techniques::SequencerContext result;
		result._sequencerConfig = &sequencerConfig;
		// techniqueIndex is not read in the newest iteration of Techniques::Draw. Consider using CreateTechniqueDelegateLegacy()
		(void)techniqueIndex;
		return result;
	}

	RenderCore::Techniques::SequencerContext MakeSequencerContext(
		RenderCore::Techniques::ParsingContext& parserContext,
		unsigned techniqueIndex)
	{
		RenderCore::Techniques::SequencerContext result;
		// techniqueIndex is not read in the newest iteration of Techniques::Draw. Consider using CreateTechniqueDelegateLegacy()
		(void)techniqueIndex;
		return result;
	}

}