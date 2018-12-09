// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/State.h"

namespace RenderCore { namespace Techniques
{
    class CompiledRenderStateSet
    {
    public:
        Metal::BlendState _blendState = Metal::BlendState::Null();
        Metal::RasterizerState _rasterizerState = Metal::RasterizerState::Null();
    };

    Metal::RasterizerState BuildDefaultRastizerState(const RenderStateSet& states);
}}

