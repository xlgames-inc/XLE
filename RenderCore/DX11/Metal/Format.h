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

            R24G8_TYPELESS = DXGI_FORMAT_R24G8_TYPELESS,
            D24_UNORM_S8_UINT = DXGI_FORMAT_D24_UNORM_S8_UINT,
            R24_UNORM_X8_TYPELESS = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
            X24_TYPELESS_G8_UINT = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,

            Matrix4x4 = 150,
            Matrix3x4 = 151
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
    unsigned                        GetComponentPrecision(NativeFormat::Enum format);
    unsigned                        GetComponentCount(FormatComponents::Enum components);

    NativeFormat::Enum FindFormat(
        FormatCompressionType::Enum compression, 
        FormatComponents::Enum components,
        FormatComponentType::Enum componentType,
        unsigned precision);

    inline DXGI_FORMAT              AsDXGIFormat(NativeFormat::Enum format) { return DXGI_FORMAT(format); }
}}

