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
    class TransparencyTargetsBox;
    TransparencyTargetsBox* OrderIndependentTransparency_Prepare(
        RenderCore::Metal::DeviceContext& context, 
        LightingParserContext& parserContext,
        const RenderCore::Metal::ShaderResourceView& depthBufferDupe);
    void OrderIndependentTransparency_Resolve(  
        RenderCore::Metal::DeviceContext& context,
        LightingParserContext& parserContext,
        TransparencyTargetsBox& targets,
        const RenderCore::Metal::ShaderResourceView& originalDepthStencilSRV);
}
