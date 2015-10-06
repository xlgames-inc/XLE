// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"

namespace SceneEngine
{
    class LightingParserContext;
    class ShadowProjectionDesc;
    class PreparedRTShadowFrustum;
    class MainTargetsBox;

    PreparedRTShadowFrustum PrepareRTShadows(
        RenderCore::Metal::DeviceContext& metalContext, 
        LightingParserContext& parserContext,
        const ShadowProjectionDesc& frustum,
        unsigned shadowFrustumIndex);

    void RTShadows_DrawMetrics(
        RenderCore::Metal::DeviceContext& context, LightingParserContext& parserContext, 
        MainTargetsBox& mainTargets);
}
