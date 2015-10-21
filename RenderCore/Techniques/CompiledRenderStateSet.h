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
        Metal::BlendState _blendState;
        Metal::RasterizerState _rasterizerState;

            //  We need to initialise the members in the default
            //  constructor, otherwise they'll build the new
            //  underlying state objects
        CompiledRenderStateSet()
            : _blendState(Metal::BlendState::Null())
            , _rasterizerState(Metal::RasterizerState::Null())
        {}

        CompiledRenderStateSet(Metal::BlendState&& blendState, Metal::RasterizerState&& rasterizerState)
        : _blendState(std::move(blendState))
        , _rasterizerState(std::move(rasterizerState)) {}
        CompiledRenderStateSet(CompiledRenderStateSet&& moveFrom)
        : _blendState(std::move(moveFrom._blendState))
        , _rasterizerState(std::move(moveFrom._rasterizerState)) {}
        CompiledRenderStateSet& operator=(CompiledRenderStateSet&& moveFrom)
        {
            _blendState = std::move(moveFrom._blendState);
            _rasterizerState = std::move(moveFrom._rasterizerState);
            return *this;
        }

        CompiledRenderStateSet(const CompiledRenderStateSet&) = delete;
        CompiledRenderStateSet& operator=(const CompiledRenderStateSet&) = delete;
    };

    Metal::RasterizerState BuildDefaultRastizerState(const RenderStateSet& states);
}}

