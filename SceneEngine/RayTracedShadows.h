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

    void PrepareRTShadows(
        RenderCore::Metal::DeviceContext& metalContext, 
        LightingParserContext& parserContext,
        const ShadowProjectionDesc& frustum,
        unsigned shadowFrustumIndex);
}