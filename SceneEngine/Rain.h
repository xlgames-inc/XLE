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
    void    Rain_Render(RenderCore::Metal::DeviceContext* context, 
                        LightingParserContext& parserContext);
    void    Rain_RenderSimParticles(RenderCore::Metal::DeviceContext* context, 
                                    LightingParserContext& parserContext,
                                    const RenderCore::Metal::ShaderResourceView& depthsSRV,
                                    const RenderCore::Metal::ShaderResourceView& normalsSRV);

    void    SparkParticleTest_RenderSimParticles(   RenderCore::Metal::DeviceContext* context, 
                                                    LightingParserContext& parserContext,
                                                    const RenderCore::Metal::ShaderResourceView& depthsSRV,
                                                    const RenderCore::Metal::ShaderResourceView& normalsSRV);
}

