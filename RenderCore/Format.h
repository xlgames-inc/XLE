// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <dxgiformat.h>         // maintain format number compatibility with DXGI whenever possible (note that dxgiformat.h is very simple and has no dependencies!)

namespace Utility { namespace ImpliedTyping { class TypeDesc; } }

namespace RenderCore 
{
    enum class Format : int
    {
        Unknown = 0,

        #undef _EXP
        #define _EXP(X, Y, Z, U)    X##_##Y = DXGI_FORMAT_##X##_##Y,
            #include "Metal/Detail/DXGICompatibleFormats.h"
        #undef _EXP

        R24G8_TYPELESS = DXGI_FORMAT_R24G8_TYPELESS,
        D24_UNORM_S8_UINT = DXGI_FORMAT_D24_UNORM_S8_UINT,
        R24_UNORM_X8_TYPELESS = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
        X24_TYPELESS_G8_UINT = DXGI_FORMAT_X24_TYPELESS_G8_UINT,

        Matrix4x4 = 150,
        Matrix3x4 = 151,

        Max
    };

    enum class FormatCompressionType
    {
        None, BlockCompression
    };

    enum class FormatComponents
    {
        Unknown,
        Alpha, 
        Luminance, LuminanceAlpha,
        RGB, RGBAlpha,
        RG, Depth, DepthStencil, RGBE
    };

    enum class FormatComponentType
    {
        Typeless,
        Float, UInt, SInt,
        UNorm, SNorm, UNorm_SRGB,
        Exponential,
        UnsignedFloat16, SignedFloat16
    };

    auto        GetCompressionType(Format format) -> FormatCompressionType;
    auto        GetComponents(Format format) -> FormatComponents;
    auto        GetComponentType(Format format) -> FormatComponentType;
    unsigned    BitsPerPixel(Format format);
    unsigned    GetComponentPrecision(Format format);
    unsigned    GetDecompressedComponentPrecision(Format format);
    unsigned    GetComponentCount(FormatComponents components);

	Format FindFormat(
        FormatCompressionType compression, 
        FormatComponents components,
        FormatComponentType componentType,
        unsigned precision);

    Format		AsSRGBFormat(Format inputFormat);
    Format		AsLinearFormat(Format inputFormat);
    Format		AsTypelessFormat(Format inputFormat);
    bool        HasLinearAndSRGBFormats(Format inputFormat);

    enum class ShaderNormalizationMode
    {
        Integer, Normalized, Float
    };

	Format AsFormat(
        const Utility::ImpliedTyping::TypeDesc& type,
        ShaderNormalizationMode norm = ShaderNormalizationMode::Integer);

    const char* AsString(Format);
	Format AsFormat(const char name[]);
}

