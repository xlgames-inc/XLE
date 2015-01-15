// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/State.h"

namespace SceneEngine
{
    class CommonResourceBox
    {
    public:
        class Desc {};
        CommonResourceBox(const Desc&);

        RenderCore::Metal::DepthStencilState _dssReadWrite;
        RenderCore::Metal::DepthStencilState _dssReadOnly;
        RenderCore::Metal::DepthStencilState _dssDisable;

        RenderCore::Metal::BlendState _blendStraightAlpha;
        RenderCore::Metal::BlendState _blendAlphaPremultiplied;
        RenderCore::Metal::BlendState _blendOpaque;
        RenderCore::Metal::BlendState _blendOneSrcAlpha;

        RenderCore::Metal::RasterizerState _defaultRasterizer;
        RenderCore::Metal::RasterizerState _cullDisable;
        RenderCore::Metal::RasterizerState _cullReverse;

        RenderCore::Metal::SamplerState _defaultSampler;

    private:
        CommonResourceBox(CommonResourceBox&);
        CommonResourceBox& operator=(const CommonResourceBox&);
    };

    CommonResourceBox& CommonResources();

}