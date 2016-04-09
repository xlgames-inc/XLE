// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IncludeVulkan.h"      // just for VkFormat

namespace Utility { namespace ImpliedTyping { class TypeDesc; }}

namespace RenderCore { namespace Metal_Vulkan
{
    /// Container for NativeFormat::Enum
    namespace NativeFormat
    {
        enum Enum
        {
            Unknown = 0,

            #undef _EXP
            #define _EXP(X, Y, Z, U)    X##_##Y,
                #include "../../Metal/Detail/DXGICompatibleFormats.h"
            #undef _EXP

            R24G8_TYPELESS, // = DXGI_FORMAT_R24G8_TYPELESS,
            D24_UNORM_S8_UINT, // = DXGI_FORMAT_D24_UNORM_S8_UINT,
            R24_UNORM_X8_TYPELESS, // = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
            X24_TYPELESS_G8_UINT, // = DXGI_FORMAT_X24_TYPELESS_G8_UINT,

            Matrix4x4 = 150,
            Matrix3x4 = 151,

            Max
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
            Exponential,
            UnsignedFloat16, SignedFloat16
        };
    }

    auto        GetCompressionType(NativeFormat::Enum format) -> FormatCompressionType::Enum;
    auto        GetComponents(NativeFormat::Enum format) -> FormatComponents::Enum;
    auto        GetComponentType(NativeFormat::Enum format) -> FormatComponentType::Enum;
    unsigned    BitsPerPixel(NativeFormat::Enum format);
    unsigned    GetComponentPrecision(NativeFormat::Enum format);
    unsigned    GetDecompressedComponentPrecision(NativeFormat::Enum format);
    unsigned    GetComponentCount(FormatComponents::Enum components);

    NativeFormat::Enum FindFormat(
        FormatCompressionType::Enum compression, 
        FormatComponents::Enum components,
        FormatComponentType::Enum componentType,
        unsigned precision);

    NativeFormat::Enum      AsSRGBFormat(NativeFormat::Enum inputFormat);
    NativeFormat::Enum      AsLinearFormat(NativeFormat::Enum inputFormat);
    NativeFormat::Enum      AsTypelessFormat(NativeFormat::Enum inputFormat);
    bool                    HasLinearAndSRGBFormats(NativeFormat::Enum inputFormat);

    namespace ShaderNormalizationMode
    {
        enum Enum 
        {
            Integer, Normalized, Float
        };
    }

    NativeFormat::Enum      AsNativeFormat(
        const Utility::ImpliedTyping::TypeDesc& type,
        ShaderNormalizationMode::Enum norm = ShaderNormalizationMode::Integer);

    const char* AsString(NativeFormat::Enum);
    NativeFormat::Enum AsNativeFormat(const char name[]);

    VkFormat AsVkFormat(NativeFormat::Enum);
    void InitFormatConversionTables();
}}

