// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"

namespace RenderCore { namespace Techniques { class ParsingContext; } }

namespace SceneEngine
{
	class GlobalLightingDesc;
    RenderCore::Metal::ShaderResourceView
        ScreenSpaceReflections_BuildTextures(	
			RenderCore::Metal::DeviceContext& context, 
            RenderCore::Techniques::ParsingContext& parserContext,
            unsigned width, unsigned height, bool useMsaaSamplers, 
            const RenderCore::Metal::ShaderResourceView& gbufferDiffuse,
            const RenderCore::Metal::ShaderResourceView& gbufferNormals,
            const RenderCore::Metal::ShaderResourceView& gbufferParam,
            const RenderCore::Metal::ShaderResourceView& depthsSRV,
			const GlobalLightingDesc& globalLightingDesc);
}


