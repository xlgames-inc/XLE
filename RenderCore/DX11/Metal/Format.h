// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <dxgiformat.h>         // maintain format number compatibility with DXGI whenever possible (note that dxgiformat.h is very simple and has no dependencies!)

namespace RenderCore { namespace Metal_DX11
{
    /// Container for NativeFormat::Enum
    namespace NativeFormat
    {
        enum Enum
        {
            Unknown = 0,

            #undef _EXP
            #define _EXP(X, Y, Z, U)    X##_##Y = DXGI_FORMAT_##X##_##Y,
                #include "../../Metal/Detail/DXGICompatibleFormats.h"
            #undef _EXP

            Matrix4x4,
            Matrix3x4
        };
    }

    /// Container for FormatCompressionType::Enum
    namespace FormatCompressionType
    {
        enum Enum
        {
            None, BlockCompression
        };
    }

    /// Container for FormatComponents::Enum
    namespace FormatComponents
    {
        enum Enum
        {
            Unknown,
            Alpha, 
            Luminance, LuminanceAlpha,
            RGB, RGBAlpha,
            RG, Depth, RGBE
        };
    }

    /// Container for FormatComponentType::Enum
    namespace FormatComponentType
    {
        enum Enum
        {
            Typeless,
            Float, UInt, SInt,
            UNorm, SNorm, UNorm_SRGB,
            Exponential
        };
    }

    FormatCompressionType::Enum     GetCompressionType(NativeFormat::Enum format);
    FormatComponents::Enum          GetComponents(NativeFormat::Enum format);
    FormatComponentType::Enum       GetComponentType(NativeFormat::Enum format);
    unsigned                        BitsPerPixel(NativeFormat::Enum format);

    inline DXGI_FORMAT              AsDXGIFormat(NativeFormat::Enum format) { return DXGI_FORMAT(format); }
}}

