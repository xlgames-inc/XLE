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
    void    Sky_Render( RenderCore::Metal::DeviceContext* context, 
                        LightingParserContext& parserContext,
                        bool blendFog);
    void    Sky_RenderPostFog(  RenderCore::Metal::DeviceContext* context, 
                                LightingParserContext& parserContext);

    class SkyTextureParts
    {
    public:
        RenderCore::Metal::DeferredShaderResource*      _faces12;
        RenderCore::Metal::DeferredShaderResource*      _faces34;
        RenderCore::Metal::DeferredShaderResource*      _face5;
        int                                             _projectionType;

        SkyTextureParts(const char skyTextureName[]);
    };

    unsigned    SkyTexture_BindPS(  RenderCore::Metal::DeviceContext* context, 
                                    LightingParserContext& parserContext,
                                    const SkyTextureParts& skyTextureParts,
                                    int bindSlot);
}

