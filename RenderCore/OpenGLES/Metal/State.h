// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Core/Exceptions.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    class DeviceContext;

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class SamplerState
    {
    public:
        SamplerState();
        void Apply(unsigned samplerIndex) const never_throws;

        typedef SamplerState UnderlyingType;
        UnderlyingType GetUnderlying() const never_throws { return *this; }
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class RasterizerState
    {
    public:
        RasterizerState();
        void Apply() const never_throws;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class BlendState
    {
    public:
        BlendState();
        void Apply() const never_throws;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class DepthStencilState
    {
    public:
        DepthStencilState();
        void Apply() const never_throws;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ViewportDesc
    {
    public:
            // (naming convention as per D3D11_VIEWPORT)
        float TopLeftX, TopLeftY;
        float Width, Height;
        float MinDepth, MaxDepth;

        ViewportDesc(DeviceContext&);
    };
}}

