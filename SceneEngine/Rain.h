// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/IThreadContext_Forward.h"
#include "../RenderCore/Metal/Forward.h"

namespace RenderCore { namespace Techniques { class ParsingContext; }}

namespace SceneEngine
{
    void    Rain_Render(
		RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext);

    void    Rain_RenderSimParticles(
		RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext,
        const RenderCore::Metal::ShaderResourceView& depthsSRV,
        const RenderCore::Metal::ShaderResourceView& normalsSRV);

    void    SparkParticleTest_RenderSimParticles(
		RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext,
		float timeValue,
		const RenderCore::Metal::ShaderResourceView& depthsSRV,
        const RenderCore::Metal::ShaderResourceView& normalsSRV);
}

