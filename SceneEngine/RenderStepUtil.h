// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/Drawables.h"
#include <memory>

namespace SceneEngine
{
	class RenderStateDelegateChangeMarker
    {
    public:
        RenderStateDelegateChangeMarker(
            RenderCore::Techniques::ParsingContext& parsingContext,
            std::shared_ptr<RenderCore::Techniques::IRenderStateDelegate> newResolver)
        {
            _parsingContext = &parsingContext;
            _oldResolver = parsingContext.SetRenderStateDelegate(std::move(newResolver));
        }
        ~RenderStateDelegateChangeMarker()
        {
            if (_parsingContext)
                _parsingContext->SetRenderStateDelegate(std::move(_oldResolver));
        }
        RenderStateDelegateChangeMarker(const RenderStateDelegateChangeMarker&);
        RenderStateDelegateChangeMarker& operator=(const RenderStateDelegateChangeMarker&);
    private:
        std::shared_ptr<RenderCore::Techniques::IRenderStateDelegate> _oldResolver;
        RenderCore::Techniques::ParsingContext* _parsingContext;
    };

	class ExecuteDrawablesContext
	{
	public:
		RenderCore::Techniques::SequencerTechnique _sequencerTechnique;
		ParameterBox _seqShaderSelectors;
	};

    static void ExecuteDrawables(
        RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parserContext,
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

    static bool BatchHasContent(const RenderCore::Techniques::DrawablesPacket& drawables)
    {
        return !drawables._drawables.empty();
    }

	class StateSetResolvers
    {
    public:
        class Desc {};

        using Resolver = std::shared_ptr<Techniques::IRenderStateDelegate>;
        Resolver _forward, _deferred, _depthOnly;

        StateSetResolvers(const Desc&)
        {
            _forward = Techniques::CreateRenderStateDelegate_Forward();
            _deferred = Techniques::CreateRenderStateDelegate_Deferred();
            _depthOnly = Techniques::CreateRenderStateDelegate_DepthOnly();
        }
    };

    StateSetResolvers& GetStateSetResolvers() { return ConsoleRig::FindCachedBox2<StateSetResolvers>(); }


	class LightResolveResourcesRes
    {
    public:
        unsigned    _skyTextureProjection;
        bool        _hasDiffuseIBL;
        bool        _hasSpecularIBL;
    };

    LightResolveResourcesRes LightingParser_BindLightResolveResources( 
        RenderCore::Metal::DeviceContext& context,
		RenderCore::Techniques::ParsingContext& parserContext,
        ILightingParserDelegate& delegate);

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

}
