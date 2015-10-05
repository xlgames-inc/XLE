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
    class StochasticTransparencyBox;

    StochasticTransparencyBox* StochasticTransparency_Prepare(
        RenderCore::Metal::DeviceContext& context, 
        LightingParserContext& parserContext);

    void StochasticTransparencyBox_PrepareSecondPass(  
        RenderCore::Metal::DeviceContext& context,
        LightingParserContext& parserContext,
        StochasticTransparencyBox& box,
        RenderCore::Metal::DepthStencilView& mainDSV);

    void StochasticTransparencyBox_Resolve(  
        RenderCore::Metal::DeviceContext& context,
        LightingParserContext& parserContext,
        StochasticTransparencyBox& targets);
}
