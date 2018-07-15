// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/TextureView.h"

namespace RenderCore { namespace Techniques { class ParsingContext; } }

namespace SceneEngine
{
    class LightingParserContext;
    RenderCore::Metal::ShaderResourceView TiledLighting_CalculateLighting(
        RenderCore::Metal::DeviceContext* context, 
        RenderCore::Techniques::ParsingContext& parsingContext,
        const RenderCore::Metal::ShaderResourceView& depthsSRV, 
        const RenderCore::Metal::ShaderResourceView& normalsSRV,
		RenderCore::Metal::UnorderedAccessView& metricBufferUAV);

    void TiledLighting_RenderBeamsDebugging(  
        RenderCore::Metal::DeviceContext* context, 
        RenderCore::Techniques::ParsingContext& parsingContext,
        bool active, unsigned mainViewportWidth, unsigned mainViewportHeight, 
        unsigned techniqueIndex);
}

