// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/Drawables.h"
#include "../Utility/ParameterBox.h"
#include <memory>

namespace RenderCore { class IThreadContext; }
namespace RenderCore { namespace Techniques { class ParsingContext; class IRenderStateDelegate; class ProjectionDesc; }}

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
        RenderStateDelegateChangeMarker(const RenderStateDelegateChangeMarker&) = delete;
        RenderStateDelegateChangeMarker& operator=(const RenderStateDelegateChangeMarker&) = delete;
    private:
        std::shared_ptr<RenderCore::Techniques::IRenderStateDelegate> _oldResolver;
        RenderCore::Techniques::ParsingContext* _parsingContext;
    };

	class ExecuteDrawablesContext
	{
	public:
		RenderCore::Techniques::SequencerTechnique _sequencerTechnique;

		ExecuteDrawablesContext(RenderCore::Techniques::ParsingContext& parserContext);
		~ExecuteDrawablesContext();
	};

    void ExecuteDrawables(
        RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parserContext,
		ExecuteDrawablesContext& context,
		const RenderCore::Techniques::DrawablesPacket& drawables,
        unsigned techniqueIndex,
		const char name[]);

    bool BatchHasContent(const RenderCore::Techniques::DrawablesPacket& drawables);

	class StateSetResolvers
    {
    public:
        class Desc {};

        using Resolver = std::shared_ptr<RenderCore::Techniques::IRenderStateDelegate>;
        Resolver _forward, _deferred, _depthOnly;

        StateSetResolvers(const Desc&);
    };

    StateSetResolvers& GetStateSetResolvers();

	class LightResolveResourcesRes
    {
    public:
        unsigned    _skyTextureProjection;
        bool        _hasDiffuseIBL;
        bool        _hasSpecularIBL;
    };

	class ILightingParserDelegate;

    LightResolveResourcesRes LightingParser_BindLightResolveResources( 
        RenderCore::Metal::DeviceContext& context,
		RenderCore::Techniques::ParsingContext& parserContext,
        const ILightingParserDelegate& delegate);

	void LightingParser_SetGlobalTransform(
        RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext, 
        const RenderCore::Techniques::ProjectionDesc& projDesc);

}
