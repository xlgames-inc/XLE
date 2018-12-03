// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderStepUtils.h"
#include "../RenderCore/IAnnotator.h"
#include "../RenderCore/Techniques/Drawables.h"
#include "../RenderCore/Techniques/RenderStateResolver.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/BasicDelegates.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../Assets/AssetsCore.h"
#include "../ConsoleRig/ResourceBox.h"

namespace SceneEngine
{
	using namespace RenderCore;

	void ExecuteDrawables(
        IThreadContext& threadContext,
		Techniques::ParsingContext& parserContext,
		ExecuteDrawablesContext& context,
		const RenderCore::Techniques::DrawablesPacket& drawables,
        unsigned techniqueIndex,
		const char name[])
    {
		using namespace RenderCore;
        CATCH_ASSETS_BEGIN
            GPUAnnotation anno(threadContext, name);
			for (auto d=drawables._drawables.begin(); d!=drawables._drawables.end(); ++d)
				RenderCore::Techniques::Draw(
					threadContext, 
					parserContext,
					techniqueIndex,
					context._sequencerTechnique,
					&context._seqShaderSelectors,
					*(Techniques::Drawable*)d.get());
        CATCH_ASSETS_END(parserContext)
    }

	bool BatchHasContent(const RenderCore::Techniques::DrawablesPacket& drawables)
    {
        return !drawables._drawables.empty();
    }

	StateSetResolvers::StateSetResolvers(const Desc&)
    {
        _forward = Techniques::CreateRenderStateDelegate_Forward();
        _deferred = Techniques::CreateRenderStateDelegate_Deferred();
        _depthOnly = Techniques::CreateRenderStateDelegate_DepthOnly();
    }

	StateSetResolvers& GetStateSetResolvers() { return ConsoleRig::FindCachedBox2<StateSetResolvers>(); }

	void LightingParser_SetGlobalTransform(
        RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext, 
        const RenderCore::Techniques::ProjectionDesc& projDesc)
    {
        parserContext.GetProjectionDesc() = projDesc;
        auto globalTransform = BuildGlobalTransformConstants(projDesc);
        parserContext.SetGlobalCB(
            *Metal::DeviceContext::Get(context), Techniques::TechniqueContext::CB_GlobalTransform,
            &globalTransform, sizeof(globalTransform));
    }

	ExecuteDrawablesContext::ExecuteDrawablesContext(RenderCore::Techniques::ParsingContext& parserContext)
	{
		_sequencerTechnique._techniqueDelegate = std::make_shared<RenderCore::Techniques::TechniqueDelegate_Basic>();
		_sequencerTechnique._materialDelegate = std::make_shared<RenderCore::Techniques::MaterialDelegate_Basic>();
		_sequencerTechnique._renderStateDelegate = parserContext.GetRenderStateDelegate();

		auto& techUSI = RenderCore::Techniques::TechniqueContext::GetGlobalUniformsStreamInterface();
		for (unsigned c=0; c<techUSI._cbBindings.size(); ++c)
			_sequencerTechnique._sequencerUniforms.emplace_back(std::make_pair(techUSI._cbBindings[c]._hashName, std::make_shared<RenderCore::Techniques::GlobalCBDelegate>(c)));
	}

	ExecuteDrawablesContext::~ExecuteDrawablesContext() {}

}