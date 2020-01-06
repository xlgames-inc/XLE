// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/MaterialScaffold.h"
#include "../Metal/State.h"

namespace RenderCore { namespace Techniques
{
	// Should no longer be used, because ITechniqueDelegate now takes over this transformation
    DEPRECATED_ATTRIBUTE class CompiledRenderStateSet
    {
    public:
        Metal::BlendState _blendState = Metal::BlendState::Null();
        Metal::RasterizerState _rasterizerState = Metal::RasterizerState::Null();
    };

    Metal::RasterizerState BuildDefaultRastizerState(const RenderCore::Assets::RenderStateSet& states);
}}

