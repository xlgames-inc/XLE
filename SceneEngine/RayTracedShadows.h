// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/IThreadContext_Forward.h"

namespace RenderCore { namespace Techniques { class ParsingContext; }}

namespace SceneEngine
{
    class ShadowProjectionDesc;
    class PreparedRTShadowFrustum;
    class IMainTargets;
    class PreparedScene;
	class ISceneParser;
	class LightingParserContext;

    PreparedRTShadowFrustum PrepareRTShadows(
        RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext,
		LightingParserContext& lightingParserContext,
        ISceneParser& sceneParser,
		PreparedScene& preparedScene,
        const ShadowProjectionDesc& frustum,
        unsigned shadowFrustumIndex);

    void RTShadows_DrawMetrics(
        RenderCore::Metal::DeviceContext& context,
		RenderCore::Techniques::ParsingContext& parserContext, 
		LightingParserContext& lightingParserContext,
        IMainTargets& mainTargets);
}
