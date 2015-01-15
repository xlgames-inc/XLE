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
    RenderCore::Metal::ShaderResourceView
        ScreenSpaceReflections_BuildTextures(  RenderCore::Metal::DeviceContext* context, 
                                                LightingParserContext& parserContext,
                                                unsigned width, unsigned height, bool useMsaaSamplers, 
                                                RenderCore::Metal::ShaderResourceView& gbufferDiffuse,
                                                RenderCore::Metal::ShaderResourceView& gbufferNormals,
                                                RenderCore::Metal::ShaderResourceView& gbufferParam,
                                                RenderCore::Metal::ShaderResourceView& depthsSRV);
}


