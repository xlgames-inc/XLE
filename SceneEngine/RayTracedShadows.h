// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "RenderStep.h"
#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/IThreadContext_Forward.h"
#include "../RenderCore/Techniques/Drawables.h"

namespace RenderCore { namespace Techniques { class ParsingContext; }}

namespace SceneEngine
{
    class ShadowProjectionDesc;
    class PreparedRTShadowFrustum;
    class PreparedScene;
	class LightingParserContext;
	class ILightingParserDelegate;
	class MainTargets;

	class ViewDrawables_Shadow;

    PreparedRTShadowFrustum PrepareRTShadows(
        RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext,
		LightingParserContext& lightingParserContext,
        ViewDrawables_Shadow& executedScene,
		PreparedScene& preparedScene,
        const ShadowProjectionDesc& frustum,
        unsigned shadowFrustumIndex);

    void RTShadows_DrawMetrics(
        RenderCore::Metal::DeviceContext& context,
		RenderCore::Techniques::ParsingContext& parserContext, 
		LightingParserContext& lightingParserContext,
        MainTargets& mainTargets);
}
