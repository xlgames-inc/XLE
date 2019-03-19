// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"

namespace RenderCore { namespace Techniques { class ParsingContext; }}

namespace SceneEngine
{
    class MetricsBox;
    class TransparencyTargetsBox;
    TransparencyTargetsBox* OrderIndependentTransparency_Prepare(
        RenderCore::Metal::DeviceContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext,
        const RenderCore::Metal::ShaderResourceView& depthBufferDupe);
    void OrderIndependentTransparency_Resolve(  
        RenderCore::Metal::DeviceContext& context,
        RenderCore::Techniques::ParsingContext& parserContext,
        TransparencyTargetsBox& targets,
        const RenderCore::Metal::ShaderResourceView& originalDepthStencilSRV,
		MetricsBox& metricsBox);
}
