// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/TextureView.h"

namespace SceneEngine
{
    class LightingParserContext;
    RenderCore::Metal::ShaderResourceView TiledLighting_CalculateLighting(
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& lightingParserContext,
        RenderCore::Metal::ShaderResourceView& depthsSRV, 
        RenderCore::Metal::ShaderResourceView& normalsSRV);

    void TiledLighting_RenderBeamsDebugging(  
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& lightingParserContext,
        bool active, unsigned mainViewportWidth, unsigned mainViewportHeight, 
        unsigned techniqueIndex);
}

