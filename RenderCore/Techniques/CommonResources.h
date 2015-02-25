// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Metal/State.h"

namespace RenderCore { namespace Techniques 
{
    class CommonResourceBox
    {
    public:
        class Desc {};
        CommonResourceBox(const Desc&);

        Metal::DepthStencilState _dssReadWrite;
        Metal::DepthStencilState _dssReadOnly;
        Metal::DepthStencilState _dssDisable;

        Metal::BlendState _blendStraightAlpha;
        Metal::BlendState _blendAlphaPremultiplied;
        Metal::BlendState _blendOpaque;
        Metal::BlendState _blendOneSrcAlpha;

        Metal::RasterizerState _defaultRasterizer;
        Metal::RasterizerState _cullDisable;
        Metal::RasterizerState _cullReverse;

        Metal::SamplerState _defaultSampler;

    private:
        CommonResourceBox(CommonResourceBox&);
        CommonResourceBox& operator=(const CommonResourceBox&);
    };

    CommonResourceBox& CommonResources();

}}

