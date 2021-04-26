// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/Drawables.h"
#include "../Utility/ParameterBox.h"
#include <memory>

namespace RenderCore { class IThreadContext; }
namespace RenderCore { namespace Techniques { class ParsingContext; class IRenderStateDelegate; class ProjectionDesc; class SequencerConfig; class IPipelineAcceleratorPool; }}

namespace SceneEngine
{
	RenderCore::Techniques::SequencerContext MakeSequencerContext(
		RenderCore::Techniques::ParsingContext& parserContext,
		RenderCore::Techniques::SequencerConfig& sequencerConfig,
		unsigned techniqueIndex);

	RenderCore::Techniques::SequencerContext MakeSequencerContext(
		RenderCore::Techniques::ParsingContext& parserContext,
		unsigned techniqueIndex);

    void ExecuteDrawables(
        RenderCore::IThreadContext& threadContext,
		RenderCore::Techniques::ParsingContext& parserContext,
        const RenderCore::Techniques::IPipelineAcceleratorPool& pipelineAccelerators,
		const RenderCore::Techniques::SequencerContext& sequencerContext,
		const RenderCore::Techniques::DrawablesPacket& drawables,
		const char name[]);

    bool BatchHasContent(const RenderCore::Techniques::DrawablesPacket& drawables);

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
