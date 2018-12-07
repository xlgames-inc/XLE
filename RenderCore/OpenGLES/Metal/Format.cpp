// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Format.h"
#include "../../../Utility/ParameterBox.h"
#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    enum class FormatPrefix
    {
        R32G32B32A32, R32G32B32, R16G16B16A16, R32G32,
        R10G10B10A2, R11G11B10,
        R8G8B8A8, R16G16, R32, D32,
        R8G8, R16, D16,
        R8, A8, A1, R1,
        R9G9B9E5, R8G8_B8G8, G8R8_G8B8,
        BC1, BC2, BC3, BC4, BC5, BC6H, BC7,
        B5G6R5, B5G5R5A1, B8G8R8A8, B8G8R8X8,
        R8G8B8, R4G4B4A4,

        RGB_PVRTC1_2BPP, RGB_PVRTC1_4BPP, RGBA_PVRTC1_2BPP, RGBA_PVRTC1_4BPP,
        RGBA_PVRTC2_2BPP, RGBA_PVRTC2_4BPP,
        RGB_ETC1, RGB_ETC2, RGBA_ETC2, RGBA1_ETC2,
        RGB_ATITC, RGBA_ATITC,

        Unknown
    };

    static FormatPrefix   GetPrefix(Format format)
    {
        switch (format) {
        #undef _EXP
        #define _EXP(X, Y, Z, U)    case Format::X##_##Y: return FormatPrefix::X;
            #include "../../Metal/Detail/DXGICompatibleFormats.h"
        #undef _EXP
        default: return FormatPrefix::R32G32B32A32;
        }
    }
    
    unsigned                    AsGLComponents(FormatComponents components)
    {
        switch (components) {
        case FormatComponents::Alpha:             return GL_ALPHA;
        case FormatComponents::Luminance:         return GL_LUMINANCE;
        case FormatComponents::LuminanceAlpha:    return GL_LUMINANCE_ALPHA;
        case FormatComponents::RGB:               return GL_RGB;
        default:
        case FormatComponents::RGBAlpha:          return GL_RGBA;
        case FormatComponents::Depth:             return GL_LUMINANCE;
        case FormatComponents::RG:                            
        case FormatComponents::RGBE:              return GL_RGB;  /* closest approx. */
        }
    }

    unsigned                    AsGLCompressionType(FormatCompressionType compressionType)
    {
        return 0;   // (requires querying device for capabilities)
    }

    unsigned                    AsGLComponentWidths(Format format)
    {
        auto prefix = GetPrefix(format);
        switch (prefix) {
        case FormatPrefix::R32G32B32A32:  
        case FormatPrefix::R32G32B32:       return GL_UNSIGNED_BYTE;
        default:                            return GL_UNSIGNED_BYTE;
        case FormatPrefix::B5G6R5:          return GL_UNSIGNED_SHORT_5_6_5;
        case FormatPrefix::B5G5R5A1:        return GL_UNSIGNED_SHORT_5_5_5_1;
        }
    }

    unsigned                    AsGLVertexComponentType(Format format)
    {
        auto type = GetComponentType(format);
        auto prefix = GetPrefix(format);
        switch (prefix) {
        case FormatPrefix::A8:
        case FormatPrefix::R8:
        case FormatPrefix::B8G8R8A8:
        case FormatPrefix::R8G8B8A8:
        case FormatPrefix::B8G8R8X8:
        case FormatPrefix::R8G8:
        case FormatPrefix::R8G8_B8G8:
        case FormatPrefix::G8R8_G8B8:
            return (type==FormatComponentType::SInt || type==FormatComponentType::SNorm) ? GL_BYTE : GL_UNSIGNED_BYTE;

        case FormatPrefix::R16:
        case FormatPrefix::D16:
        case FormatPrefix::R16G16:
        case FormatPrefix::R16G16B16A16:
            if (type==FormatComponentType::Float || type == FormatComponentType::UnsignedFloat16 || type == FormatComponentType::SignedFloat16) return GL_HALF_FLOAT;
            return (type==FormatComponentType::SInt || type==FormatComponentType::SNorm) ? GL_SHORT : GL_UNSIGNED_SHORT;

        case FormatPrefix::D32:
        case FormatPrefix::R32:
        case FormatPrefix::R32G32:
        case FormatPrefix::R32G32B32:
        case FormatPrefix::R32G32B32A32:
            if (type==FormatComponentType::Float) return GL_FLOAT;
            return (type==FormatComponentType::SInt || type==FormatComponentType::SNorm) ? GL_INT : GL_UNSIGNED_INT;

        case FormatPrefix::A1:
        case FormatPrefix::R1:
        case FormatPrefix::B5G5R5A1:
        case FormatPrefix::R10G10B10A2:
        case FormatPrefix::B5G6R5:
        case FormatPrefix::R11G11B10:
        case FormatPrefix::R9G9B9E5:
        case FormatPrefix::BC1:
        case FormatPrefix::BC2:
        case FormatPrefix::BC3:
        case FormatPrefix::BC4:
        case FormatPrefix::BC5:
        default:                return 0;       // (invalid for vertex buffers)
        }
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    static unsigned glUnsignedFmtForPrec(unsigned prec)
    {
        switch (prec) {
        case 8: return GL_UNSIGNED_BYTE;
        case 16: return GL_UNSIGNED_SHORT;
        case 32: return GL_UNSIGNED_INT;
        default:
            assert(0);
            return GL_UNSIGNED_INT;
        }
    }

    static unsigned glSignedFmtForPrec(unsigned prec)
    {
        switch (prec) {
        case 8: return GL_BYTE;
        case 16: return GL_SHORT;
        case 32: return GL_INT;
        default:
            assert(0);
            return GL_INT;
        }
    }

    std::tuple<GLint, GLenum, bool> AsVertexAttributePointer(Format fmt)
    {
        using namespace RenderCore;
        auto comps = GetComponents(fmt);
        auto compsType = GetComponentType(fmt);
        auto prec = GetComponentPrecision(fmt);

        std::tuple<GLenum, GLint, bool> result { 0, 0, false };

        switch (comps) {
        case FormatComponents::Alpha:
        case FormatComponents::Luminance:
        case FormatComponents::Stencil:
        case FormatComponents::Depth:
            std::get<0>(result) = 1;
            break;

        case FormatComponents::LuminanceAlpha:
        case FormatComponents::RG:
        case FormatComponents::DepthStencil:
            std::get<0>(result) = 2;
            break;

        case FormatComponents::RGB:
            std::get<0>(result) = 3;
            break;

        case FormatComponents::RGBAlpha:
        case FormatComponents::RGBE:
            std::get<0>(result) = 4;
            break;

        case FormatComponents::Unknown:
            break;
        }

        // GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT, GL_INT, and GL_UNSIGNED_INT
        // GL_HALF_FLOAT, GL_FLOAT, GL_FIXED, GL_INT_2_10_10_10_REV, and GL_UNSIGNED_INT_2_10_10_10_REV

        switch (compsType) {
        case FormatComponentType::Float: std::get<1>(result) = (prec >= 32)?GL_FLOAT:GL_HALF_FLOAT; break;
        case FormatComponentType::SignedFloat16: std::get<1>(result) = GL_FLOAT; break;
        case FormatComponentType::UInt: std::get<1>(result) = glUnsignedFmtForPrec(prec); break;
        case FormatComponentType::SInt: std::get<1>(result) = glSignedFmtForPrec(prec); break;

        case FormatComponentType::UNorm_SRGB:
        case FormatComponentType::UNorm: std::get<1>(result) = glUnsignedFmtForPrec(prec); std::get<2>(result) = true; break;
        case FormatComponentType::SNorm: std::get<1>(result) = glSignedFmtForPrec(prec); std::get<2>(result) = true; break;

        case FormatComponentType::UnsignedFloat16:
        case FormatComponentType::Exponential:
        case FormatComponentType::Typeless:
            break;
        }

        return result;
    }

    RenderCore::Format VertexAttributePointerAsFormat(GLint size, GLenum type, bool normalized)
    {
        // For OpenGLES 3:
        // glVertexAttribPointer + glVertexAttribIPointer: GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT, GL_INT, and GL_UNSIGNED_INT
        // just glVertexAttribPointer: GL_HALF_FLOAT, GL_FLOAT, GL_FIXED, GL_INT_2_10_10_10_REV, and GL_UNSIGNED_INT_2_10_10_10_REV
        using RenderCore::Format;
        if (size < 1 || size > 4) return Format::Unknown;

        switch (type)
        {
        case GL_BYTE:
            if (normalized) return (Format[]){ Format::R8_SNORM, Format::R8G8_SNORM, Format::Unknown, Format::R8G8B8A8_SNORM }[size-1];
            else            return (Format[]){ Format::R8_SINT, Format::R8G8_SINT, Format::Unknown, Format::R8G8B8A8_SINT }[size-1];
            break;

        case GL_UNSIGNED_BYTE:
            if (normalized) return (Format[]){ Format::R8_UNORM, Format::R8G8_UNORM, Format::Unknown, Format::R8G8B8A8_UNORM }[size-1];
            else            return (Format[]){ Format::R8_UINT, Format::R8G8_UINT, Format::Unknown, Format::R8G8B8A8_UINT }[size-1];
            break;

        case GL_SHORT:
            if (normalized) return (Format[]){ Format::R16_SNORM, Format::R16G16_SNORM, Format::Unknown, Format::R16G16B16A16_SNORM }[size-1];
            else            return (Format[]){ Format::R16_SINT, Format::R16G16_SINT, Format::Unknown, Format::R16G16B16A16_SINT }[size-1];
            break;

        case GL_UNSIGNED_SHORT:
            if (normalized) return (Format[]){ Format::R16_UNORM, Format::R16G16_UNORM, Format::Unknown, Format::R16G16B16A16_UNORM }[size-1];
            else            return (Format[]){ Format::R16_UINT, Format::R16G16_UINT, Format::Unknown, Format::R16G16B16A16_UINT }[size-1];
            break;

        case GL_INT:
            if (!normalized) return (Format[]){ Format::R32_SINT, Format::R32G32_SINT, Format::R32G32B32_SINT, Format::R32G32B32A32_SINT }[size-1];
            break;

        case GL_UNSIGNED_INT:
            if (!normalized) return (Format[]){ Format::R32_UINT, Format::R32G32_UINT, Format::R32G32B32_UINT, Format::R32G32B32A32_UINT }[size-1];
            break;

        case GL_HALF_FLOAT:
            if (!normalized) return (Format[]){ Format::R16_FLOAT, Format::R16G16_FLOAT, Format::Unknown, Format::R16G16B16A16_FLOAT }[size-1];
            break;

        case GL_FLOAT:
            if (!normalized) return (Format[]){ Format::R32_FLOAT, Format::R32G32_FLOAT, Format::R32G32B32_FLOAT, Format::R32G32B32A32_FLOAT }[size-1];
            break;

        case GL_FIXED:
            if (!normalized) return (Format[]){ Format::R32_SINT, Format::R32G32_SINT, Format::R32G32B32_SINT, Format::R32G32B32A32_SINT }[size-1];
            break;

        case GL_INT_2_10_10_10_REV:
            break;

        case GL_UNSIGNED_INT_2_10_10_10_REV:
            if (normalized) return (Format[]){ Format::Unknown, Format::Unknown, Format::Unknown, Format::R10G10B10A2_UNORM }[size-1];
            else            return (Format[]){ Format::Unknown, Format::Unknown, Format::Unknown, Format::R10G10B10A2_UINT }[size-1];
            break;
        }

        return Format::Unknown;
    }

    #ifndef GL_IMG_texture_compression_pvrtc2
    #define GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG 0x9137
    #define GL_COMPRESSED_RGBA_PVRTC_4BPPV2_IMG 0x9138
    #endif /* GL_IMG_texture_compression_pvrtc2 */

    #ifndef GL_OES_compressed_ETC1_RGB8_texture
    #define GL_ETC1_RGB8_OES                  0x8D64
	#endif /* GL_OES_compressed_ETC1_RGB8_texture */

    #ifndef GL_AMD_compressed_ATC_texture
    #define GL_ATC_RGB_AMD                                          0x8C92
    #define GL_ATC_RGBA_EXPLICIT_ALPHA_AMD                          0x8C93
    #define GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD                      0x87EE
    #endif /* GL_AMD_compressed_ATC_texture */

#pragma clang diagnostic ignored "-Wswitch" // (enumeration values not used)

    glPixelFormat AsTexelFormatType(RenderCore::Format fmt)
    {
        using namespace RenderCore;
        switch (fmt)
        {
        case Format::R32G32B32A32_FLOAT: return {GL_RGBA, GL_FLOAT, GL_RGBA32F, FeatureSet::GLES300, 0};
        case Format::R32G32B32A32_UINT: return {GL_RGBA_INTEGER, GL_UNSIGNED_INT, GL_RGBA32UI, FeatureSet::GLES300, 0};
        case Format::R32G32B32A32_SINT: return {GL_RGBA_INTEGER, GL_INT, GL_RGBA32I, FeatureSet::GLES300, 0};

        case Format::R32G32B32_FLOAT: return {GL_RGB, GL_FLOAT, GL_RGB32F, FeatureSet::GLES300, 0};
        case Format::R32G32B32_UINT: return {GL_RGB_INTEGER, GL_UNSIGNED_INT, GL_RGB32UI, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R32G32B32_SINT: return {GL_RGB_INTEGER, GL_INT, GL_RGB32I, FeatureSet::GLES300, FeatureSet::GLES300};

        case Format::R16G16B16A16_FLOAT: return {GL_RGBA, GL_HALF_FLOAT, GL_RGBA16F, FeatureSet::GLES300, 0};
        case Format::R16G16B16A16_UNORM: return {GL_RGBA, GL_UNSIGNED_SHORT, 0, FeatureSet::GLES300, 0};
        case Format::R16G16B16A16_UINT: return {GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, GL_RGBA16UI, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R16G16B16A16_SNORM: return {GL_RGBA, GL_SHORT, 0, FeatureSet::GLES300, 0};
        case Format::R16G16B16A16_SINT: return {GL_RGBA_INTEGER, GL_SHORT, GL_RGBA16I, FeatureSet::GLES300, FeatureSet::GLES300};

        case Format::R32G32_FLOAT: return {GL_RG, GL_FLOAT, GL_RG32F, FeatureSet::GLES300, 0};
        case Format::R32G32_UINT: return {GL_RG_INTEGER, GL_UNSIGNED_INT, GL_RG32UI, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R32G32_SINT: return {GL_RG_INTEGER, GL_INT, GL_RG32I, FeatureSet::GLES300, FeatureSet::GLES300};

        case Format::R10G10B10A2_UNORM: return {GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, GL_RGB10_A2, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R10G10B10A2_UINT: return {GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, GL_RGB10_A2UI, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R11G11B10_FLOAT: return {GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, GL_R11F_G11F_B10F, FeatureSet::GLES300, 0};

        case Format::R8G8B8A8_UNORM: return {GL_RGBA, GL_UNSIGNED_BYTE, GL_RGBA8, FeatureSet::GLES200, FeatureSet::GLES300};
        case Format::R8G8B8A8_UINT: return {GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, GL_RGBA8UI, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R8G8B8A8_SNORM: return {GL_RGBA, GL_BYTE, GL_RGBA8_SNORM, FeatureSet::GLES300, 0};
        case Format::R8G8B8A8_SINT: return {GL_RGBA_INTEGER, GL_BYTE, GL_RGBA8I, FeatureSet::GLES300, FeatureSet::GLES300};

        case Format::R16G16_FLOAT: return {GL_RG, GL_HALF_FLOAT, GL_RG16F, FeatureSet::GLES300, 0};
        case Format::R16G16_UNORM: return {GL_RG, GL_UNSIGNED_SHORT, 0, FeatureSet::GLES300, 0};
        case Format::R16G16_UINT: return {GL_RG_INTEGER, GL_UNSIGNED_SHORT, GL_RG16UI, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R16G16_SNORM: return {GL_RG, GL_SHORT, 0, FeatureSet::GLES300, 0};
        case Format::R16G16_SINT: return {GL_RG_INTEGER, GL_SHORT, GL_RG16I, FeatureSet::GLES300, FeatureSet::GLES300};

        case Format::D32_FLOAT: return {GL_DEPTH_COMPONENT, GL_FLOAT, GL_DEPTH_COMPONENT32F, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R32_FLOAT: return {GL_RED, GL_FLOAT, GL_R32F, FeatureSet::GLES300, 0};
        case Format::R32_UINT: return {GL_RED_INTEGER, GL_UNSIGNED_INT, GL_R32UI, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R32_SINT: return {GL_RED_INTEGER, GL_INT, GL_R32I, FeatureSet::GLES300, FeatureSet::GLES300};

        case Format::R8G8B8_UNORM: return {GL_RGB, GL_UNSIGNED_BYTE, GL_RGB8, FeatureSet::GLES200, FeatureSet::GLES300};
        case Format::R8G8B8_UINT: return {GL_RGB_INTEGER, GL_UNSIGNED_BYTE, GL_RGB8UI, FeatureSet::GLES300, 0};
        case Format::R8G8B8_SNORM: return {GL_RGB, GL_BYTE, GL_RGB8_SNORM, FeatureSet::GLES300, 0};
        case Format::R8G8B8_SINT: return {GL_RGB_INTEGER, GL_BYTE, GL_RGB8I, FeatureSet::GLES300, 0};

        case Format::R8G8_UNORM: return {GL_RG, GL_UNSIGNED_BYTE, GL_RG8, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R8G8_UINT: return {GL_RG_INTEGER, GL_UNSIGNED_BYTE, GL_RG8UI, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R8G8_SNORM: return {GL_RG, GL_BYTE, GL_RG8_SNORM, FeatureSet::GLES300, 0};
        case Format::R8G8_SINT: return {GL_RG_INTEGER, GL_BYTE, GL_RG8I, FeatureSet::GLES300, FeatureSet::GLES300};

        case Format::R16_FLOAT: return {GL_RED, GL_HALF_FLOAT, GL_R16F, FeatureSet::GLES300, 0};
        case Format::D16_UNORM: return {GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, GL_DEPTH_COMPONENT16, FeatureSet::GLES300, FeatureSet::GLES200};
        case Format::R16_UNORM: return {GL_RED, GL_UNSIGNED_SHORT, 0, FeatureSet::GLES300, 0};
        case Format::R16_UINT: return {GL_RED_INTEGER, GL_UNSIGNED_SHORT, GL_R16UI, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R16_SNORM: return {GL_RED, GL_SHORT, 0, FeatureSet::GLES300, 0};
        case Format::R16_SINT: return {GL_RED_INTEGER, GL_SHORT, GL_R16I, FeatureSet::GLES300, FeatureSet::GLES300};

        case Format::R8_UNORM: return {GL_RED, GL_UNSIGNED_BYTE, GL_R8, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R8_UINT: return {GL_RED_INTEGER, GL_UNSIGNED_BYTE, GL_R8UI, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R8_SNORM: return {GL_RED, GL_BYTE, GL_R8_SNORM, FeatureSet::GLES300, 0};
        case Format::R8_SINT: return {GL_RED_INTEGER, GL_BYTE, GL_R8I, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::A8_UNORM: return {GL_ALPHA, GL_UNSIGNED_BYTE, GL_ALPHA, FeatureSet::GLES200, FeatureSet::GLES300};

        case Format::R9G9B9E5_SHAREDEXP: return {GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, GL_RGB9_E5, FeatureSet::GLES300, 0};
        case Format::B5G6R5_UNORM: return {GL_RGB, GL_UNSIGNED_SHORT_5_6_5, GL_RGB565, FeatureSet::GLES200, FeatureSet::GLES200};
        case Format::B5G5R5A1_UNORM: return {GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, GL_RGB5_A1, FeatureSet::GLES200, FeatureSet::GLES200};
        case Format::R4G4B4A4_UNORM: return {GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, GL_RGBA4, FeatureSet::GLES200, FeatureSet::GLES200};

        case Format::D24_UNORM_S8_UINT: return {GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_DEPTH24_STENCIL8, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::D32_SFLOAT_S8_UINT: return {GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, GL_DEPTH32F_STENCIL8, FeatureSet::GLES300, FeatureSet::GLES300};

        case Format::R8G8B8A8_UNORM_SRGB: return {GL_RGBA, GL_BYTE, GL_SRGB8_ALPHA8, FeatureSet::GLES300, FeatureSet::GLES300};
        case Format::R8G8B8_UNORM_SRGB: return {GL_RGBA, GL_BYTE, GL_SRGB8, FeatureSet::GLES300, 0};

        case Format::RGB_PVRTC1_2BPP_UNORM: return {0, 0, GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG, FeatureSet::PVRTC, 0};
        case Format::RGBA_PVRTC1_2BPP_UNORM: return {0, 0, GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG, FeatureSet::PVRTC, 0};
        case Format::RGB_PVRTC1_4BPP_UNORM: return {0, 0, GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG, FeatureSet::PVRTC, 0};
        case Format::RGBA_PVRTC1_4BPP_UNORM: return {0, 0, GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, FeatureSet::PVRTC, 0};
        case Format::RGBA_PVRTC2_2BPP_UNORM: return {0, 0, GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG, FeatureSet::PVRTC, 0};
        case Format::RGBA_PVRTC2_4BPP_UNORM: return {0, 0, GL_COMPRESSED_RGBA_PVRTC_4BPPV2_IMG, FeatureSet::PVRTC, 0};

        case Format::RGB_ETC1_UNORM: return {0, 0, GL_ETC1_RGB8_OES, FeatureSet::ETC1TC, 0};

        case Format::RGB_ETC2_UNORM: return {0, 0, GL_COMPRESSED_RGB8_ETC2, FeatureSet::ETC2TC, 0};
        case Format::RGBA_ETC2_UNORM: return {0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, FeatureSet::ETC2TC, 0};
        case Format::RGBA1_ETC2_UNORM: return {0, 0, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, FeatureSet::ETC2TC, 0};

        case Format::RGB_ETC2_UNORM_SRGB: return {0, 0, GL_COMPRESSED_SRGB8_ETC2, FeatureSet::ETC2TC, 0};
        case Format::RGBA_ETC2_UNORM_SRGB: return {0, 0, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC, FeatureSet::ETC2TC, 0};
        case Format::RGBA1_ETC2_UNORM_SRGB: return {0, 0, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, FeatureSet::ETC2TC, 0};

        case Format::RGB_ATITC_UNORM: return {0, 0, GL_ATC_RGB_AMD, FeatureSet::ATITC, 0};
        case Format::RGBA_ATITC_UNORM: return {0, 0, GL_ATC_RGBA_EXPLICIT_ALPHA_AMD, FeatureSet::ATITC, 0};        // see also ATC_RGBA_INTERPOLATED_ALPHA_AMD

        case Format::RGB_PVRTC1_2BPP_UNORM_SRGB:
        case Format::RGBA_PVRTC1_2BPP_UNORM_SRGB:
        case Format::RGB_PVRTC1_4BPP_UNORM_SRGB:
        case Format::RGBA_PVRTC1_4BPP_UNORM_SRGB:
        case Format::RGBA_PVRTC2_2BPP_UNORM_SRGB:
        case Format::RGBA_PVRTC2_4BPP_UNORM_SRGB:
        case Format::RGB_ETC1_UNORM_SRGB:
            break;
        }

        return {0, 0, 0};
    }

    ImpliedTyping::TypeDesc GLUniformTypeAsTypeDesc(GLenum glType)
    {
        switch (glType) {
        case GL_FLOAT:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Float);
        case GL_FLOAT_VEC2:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Float, 2, ImpliedTyping::TypeHint::Vector);
        case GL_FLOAT_VEC3:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Float, 3, ImpliedTyping::TypeHint::Vector);
        case GL_FLOAT_VEC4:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Float, 4, ImpliedTyping::TypeHint::Vector);

        case GL_FLOAT_MAT2:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Float, 4, ImpliedTyping::TypeHint::Matrix);
        case GL_FLOAT_MAT3:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Float, 9, ImpliedTyping::TypeHint::Matrix);
        case GL_FLOAT_MAT4:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Float, 16, ImpliedTyping::TypeHint::Matrix);

        case GL_SAMPLER_2D:
        case GL_SAMPLER_CUBE:
        case GL_SAMPLER_2D_SHADOW:
        case GL_INT_SAMPLER_2D:
        case GL_UNSIGNED_INT_SAMPLER_2D:
        case GL_INT:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Int32);

        case GL_INT_VEC2:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Int32, 2);

        case GL_INT_VEC3:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Int32, 3);

        case GL_INT_VEC4:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Int32, 4);

        case GL_BOOL:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Bool);

        case GL_BOOL_VEC2:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Bool, 2, ImpliedTyping::TypeHint::Vector);

        case GL_BOOL_VEC3:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Bool, 3, ImpliedTyping::TypeHint::Vector);

        case GL_BOOL_VEC4:
            return ImpliedTyping::TypeDesc(ImpliedTyping::TypeCat::Bool, 4, ImpliedTyping::TypeHint::Vector);
        }
        return ImpliedTyping::TypeDesc();
    }

    Format SizedInternalFormatAsRenderCoreFormat(GLenum sizedInternalFormat)
    {
        switch (sizedInternalFormat) {
        case GL_R8: return Format::R8_UNORM;
        case GL_R8_SNORM: return Format::R8_SNORM;
        case GL_R16F: return Format::R16_FLOAT;
        case GL_R32F: return Format::R32_FLOAT;

        case GL_R8UI: return Format::R8_UINT;
        case GL_R8I: return Format::R8_SINT;
        case GL_R16UI: return Format::R16_UINT;
        case GL_R16I: return Format::R16_SINT;
        case GL_R32UI: return Format::R32_UINT;
        case GL_R32I: return Format::R32_SINT;

        case GL_RG8: return Format::R8G8_UNORM;
        case GL_RG8_SNORM: return Format::R8G8_SNORM;
        case GL_RG16F: return Format::R16G16_FLOAT;
        case GL_RG32F: return Format::R32G32_FLOAT;
        case GL_RG8UI: return Format::R8G8_UINT;
        case GL_RG8I: return Format::R8G8_SINT;
        case GL_RG16UI: return Format::R16G16_UINT;
        case GL_RG16I: return Format::R16G16_SINT;
        case GL_RG32UI: return Format::R32G32_UINT;
        case GL_RG32I: return Format::R32G32_SINT;

        case GL_RGB8: return Format::R8G8B8_UNORM;
        case GL_SRGB8: return Format::R8G8B8_UNORM_SRGB;
        // case GL_RGB565: return Format::R5G6B5_UNORM;
        case GL_RGB8_SNORM: return Format::R8G8B8_SNORM;
        case GL_R11F_G11F_B10F: return Format::R11G11B10_FLOAT;
        case GL_RGB9_E5: return Format::R9G9B9E5_SHAREDEXP;
        // case GL_RGB16F: return Format::R16G16B16_FLOAT;
        case GL_RGB32F: return Format::R32G32B32_FLOAT;
        case GL_RGB8UI: return Format::R8G8B8_UINT;
        case GL_RGB8I: return Format::R8G8B8_SINT;
        // case GL_RGB16UI: return Format::R16G16B16_UINT;
        // case GL_RGB16I: return Format::R16G16B16_SINT;
        case GL_RGB32UI: return Format::R32G32B32_UINT;
        case GL_RGB32I: return Format::R32G32B32_SINT;

        case GL_RGBA8: return Format::R8G8B8A8_UNORM;
        case GL_SRGB8_ALPHA8: return Format::R8G8B8A8_UNORM_SRGB;
        case GL_RGBA8_SNORM: return Format::R8G8B8A8_SNORM;
        case GL_RGB5_A1: return Format::B5G5R5A1_UNORM;
        case GL_RGBA4: return Format::R4G4B4A4_UNORM;
        case GL_RGB10_A2: return Format::R10G10B10A2_UNORM;
        case GL_RGBA16F: return Format::R16G16B16A16_FLOAT;
        case GL_RGBA32F: return Format::R32G32B32A32_FLOAT;
        case GL_RGBA8UI: return Format::R8G8B8A8_UINT;
        case GL_RGBA8I: return Format::R8G8B8A8_SINT;
        case GL_RGB10_A2UI: return Format::R10G10B10A2_UINT;
        case GL_RGBA16UI: return Format::R16G16B16A16_UINT;
        case GL_RGBA16I: return Format::R16G16B16A16_SINT;
        case GL_RGBA32I: return Format::R32G32B32A32_SINT;
        case GL_RGBA32UI: return Format::R32G32B32A32_UINT;

        case GL_DEPTH_COMPONENT16: return Format::D16_UNORM;
        // case GL_DEPTH_COMPONENT24: return Format::R24_UNORM_X8_TYPELESS;
        case GL_DEPTH_COMPONENT32F: return Format::D32_FLOAT;
        case GL_DEPTH24_STENCIL8: return Format::D24_UNORM_S8_UINT;
        case GL_DEPTH32F_STENCIL8: return Format::D32_SFLOAT_S8_UINT;

        default: return Format::Unknown;
        }
    }

    GLenum AsGLenum(RenderCore::Topology topology)
    {
        // GL_LINE_LOOP, GL_TRIANGLE_FAN not accessible
        switch (topology)
        {
        case Topology::PointList: return GL_POINTS;
        case Topology::LineList: return GL_LINES;
        case Topology::LineStrip: return GL_LINE_STRIP;
        case Topology::TriangleList: return GL_TRIANGLES;
        case Topology::TriangleStrip: return GL_TRIANGLE_STRIP;
        default: return GL_ZERO;
        }
    }

    GLenum AsGLenum(Blend input)
    {
        switch (input)
        {
        case Blend::Zero: return GL_ZERO;
        case Blend::One: return GL_ONE;

        case Blend::SrcColor: return GL_SRC_COLOR;
        case Blend::InvSrcColor: return GL_ONE_MINUS_SRC_COLOR;
        case Blend::DestColor: return GL_DST_COLOR;
        case Blend::InvDestColor: return GL_ONE_MINUS_DST_COLOR;

        case Blend::SrcAlpha: return GL_SRC_ALPHA;
        case Blend::InvSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        case Blend::DestAlpha: return GL_DST_ALPHA;
        case Blend::InvDestAlpha: return GL_ONE_MINUS_DST_ALPHA;

        default: return GL_ZERO;
        }
    }

    Blend AsBlend(GLenum blend)
    {
        switch (blend)
        {
        case GL_ZERO: return Blend::Zero;
        case GL_ONE: return Blend::One;

        case GL_SRC_COLOR: return Blend::SrcColor;
        case GL_ONE_MINUS_SRC_COLOR: return Blend::InvSrcColor;
        case GL_DST_COLOR: return Blend::DestColor;
        case GL_ONE_MINUS_DST_COLOR: return Blend::InvDestColor;

        case GL_SRC_ALPHA: return Blend::SrcAlpha;
        case GL_ONE_MINUS_SRC_ALPHA: return Blend::InvSrcAlpha;
        case GL_DST_ALPHA: return Blend::DestAlpha;
        case GL_ONE_MINUS_DST_ALPHA: return Blend::InvDestAlpha;

        default: return Blend::Zero;
        }
    }

    GLenum AsGLenum(CullMode cullMode)
    {
        // GL_FRONT_AND_BACK can't be returned from this function (because doesn't seem useful)
        switch (cullMode) {
        case CullMode::Front: return GL_FRONT;
        case CullMode::Back: return GL_BACK;

        case CullMode::None:
        default:
            assert(0);
            return GL_BACK;
        }
    }

    GLenum AsGLenum(FaceWinding faceWinding)
    {
        switch (faceWinding) {
        case FaceWinding::CCW: return GL_CCW;
        case FaceWinding::CW: return GL_CW;
        default:
            assert(0);
            return GL_CCW;
        }
    }

    GLenum AsGLenum(StencilOp stencilOp)
    {
        switch (stencilOp) {
        case StencilOp::Keep: return GL_KEEP;
        case StencilOp::Zero: return GL_ZERO;
        case StencilOp::Replace: return GL_REPLACE;
        case StencilOp::IncreaseSat: return GL_INCR;
        case StencilOp::DecreaseSat: return GL_DECR;
        case StencilOp::Invert: return GL_INVERT;
        case StencilOp::Increase: return GL_INCR_WRAP;
        case StencilOp::Decrease: return GL_DECR_WRAP;
        default: return GL_KEEP;
        }
    }
    
    StencilOp AsStencilOp(GLenum stencilOp)
    {
        switch (stencilOp) {
        case GL_KEEP: return StencilOp::Keep;
        case GL_ZERO: return StencilOp::Zero;
        case GL_REPLACE: return StencilOp::Replace;
        case GL_INCR: return StencilOp::IncreaseSat;
        case GL_DECR: return StencilOp::DecreaseSat;
        case GL_INVERT: return StencilOp::Invert;
        case GL_INCR_WRAP: return StencilOp::Increase;
        case GL_DECR_WRAP: return StencilOp::Decrease;
        default: return StencilOp::Keep;
        }
    }

    GLenum AsGLenum(BlendOp blendOp)
    {
        switch (blendOp) {
            case BlendOp::Add: return GL_FUNC_ADD;
            case BlendOp::Subtract: return GL_FUNC_SUBTRACT;
            case BlendOp::RevSubtract: return GL_FUNC_REVERSE_SUBTRACT;
            case BlendOp::Min: return GL_MIN;
            case BlendOp::Max: return GL_MAX;
            default: return GL_INVALID_ENUM;
        }
    }

    const char* GLenumAsString(GLenum value)
    {
#if defined(GL_ES_VERSION_3_0)
        // Should contain all standard GLES 3.0 enums, but no extensions
        // Note that some enums overlap; in these cases the one that wins hasn't been particularly
        // carefully selected.
        switch (value) {
        case GL_DEPTH_BUFFER_BIT: return "GL_DEPTH_BUFFER_BIT";
        case GL_STENCIL_BUFFER_BIT: return "GL_STENCIL_BUFFER_BIT";
        case GL_COLOR_BUFFER_BIT: return "GL_COLOR_BUFFER_BIT";
        case GL_POINTS: return "GL_POINTS";
        case GL_LINES: return "GL_LINES";
        case GL_LINE_LOOP: return "GL_LINE_LOOP";
        case GL_LINE_STRIP: return "GL_LINE_STRIP";
        // case GL_TRIANGLES: return "GL_TRIANGLES";
        case GL_TRIANGLE_STRIP: return "GL_TRIANGLE_STRIP";
        case GL_TRIANGLE_FAN: return "GL_TRIANGLE_FAN";
        case GL_SRC_COLOR: return "GL_SRC_COLOR";
        case GL_ONE_MINUS_SRC_COLOR: return "GL_ONE_MINUS_SRC_COLOR";
        case GL_SRC_ALPHA: return "GL_SRC_ALPHA";
        case GL_ONE_MINUS_SRC_ALPHA: return "GL_ONE_MINUS_SRC_ALPHA";
        case GL_DST_ALPHA: return "GL_DST_ALPHA";
        case GL_ONE_MINUS_DST_ALPHA: return "GL_ONE_MINUS_DST_ALPHA";
        case GL_DST_COLOR: return "GL_DST_COLOR";
        case GL_ONE_MINUS_DST_COLOR: return "GL_ONE_MINUS_DST_COLOR";
        case GL_SRC_ALPHA_SATURATE: return "GL_SRC_ALPHA_SATURATE";
        case GL_FUNC_ADD: return "GL_FUNC_ADD";
        // case GL_BLEND_EQUATION: return "GL_BLEND_EQUATION";
        // case GL_BLEND_EQUATION_RGB: return "GL_BLEND_EQUATION_RGB";
        case GL_BLEND_EQUATION_ALPHA: return "GL_BLEND_EQUATION_ALPHA";
        case GL_FUNC_SUBTRACT: return "GL_FUNC_SUBTRACT";
        case GL_FUNC_REVERSE_SUBTRACT: return "GL_FUNC_REVERSE_SUBTRACT";
        case GL_BLEND_DST_RGB: return "GL_BLEND_DST_RGB";
        case GL_BLEND_SRC_RGB: return "GL_BLEND_SRC_RGB";
        case GL_BLEND_DST_ALPHA: return "GL_BLEND_DST_ALPHA";
        case GL_BLEND_SRC_ALPHA: return "GL_BLEND_SRC_ALPHA";
        case GL_CONSTANT_COLOR: return "GL_CONSTANT_COLOR";
        case GL_ONE_MINUS_CONSTANT_COLOR: return "GL_ONE_MINUS_CONSTANT_COLOR";
        case GL_CONSTANT_ALPHA: return "GL_CONSTANT_ALPHA";
        case GL_ONE_MINUS_CONSTANT_ALPHA: return "GL_ONE_MINUS_CONSTANT_ALPHA";
        case GL_BLEND_COLOR: return "GL_BLEND_COLOR";
        case GL_ARRAY_BUFFER: return "GL_ARRAY_BUFFER";
        case GL_ELEMENT_ARRAY_BUFFER: return "GL_ELEMENT_ARRAY_BUFFER";
        case GL_ARRAY_BUFFER_BINDING: return "GL_ARRAY_BUFFER_BINDING";
        case GL_ELEMENT_ARRAY_BUFFER_BINDING: return "GL_ELEMENT_ARRAY_BUFFER_BINDING";

        case GL_STREAM_DRAW: return "GL_STREAM_DRAW";
        case GL_STATIC_DRAW: return "GL_STATIC_DRAW";
        case GL_DYNAMIC_DRAW: return "GL_DYNAMIC_DRAW";

        case GL_BUFFER_SIZE: return "GL_BUFFER_SIZE";
        case GL_BUFFER_USAGE: return "GL_BUFFER_USAGE";

        case GL_CURRENT_VERTEX_ATTRIB: return "GL_CURRENT_VERTEX_ATTRIB";

        case GL_FRONT: return "GL_FRONT";
        case GL_BACK: return "GL_BACK";
        case GL_FRONT_AND_BACK: return "GL_FRONT_AND_BACK";

        case GL_TEXTURE_2D: return "GL_TEXTURE_2D";
        case GL_CULL_FACE: return "GL_CULL_FACE";
        case GL_BLEND: return "GL_BLEND";
        case GL_DITHER: return "GL_DITHER";
        case GL_STENCIL_TEST: return "GL_STENCIL_TEST";
        case GL_DEPTH_TEST: return "GL_DEPTH_TEST";
        case GL_SCISSOR_TEST: return "GL_SCISSOR_TEST";
        case GL_POLYGON_OFFSET_FILL: return "GL_POLYGON_OFFSET_FILL";
        case GL_SAMPLE_ALPHA_TO_COVERAGE: return "GL_SAMPLE_ALPHA_TO_COVERAGE";
        case GL_SAMPLE_COVERAGE: return "GL_SAMPLE_COVERAGE";

        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";

        case GL_CW: return "GL_CW";
        case GL_CCW: return "GL_CCW";

        case GL_LINE_WIDTH: return "GL_LINE_WIDTH";
        case GL_ALIASED_POINT_SIZE_RANGE: return "GL_ALIASED_POINT_SIZE_RANGE";
        case GL_ALIASED_LINE_WIDTH_RANGE: return "GL_ALIASED_LINE_WIDTH_RANGE";
        case GL_CULL_FACE_MODE: return "GL_CULL_FACE_MODE";
        case GL_FRONT_FACE: return "GL_FRONT_FACE";
        case GL_DEPTH_RANGE: return "GL_DEPTH_RANGE";
        case GL_DEPTH_WRITEMASK: return "GL_DEPTH_WRITEMASK";
        case GL_DEPTH_CLEAR_VALUE: return "GL_DEPTH_CLEAR_VALUE";
        case GL_DEPTH_FUNC: return "GL_DEPTH_FUNC";
        case GL_STENCIL_CLEAR_VALUE: return "GL_STENCIL_CLEAR_VALUE";
        case GL_STENCIL_FUNC: return "GL_STENCIL_FUNC";
        case GL_STENCIL_FAIL: return "GL_STENCIL_FAIL";
        case GL_STENCIL_PASS_DEPTH_FAIL: return "GL_STENCIL_PASS_DEPTH_FAIL";
        case GL_STENCIL_PASS_DEPTH_PASS: return "GL_STENCIL_PASS_DEPTH_PASS";
        case GL_STENCIL_REF: return "GL_STENCIL_REF";
        case GL_STENCIL_VALUE_MASK: return "GL_STENCIL_VALUE_MASK";
        case GL_STENCIL_WRITEMASK: return "GL_STENCIL_WRITEMASK";
        case GL_STENCIL_BACK_FUNC: return "GL_STENCIL_BACK_FUNC";
        case GL_STENCIL_BACK_FAIL: return "GL_STENCIL_BACK_FAIL";
        case GL_STENCIL_BACK_PASS_DEPTH_FAIL: return "GL_STENCIL_BACK_PASS_DEPTH_FAIL";
        case GL_STENCIL_BACK_PASS_DEPTH_PASS: return "GL_STENCIL_BACK_PASS_DEPTH_PASS";
        case GL_STENCIL_BACK_REF: return "GL_STENCIL_BACK_REF";
        case GL_STENCIL_BACK_VALUE_MASK: return "GL_STENCIL_BACK_VALUE_MASK";
        case GL_STENCIL_BACK_WRITEMASK: return "GL_STENCIL_BACK_WRITEMASK";
        case GL_VIEWPORT: return "GL_VIEWPORT";
        case GL_SCISSOR_BOX: return "GL_SCISSOR_BOX";
        case GL_COLOR_CLEAR_VALUE: return "GL_COLOR_CLEAR_VALUE";
        case GL_COLOR_WRITEMASK: return "GL_COLOR_WRITEMASK";
        case GL_UNPACK_ALIGNMENT: return "GL_UNPACK_ALIGNMENT";
        case GL_PACK_ALIGNMENT: return "GL_PACK_ALIGNMENT";
        case GL_MAX_TEXTURE_SIZE: return "GL_MAX_TEXTURE_SIZE";
        case GL_MAX_VIEWPORT_DIMS: return "GL_MAX_VIEWPORT_DIMS";
        case GL_SUBPIXEL_BITS: return "GL_SUBPIXEL_BITS";
        case GL_RED_BITS: return "GL_RED_BITS";
        case GL_GREEN_BITS: return "GL_GREEN_BITS";
        case GL_BLUE_BITS: return "GL_BLUE_BITS";
        case GL_ALPHA_BITS: return "GL_ALPHA_BITS";
        case GL_DEPTH_BITS: return "GL_DEPTH_BITS";
        case GL_STENCIL_BITS: return "GL_STENCIL_BITS";
        case GL_POLYGON_OFFSET_UNITS: return "GL_POLYGON_OFFSET_UNITS";
        case GL_POLYGON_OFFSET_FACTOR: return "GL_POLYGON_OFFSET_FACTOR";
        case GL_TEXTURE_BINDING_2D: return "GL_TEXTURE_BINDING_2D";
        case GL_SAMPLE_BUFFERS: return "GL_SAMPLE_BUFFERS";
        case GL_SAMPLES: return "GL_SAMPLES";
        case GL_SAMPLE_COVERAGE_VALUE: return "GL_SAMPLE_COVERAGE_VALUE";
        case GL_SAMPLE_COVERAGE_INVERT: return "GL_SAMPLE_COVERAGE_INVERT";

        case GL_NUM_COMPRESSED_TEXTURE_FORMATS: return "GL_NUM_COMPRESSED_TEXTURE_FORMATS";
        case GL_COMPRESSED_TEXTURE_FORMATS: return "GL_COMPRESSED_TEXTURE_FORMATS";

        case GL_DONT_CARE: return "GL_DONT_CARE";
        case GL_FASTEST: return "GL_FASTEST";
        case GL_NICEST: return "GL_NICEST";

        case GL_GENERATE_MIPMAP_HINT: return "GL_GENERATE_MIPMAP_HINT";

        case GL_BYTE: return "GL_BYTE";
        case GL_UNSIGNED_BYTE: return "GL_UNSIGNED_BYTE";
        case GL_SHORT: return "GL_SHORT";
        case GL_UNSIGNED_SHORT: return "GL_UNSIGNED_SHORT";
        case GL_INT: return "GL_INT";
        case GL_UNSIGNED_INT: return "GL_UNSIGNED_INT";
        case GL_FLOAT: return "GL_FLOAT";
        case GL_FIXED: return "GL_FIXED";

        case GL_DEPTH_COMPONENT: return "GL_DEPTH_COMPONENT";
        case GL_ALPHA: return "GL_ALPHA";
        case GL_RGB: return "GL_RGB";
        case GL_RGBA: return "GL_RGBA";
        case GL_LUMINANCE: return "GL_LUMINANCE";
        case GL_LUMINANCE_ALPHA: return "GL_LUMINANCE_ALPHA";

        case GL_UNSIGNED_SHORT_4_4_4_4: return "GL_UNSIGNED_SHORT_4_4_4_4";
        case GL_UNSIGNED_SHORT_5_5_5_1: return "GL_UNSIGNED_SHORT_5_5_5_1";
        case GL_UNSIGNED_SHORT_5_6_5: return "GL_UNSIGNED_SHORT_5_6_5";

        case GL_FRAGMENT_SHADER: return "GL_FRAGMENT_SHADER";
        case GL_VERTEX_SHADER: return "GL_VERTEX_SHADER";
        case GL_MAX_VERTEX_ATTRIBS: return "GL_MAX_VERTEX_ATTRIBS";
        case GL_MAX_VERTEX_UNIFORM_VECTORS: return "GL_MAX_VERTEX_UNIFORM_VECTORS";
        case GL_MAX_VARYING_VECTORS: return "GL_MAX_VARYING_VECTORS";
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: return "GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS";
        case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS: return "GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS";
        case GL_MAX_TEXTURE_IMAGE_UNITS: return "GL_MAX_TEXTURE_IMAGE_UNITS";
        case GL_MAX_FRAGMENT_UNIFORM_VECTORS: return "GL_MAX_FRAGMENT_UNIFORM_VECTORS";
        case GL_SHADER_TYPE: return "GL_SHADER_TYPE";
        case GL_DELETE_STATUS: return "GL_DELETE_STATUS";
        case GL_LINK_STATUS: return "GL_LINK_STATUS";
        case GL_VALIDATE_STATUS: return "GL_VALIDATE_STATUS";
        case GL_ATTACHED_SHADERS: return "GL_ATTACHED_SHADERS";
        case GL_ACTIVE_UNIFORMS: return "GL_ACTIVE_UNIFORMS";
        case GL_ACTIVE_UNIFORM_MAX_LENGTH: return "GL_ACTIVE_UNIFORM_MAX_LENGTH";
        case GL_ACTIVE_ATTRIBUTES: return "GL_ACTIVE_ATTRIBUTES";
        case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH: return "GL_ACTIVE_ATTRIBUTE_MAX_LENGTH";
        case GL_SHADING_LANGUAGE_VERSION: return "GL_SHADING_LANGUAGE_VERSION";
        case GL_CURRENT_PROGRAM: return "GL_CURRENT_PROGRAM";

        case GL_NEVER: return "GL_NEVER";
        case GL_LESS: return "GL_LESS";
        case GL_EQUAL: return "GL_EQUAL";
        case GL_LEQUAL: return "GL_LEQUAL";
        case GL_GREATER: return "GL_GREATER";
        case GL_NOTEQUAL: return "GL_NOTEQUAL";
        case GL_GEQUAL: return "GL_GEQUAL";
        case GL_ALWAYS: return "GL_ALWAYS";

        case GL_KEEP: return "GL_KEEP";
        case GL_REPLACE: return "GL_REPLACE";
        case GL_INCR: return "GL_INCR";
        case GL_DECR: return "GL_DECR";
        case GL_INVERT: return "GL_INVERT";
        case GL_INCR_WRAP: return "GL_INCR_WRAP";
        case GL_DECR_WRAP: return "GL_DECR_WRAP";

        case GL_VENDOR: return "GL_VENDOR";
        case GL_RENDERER: return "GL_RENDERER";
        case GL_VERSION: return "GL_VERSION";
        case GL_EXTENSIONS: return "GL_EXTENSIONS";

        case GL_NEAREST: return "GL_NEAREST";
        case GL_LINEAR: return "GL_LINEAR";

        case GL_NEAREST_MIPMAP_NEAREST: return "GL_NEAREST_MIPMAP_NEAREST";
        case GL_LINEAR_MIPMAP_NEAREST: return "GL_LINEAR_MIPMAP_NEAREST";
        case GL_NEAREST_MIPMAP_LINEAR: return "GL_NEAREST_MIPMAP_LINEAR";
        case GL_LINEAR_MIPMAP_LINEAR: return "GL_LINEAR_MIPMAP_LINEAR";

        case GL_TEXTURE_MAG_FILTER: return "GL_TEXTURE_MAG_FILTER";
        case GL_TEXTURE_MIN_FILTER: return "GL_TEXTURE_MIN_FILTER";
        case GL_TEXTURE_WRAP_S: return "GL_TEXTURE_WRAP_S";
        case GL_TEXTURE_WRAP_T: return "GL_TEXTURE_WRAP_T";

        case GL_TEXTURE: return "GL_TEXTURE";

        case GL_TEXTURE_CUBE_MAP: return "GL_TEXTURE_CUBE_MAP";
        case GL_TEXTURE_BINDING_CUBE_MAP: return "GL_TEXTURE_BINDING_CUBE_MAP";
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X: return "GL_TEXTURE_CUBE_MAP_POSITIVE_X";
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X: return "GL_TEXTURE_CUBE_MAP_NEGATIVE_X";
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y: return "GL_TEXTURE_CUBE_MAP_POSITIVE_Y";
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y: return "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y";
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z: return "GL_TEXTURE_CUBE_MAP_POSITIVE_Z";
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z: return "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z";
        case GL_MAX_CUBE_MAP_TEXTURE_SIZE: return "GL_MAX_CUBE_MAP_TEXTURE_SIZE";

        case GL_TEXTURE0: return "GL_TEXTURE0";
        case GL_TEXTURE1: return "GL_TEXTURE1";
        case GL_TEXTURE2: return "GL_TEXTURE2";
        case GL_TEXTURE3: return "GL_TEXTURE3";
        case GL_TEXTURE4: return "GL_TEXTURE4";
        case GL_TEXTURE5: return "GL_TEXTURE5";
        case GL_TEXTURE6: return "GL_TEXTURE6";
        case GL_TEXTURE7: return "GL_TEXTURE7";
        case GL_TEXTURE8: return "GL_TEXTURE8";
        case GL_TEXTURE9: return "GL_TEXTURE9";
        case GL_TEXTURE10: return "GL_TEXTURE10";
        case GL_TEXTURE11: return "GL_TEXTURE11";
        case GL_TEXTURE12: return "GL_TEXTURE12";
        case GL_TEXTURE13: return "GL_TEXTURE13";
        case GL_TEXTURE14: return "GL_TEXTURE14";
        case GL_TEXTURE15: return "GL_TEXTURE15";
        case GL_TEXTURE16: return "GL_TEXTURE16";
        case GL_TEXTURE17: return "GL_TEXTURE17";
        case GL_TEXTURE18: return "GL_TEXTURE18";
        case GL_TEXTURE19: return "GL_TEXTURE19";
        case GL_TEXTURE20: return "GL_TEXTURE20";
        case GL_TEXTURE21: return "GL_TEXTURE21";
        case GL_TEXTURE22: return "GL_TEXTURE22";
        case GL_TEXTURE23: return "GL_TEXTURE23";
        case GL_TEXTURE24: return "GL_TEXTURE24";
        case GL_TEXTURE25: return "GL_TEXTURE25";
        case GL_TEXTURE26: return "GL_TEXTURE26";
        case GL_TEXTURE27: return "GL_TEXTURE27";
        case GL_TEXTURE28: return "GL_TEXTURE28";
        case GL_TEXTURE29: return "GL_TEXTURE29";
        case GL_TEXTURE30: return "GL_TEXTURE30";
        case GL_TEXTURE31: return "GL_TEXTURE31";
        case GL_ACTIVE_TEXTURE: return "GL_ACTIVE_TEXTURE";

        case GL_REPEAT: return "GL_REPEAT";
        case GL_CLAMP_TO_EDGE: return "GL_CLAMP_TO_EDGE";
        case GL_MIRRORED_REPEAT: return "GL_MIRRORED_REPEAT";

        case GL_FLOAT_VEC2: return "GL_FLOAT_VEC2";
        case GL_FLOAT_VEC3: return "GL_FLOAT_VEC3";
        case GL_FLOAT_VEC4: return "GL_FLOAT_VEC4";
        case GL_INT_VEC2: return "GL_INT_VEC2";
        case GL_INT_VEC3: return "GL_INT_VEC3";
        case GL_INT_VEC4: return "GL_INT_VEC4";
        case GL_BOOL: return "GL_BOOL";
        case GL_BOOL_VEC2: return "GL_BOOL_VEC2";
        case GL_BOOL_VEC3: return "GL_BOOL_VEC3";
        case GL_BOOL_VEC4: return "GL_BOOL_VEC4";
        case GL_FLOAT_MAT2: return "GL_FLOAT_MAT2";
        case GL_FLOAT_MAT3: return "GL_FLOAT_MAT3";
        case GL_FLOAT_MAT4: return "GL_FLOAT_MAT4";
        case GL_SAMPLER_2D: return "GL_SAMPLER_2D";
        case GL_SAMPLER_CUBE: return "GL_SAMPLER_CUBE";

        case GL_VERTEX_ATTRIB_ARRAY_ENABLED: return "GL_VERTEX_ATTRIB_ARRAY_ENABLED";
        case GL_VERTEX_ATTRIB_ARRAY_SIZE: return "GL_VERTEX_ATTRIB_ARRAY_SIZE";
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE: return "GL_VERTEX_ATTRIB_ARRAY_STRIDE";
        case GL_VERTEX_ATTRIB_ARRAY_TYPE: return "GL_VERTEX_ATTRIB_ARRAY_TYPE";
        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED: return "GL_VERTEX_ATTRIB_ARRAY_NORMALIZED";
        case GL_VERTEX_ATTRIB_ARRAY_POINTER: return "GL_VERTEX_ATTRIB_ARRAY_POINTER";
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: return "GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING";

        case GL_IMPLEMENTATION_COLOR_READ_TYPE: return "GL_IMPLEMENTATION_COLOR_READ_TYPE";
        case GL_IMPLEMENTATION_COLOR_READ_FORMAT: return "GL_IMPLEMENTATION_COLOR_READ_FORMAT";

        case GL_COMPILE_STATUS: return "GL_COMPILE_STATUS";
        case GL_INFO_LOG_LENGTH: return "GL_INFO_LOG_LENGTH";
        case GL_SHADER_SOURCE_LENGTH: return "GL_SHADER_SOURCE_LENGTH";
        case GL_SHADER_COMPILER: return "GL_SHADER_COMPILER";

        case GL_SHADER_BINARY_FORMATS: return "GL_SHADER_BINARY_FORMATS";
        case GL_NUM_SHADER_BINARY_FORMATS: return "GL_NUM_SHADER_BINARY_FORMATS";

        case GL_LOW_FLOAT: return "GL_LOW_FLOAT";
        case GL_MEDIUM_FLOAT: return "GL_MEDIUM_FLOAT";
        case GL_HIGH_FLOAT: return "GL_HIGH_FLOAT";
        case GL_LOW_INT: return "GL_LOW_INT";
        case GL_MEDIUM_INT: return "GL_MEDIUM_INT";
        case GL_HIGH_INT: return "GL_HIGH_INT";

        case GL_FRAMEBUFFER: return "GL_FRAMEBUFFER";
        case GL_RENDERBUFFER: return "GL_RENDERBUFFER";

        case GL_RGBA4: return "GL_RGBA4";
        case GL_RGB5_A1: return "GL_RGB5_A1";
        case GL_RGB565: return "GL_RGB565";
        case GL_DEPTH_COMPONENT16: return "GL_DEPTH_COMPONENT16";
        case GL_STENCIL_INDEX8: return "GL_STENCIL_INDEX8";

        case GL_RENDERBUFFER_WIDTH: return "GL_RENDERBUFFER_WIDTH";
        case GL_RENDERBUFFER_HEIGHT: return "GL_RENDERBUFFER_HEIGHT";
        case GL_RENDERBUFFER_INTERNAL_FORMAT: return "GL_RENDERBUFFER_INTERNAL_FORMAT";
        case GL_RENDERBUFFER_RED_SIZE: return "GL_RENDERBUFFER_RED_SIZE";
        case GL_RENDERBUFFER_GREEN_SIZE: return "GL_RENDERBUFFER_GREEN_SIZE";
        case GL_RENDERBUFFER_BLUE_SIZE: return "GL_RENDERBUFFER_BLUE_SIZE";
        case GL_RENDERBUFFER_ALPHA_SIZE: return "GL_RENDERBUFFER_ALPHA_SIZE";
        case GL_RENDERBUFFER_DEPTH_SIZE: return "GL_RENDERBUFFER_DEPTH_SIZE";
        case GL_RENDERBUFFER_STENCIL_SIZE: return "GL_RENDERBUFFER_STENCIL_SIZE";

        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE: return "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE";
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME: return "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME";
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL: return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL";
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE: return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE";

        case GL_COLOR_ATTACHMENT0: return "GL_COLOR_ATTACHMENT0";
        case GL_DEPTH_ATTACHMENT: return "GL_DEPTH_ATTACHMENT";
        case GL_STENCIL_ATTACHMENT: return "GL_STENCIL_ATTACHMENT";

        case GL_FRAMEBUFFER_COMPLETE: return "GL_FRAMEBUFFER_COMPLETE";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS: return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
        case GL_FRAMEBUFFER_UNSUPPORTED: return "GL_FRAMEBUFFER_UNSUPPORTED";

        case GL_FRAMEBUFFER_BINDING: return "GL_FRAMEBUFFER_BINDING";
        case GL_RENDERBUFFER_BINDING: return "GL_RENDERBUFFER_BINDING";
        case GL_MAX_RENDERBUFFER_SIZE: return "GL_MAX_RENDERBUFFER_SIZE";

        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";

        case GL_READ_BUFFER: return "GL_READ_BUFFER";
        case GL_UNPACK_ROW_LENGTH: return "GL_UNPACK_ROW_LENGTH";
        case GL_UNPACK_SKIP_ROWS: return "GL_UNPACK_SKIP_ROWS";
        case GL_UNPACK_SKIP_PIXELS: return "GL_UNPACK_SKIP_PIXELS";
        case GL_PACK_ROW_LENGTH: return "GL_PACK_ROW_LENGTH";
        case GL_PACK_SKIP_ROWS: return "GL_PACK_SKIP_ROWS";
        case GL_PACK_SKIP_PIXELS: return "GL_PACK_SKIP_PIXELS";
        case GL_COLOR: return "GL_COLOR";
        case GL_DEPTH: return "GL_DEPTH";
        case GL_STENCIL: return "GL_STENCIL";
        case GL_RED: return "GL_RED";
        case GL_RGB8: return "GL_RGB8";
        case GL_RGBA8: return "GL_RGBA8";
        case GL_RGB10_A2: return "GL_RGB10_A2";
        case GL_TEXTURE_BINDING_3D: return "GL_TEXTURE_BINDING_3D";
        case GL_UNPACK_SKIP_IMAGES: return "GL_UNPACK_SKIP_IMAGES";
        case GL_UNPACK_IMAGE_HEIGHT: return "GL_UNPACK_IMAGE_HEIGHT";
        case GL_TEXTURE_3D: return "GL_TEXTURE_3D";
        case GL_TEXTURE_WRAP_R: return "GL_TEXTURE_WRAP_R";
        case GL_MAX_3D_TEXTURE_SIZE: return "GL_MAX_3D_TEXTURE_SIZE";
        case GL_UNSIGNED_INT_2_10_10_10_REV: return "GL_UNSIGNED_INT_2_10_10_10_REV";
        case GL_MAX_ELEMENTS_VERTICES: return "GL_MAX_ELEMENTS_VERTICES";
        case GL_MAX_ELEMENTS_INDICES: return "GL_MAX_ELEMENTS_INDICES";
        case GL_TEXTURE_MIN_LOD: return "GL_TEXTURE_MIN_LOD";
        case GL_TEXTURE_MAX_LOD: return "GL_TEXTURE_MAX_LOD";
        case GL_TEXTURE_BASE_LEVEL: return "GL_TEXTURE_BASE_LEVEL";
        case GL_TEXTURE_MAX_LEVEL: return "GL_TEXTURE_MAX_LEVEL";
        case GL_MIN: return "GL_MIN";
        case GL_MAX: return "GL_MAX";
        case GL_DEPTH_COMPONENT24: return "GL_DEPTH_COMPONENT24";
        case GL_MAX_TEXTURE_LOD_BIAS: return "GL_MAX_TEXTURE_LOD_BIAS";
        case GL_TEXTURE_COMPARE_MODE: return "GL_TEXTURE_COMPARE_MODE";
        case GL_TEXTURE_COMPARE_FUNC: return "GL_TEXTURE_COMPARE_FUNC";
        case GL_CURRENT_QUERY: return "GL_CURRENT_QUERY";
        case GL_QUERY_RESULT: return "GL_QUERY_RESULT";
        case GL_QUERY_RESULT_AVAILABLE: return "GL_QUERY_RESULT_AVAILABLE";
        case GL_BUFFER_MAPPED: return "GL_BUFFER_MAPPED";
        case GL_BUFFER_MAP_POINTER: return "GL_BUFFER_MAP_POINTER";
        case GL_STREAM_READ: return "GL_STREAM_READ";
        case GL_STREAM_COPY: return "GL_STREAM_COPY";
        case GL_STATIC_READ: return "GL_STATIC_READ";
        case GL_STATIC_COPY: return "GL_STATIC_COPY";
        case GL_DYNAMIC_READ: return "GL_DYNAMIC_READ";
        case GL_DYNAMIC_COPY: return "GL_DYNAMIC_COPY";
        case GL_MAX_DRAW_BUFFERS: return "GL_MAX_DRAW_BUFFERS";
        case GL_DRAW_BUFFER0: return "GL_DRAW_BUFFER0";
        case GL_DRAW_BUFFER1: return "GL_DRAW_BUFFER1";
        case GL_DRAW_BUFFER2: return "GL_DRAW_BUFFER2";
        case GL_DRAW_BUFFER3: return "GL_DRAW_BUFFER3";
        case GL_DRAW_BUFFER4: return "GL_DRAW_BUFFER4";
        case GL_DRAW_BUFFER5: return "GL_DRAW_BUFFER5";
        case GL_DRAW_BUFFER6: return "GL_DRAW_BUFFER6";
        case GL_DRAW_BUFFER7: return "GL_DRAW_BUFFER7";
        case GL_DRAW_BUFFER8: return "GL_DRAW_BUFFER8";
        case GL_DRAW_BUFFER9: return "GL_DRAW_BUFFER9";
        case GL_DRAW_BUFFER10: return "GL_DRAW_BUFFER10";
        case GL_DRAW_BUFFER11: return "GL_DRAW_BUFFER11";
        case GL_DRAW_BUFFER12: return "GL_DRAW_BUFFER12";
        case GL_DRAW_BUFFER13: return "GL_DRAW_BUFFER13";
        case GL_DRAW_BUFFER14: return "GL_DRAW_BUFFER14";
        case GL_DRAW_BUFFER15: return "GL_DRAW_BUFFER15";
        case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS: return "GL_MAX_FRAGMENT_UNIFORM_COMPONENTS";
        case GL_MAX_VERTEX_UNIFORM_COMPONENTS: return "GL_MAX_VERTEX_UNIFORM_COMPONENTS";
        case GL_SAMPLER_3D: return "GL_SAMPLER_3D";
        case GL_SAMPLER_2D_SHADOW: return "GL_SAMPLER_2D_SHADOW";
        case GL_FRAGMENT_SHADER_DERIVATIVE_HINT: return "GL_FRAGMENT_SHADER_DERIVATIVE_HINT";
        case GL_PIXEL_PACK_BUFFER: return "GL_PIXEL_PACK_BUFFER";
        case GL_PIXEL_UNPACK_BUFFER: return "GL_PIXEL_UNPACK_BUFFER";
        case GL_PIXEL_PACK_BUFFER_BINDING: return "GL_PIXEL_PACK_BUFFER_BINDING";
        case GL_PIXEL_UNPACK_BUFFER_BINDING: return "GL_PIXEL_UNPACK_BUFFER_BINDING";
        case GL_FLOAT_MAT2x3: return "GL_FLOAT_MAT2x3";
        case GL_FLOAT_MAT2x4: return "GL_FLOAT_MAT2x4";
        case GL_FLOAT_MAT3x2: return "GL_FLOAT_MAT3x2";
        case GL_FLOAT_MAT3x4: return "GL_FLOAT_MAT3x4";
        case GL_FLOAT_MAT4x2: return "GL_FLOAT_MAT4x2";
        case GL_FLOAT_MAT4x3: return "GL_FLOAT_MAT4x3";
        case GL_SRGB: return "GL_SRGB";
        case GL_SRGB8: return "GL_SRGB8";
        case GL_SRGB8_ALPHA8: return "GL_SRGB8_ALPHA8";
        case GL_COMPARE_REF_TO_TEXTURE: return "GL_COMPARE_REF_TO_TEXTURE";
        case GL_MAJOR_VERSION: return "GL_MAJOR_VERSION";
        case GL_MINOR_VERSION: return "GL_MINOR_VERSION";
        case GL_NUM_EXTENSIONS: return "GL_NUM_EXTENSIONS";
        case GL_RGBA32F: return "GL_RGBA32F";
        case GL_RGB32F: return "GL_RGB32F";
        case GL_RGBA16F: return "GL_RGBA16F";
        case GL_RGB16F: return "GL_RGB16F";
        case GL_VERTEX_ATTRIB_ARRAY_INTEGER: return "GL_VERTEX_ATTRIB_ARRAY_INTEGER";
        case GL_MAX_ARRAY_TEXTURE_LAYERS: return "GL_MAX_ARRAY_TEXTURE_LAYERS";
        case GL_MIN_PROGRAM_TEXEL_OFFSET: return "GL_MIN_PROGRAM_TEXEL_OFFSET";
        case GL_MAX_PROGRAM_TEXEL_OFFSET: return "GL_MAX_PROGRAM_TEXEL_OFFSET";
        case GL_MAX_VARYING_COMPONENTS: return "GL_MAX_VARYING_COMPONENTS";
        case GL_TEXTURE_2D_ARRAY: return "GL_TEXTURE_2D_ARRAY";
        case GL_TEXTURE_BINDING_2D_ARRAY: return "GL_TEXTURE_BINDING_2D_ARRAY";
        case GL_R11F_G11F_B10F: return "GL_R11F_G11F_B10F";
        case GL_UNSIGNED_INT_10F_11F_11F_REV: return "GL_UNSIGNED_INT_10F_11F_11F_REV";
        case GL_RGB9_E5: return "GL_RGB9_E5";
        case GL_UNSIGNED_INT_5_9_9_9_REV: return "GL_UNSIGNED_INT_5_9_9_9_REV";
        case GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH: return "GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH";
        case GL_TRANSFORM_FEEDBACK_BUFFER_MODE: return "GL_TRANSFORM_FEEDBACK_BUFFER_MODE";
        case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS: return "GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS";
        case GL_TRANSFORM_FEEDBACK_VARYINGS: return "GL_TRANSFORM_FEEDBACK_VARYINGS";
        case GL_TRANSFORM_FEEDBACK_BUFFER_START: return "GL_TRANSFORM_FEEDBACK_BUFFER_START";
        case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE: return "GL_TRANSFORM_FEEDBACK_BUFFER_SIZE";
        case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN: return "GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN";
        case GL_RASTERIZER_DISCARD: return "GL_RASTERIZER_DISCARD";
        case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS: return "GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS";
        case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS: return "GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS";
        case GL_INTERLEAVED_ATTRIBS: return "GL_INTERLEAVED_ATTRIBS";
        case GL_SEPARATE_ATTRIBS: return "GL_SEPARATE_ATTRIBS";
        case GL_TRANSFORM_FEEDBACK_BUFFER: return "GL_TRANSFORM_FEEDBACK_BUFFER";
        case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING: return "GL_TRANSFORM_FEEDBACK_BUFFER_BINDING";
        case GL_RGBA32UI: return "GL_RGBA32UI";
        case GL_RGB32UI: return "GL_RGB32UI";
        case GL_RGBA16UI: return "GL_RGBA16UI";
        case GL_RGB16UI: return "GL_RGB16UI";
        case GL_RGBA8UI: return "GL_RGBA8UI";
        case GL_RGB8UI: return "GL_RGB8UI";
        case GL_RGBA32I: return "GL_RGBA32I";
        case GL_RGB32I: return "GL_RGB32I";
        case GL_RGBA16I: return "GL_RGBA16I";
        case GL_RGB16I: return "GL_RGB16I";
        case GL_RGBA8I: return "GL_RGBA8I";
        case GL_RGB8I: return "GL_RGB8I";
        case GL_RED_INTEGER: return "GL_RED_INTEGER";
        case GL_RGB_INTEGER: return "GL_RGB_INTEGER";
        case GL_RGBA_INTEGER: return "GL_RGBA_INTEGER";
        case GL_SAMPLER_2D_ARRAY: return "GL_SAMPLER_2D_ARRAY";
        case GL_SAMPLER_2D_ARRAY_SHADOW: return "GL_SAMPLER_2D_ARRAY_SHADOW";
        case GL_SAMPLER_CUBE_SHADOW: return "GL_SAMPLER_CUBE_SHADOW";
        case GL_UNSIGNED_INT_VEC2: return "GL_UNSIGNED_INT_VEC2";
        case GL_UNSIGNED_INT_VEC3: return "GL_UNSIGNED_INT_VEC3";
        case GL_UNSIGNED_INT_VEC4: return "GL_UNSIGNED_INT_VEC4";
        case GL_INT_SAMPLER_2D: return "GL_INT_SAMPLER_2D";
        case GL_INT_SAMPLER_3D: return "GL_INT_SAMPLER_3D";
        case GL_INT_SAMPLER_CUBE: return "GL_INT_SAMPLER_CUBE";
        case GL_INT_SAMPLER_2D_ARRAY: return "GL_INT_SAMPLER_2D_ARRAY";
        case GL_UNSIGNED_INT_SAMPLER_2D: return "GL_UNSIGNED_INT_SAMPLER_2D";
        case GL_UNSIGNED_INT_SAMPLER_3D: return "GL_UNSIGNED_INT_SAMPLER_3D";
        case GL_UNSIGNED_INT_SAMPLER_CUBE: return "GL_UNSIGNED_INT_SAMPLER_CUBE";
        case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY: return "GL_UNSIGNED_INT_SAMPLER_2D_ARRAY";
        case GL_BUFFER_ACCESS_FLAGS: return "GL_BUFFER_ACCESS_FLAGS";
        case GL_BUFFER_MAP_LENGTH: return "GL_BUFFER_MAP_LENGTH";
        case GL_BUFFER_MAP_OFFSET: return "GL_BUFFER_MAP_OFFSET";
        case GL_DEPTH_COMPONENT32F: return "GL_DEPTH_COMPONENT32F";
        case GL_DEPTH32F_STENCIL8: return "GL_DEPTH32F_STENCIL8";
        case GL_FLOAT_32_UNSIGNED_INT_24_8_REV: return "GL_FLOAT_32_UNSIGNED_INT_24_8_REV";
        case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING: return "GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING";
        case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE: return "GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE";
        case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE: return "GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE";
        case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE: return "GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE";
        case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE: return "GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE";
        case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE: return "GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE";
        case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE: return "GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE";
        case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE: return "GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE";
        case GL_FRAMEBUFFER_DEFAULT: return "GL_FRAMEBUFFER_DEFAULT";
        case GL_FRAMEBUFFER_UNDEFINED: return "GL_FRAMEBUFFER_UNDEFINED";
        case GL_DEPTH_STENCIL_ATTACHMENT: return "GL_DEPTH_STENCIL_ATTACHMENT";
        case GL_DEPTH_STENCIL: return "GL_DEPTH_STENCIL";
        case GL_UNSIGNED_INT_24_8: return "GL_UNSIGNED_INT_24_8";
        case GL_DEPTH24_STENCIL8: return "GL_DEPTH24_STENCIL8";
        case GL_UNSIGNED_NORMALIZED: return "GL_UNSIGNED_NORMALIZED";
        case GL_READ_FRAMEBUFFER: return "GL_READ_FRAMEBUFFER";
        case GL_DRAW_FRAMEBUFFER: return "GL_DRAW_FRAMEBUFFER";
        case GL_READ_FRAMEBUFFER_BINDING: return "GL_READ_FRAMEBUFFER_BINDING";
        case GL_RENDERBUFFER_SAMPLES: return "GL_RENDERBUFFER_SAMPLES";
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER: return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER";
        case GL_MAX_COLOR_ATTACHMENTS: return "GL_MAX_COLOR_ATTACHMENTS";
        case GL_COLOR_ATTACHMENT1: return "GL_COLOR_ATTACHMENT1";
        case GL_COLOR_ATTACHMENT2: return "GL_COLOR_ATTACHMENT2";
        case GL_COLOR_ATTACHMENT3: return "GL_COLOR_ATTACHMENT3";
        case GL_COLOR_ATTACHMENT4: return "GL_COLOR_ATTACHMENT4";
        case GL_COLOR_ATTACHMENT5: return "GL_COLOR_ATTACHMENT5";
        case GL_COLOR_ATTACHMENT6: return "GL_COLOR_ATTACHMENT6";
        case GL_COLOR_ATTACHMENT7: return "GL_COLOR_ATTACHMENT7";
        case GL_COLOR_ATTACHMENT8: return "GL_COLOR_ATTACHMENT8";
        case GL_COLOR_ATTACHMENT9: return "GL_COLOR_ATTACHMENT9";
        case GL_COLOR_ATTACHMENT10: return "GL_COLOR_ATTACHMENT10";
        case GL_COLOR_ATTACHMENT11: return "GL_COLOR_ATTACHMENT11";
        case GL_COLOR_ATTACHMENT12: return "GL_COLOR_ATTACHMENT12";
        case GL_COLOR_ATTACHMENT13: return "GL_COLOR_ATTACHMENT13";
        case GL_COLOR_ATTACHMENT14: return "GL_COLOR_ATTACHMENT14";
        case GL_COLOR_ATTACHMENT15: return "GL_COLOR_ATTACHMENT15";
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
        case GL_MAX_SAMPLES: return "GL_MAX_SAMPLES";
        case GL_HALF_FLOAT: return "GL_HALF_FLOAT";
        // case GL_MAP_READ_BIT: return "GL_MAP_READ_BIT";
        // case GL_MAP_WRITE_BIT: return "GL_MAP_WRITE_BIT";
        case GL_MAP_INVALIDATE_RANGE_BIT: return "GL_MAP_INVALIDATE_RANGE_BIT";
        case GL_MAP_INVALIDATE_BUFFER_BIT: return "GL_MAP_INVALIDATE_BUFFER_BIT";
        case GL_MAP_FLUSH_EXPLICIT_BIT: return "GL_MAP_FLUSH_EXPLICIT_BIT";
        case GL_MAP_UNSYNCHRONIZED_BIT: return "GL_MAP_UNSYNCHRONIZED_BIT";
        case GL_RG: return "GL_RG";
        case GL_RG_INTEGER: return "GL_RG_INTEGER";
        case GL_R8: return "GL_R8";
        case GL_RG8: return "GL_RG8";
        case GL_R16F: return "GL_R16F";
        case GL_R32F: return "GL_R32F";
        case GL_RG16F: return "GL_RG16F";
        case GL_RG32F: return "GL_RG32F";
        case GL_R8I: return "GL_R8I";
        case GL_R8UI: return "GL_R8UI";
        case GL_R16I: return "GL_R16I";
        case GL_R16UI: return "GL_R16UI";
        case GL_R32I: return "GL_R32I";
        case GL_R32UI: return "GL_R32UI";
        case GL_RG8I: return "GL_RG8I";
        case GL_RG8UI: return "GL_RG8UI";
        case GL_RG16I: return "GL_RG16I";
        case GL_RG16UI: return "GL_RG16UI";
        case GL_RG32I: return "GL_RG32I";
        case GL_RG32UI: return "GL_RG32UI";
        case GL_VERTEX_ARRAY_BINDING: return "GL_VERTEX_ARRAY_BINDING";
        case GL_R8_SNORM: return "GL_R8_SNORM";
        case GL_RG8_SNORM: return "GL_RG8_SNORM";
        case GL_RGB8_SNORM: return "GL_RGB8_SNORM";
        case GL_RGBA8_SNORM: return "GL_RGBA8_SNORM";
        case GL_SIGNED_NORMALIZED: return "GL_SIGNED_NORMALIZED";
        case GL_PRIMITIVE_RESTART_FIXED_INDEX: return "GL_PRIMITIVE_RESTART_FIXED_INDEX";
        case GL_COPY_READ_BUFFER: return "GL_COPY_READ_BUFFER";
        case GL_COPY_WRITE_BUFFER: return "GL_COPY_WRITE_BUFFER";
        case GL_UNIFORM_BUFFER: return "GL_UNIFORM_BUFFER";
        case GL_UNIFORM_BUFFER_BINDING: return "GL_UNIFORM_BUFFER_BINDING";
        case GL_UNIFORM_BUFFER_START: return "GL_UNIFORM_BUFFER_START";
        case GL_UNIFORM_BUFFER_SIZE: return "GL_UNIFORM_BUFFER_SIZE";
        case GL_MAX_VERTEX_UNIFORM_BLOCKS: return "GL_MAX_VERTEX_UNIFORM_BLOCKS";
        case GL_MAX_FRAGMENT_UNIFORM_BLOCKS: return "GL_MAX_FRAGMENT_UNIFORM_BLOCKS";
        case GL_MAX_COMBINED_UNIFORM_BLOCKS: return "GL_MAX_COMBINED_UNIFORM_BLOCKS";
        case GL_MAX_UNIFORM_BUFFER_BINDINGS: return "GL_MAX_UNIFORM_BUFFER_BINDINGS";
        case GL_MAX_UNIFORM_BLOCK_SIZE: return "GL_MAX_UNIFORM_BLOCK_SIZE";
        case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS: return "GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS";
        case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS: return "GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS";
        case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT: return "GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT";
        case GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH: return "GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH";
        case GL_ACTIVE_UNIFORM_BLOCKS: return "GL_ACTIVE_UNIFORM_BLOCKS";
        case GL_UNIFORM_TYPE: return "GL_UNIFORM_TYPE";
        case GL_UNIFORM_SIZE: return "GL_UNIFORM_SIZE";
        case GL_UNIFORM_NAME_LENGTH: return "GL_UNIFORM_NAME_LENGTH";
        case GL_UNIFORM_BLOCK_INDEX: return "GL_UNIFORM_BLOCK_INDEX";
        case GL_UNIFORM_OFFSET: return "GL_UNIFORM_OFFSET";
        case GL_UNIFORM_ARRAY_STRIDE: return "GL_UNIFORM_ARRAY_STRIDE";
        case GL_UNIFORM_MATRIX_STRIDE: return "GL_UNIFORM_MATRIX_STRIDE";
        case GL_UNIFORM_IS_ROW_MAJOR: return "GL_UNIFORM_IS_ROW_MAJOR";
        case GL_UNIFORM_BLOCK_BINDING: return "GL_UNIFORM_BLOCK_BINDING";
        case GL_UNIFORM_BLOCK_DATA_SIZE: return "GL_UNIFORM_BLOCK_DATA_SIZE";
        case GL_UNIFORM_BLOCK_NAME_LENGTH: return "GL_UNIFORM_BLOCK_NAME_LENGTH";
        case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS: return "GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS";
        case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES: return "GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES";
        case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER: return "GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER";
        case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER: return "GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER";
        case GL_INVALID_INDEX: return "GL_INVALID_INDEX";
        case GL_MAX_VERTEX_OUTPUT_COMPONENTS: return "GL_MAX_VERTEX_OUTPUT_COMPONENTS";
        case GL_MAX_FRAGMENT_INPUT_COMPONENTS: return "GL_MAX_FRAGMENT_INPUT_COMPONENTS";
        case GL_MAX_SERVER_WAIT_TIMEOUT: return "GL_MAX_SERVER_WAIT_TIMEOUT";
        case GL_OBJECT_TYPE: return "GL_OBJECT_TYPE";
        case GL_SYNC_CONDITION: return "GL_SYNC_CONDITION";
        case GL_SYNC_STATUS: return "GL_SYNC_STATUS";
        case GL_SYNC_FLAGS: return "GL_SYNC_FLAGS";
        case GL_SYNC_FENCE: return "GL_SYNC_FENCE";
        case GL_SYNC_GPU_COMMANDS_COMPLETE: return "GL_SYNC_GPU_COMMANDS_COMPLETE";
        case GL_UNSIGNALED: return "GL_UNSIGNALED";
        case GL_SIGNALED: return "GL_SIGNALED";
        case GL_ALREADY_SIGNALED: return "GL_ALREADY_SIGNALED";
        case GL_TIMEOUT_EXPIRED: return "GL_TIMEOUT_EXPIRED";
        case GL_CONDITION_SATISFIED: return "GL_CONDITION_SATISFIED";
        case GL_WAIT_FAILED: return "GL_WAIT_FAILED";
        // case GL_SYNC_FLUSH_COMMANDS_BIT: return "GL_SYNC_FLUSH_COMMANDS_BIT";
        // case GL_TIMEOUT_IGNORED: return "GL_TIMEOUT_IGNORED";
        case GL_VERTEX_ATTRIB_ARRAY_DIVISOR: return "GL_VERTEX_ATTRIB_ARRAY_DIVISOR";
        case GL_ANY_SAMPLES_PASSED: return "GL_ANY_SAMPLES_PASSED";
        case GL_ANY_SAMPLES_PASSED_CONSERVATIVE: return "GL_ANY_SAMPLES_PASSED_CONSERVATIVE";
        case GL_SAMPLER_BINDING: return "GL_SAMPLER_BINDING";
        case GL_RGB10_A2UI: return "GL_RGB10_A2UI";
        case GL_TEXTURE_SWIZZLE_R: return "GL_TEXTURE_SWIZZLE_R";
        case GL_TEXTURE_SWIZZLE_G: return "GL_TEXTURE_SWIZZLE_G";
        case GL_TEXTURE_SWIZZLE_B: return "GL_TEXTURE_SWIZZLE_B";
        case GL_TEXTURE_SWIZZLE_A: return "GL_TEXTURE_SWIZZLE_A";
        case GL_GREEN: return "GL_GREEN";
        case GL_BLUE: return "GL_BLUE";
        case GL_INT_2_10_10_10_REV: return "GL_INT_2_10_10_10_REV";
        case GL_TRANSFORM_FEEDBACK: return "GL_TRANSFORM_FEEDBACK";
        case GL_TRANSFORM_FEEDBACK_PAUSED: return "GL_TRANSFORM_FEEDBACK_PAUSED";
        case GL_TRANSFORM_FEEDBACK_ACTIVE: return "GL_TRANSFORM_FEEDBACK_ACTIVE";
        case GL_TRANSFORM_FEEDBACK_BINDING: return "GL_TRANSFORM_FEEDBACK_BINDING";
        case GL_PROGRAM_BINARY_RETRIEVABLE_HINT: return "GL_PROGRAM_BINARY_RETRIEVABLE_HINT";
        case GL_PROGRAM_BINARY_LENGTH: return "GL_PROGRAM_BINARY_LENGTH";
        case GL_NUM_PROGRAM_BINARY_FORMATS: return "GL_NUM_PROGRAM_BINARY_FORMATS";
        case GL_PROGRAM_BINARY_FORMATS: return "GL_PROGRAM_BINARY_FORMATS";
        case GL_COMPRESSED_R11_EAC: return "GL_COMPRESSED_R11_EAC";
        case GL_COMPRESSED_SIGNED_R11_EAC: return "GL_COMPRESSED_SIGNED_R11_EAC";
        case GL_COMPRESSED_RG11_EAC: return "GL_COMPRESSED_RG11_EAC";
        case GL_COMPRESSED_SIGNED_RG11_EAC: return "GL_COMPRESSED_SIGNED_RG11_EAC";
        case GL_COMPRESSED_RGB8_ETC2: return "GL_COMPRESSED_RGB8_ETC2";
        case GL_COMPRESSED_SRGB8_ETC2: return "GL_COMPRESSED_SRGB8_ETC2";
        case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2: return "GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2";
        case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2: return "GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2";
        case GL_COMPRESSED_RGBA8_ETC2_EAC: return "GL_COMPRESSED_RGBA8_ETC2_EAC";
        case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC: return "GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC";
        case GL_TEXTURE_IMMUTABLE_FORMAT: return "GL_TEXTURE_IMMUTABLE_FORMAT";
        case GL_MAX_ELEMENT_INDEX: return "GL_MAX_ELEMENT_INDEX";
        case GL_NUM_SAMPLE_COUNTS: return "GL_NUM_SAMPLE_COUNTS";
        case GL_TEXTURE_IMMUTABLE_LEVELS: return "GL_TEXTURE_IMMUTABLE_LEVELS";
        default: return "<<unknown>>";
        }
#elif defined(GL_VERSION_2_1)
        // Should contain desktop GL up to 2.1, but without extensions or legacy tuff
        switch (value) {
        case GL_ACCUM: return "GL_ACCUM";
        case GL_LOAD: return "GL_LOAD";
        case GL_RETURN: return "GL_RETURN";
        case GL_MULT: return "GL_MULT";
        case GL_ADD: return "GL_ADD";

        case GL_NEVER: return "GL_NEVER";
        case GL_LESS: return "GL_LESS";
        case GL_EQUAL: return "GL_EQUAL";
        case GL_LEQUAL: return "GL_LEQUAL";
        case GL_GREATER: return "GL_GREATER";
        case GL_NOTEQUAL: return "GL_NOTEQUAL";
        case GL_GEQUAL: return "GL_GEQUAL";
        case GL_ALWAYS: return "GL_ALWAYS";

        // case GL_CURRENT_BIT: return "GL_CURRENT_BIT";
        // case GL_POINT_BIT: return "GL_POINT_BIT";
        // case GL_LINE_BIT: return "GL_LINE_BIT";
        // case GL_POLYGON_BIT: return "GL_POLYGON_BIT";
        // case GL_POLYGON_STIPPLE_BIT: return "GL_POLYGON_STIPPLE_BIT";
        // case GL_PIXEL_MODE_BIT: return "GL_PIXEL_MODE_BIT";
        // case GL_LIGHTING_BIT: return "GL_LIGHTING_BIT";
        // case GL_FOG_BIT: return "GL_FOG_BIT";
        // case GL_DEPTH_BUFFER_BIT: return "GL_DEPTH_BUFFER_BIT";
        // case GL_ACCUM_BUFFER_BIT: return "GL_ACCUM_BUFFER_BIT";
        // case GL_STENCIL_BUFFER_BIT: return "GL_STENCIL_BUFFER_BIT";
        // case GL_VIEWPORT_BIT: return "GL_VIEWPORT_BIT";
        // case GL_TRANSFORM_BIT: return "GL_TRANSFORM_BIT";
        // case GL_ENABLE_BIT: return "GL_ENABLE_BIT";
        // case GL_COLOR_BUFFER_BIT: return "GL_COLOR_BUFFER_BIT";
        // case GL_HINT_BIT: return "GL_HINT_BIT";
        // case GL_EVAL_BIT: return "GL_EVAL_BIT";
        // case GL_LIST_BIT: return "GL_LIST_BIT";
        // case GL_TEXTURE_BIT: return "GL_TEXTURE_BIT";
        // case GL_SCISSOR_BIT: return "GL_SCISSOR_BIT";
        // case GL_ALL_ATTRIB_BITS: return "GL_ALL_ATTRIB_BITS";

        // case GL_POINTS: return "GL_POINTS";
        // case GL_LINES: return "GL_LINES";
        case GL_LINE_LOOP: return "GL_LINE_LOOP";
        case GL_LINE_STRIP: return "GL_LINE_STRIP";
        case GL_TRIANGLES: return "GL_TRIANGLES";
        case GL_TRIANGLE_STRIP: return "GL_TRIANGLE_STRIP";
        case GL_TRIANGLE_FAN: return "GL_TRIANGLE_FAN";
        case GL_QUADS: return "GL_QUADS";
        case GL_QUAD_STRIP: return "GL_QUAD_STRIP";
        case GL_POLYGON: return "GL_POLYGON";

        case GL_ZERO: return "GL_ZERO";
        case GL_ONE: return "GL_ONE";
        case GL_SRC_COLOR: return "GL_SRC_COLOR";
        case GL_ONE_MINUS_SRC_COLOR: return "GL_ONE_MINUS_SRC_COLOR";
        case GL_SRC_ALPHA: return "GL_SRC_ALPHA";
        case GL_ONE_MINUS_SRC_ALPHA: return "GL_ONE_MINUS_SRC_ALPHA";
        case GL_DST_ALPHA: return "GL_DST_ALPHA";
        case GL_ONE_MINUS_DST_ALPHA: return "GL_ONE_MINUS_DST_ALPHA";
        case GL_DST_COLOR: return "GL_DST_COLOR";
        case GL_ONE_MINUS_DST_COLOR: return "GL_ONE_MINUS_DST_COLOR";
        case GL_SRC_ALPHA_SATURATE: return "GL_SRC_ALPHA_SATURATE";

        // case GL_TRUE: return "GL_TRUE";
        // case GL_FALSE: return "GL_FALSE";

        case GL_CLIP_PLANE0: return "GL_CLIP_PLANE0";
        case GL_CLIP_PLANE1: return "GL_CLIP_PLANE1";
        case GL_CLIP_PLANE2: return "GL_CLIP_PLANE2";
        case GL_CLIP_PLANE3: return "GL_CLIP_PLANE3";
        case GL_CLIP_PLANE4: return "GL_CLIP_PLANE4";
        case GL_CLIP_PLANE5: return "GL_CLIP_PLANE5";

        case GL_BYTE: return "GL_BYTE";
        case GL_UNSIGNED_BYTE: return "GL_UNSIGNED_BYTE";
        case GL_SHORT: return "GL_SHORT";
        case GL_UNSIGNED_SHORT: return "GL_UNSIGNED_SHORT";
        case GL_INT: return "GL_INT";
        case GL_UNSIGNED_INT: return "GL_UNSIGNED_INT";
        case GL_FLOAT: return "GL_FLOAT";
        case GL_2_BYTES: return "GL_2_BYTES";
        case GL_3_BYTES: return "GL_3_BYTES";
        case GL_4_BYTES: return "GL_4_BYTES";
        case GL_DOUBLE: return "GL_DOUBLE";

        // case GL_NONE: return "GL_NONE";
        case GL_FRONT_LEFT: return "GL_FRONT_LEFT";
        case GL_FRONT_RIGHT: return "GL_FRONT_RIGHT";
        case GL_BACK_LEFT: return "GL_BACK_LEFT";
        case GL_BACK_RIGHT: return "GL_BACK_RIGHT";
        case GL_FRONT: return "GL_FRONT";
        case GL_BACK: return "GL_BACK";
        case GL_LEFT: return "GL_LEFT";
        case GL_RIGHT: return "GL_RIGHT";
        case GL_FRONT_AND_BACK: return "GL_FRONT_AND_BACK";
        case GL_AUX0: return "GL_AUX0";
        case GL_AUX1: return "GL_AUX1";
        case GL_AUX2: return "GL_AUX2";
        case GL_AUX3: return "GL_AUX3";

        // case GL_NO_ERROR: return "GL_NO_ERROR";
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
        case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";

        case GL_2D: return "GL_2D";
        case GL_3D: return "GL_3D";
        case GL_3D_COLOR: return "GL_3D_COLOR";
        case GL_3D_COLOR_TEXTURE: return "GL_3D_COLOR_TEXTURE";
        case GL_4D_COLOR_TEXTURE: return "GL_4D_COLOR_TEXTURE";

        case GL_PASS_THROUGH_TOKEN: return "GL_PASS_THROUGH_TOKEN";
        case GL_POINT_TOKEN: return "GL_POINT_TOKEN";
        case GL_LINE_TOKEN: return "GL_LINE_TOKEN";
        case GL_POLYGON_TOKEN: return "GL_POLYGON_TOKEN";
        case GL_BITMAP_TOKEN: return "GL_BITMAP_TOKEN";
        case GL_DRAW_PIXEL_TOKEN: return "GL_DRAW_PIXEL_TOKEN";
        case GL_COPY_PIXEL_TOKEN: return "GL_COPY_PIXEL_TOKEN";
        case GL_LINE_RESET_TOKEN: return "GL_LINE_RESET_TOKEN";

        case GL_EXP: return "GL_EXP";
        case GL_EXP2: return "GL_EXP2";

        case GL_CW: return "GL_CW";
        case GL_CCW: return "GL_CCW";

        case GL_COEFF: return "GL_COEFF";
        case GL_ORDER: return "GL_ORDER";
        case GL_DOMAIN: return "GL_DOMAIN";

        case GL_CURRENT_COLOR: return "GL_CURRENT_COLOR";
        case GL_CURRENT_INDEX: return "GL_CURRENT_INDEX";
        case GL_CURRENT_NORMAL: return "GL_CURRENT_NORMAL";
        case GL_CURRENT_TEXTURE_COORDS: return "GL_CURRENT_TEXTURE_COORDS";
        case GL_CURRENT_RASTER_COLOR: return "GL_CURRENT_RASTER_COLOR";
        case GL_CURRENT_RASTER_INDEX: return "GL_CURRENT_RASTER_INDEX";
        case GL_CURRENT_RASTER_TEXTURE_COORDS: return "GL_CURRENT_RASTER_TEXTURE_COORDS";
        case GL_CURRENT_RASTER_POSITION: return "GL_CURRENT_RASTER_POSITION";
        case GL_CURRENT_RASTER_POSITION_VALID: return "GL_CURRENT_RASTER_POSITION_VALID";
        case GL_CURRENT_RASTER_DISTANCE: return "GL_CURRENT_RASTER_DISTANCE";
        case GL_POINT_SMOOTH: return "GL_POINT_SMOOTH";
        case GL_POINT_SIZE: return "GL_POINT_SIZE";
        case GL_POINT_SIZE_RANGE: return "GL_POINT_SIZE_RANGE";
        case GL_POINT_SIZE_GRANULARITY: return "GL_POINT_SIZE_GRANULARITY";
        case GL_LINE_SMOOTH: return "GL_LINE_SMOOTH";
        case GL_LINE_WIDTH: return "GL_LINE_WIDTH";
        case GL_LINE_WIDTH_RANGE: return "GL_LINE_WIDTH_RANGE";
        case GL_LINE_WIDTH_GRANULARITY: return "GL_LINE_WIDTH_GRANULARITY";
        case GL_LINE_STIPPLE: return "GL_LINE_STIPPLE";
        case GL_LINE_STIPPLE_PATTERN: return "GL_LINE_STIPPLE_PATTERN";
        case GL_LINE_STIPPLE_REPEAT: return "GL_LINE_STIPPLE_REPEAT";
        case GL_LIST_MODE: return "GL_LIST_MODE";
        case GL_MAX_LIST_NESTING: return "GL_MAX_LIST_NESTING";
        case GL_LIST_BASE: return "GL_LIST_BASE";
        case GL_LIST_INDEX: return "GL_LIST_INDEX";
        case GL_POLYGON_MODE: return "GL_POLYGON_MODE";
        case GL_POLYGON_SMOOTH: return "GL_POLYGON_SMOOTH";
        case GL_POLYGON_STIPPLE: return "GL_POLYGON_STIPPLE";
        case GL_EDGE_FLAG: return "GL_EDGE_FLAG";
        case GL_CULL_FACE: return "GL_CULL_FACE";
        case GL_CULL_FACE_MODE: return "GL_CULL_FACE_MODE";
        case GL_FRONT_FACE: return "GL_FRONT_FACE";
        case GL_LIGHTING: return "GL_LIGHTING";
        case GL_LIGHT_MODEL_LOCAL_VIEWER: return "GL_LIGHT_MODEL_LOCAL_VIEWER";
        case GL_LIGHT_MODEL_TWO_SIDE: return "GL_LIGHT_MODEL_TWO_SIDE";
        case GL_LIGHT_MODEL_AMBIENT: return "GL_LIGHT_MODEL_AMBIENT";
        case GL_SHADE_MODEL: return "GL_SHADE_MODEL";
        case GL_COLOR_MATERIAL_FACE: return "GL_COLOR_MATERIAL_FACE";
        case GL_COLOR_MATERIAL_PARAMETER: return "GL_COLOR_MATERIAL_PARAMETER";
        case GL_COLOR_MATERIAL: return "GL_COLOR_MATERIAL";
        case GL_FOG: return "GL_FOG";
        case GL_FOG_INDEX: return "GL_FOG_INDEX";
        case GL_FOG_DENSITY: return "GL_FOG_DENSITY";
        case GL_FOG_START: return "GL_FOG_START";
        case GL_FOG_END: return "GL_FOG_END";
        case GL_FOG_MODE: return "GL_FOG_MODE";
        case GL_FOG_COLOR: return "GL_FOG_COLOR";
        case GL_DEPTH_RANGE: return "GL_DEPTH_RANGE";
        case GL_DEPTH_TEST: return "GL_DEPTH_TEST";
        case GL_DEPTH_WRITEMASK: return "GL_DEPTH_WRITEMASK";
        case GL_DEPTH_CLEAR_VALUE: return "GL_DEPTH_CLEAR_VALUE";
        case GL_DEPTH_FUNC: return "GL_DEPTH_FUNC";
        case GL_ACCUM_CLEAR_VALUE: return "GL_ACCUM_CLEAR_VALUE";
        case GL_STENCIL_TEST: return "GL_STENCIL_TEST";
        case GL_STENCIL_CLEAR_VALUE: return "GL_STENCIL_CLEAR_VALUE";
        case GL_STENCIL_FUNC: return "GL_STENCIL_FUNC";
        case GL_STENCIL_VALUE_MASK: return "GL_STENCIL_VALUE_MASK";
        case GL_STENCIL_FAIL: return "GL_STENCIL_FAIL";
        case GL_STENCIL_PASS_DEPTH_FAIL: return "GL_STENCIL_PASS_DEPTH_FAIL";
        case GL_STENCIL_PASS_DEPTH_PASS: return "GL_STENCIL_PASS_DEPTH_PASS";
        case GL_STENCIL_REF: return "GL_STENCIL_REF";
        case GL_STENCIL_WRITEMASK: return "GL_STENCIL_WRITEMASK";
        case GL_MATRIX_MODE: return "GL_MATRIX_MODE";
        case GL_NORMALIZE: return "GL_NORMALIZE";
        case GL_VIEWPORT: return "GL_VIEWPORT";
        case GL_MODELVIEW_STACK_DEPTH: return "GL_MODELVIEW_STACK_DEPTH";
        case GL_PROJECTION_STACK_DEPTH: return "GL_PROJECTION_STACK_DEPTH";
        case GL_TEXTURE_STACK_DEPTH: return "GL_TEXTURE_STACK_DEPTH";
        case GL_MODELVIEW_MATRIX: return "GL_MODELVIEW_MATRIX";
        case GL_PROJECTION_MATRIX: return "GL_PROJECTION_MATRIX";
        case GL_TEXTURE_MATRIX: return "GL_TEXTURE_MATRIX";
        case GL_ATTRIB_STACK_DEPTH: return "GL_ATTRIB_STACK_DEPTH";
        case GL_CLIENT_ATTRIB_STACK_DEPTH: return "GL_CLIENT_ATTRIB_STACK_DEPTH";
        case GL_ALPHA_TEST: return "GL_ALPHA_TEST";
        case GL_ALPHA_TEST_FUNC: return "GL_ALPHA_TEST_FUNC";
        case GL_ALPHA_TEST_REF: return "GL_ALPHA_TEST_REF";
        case GL_DITHER: return "GL_DITHER";
        case GL_BLEND_DST: return "GL_BLEND_DST";
        case GL_BLEND_SRC: return "GL_BLEND_SRC";
        case GL_BLEND: return "GL_BLEND";
        case GL_LOGIC_OP_MODE: return "GL_LOGIC_OP_MODE";
        case GL_INDEX_LOGIC_OP: return "GL_INDEX_LOGIC_OP";
        case GL_COLOR_LOGIC_OP: return "GL_COLOR_LOGIC_OP";
        case GL_AUX_BUFFERS: return "GL_AUX_BUFFERS";
        case GL_DRAW_BUFFER: return "GL_DRAW_BUFFER";
        case GL_READ_BUFFER: return "GL_READ_BUFFER";
        case GL_SCISSOR_BOX: return "GL_SCISSOR_BOX";
        case GL_SCISSOR_TEST: return "GL_SCISSOR_TEST";
        case GL_INDEX_CLEAR_VALUE: return "GL_INDEX_CLEAR_VALUE";
        case GL_INDEX_WRITEMASK: return "GL_INDEX_WRITEMASK";
        case GL_COLOR_CLEAR_VALUE: return "GL_COLOR_CLEAR_VALUE";
        case GL_COLOR_WRITEMASK: return "GL_COLOR_WRITEMASK";
        case GL_INDEX_MODE: return "GL_INDEX_MODE";
        case GL_RGBA_MODE: return "GL_RGBA_MODE";
        case GL_DOUBLEBUFFER: return "GL_DOUBLEBUFFER";
        case GL_STEREO: return "GL_STEREO";
        case GL_RENDER_MODE: return "GL_RENDER_MODE";
        case GL_PERSPECTIVE_CORRECTION_HINT: return "GL_PERSPECTIVE_CORRECTION_HINT";
        case GL_POINT_SMOOTH_HINT: return "GL_POINT_SMOOTH_HINT";
        case GL_LINE_SMOOTH_HINT: return "GL_LINE_SMOOTH_HINT";
        case GL_POLYGON_SMOOTH_HINT: return "GL_POLYGON_SMOOTH_HINT";
        case GL_FOG_HINT: return "GL_FOG_HINT";
        case GL_TEXTURE_GEN_S: return "GL_TEXTURE_GEN_S";
        case GL_TEXTURE_GEN_T: return "GL_TEXTURE_GEN_T";
        case GL_TEXTURE_GEN_R: return "GL_TEXTURE_GEN_R";
        case GL_TEXTURE_GEN_Q: return "GL_TEXTURE_GEN_Q";
        case GL_PIXEL_MAP_I_TO_I: return "GL_PIXEL_MAP_I_TO_I";
        case GL_PIXEL_MAP_S_TO_S: return "GL_PIXEL_MAP_S_TO_S";
        case GL_PIXEL_MAP_I_TO_R: return "GL_PIXEL_MAP_I_TO_R";
        case GL_PIXEL_MAP_I_TO_G: return "GL_PIXEL_MAP_I_TO_G";
        case GL_PIXEL_MAP_I_TO_B: return "GL_PIXEL_MAP_I_TO_B";
        case GL_PIXEL_MAP_I_TO_A: return "GL_PIXEL_MAP_I_TO_A";
        case GL_PIXEL_MAP_R_TO_R: return "GL_PIXEL_MAP_R_TO_R";
        case GL_PIXEL_MAP_G_TO_G: return "GL_PIXEL_MAP_G_TO_G";
        case GL_PIXEL_MAP_B_TO_B: return "GL_PIXEL_MAP_B_TO_B";
        case GL_PIXEL_MAP_A_TO_A: return "GL_PIXEL_MAP_A_TO_A";
        case GL_PIXEL_MAP_I_TO_I_SIZE: return "GL_PIXEL_MAP_I_TO_I_SIZE";
        case GL_PIXEL_MAP_S_TO_S_SIZE: return "GL_PIXEL_MAP_S_TO_S_SIZE";
        case GL_PIXEL_MAP_I_TO_R_SIZE: return "GL_PIXEL_MAP_I_TO_R_SIZE";
        case GL_PIXEL_MAP_I_TO_G_SIZE: return "GL_PIXEL_MAP_I_TO_G_SIZE";
        case GL_PIXEL_MAP_I_TO_B_SIZE: return "GL_PIXEL_MAP_I_TO_B_SIZE";
        case GL_PIXEL_MAP_I_TO_A_SIZE: return "GL_PIXEL_MAP_I_TO_A_SIZE";
        case GL_PIXEL_MAP_R_TO_R_SIZE: return "GL_PIXEL_MAP_R_TO_R_SIZE";
        case GL_PIXEL_MAP_G_TO_G_SIZE: return "GL_PIXEL_MAP_G_TO_G_SIZE";
        case GL_PIXEL_MAP_B_TO_B_SIZE: return "GL_PIXEL_MAP_B_TO_B_SIZE";
        case GL_PIXEL_MAP_A_TO_A_SIZE: return "GL_PIXEL_MAP_A_TO_A_SIZE";
        case GL_UNPACK_SWAP_BYTES: return "GL_UNPACK_SWAP_BYTES";
        case GL_UNPACK_LSB_FIRST: return "GL_UNPACK_LSB_FIRST";
        case GL_UNPACK_ROW_LENGTH: return "GL_UNPACK_ROW_LENGTH";
        case GL_UNPACK_SKIP_ROWS: return "GL_UNPACK_SKIP_ROWS";
        case GL_UNPACK_SKIP_PIXELS: return "GL_UNPACK_SKIP_PIXELS";
        case GL_UNPACK_ALIGNMENT: return "GL_UNPACK_ALIGNMENT";
        case GL_PACK_SWAP_BYTES: return "GL_PACK_SWAP_BYTES";
        case GL_PACK_LSB_FIRST: return "GL_PACK_LSB_FIRST";
        case GL_PACK_ROW_LENGTH: return "GL_PACK_ROW_LENGTH";
        case GL_PACK_SKIP_ROWS: return "GL_PACK_SKIP_ROWS";
        case GL_PACK_SKIP_PIXELS: return "GL_PACK_SKIP_PIXELS";
        case GL_PACK_ALIGNMENT: return "GL_PACK_ALIGNMENT";
        case GL_MAP_COLOR: return "GL_MAP_COLOR";
        case GL_MAP_STENCIL: return "GL_MAP_STENCIL";
        case GL_INDEX_SHIFT: return "GL_INDEX_SHIFT";
        case GL_INDEX_OFFSET: return "GL_INDEX_OFFSET";
        case GL_RED_SCALE: return "GL_RED_SCALE";
        case GL_RED_BIAS: return "GL_RED_BIAS";
        case GL_ZOOM_X: return "GL_ZOOM_X";
        case GL_ZOOM_Y: return "GL_ZOOM_Y";
        case GL_GREEN_SCALE: return "GL_GREEN_SCALE";
        case GL_GREEN_BIAS: return "GL_GREEN_BIAS";
        case GL_BLUE_SCALE: return "GL_BLUE_SCALE";
        case GL_BLUE_BIAS: return "GL_BLUE_BIAS";
        case GL_ALPHA_SCALE: return "GL_ALPHA_SCALE";
        case GL_ALPHA_BIAS: return "GL_ALPHA_BIAS";
        case GL_DEPTH_SCALE: return "GL_DEPTH_SCALE";
        case GL_DEPTH_BIAS: return "GL_DEPTH_BIAS";
        case GL_MAX_EVAL_ORDER: return "GL_MAX_EVAL_ORDER";
        case GL_MAX_LIGHTS: return "GL_MAX_LIGHTS";
        case GL_MAX_CLIP_PLANES: return "GL_MAX_CLIP_PLANES";
        case GL_MAX_TEXTURE_SIZE: return "GL_MAX_TEXTURE_SIZE";
        case GL_MAX_PIXEL_MAP_TABLE: return "GL_MAX_PIXEL_MAP_TABLE";
        case GL_MAX_ATTRIB_STACK_DEPTH: return "GL_MAX_ATTRIB_STACK_DEPTH";
        case GL_MAX_MODELVIEW_STACK_DEPTH: return "GL_MAX_MODELVIEW_STACK_DEPTH";
        case GL_MAX_NAME_STACK_DEPTH: return "GL_MAX_NAME_STACK_DEPTH";
        case GL_MAX_PROJECTION_STACK_DEPTH: return "GL_MAX_PROJECTION_STACK_DEPTH";
        case GL_MAX_TEXTURE_STACK_DEPTH: return "GL_MAX_TEXTURE_STACK_DEPTH";
        case GL_MAX_VIEWPORT_DIMS: return "GL_MAX_VIEWPORT_DIMS";
        case GL_MAX_CLIENT_ATTRIB_STACK_DEPTH: return "GL_MAX_CLIENT_ATTRIB_STACK_DEPTH";
        case GL_SUBPIXEL_BITS: return "GL_SUBPIXEL_BITS";
        case GL_INDEX_BITS: return "GL_INDEX_BITS";
        case GL_RED_BITS: return "GL_RED_BITS";
        case GL_GREEN_BITS: return "GL_GREEN_BITS";
        case GL_BLUE_BITS: return "GL_BLUE_BITS";
        case GL_ALPHA_BITS: return "GL_ALPHA_BITS";
        case GL_DEPTH_BITS: return "GL_DEPTH_BITS";
        case GL_STENCIL_BITS: return "GL_STENCIL_BITS";
        case GL_ACCUM_RED_BITS: return "GL_ACCUM_RED_BITS";
        case GL_ACCUM_GREEN_BITS: return "GL_ACCUM_GREEN_BITS";
        case GL_ACCUM_BLUE_BITS: return "GL_ACCUM_BLUE_BITS";
        case GL_ACCUM_ALPHA_BITS: return "GL_ACCUM_ALPHA_BITS";
        case GL_NAME_STACK_DEPTH: return "GL_NAME_STACK_DEPTH";
        case GL_AUTO_NORMAL: return "GL_AUTO_NORMAL";
        case GL_MAP1_COLOR_4: return "GL_MAP1_COLOR_4";
        case GL_MAP1_INDEX: return "GL_MAP1_INDEX";
        case GL_MAP1_NORMAL: return "GL_MAP1_NORMAL";
        case GL_MAP1_TEXTURE_COORD_1: return "GL_MAP1_TEXTURE_COORD_1";
        case GL_MAP1_TEXTURE_COORD_2: return "GL_MAP1_TEXTURE_COORD_2";
        case GL_MAP1_TEXTURE_COORD_3: return "GL_MAP1_TEXTURE_COORD_3";
        case GL_MAP1_TEXTURE_COORD_4: return "GL_MAP1_TEXTURE_COORD_4";
        case GL_MAP1_VERTEX_3: return "GL_MAP1_VERTEX_3";
        case GL_MAP1_VERTEX_4: return "GL_MAP1_VERTEX_4";
        case GL_MAP2_COLOR_4: return "GL_MAP2_COLOR_4";
        case GL_MAP2_INDEX: return "GL_MAP2_INDEX";
        case GL_MAP2_NORMAL: return "GL_MAP2_NORMAL";
        case GL_MAP2_TEXTURE_COORD_1: return "GL_MAP2_TEXTURE_COORD_1";
        case GL_MAP2_TEXTURE_COORD_2: return "GL_MAP2_TEXTURE_COORD_2";
        case GL_MAP2_TEXTURE_COORD_3: return "GL_MAP2_TEXTURE_COORD_3";
        case GL_MAP2_TEXTURE_COORD_4: return "GL_MAP2_TEXTURE_COORD_4";
        case GL_MAP2_VERTEX_3: return "GL_MAP2_VERTEX_3";
        case GL_MAP2_VERTEX_4: return "GL_MAP2_VERTEX_4";
        case GL_MAP1_GRID_DOMAIN: return "GL_MAP1_GRID_DOMAIN";
        case GL_MAP1_GRID_SEGMENTS: return "GL_MAP1_GRID_SEGMENTS";
        case GL_MAP2_GRID_DOMAIN: return "GL_MAP2_GRID_DOMAIN";
        case GL_MAP2_GRID_SEGMENTS: return "GL_MAP2_GRID_SEGMENTS";
        case GL_TEXTURE_1D: return "GL_TEXTURE_1D";
        case GL_TEXTURE_2D: return "GL_TEXTURE_2D";
        case GL_FEEDBACK_BUFFER_POINTER: return "GL_FEEDBACK_BUFFER_POINTER";
        case GL_FEEDBACK_BUFFER_SIZE: return "GL_FEEDBACK_BUFFER_SIZE";
        case GL_FEEDBACK_BUFFER_TYPE: return "GL_FEEDBACK_BUFFER_TYPE";
        case GL_SELECTION_BUFFER_POINTER: return "GL_SELECTION_BUFFER_POINTER";
        case GL_SELECTION_BUFFER_SIZE: return "GL_SELECTION_BUFFER_SIZE";

        case GL_TEXTURE_WIDTH: return "GL_TEXTURE_WIDTH";
        case GL_TEXTURE_HEIGHT: return "GL_TEXTURE_HEIGHT";
        case GL_TEXTURE_INTERNAL_FORMAT: return "GL_TEXTURE_INTERNAL_FORMAT";
        case GL_TEXTURE_BORDER_COLOR: return "GL_TEXTURE_BORDER_COLOR";
        case GL_TEXTURE_BORDER: return "GL_TEXTURE_BORDER";

        case GL_DONT_CARE: return "GL_DONT_CARE";
        case GL_FASTEST: return "GL_FASTEST";
        case GL_NICEST: return "GL_NICEST";

        case GL_LIGHT0: return "GL_LIGHT0";
        case GL_LIGHT1: return "GL_LIGHT1";
        case GL_LIGHT2: return "GL_LIGHT2";
        case GL_LIGHT3: return "GL_LIGHT3";
        case GL_LIGHT4: return "GL_LIGHT4";
        case GL_LIGHT5: return "GL_LIGHT5";
        case GL_LIGHT6: return "GL_LIGHT6";
        case GL_LIGHT7: return "GL_LIGHT7";

        case GL_AMBIENT: return "GL_AMBIENT";
        case GL_DIFFUSE: return "GL_DIFFUSE";
        case GL_SPECULAR: return "GL_SPECULAR";
        case GL_POSITION: return "GL_POSITION";
        case GL_SPOT_DIRECTION: return "GL_SPOT_DIRECTION";
        case GL_SPOT_EXPONENT: return "GL_SPOT_EXPONENT";
        case GL_SPOT_CUTOFF: return "GL_SPOT_CUTOFF";
        case GL_CONSTANT_ATTENUATION: return "GL_CONSTANT_ATTENUATION";
        case GL_LINEAR_ATTENUATION: return "GL_LINEAR_ATTENUATION";
        case GL_QUADRATIC_ATTENUATION: return "GL_QUADRATIC_ATTENUATION";

        case GL_COMPILE: return "GL_COMPILE";
        case GL_COMPILE_AND_EXECUTE: return "GL_COMPILE_AND_EXECUTE";

        case GL_CLEAR: return "GL_CLEAR";
        case GL_AND: return "GL_AND";
        case GL_AND_REVERSE: return "GL_AND_REVERSE";
        case GL_COPY: return "GL_COPY";
        case GL_AND_INVERTED: return "GL_AND_INVERTED";
        case GL_NOOP: return "GL_NOOP";
        case GL_XOR: return "GL_XOR";
        case GL_OR: return "GL_OR";
        case GL_NOR: return "GL_NOR";
        case GL_EQUIV: return "GL_EQUIV";
        case GL_INVERT: return "GL_INVERT";
        case GL_OR_REVERSE: return "GL_OR_REVERSE";
        case GL_COPY_INVERTED: return "GL_COPY_INVERTED";
        case GL_OR_INVERTED: return "GL_OR_INVERTED";
        case GL_NAND: return "GL_NAND";
        case GL_SET: return "GL_SET";

        case GL_EMISSION: return "GL_EMISSION";
        case GL_SHININESS: return "GL_SHININESS";
        case GL_AMBIENT_AND_DIFFUSE: return "GL_AMBIENT_AND_DIFFUSE";
        case GL_COLOR_INDEXES: return "GL_COLOR_INDEXES";

        case GL_MODELVIEW: return "GL_MODELVIEW";
        case GL_PROJECTION: return "GL_PROJECTION";
        case GL_TEXTURE: return "GL_TEXTURE";

        case GL_COLOR: return "GL_COLOR";
        case GL_DEPTH: return "GL_DEPTH";
        case GL_STENCIL: return "GL_STENCIL";

        case GL_COLOR_INDEX: return "GL_COLOR_INDEX";
        case GL_STENCIL_INDEX: return "GL_STENCIL_INDEX";
        case GL_DEPTH_COMPONENT: return "GL_DEPTH_COMPONENT";
        case GL_RED: return "GL_RED";
        case GL_GREEN: return "GL_GREEN";
        case GL_BLUE: return "GL_BLUE";
        case GL_ALPHA: return "GL_ALPHA";
        case GL_RGB: return "GL_RGB";
        case GL_RGBA: return "GL_RGBA";
        case GL_LUMINANCE: return "GL_LUMINANCE";
        case GL_LUMINANCE_ALPHA: return "GL_LUMINANCE_ALPHA";

        case GL_BITMAP: return "GL_BITMAP";

        case GL_POINT: return "GL_POINT";
        case GL_LINE: return "GL_LINE";
        case GL_FILL: return "GL_FILL";

        case GL_RENDER: return "GL_RENDER";
        case GL_FEEDBACK: return "GL_FEEDBACK";
        case GL_SELECT: return "GL_SELECT";

        case GL_FLAT: return "GL_FLAT";
        case GL_SMOOTH: return "GL_SMOOTH";

        case GL_KEEP: return "GL_KEEP";
        case GL_REPLACE: return "GL_REPLACE";
        case GL_INCR: return "GL_INCR";
        case GL_DECR: return "GL_DECR";

        case GL_VENDOR: return "GL_VENDOR";
        case GL_RENDERER: return "GL_RENDERER";
        case GL_VERSION: return "GL_VERSION";
        case GL_EXTENSIONS: return "GL_EXTENSIONS";

        case GL_S: return "GL_S";
        case GL_T: return "GL_T";
        case GL_R: return "GL_R";
        case GL_Q: return "GL_Q";

        case GL_MODULATE: return "GL_MODULATE";
        case GL_DECAL: return "GL_DECAL";

        case GL_TEXTURE_ENV_MODE: return "GL_TEXTURE_ENV_MODE";
        case GL_TEXTURE_ENV_COLOR: return "GL_TEXTURE_ENV_COLOR";

        case GL_TEXTURE_ENV: return "GL_TEXTURE_ENV";

        case GL_EYE_LINEAR: return "GL_EYE_LINEAR";
        case GL_OBJECT_LINEAR: return "GL_OBJECT_LINEAR";
        case GL_SPHERE_MAP: return "GL_SPHERE_MAP";

        case GL_TEXTURE_GEN_MODE: return "GL_TEXTURE_GEN_MODE";
        case GL_OBJECT_PLANE: return "GL_OBJECT_PLANE";
        case GL_EYE_PLANE: return "GL_EYE_PLANE";

        case GL_NEAREST: return "GL_NEAREST";
        case GL_LINEAR: return "GL_LINEAR";

        case GL_NEAREST_MIPMAP_NEAREST: return "GL_NEAREST_MIPMAP_NEAREST";
        case GL_LINEAR_MIPMAP_NEAREST: return "GL_LINEAR_MIPMAP_NEAREST";
        case GL_NEAREST_MIPMAP_LINEAR: return "GL_NEAREST_MIPMAP_LINEAR";
        case GL_LINEAR_MIPMAP_LINEAR: return "GL_LINEAR_MIPMAP_LINEAR";

        case GL_TEXTURE_MAG_FILTER: return "GL_TEXTURE_MAG_FILTER";
        case GL_TEXTURE_MIN_FILTER: return "GL_TEXTURE_MIN_FILTER";
        case GL_TEXTURE_WRAP_S: return "GL_TEXTURE_WRAP_S";
        case GL_TEXTURE_WRAP_T: return "GL_TEXTURE_WRAP_T";

        case GL_CLAMP: return "GL_CLAMP";
        case GL_REPEAT: return "GL_REPEAT";

        // case GL_CLIENT_PIXEL_STORE_BIT: return "GL_CLIENT_PIXEL_STORE_BIT";
        // case GL_CLIENT_VERTEX_ARRAY_BIT: return "GL_CLIENT_VERTEX_ARRAY_BIT";
        // case GL_CLIENT_ALL_ATTRIB_BITS: return "GL_CLIENT_ALL_ATTRIB_BITS";

        case GL_POLYGON_OFFSET_FACTOR: return "GL_POLYGON_OFFSET_FACTOR";
        case GL_POLYGON_OFFSET_UNITS: return "GL_POLYGON_OFFSET_UNITS";
        case GL_POLYGON_OFFSET_POINT: return "GL_POLYGON_OFFSET_POINT";
        case GL_POLYGON_OFFSET_LINE: return "GL_POLYGON_OFFSET_LINE";
        case GL_POLYGON_OFFSET_FILL: return "GL_POLYGON_OFFSET_FILL";

        case GL_ALPHA4: return "GL_ALPHA4";
        case GL_ALPHA8: return "GL_ALPHA8";
        case GL_ALPHA12: return "GL_ALPHA12";
        case GL_ALPHA16: return "GL_ALPHA16";
        case GL_LUMINANCE4: return "GL_LUMINANCE4";
        case GL_LUMINANCE8: return "GL_LUMINANCE8";
        case GL_LUMINANCE12: return "GL_LUMINANCE12";
        case GL_LUMINANCE16: return "GL_LUMINANCE16";
        case GL_LUMINANCE4_ALPHA4: return "GL_LUMINANCE4_ALPHA4";
        case GL_LUMINANCE6_ALPHA2: return "GL_LUMINANCE6_ALPHA2";
        case GL_LUMINANCE8_ALPHA8: return "GL_LUMINANCE8_ALPHA8";
        case GL_LUMINANCE12_ALPHA4: return "GL_LUMINANCE12_ALPHA4";
        case GL_LUMINANCE12_ALPHA12: return "GL_LUMINANCE12_ALPHA12";
        case GL_LUMINANCE16_ALPHA16: return "GL_LUMINANCE16_ALPHA16";
        case GL_INTENSITY: return "GL_INTENSITY";
        case GL_INTENSITY4: return "GL_INTENSITY4";
        case GL_INTENSITY8: return "GL_INTENSITY8";
        case GL_INTENSITY12: return "GL_INTENSITY12";
        case GL_INTENSITY16: return "GL_INTENSITY16";
        case GL_R3_G3_B2: return "GL_R3_G3_B2";
        case GL_RGB4: return "GL_RGB4";
        case GL_RGB5: return "GL_RGB5";
        case GL_RGB8: return "GL_RGB8";
        case GL_RGB10: return "GL_RGB10";
        case GL_RGB12: return "GL_RGB12";
        case GL_RGB16: return "GL_RGB16";
        case GL_RGBA2: return "GL_RGBA2";
        case GL_RGBA4: return "GL_RGBA4";
        case GL_RGB5_A1: return "GL_RGB5_A1";
        case GL_RGBA8: return "GL_RGBA8";
        case GL_RGB10_A2: return "GL_RGB10_A2";
        case GL_RGBA12: return "GL_RGBA12";
        case GL_RGBA16: return "GL_RGBA16";
        case GL_TEXTURE_RED_SIZE: return "GL_TEXTURE_RED_SIZE";
        case GL_TEXTURE_GREEN_SIZE: return "GL_TEXTURE_GREEN_SIZE";
        case GL_TEXTURE_BLUE_SIZE: return "GL_TEXTURE_BLUE_SIZE";
        case GL_TEXTURE_ALPHA_SIZE: return "GL_TEXTURE_ALPHA_SIZE";
        case GL_TEXTURE_LUMINANCE_SIZE: return "GL_TEXTURE_LUMINANCE_SIZE";
        case GL_TEXTURE_INTENSITY_SIZE: return "GL_TEXTURE_INTENSITY_SIZE";
        case GL_PROXY_TEXTURE_1D: return "GL_PROXY_TEXTURE_1D";
        case GL_PROXY_TEXTURE_2D: return "GL_PROXY_TEXTURE_2D";

        case GL_TEXTURE_PRIORITY: return "GL_TEXTURE_PRIORITY";
        case GL_TEXTURE_RESIDENT: return "GL_TEXTURE_RESIDENT";
        case GL_TEXTURE_BINDING_1D: return "GL_TEXTURE_BINDING_1D";
        case GL_TEXTURE_BINDING_2D: return "GL_TEXTURE_BINDING_2D";
        case GL_TEXTURE_BINDING_3D: return "GL_TEXTURE_BINDING_3D";

        case GL_VERTEX_ARRAY: return "GL_VERTEX_ARRAY";
        case GL_NORMAL_ARRAY: return "GL_NORMAL_ARRAY";
        case GL_COLOR_ARRAY: return "GL_COLOR_ARRAY";
        case GL_INDEX_ARRAY: return "GL_INDEX_ARRAY";
        case GL_TEXTURE_COORD_ARRAY: return "GL_TEXTURE_COORD_ARRAY";
        case GL_EDGE_FLAG_ARRAY: return "GL_EDGE_FLAG_ARRAY";
        case GL_VERTEX_ARRAY_SIZE: return "GL_VERTEX_ARRAY_SIZE";
        case GL_VERTEX_ARRAY_TYPE: return "GL_VERTEX_ARRAY_TYPE";
        case GL_VERTEX_ARRAY_STRIDE: return "GL_VERTEX_ARRAY_STRIDE";
        case GL_NORMAL_ARRAY_TYPE: return "GL_NORMAL_ARRAY_TYPE";
        case GL_NORMAL_ARRAY_STRIDE: return "GL_NORMAL_ARRAY_STRIDE";
        case GL_COLOR_ARRAY_SIZE: return "GL_COLOR_ARRAY_SIZE";
        case GL_COLOR_ARRAY_TYPE: return "GL_COLOR_ARRAY_TYPE";
        case GL_COLOR_ARRAY_STRIDE: return "GL_COLOR_ARRAY_STRIDE";
        case GL_INDEX_ARRAY_TYPE: return "GL_INDEX_ARRAY_TYPE";
        case GL_INDEX_ARRAY_STRIDE: return "GL_INDEX_ARRAY_STRIDE";
        case GL_TEXTURE_COORD_ARRAY_SIZE: return "GL_TEXTURE_COORD_ARRAY_SIZE";
        case GL_TEXTURE_COORD_ARRAY_TYPE: return "GL_TEXTURE_COORD_ARRAY_TYPE";
        case GL_TEXTURE_COORD_ARRAY_STRIDE: return "GL_TEXTURE_COORD_ARRAY_STRIDE";
        case GL_EDGE_FLAG_ARRAY_STRIDE: return "GL_EDGE_FLAG_ARRAY_STRIDE";
        case GL_VERTEX_ARRAY_POINTER: return "GL_VERTEX_ARRAY_POINTER";
        case GL_NORMAL_ARRAY_POINTER: return "GL_NORMAL_ARRAY_POINTER";
        case GL_COLOR_ARRAY_POINTER: return "GL_COLOR_ARRAY_POINTER";
        case GL_INDEX_ARRAY_POINTER: return "GL_INDEX_ARRAY_POINTER";
        case GL_TEXTURE_COORD_ARRAY_POINTER: return "GL_TEXTURE_COORD_ARRAY_POINTER";
        case GL_EDGE_FLAG_ARRAY_POINTER: return "GL_EDGE_FLAG_ARRAY_POINTER";
        case GL_V2F: return "GL_V2F";
        case GL_V3F: return "GL_V3F";
        case GL_C4UB_V2F: return "GL_C4UB_V2F";
        case GL_C4UB_V3F: return "GL_C4UB_V3F";
        case GL_C3F_V3F: return "GL_C3F_V3F";
        case GL_N3F_V3F: return "GL_N3F_V3F";
        case GL_C4F_N3F_V3F: return "GL_C4F_N3F_V3F";
        case GL_T2F_V3F: return "GL_T2F_V3F";
        case GL_T4F_V4F: return "GL_T4F_V4F";
        case GL_T2F_C4UB_V3F: return "GL_T2F_C4UB_V3F";
        case GL_T2F_C3F_V3F: return "GL_T2F_C3F_V3F";
        case GL_T2F_N3F_V3F: return "GL_T2F_N3F_V3F";
        case GL_T2F_C4F_N3F_V3F: return "GL_T2F_C4F_N3F_V3F";
        case GL_T4F_C4F_N3F_V4F: return "GL_T4F_C4F_N3F_V4F";

        case GL_BGR: return "GL_BGR";
        case GL_BGRA: return "GL_BGRA";

        case GL_CONSTANT_COLOR: return "GL_CONSTANT_COLOR";
        case GL_ONE_MINUS_CONSTANT_COLOR: return "GL_ONE_MINUS_CONSTANT_COLOR";
        case GL_CONSTANT_ALPHA: return "GL_CONSTANT_ALPHA";
        case GL_ONE_MINUS_CONSTANT_ALPHA: return "GL_ONE_MINUS_CONSTANT_ALPHA";
        case GL_BLEND_COLOR: return "GL_BLEND_COLOR";

        case GL_FUNC_ADD: return "GL_FUNC_ADD";
        case GL_MIN: return "GL_MIN";
        case GL_MAX: return "GL_MAX";
        case GL_BLEND_EQUATION: return "GL_BLEND_EQUATION";

        // case GL_BLEND_EQUATION_RGB: return "GL_BLEND_EQUATION_RGB";
        case GL_BLEND_EQUATION_ALPHA: return "GL_BLEND_EQUATION_ALPHA";

        case GL_FUNC_SUBTRACT: return "GL_FUNC_SUBTRACT";
        case GL_FUNC_REVERSE_SUBTRACT: return "GL_FUNC_REVERSE_SUBTRACT";

        case GL_COLOR_MATRIX: return "GL_COLOR_MATRIX";
        case GL_COLOR_MATRIX_STACK_DEPTH: return "GL_COLOR_MATRIX_STACK_DEPTH";
        case GL_MAX_COLOR_MATRIX_STACK_DEPTH: return "GL_MAX_COLOR_MATRIX_STACK_DEPTH";
        case GL_POST_COLOR_MATRIX_RED_SCALE: return "GL_POST_COLOR_MATRIX_RED_SCALE";
        case GL_POST_COLOR_MATRIX_GREEN_SCALE: return "GL_POST_COLOR_MATRIX_GREEN_SCALE";
        case GL_POST_COLOR_MATRIX_BLUE_SCALE: return "GL_POST_COLOR_MATRIX_BLUE_SCALE";
        case GL_POST_COLOR_MATRIX_ALPHA_SCALE: return "GL_POST_COLOR_MATRIX_ALPHA_SCALE";
        case GL_POST_COLOR_MATRIX_RED_BIAS: return "GL_POST_COLOR_MATRIX_RED_BIAS";
        case GL_POST_COLOR_MATRIX_GREEN_BIAS: return "GL_POST_COLOR_MATRIX_GREEN_BIAS";
        case GL_POST_COLOR_MATRIX_BLUE_BIAS: return "GL_POST_COLOR_MATRIX_BLUE_BIAS";
        case GL_POST_COLOR_MATRIX_ALPHA_BIAS: return "GL_POST_COLOR_MATRIX_ALPHA_BIAS";

        case GL_COLOR_TABLE: return "GL_COLOR_TABLE";
        case GL_POST_CONVOLUTION_COLOR_TABLE: return "GL_POST_CONVOLUTION_COLOR_TABLE";
        case GL_POST_COLOR_MATRIX_COLOR_TABLE: return "GL_POST_COLOR_MATRIX_COLOR_TABLE";
        case GL_PROXY_COLOR_TABLE: return "GL_PROXY_COLOR_TABLE";
        case GL_PROXY_POST_CONVOLUTION_COLOR_TABLE: return "GL_PROXY_POST_CONVOLUTION_COLOR_TABLE";
        case GL_PROXY_POST_COLOR_MATRIX_COLOR_TABLE: return "GL_PROXY_POST_COLOR_MATRIX_COLOR_TABLE";
        case GL_COLOR_TABLE_SCALE: return "GL_COLOR_TABLE_SCALE";
        case GL_COLOR_TABLE_BIAS: return "GL_COLOR_TABLE_BIAS";
        case GL_COLOR_TABLE_FORMAT: return "GL_COLOR_TABLE_FORMAT";
        case GL_COLOR_TABLE_WIDTH: return "GL_COLOR_TABLE_WIDTH";
        case GL_COLOR_TABLE_RED_SIZE: return "GL_COLOR_TABLE_RED_SIZE";
        case GL_COLOR_TABLE_GREEN_SIZE: return "GL_COLOR_TABLE_GREEN_SIZE";
        case GL_COLOR_TABLE_BLUE_SIZE: return "GL_COLOR_TABLE_BLUE_SIZE";
        case GL_COLOR_TABLE_ALPHA_SIZE: return "GL_COLOR_TABLE_ALPHA_SIZE";
        case GL_COLOR_TABLE_LUMINANCE_SIZE: return "GL_COLOR_TABLE_LUMINANCE_SIZE";
        case GL_COLOR_TABLE_INTENSITY_SIZE: return "GL_COLOR_TABLE_INTENSITY_SIZE";

        case GL_CONVOLUTION_1D: return "GL_CONVOLUTION_1D";
        case GL_CONVOLUTION_2D: return "GL_CONVOLUTION_2D";
        case GL_SEPARABLE_2D: return "GL_SEPARABLE_2D";
        case GL_CONVOLUTION_BORDER_MODE: return "GL_CONVOLUTION_BORDER_MODE";
        case GL_CONVOLUTION_FILTER_SCALE: return "GL_CONVOLUTION_FILTER_SCALE";
        case GL_CONVOLUTION_FILTER_BIAS: return "GL_CONVOLUTION_FILTER_BIAS";
        case GL_REDUCE: return "GL_REDUCE";
        case GL_CONVOLUTION_FORMAT: return "GL_CONVOLUTION_FORMAT";
        case GL_CONVOLUTION_WIDTH: return "GL_CONVOLUTION_WIDTH";
        case GL_CONVOLUTION_HEIGHT: return "GL_CONVOLUTION_HEIGHT";
        case GL_MAX_CONVOLUTION_WIDTH: return "GL_MAX_CONVOLUTION_WIDTH";
        case GL_MAX_CONVOLUTION_HEIGHT: return "GL_MAX_CONVOLUTION_HEIGHT";
        case GL_POST_CONVOLUTION_RED_SCALE: return "GL_POST_CONVOLUTION_RED_SCALE";
        case GL_POST_CONVOLUTION_GREEN_SCALE: return "GL_POST_CONVOLUTION_GREEN_SCALE";
        case GL_POST_CONVOLUTION_BLUE_SCALE: return "GL_POST_CONVOLUTION_BLUE_SCALE";
        case GL_POST_CONVOLUTION_ALPHA_SCALE: return "GL_POST_CONVOLUTION_ALPHA_SCALE";
        case GL_POST_CONVOLUTION_RED_BIAS: return "GL_POST_CONVOLUTION_RED_BIAS";
        case GL_POST_CONVOLUTION_GREEN_BIAS: return "GL_POST_CONVOLUTION_GREEN_BIAS";
        case GL_POST_CONVOLUTION_BLUE_BIAS: return "GL_POST_CONVOLUTION_BLUE_BIAS";
        case GL_POST_CONVOLUTION_ALPHA_BIAS: return "GL_POST_CONVOLUTION_ALPHA_BIAS";
        case GL_CONSTANT_BORDER: return "GL_CONSTANT_BORDER";
        case GL_REPLICATE_BORDER: return "GL_REPLICATE_BORDER";
        case GL_CONVOLUTION_BORDER_COLOR: return "GL_CONVOLUTION_BORDER_COLOR";

        case GL_MAX_ELEMENTS_VERTICES: return "GL_MAX_ELEMENTS_VERTICES";
        case GL_MAX_ELEMENTS_INDICES: return "GL_MAX_ELEMENTS_INDICES";

        case GL_HISTOGRAM: return "GL_HISTOGRAM";
        case GL_PROXY_HISTOGRAM: return "GL_PROXY_HISTOGRAM";
        case GL_HISTOGRAM_WIDTH: return "GL_HISTOGRAM_WIDTH";
        case GL_HISTOGRAM_FORMAT: return "GL_HISTOGRAM_FORMAT";
        case GL_HISTOGRAM_RED_SIZE: return "GL_HISTOGRAM_RED_SIZE";
        case GL_HISTOGRAM_GREEN_SIZE: return "GL_HISTOGRAM_GREEN_SIZE";
        case GL_HISTOGRAM_BLUE_SIZE: return "GL_HISTOGRAM_BLUE_SIZE";
        case GL_HISTOGRAM_ALPHA_SIZE: return "GL_HISTOGRAM_ALPHA_SIZE";
        case GL_HISTOGRAM_LUMINANCE_SIZE: return "GL_HISTOGRAM_LUMINANCE_SIZE";
        case GL_HISTOGRAM_SINK: return "GL_HISTOGRAM_SINK";
        case GL_MINMAX: return "GL_MINMAX";
        case GL_MINMAX_FORMAT: return "GL_MINMAX_FORMAT";
        case GL_MINMAX_SINK: return "GL_MINMAX_SINK";
        case GL_TABLE_TOO_LARGE: return "GL_TABLE_TOO_LARGE";

        case GL_UNSIGNED_BYTE_3_3_2: return "GL_UNSIGNED_BYTE_3_3_2";
        case GL_UNSIGNED_SHORT_4_4_4_4: return "GL_UNSIGNED_SHORT_4_4_4_4";
        case GL_UNSIGNED_SHORT_5_5_5_1: return "GL_UNSIGNED_SHORT_5_5_5_1";
        case GL_UNSIGNED_INT_8_8_8_8: return "GL_UNSIGNED_INT_8_8_8_8";
        case GL_UNSIGNED_INT_10_10_10_2: return "GL_UNSIGNED_INT_10_10_10_2";
        case GL_UNSIGNED_BYTE_2_3_3_REV: return "GL_UNSIGNED_BYTE_2_3_3_REV";
        case GL_UNSIGNED_SHORT_5_6_5: return "GL_UNSIGNED_SHORT_5_6_5";
        case GL_UNSIGNED_SHORT_5_6_5_REV: return "GL_UNSIGNED_SHORT_5_6_5_REV";
        case GL_UNSIGNED_SHORT_4_4_4_4_REV: return "GL_UNSIGNED_SHORT_4_4_4_4_REV";
        case GL_UNSIGNED_SHORT_1_5_5_5_REV: return "GL_UNSIGNED_SHORT_1_5_5_5_REV";
        case GL_UNSIGNED_INT_8_8_8_8_REV: return "GL_UNSIGNED_INT_8_8_8_8_REV";
        case GL_UNSIGNED_INT_2_10_10_10_REV: return "GL_UNSIGNED_INT_2_10_10_10_REV";

        case GL_RESCALE_NORMAL: return "GL_RESCALE_NORMAL";

        case GL_LIGHT_MODEL_COLOR_CONTROL: return "GL_LIGHT_MODEL_COLOR_CONTROL";
        case GL_SINGLE_COLOR: return "GL_SINGLE_COLOR";
        case GL_SEPARATE_SPECULAR_COLOR: return "GL_SEPARATE_SPECULAR_COLOR";

        case GL_PACK_SKIP_IMAGES: return "GL_PACK_SKIP_IMAGES";
        case GL_PACK_IMAGE_HEIGHT: return "GL_PACK_IMAGE_HEIGHT";
        case GL_UNPACK_SKIP_IMAGES: return "GL_UNPACK_SKIP_IMAGES";
        case GL_UNPACK_IMAGE_HEIGHT: return "GL_UNPACK_IMAGE_HEIGHT";
        case GL_TEXTURE_3D: return "GL_TEXTURE_3D";
        case GL_PROXY_TEXTURE_3D: return "GL_PROXY_TEXTURE_3D";
        case GL_TEXTURE_DEPTH: return "GL_TEXTURE_DEPTH";
        case GL_TEXTURE_WRAP_R: return "GL_TEXTURE_WRAP_R";
        case GL_MAX_3D_TEXTURE_SIZE: return "GL_MAX_3D_TEXTURE_SIZE";

        case GL_CLAMP_TO_EDGE: return "GL_CLAMP_TO_EDGE";
        case GL_CLAMP_TO_BORDER: return "GL_CLAMP_TO_BORDER";

        case GL_TEXTURE_MIN_LOD: return "GL_TEXTURE_MIN_LOD";
        case GL_TEXTURE_MAX_LOD: return "GL_TEXTURE_MAX_LOD";
        case GL_TEXTURE_BASE_LEVEL: return "GL_TEXTURE_BASE_LEVEL";
        case GL_TEXTURE_MAX_LEVEL: return "GL_TEXTURE_MAX_LEVEL";

        // case GL_SMOOTH_POINT_SIZE_RANGE: return "GL_SMOOTH_POINT_SIZE_RANGE";
        // case GL_SMOOTH_POINT_SIZE_GRANULARITY: return "GL_SMOOTH_POINT_SIZE_GRANULARITY";
        // case GL_SMOOTH_LINE_WIDTH_RANGE: return "GL_SMOOTH_LINE_WIDTH_RANGE";
        // case GL_SMOOTH_LINE_WIDTH_GRANULARITY: return "GL_SMOOTH_LINE_WIDTH_GRANULARITY";
        case GL_ALIASED_POINT_SIZE_RANGE: return "GL_ALIASED_POINT_SIZE_RANGE";
        case GL_ALIASED_LINE_WIDTH_RANGE: return "GL_ALIASED_LINE_WIDTH_RANGE";

        case GL_TEXTURE0: return "GL_TEXTURE0";
        case GL_TEXTURE1: return "GL_TEXTURE1";
        case GL_TEXTURE2: return "GL_TEXTURE2";
        case GL_TEXTURE3: return "GL_TEXTURE3";
        case GL_TEXTURE4: return "GL_TEXTURE4";
        case GL_TEXTURE5: return "GL_TEXTURE5";
        case GL_TEXTURE6: return "GL_TEXTURE6";
        case GL_TEXTURE7: return "GL_TEXTURE7";
        case GL_TEXTURE8: return "GL_TEXTURE8";
        case GL_TEXTURE9: return "GL_TEXTURE9";
        case GL_TEXTURE10: return "GL_TEXTURE10";
        case GL_TEXTURE11: return "GL_TEXTURE11";
        case GL_TEXTURE12: return "GL_TEXTURE12";
        case GL_TEXTURE13: return "GL_TEXTURE13";
        case GL_TEXTURE14: return "GL_TEXTURE14";
        case GL_TEXTURE15: return "GL_TEXTURE15";
        case GL_TEXTURE16: return "GL_TEXTURE16";
        case GL_TEXTURE17: return "GL_TEXTURE17";
        case GL_TEXTURE18: return "GL_TEXTURE18";
        case GL_TEXTURE19: return "GL_TEXTURE19";
        case GL_TEXTURE20: return "GL_TEXTURE20";
        case GL_TEXTURE21: return "GL_TEXTURE21";
        case GL_TEXTURE22: return "GL_TEXTURE22";
        case GL_TEXTURE23: return "GL_TEXTURE23";
        case GL_TEXTURE24: return "GL_TEXTURE24";
        case GL_TEXTURE25: return "GL_TEXTURE25";
        case GL_TEXTURE26: return "GL_TEXTURE26";
        case GL_TEXTURE27: return "GL_TEXTURE27";
        case GL_TEXTURE28: return "GL_TEXTURE28";
        case GL_TEXTURE29: return "GL_TEXTURE29";
        case GL_TEXTURE30: return "GL_TEXTURE30";
        case GL_TEXTURE31: return "GL_TEXTURE31";
        case GL_ACTIVE_TEXTURE: return "GL_ACTIVE_TEXTURE";
        case GL_CLIENT_ACTIVE_TEXTURE: return "GL_CLIENT_ACTIVE_TEXTURE";
        case GL_MAX_TEXTURE_UNITS: return "GL_MAX_TEXTURE_UNITS";

        case GL_COMBINE: return "GL_COMBINE";
        case GL_COMBINE_RGB: return "GL_COMBINE_RGB";
        case GL_COMBINE_ALPHA: return "GL_COMBINE_ALPHA";
        case GL_RGB_SCALE: return "GL_RGB_SCALE";
        case GL_ADD_SIGNED: return "GL_ADD_SIGNED";
        case GL_INTERPOLATE: return "GL_INTERPOLATE";
        case GL_CONSTANT: return "GL_CONSTANT";
        case GL_PRIMARY_COLOR: return "GL_PRIMARY_COLOR";
        case GL_PREVIOUS: return "GL_PREVIOUS";
        case GL_SUBTRACT: return "GL_SUBTRACT";

        case GL_SRC0_RGB: return "GL_SRC0_RGB";
        case GL_SRC1_RGB: return "GL_SRC1_RGB";
        case GL_SRC2_RGB: return "GL_SRC2_RGB";
        case GL_SRC0_ALPHA: return "GL_SRC0_ALPHA";
        case GL_SRC1_ALPHA: return "GL_SRC1_ALPHA";
        case GL_SRC2_ALPHA: return "GL_SRC2_ALPHA";

        // case GL_SOURCE0_RGB: return "GL_SOURCE0_RGB";
        // case GL_SOURCE1_RGB: return "GL_SOURCE1_RGB";
        // case GL_SOURCE2_RGB: return "GL_SOURCE2_RGB";
        // case GL_SOURCE0_ALPHA: return "GL_SOURCE0_ALPHA";
        // case GL_SOURCE1_ALPHA: return "GL_SOURCE1_ALPHA";
        // case GL_SOURCE2_ALPHA: return "GL_SOURCE2_ALPHA";

        case GL_OPERAND0_RGB: return "GL_OPERAND0_RGB";
        case GL_OPERAND1_RGB: return "GL_OPERAND1_RGB";
        case GL_OPERAND2_RGB: return "GL_OPERAND2_RGB";
        case GL_OPERAND0_ALPHA: return "GL_OPERAND0_ALPHA";
        case GL_OPERAND1_ALPHA: return "GL_OPERAND1_ALPHA";
        case GL_OPERAND2_ALPHA: return "GL_OPERAND2_ALPHA";

        case GL_DOT3_RGB: return "GL_DOT3_RGB";
        case GL_DOT3_RGBA: return "GL_DOT3_RGBA";

        case GL_TRANSPOSE_MODELVIEW_MATRIX: return "GL_TRANSPOSE_MODELVIEW_MATRIX";
        case GL_TRANSPOSE_PROJECTION_MATRIX: return "GL_TRANSPOSE_PROJECTION_MATRIX";
        case GL_TRANSPOSE_TEXTURE_MATRIX: return "GL_TRANSPOSE_TEXTURE_MATRIX";
        case GL_TRANSPOSE_COLOR_MATRIX: return "GL_TRANSPOSE_COLOR_MATRIX";

        case GL_NORMAL_MAP: return "GL_NORMAL_MAP";
        case GL_REFLECTION_MAP: return "GL_REFLECTION_MAP";
        case GL_TEXTURE_CUBE_MAP: return "GL_TEXTURE_CUBE_MAP";
        case GL_TEXTURE_BINDING_CUBE_MAP: return "GL_TEXTURE_BINDING_CUBE_MAP";
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X: return "GL_TEXTURE_CUBE_MAP_POSITIVE_X";
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X: return "GL_TEXTURE_CUBE_MAP_NEGATIVE_X";
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y: return "GL_TEXTURE_CUBE_MAP_POSITIVE_Y";
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y: return "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y";
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z: return "GL_TEXTURE_CUBE_MAP_POSITIVE_Z";
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z: return "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z";
        case GL_PROXY_TEXTURE_CUBE_MAP: return "GL_PROXY_TEXTURE_CUBE_MAP";
        case GL_MAX_CUBE_MAP_TEXTURE_SIZE: return "GL_MAX_CUBE_MAP_TEXTURE_SIZE";

        case GL_COMPRESSED_ALPHA: return "GL_COMPRESSED_ALPHA";
        case GL_COMPRESSED_LUMINANCE: return "GL_COMPRESSED_LUMINANCE";
        case GL_COMPRESSED_LUMINANCE_ALPHA: return "GL_COMPRESSED_LUMINANCE_ALPHA";
        case GL_COMPRESSED_INTENSITY: return "GL_COMPRESSED_INTENSITY";
        case GL_COMPRESSED_RGB: return "GL_COMPRESSED_RGB";
        case GL_COMPRESSED_RGBA: return "GL_COMPRESSED_RGBA";
        case GL_TEXTURE_COMPRESSION_HINT: return "GL_TEXTURE_COMPRESSION_HINT";
        case GL_TEXTURE_COMPRESSED_IMAGE_SIZE: return "GL_TEXTURE_COMPRESSED_IMAGE_SIZE";
        case GL_TEXTURE_COMPRESSED: return "GL_TEXTURE_COMPRESSED";
        case GL_NUM_COMPRESSED_TEXTURE_FORMATS: return "GL_NUM_COMPRESSED_TEXTURE_FORMATS";
        case GL_COMPRESSED_TEXTURE_FORMATS: return "GL_COMPRESSED_TEXTURE_FORMATS";

        case GL_MULTISAMPLE: return "GL_MULTISAMPLE";
        case GL_SAMPLE_ALPHA_TO_COVERAGE: return "GL_SAMPLE_ALPHA_TO_COVERAGE";
        case GL_SAMPLE_ALPHA_TO_ONE: return "GL_SAMPLE_ALPHA_TO_ONE";
        case GL_SAMPLE_COVERAGE: return "GL_SAMPLE_COVERAGE";
        case GL_SAMPLE_BUFFERS: return "GL_SAMPLE_BUFFERS";
        case GL_SAMPLES: return "GL_SAMPLES";
        case GL_SAMPLE_COVERAGE_VALUE: return "GL_SAMPLE_COVERAGE_VALUE";
        case GL_SAMPLE_COVERAGE_INVERT: return "GL_SAMPLE_COVERAGE_INVERT";
        case GL_MULTISAMPLE_BIT: return "GL_MULTISAMPLE_BIT";

        case GL_DEPTH_COMPONENT16: return "GL_DEPTH_COMPONENT16";
        case GL_DEPTH_COMPONENT24: return "GL_DEPTH_COMPONENT24";
        case GL_DEPTH_COMPONENT32: return "GL_DEPTH_COMPONENT32";
        case GL_TEXTURE_DEPTH_SIZE: return "GL_TEXTURE_DEPTH_SIZE";
        case GL_DEPTH_TEXTURE_MODE: return "GL_DEPTH_TEXTURE_MODE";

        case GL_TEXTURE_COMPARE_MODE: return "GL_TEXTURE_COMPARE_MODE";
        case GL_TEXTURE_COMPARE_FUNC: return "GL_TEXTURE_COMPARE_FUNC";
        case GL_COMPARE_R_TO_TEXTURE: return "GL_COMPARE_R_TO_TEXTURE";

        case GL_QUERY_COUNTER_BITS: return "GL_QUERY_COUNTER_BITS";
        case GL_CURRENT_QUERY: return "GL_CURRENT_QUERY";
        case GL_QUERY_RESULT: return "GL_QUERY_RESULT";
        case GL_QUERY_RESULT_AVAILABLE: return "GL_QUERY_RESULT_AVAILABLE";
        case GL_SAMPLES_PASSED: return "GL_SAMPLES_PASSED";

        case GL_FOG_COORD_SRC: return "GL_FOG_COORD_SRC";
        case GL_FOG_COORD: return "GL_FOG_COORD";
        case GL_FRAGMENT_DEPTH: return "GL_FRAGMENT_DEPTH";
        case GL_CURRENT_FOG_COORD: return "GL_CURRENT_FOG_COORD";
        case GL_FOG_COORD_ARRAY_TYPE: return "GL_FOG_COORD_ARRAY_TYPE";
        case GL_FOG_COORD_ARRAY_STRIDE: return "GL_FOG_COORD_ARRAY_STRIDE";
        case GL_FOG_COORD_ARRAY_POINTER: return "GL_FOG_COORD_ARRAY_POINTER";
        case GL_FOG_COORD_ARRAY: return "GL_FOG_COORD_ARRAY";

        // case GL_FOG_COORDINATE_SOURCE: return "GL_FOG_COORDINATE_SOURCE";
        // case GL_FOG_COORDINATE: return "GL_FOG_COORDINATE";
        // case GL_CURRENT_FOG_COORDINATE: return "GL_CURRENT_FOG_COORDINATE";
        // case GL_FOG_COORDINATE_ARRAY_TYPE: return "GL_FOG_COORDINATE_ARRAY_TYPE";
        // case GL_FOG_COORDINATE_ARRAY_STRIDE: return "GL_FOG_COORDINATE_ARRAY_STRIDE";
        // case GL_FOG_COORDINATE_ARRAY_POINTER: return "GL_FOG_COORDINATE_ARRAY_POINTER";
        // case GL_FOG_COORDINATE_ARRAY: return "GL_FOG_COORDINATE_ARRAY";

        case GL_COLOR_SUM: return "GL_COLOR_SUM";
        case GL_CURRENT_SECONDARY_COLOR: return "GL_CURRENT_SECONDARY_COLOR";
        case GL_SECONDARY_COLOR_ARRAY_SIZE: return "GL_SECONDARY_COLOR_ARRAY_SIZE";
        case GL_SECONDARY_COLOR_ARRAY_TYPE: return "GL_SECONDARY_COLOR_ARRAY_TYPE";
        case GL_SECONDARY_COLOR_ARRAY_STRIDE: return "GL_SECONDARY_COLOR_ARRAY_STRIDE";
        case GL_SECONDARY_COLOR_ARRAY_POINTER: return "GL_SECONDARY_COLOR_ARRAY_POINTER";
        case GL_SECONDARY_COLOR_ARRAY: return "GL_SECONDARY_COLOR_ARRAY";

        case GL_POINT_SIZE_MIN: return "GL_POINT_SIZE_MIN";
        case GL_POINT_SIZE_MAX: return "GL_POINT_SIZE_MAX";
        case GL_POINT_FADE_THRESHOLD_SIZE: return "GL_POINT_FADE_THRESHOLD_SIZE";
        case GL_POINT_DISTANCE_ATTENUATION: return "GL_POINT_DISTANCE_ATTENUATION";

        case GL_BLEND_DST_RGB: return "GL_BLEND_DST_RGB";
        case GL_BLEND_SRC_RGB: return "GL_BLEND_SRC_RGB";
        case GL_BLEND_DST_ALPHA: return "GL_BLEND_DST_ALPHA";
        case GL_BLEND_SRC_ALPHA: return "GL_BLEND_SRC_ALPHA";

        case GL_GENERATE_MIPMAP: return "GL_GENERATE_MIPMAP";
        case GL_GENERATE_MIPMAP_HINT: return "GL_GENERATE_MIPMAP_HINT";

        case GL_INCR_WRAP: return "GL_INCR_WRAP";
        case GL_DECR_WRAP: return "GL_DECR_WRAP";

        case GL_MIRRORED_REPEAT: return "GL_MIRRORED_REPEAT";

        case GL_MAX_TEXTURE_LOD_BIAS: return "GL_MAX_TEXTURE_LOD_BIAS";
        case GL_TEXTURE_FILTER_CONTROL: return "GL_TEXTURE_FILTER_CONTROL";
        case GL_TEXTURE_LOD_BIAS: return "GL_TEXTURE_LOD_BIAS";

        case GL_ARRAY_BUFFER: return "GL_ARRAY_BUFFER";
        case GL_ELEMENT_ARRAY_BUFFER: return "GL_ELEMENT_ARRAY_BUFFER";
        case GL_ARRAY_BUFFER_BINDING: return "GL_ARRAY_BUFFER_BINDING";
        case GL_ELEMENT_ARRAY_BUFFER_BINDING: return "GL_ELEMENT_ARRAY_BUFFER_BINDING";
        case GL_VERTEX_ARRAY_BUFFER_BINDING: return "GL_VERTEX_ARRAY_BUFFER_BINDING";
        case GL_NORMAL_ARRAY_BUFFER_BINDING: return "GL_NORMAL_ARRAY_BUFFER_BINDING";
        case GL_COLOR_ARRAY_BUFFER_BINDING: return "GL_COLOR_ARRAY_BUFFER_BINDING";
        case GL_INDEX_ARRAY_BUFFER_BINDING: return "GL_INDEX_ARRAY_BUFFER_BINDING";
        case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING: return "GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING";
        case GL_EDGE_FLAG_ARRAY_BUFFER_BINDING: return "GL_EDGE_FLAG_ARRAY_BUFFER_BINDING";
        case GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING: return "GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING";
        case GL_FOG_COORD_ARRAY_BUFFER_BINDING: return "GL_FOG_COORD_ARRAY_BUFFER_BINDING";
        case GL_WEIGHT_ARRAY_BUFFER_BINDING: return "GL_WEIGHT_ARRAY_BUFFER_BINDING";
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: return "GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING";
        case GL_STREAM_DRAW: return "GL_STREAM_DRAW";
        case GL_STREAM_READ: return "GL_STREAM_READ";
        case GL_STREAM_COPY: return "GL_STREAM_COPY";
        case GL_STATIC_DRAW: return "GL_STATIC_DRAW";
        case GL_STATIC_READ: return "GL_STATIC_READ";
        case GL_STATIC_COPY: return "GL_STATIC_COPY";
        case GL_DYNAMIC_DRAW: return "GL_DYNAMIC_DRAW";
        case GL_DYNAMIC_READ: return "GL_DYNAMIC_READ";
        case GL_DYNAMIC_COPY: return "GL_DYNAMIC_COPY";
        case GL_READ_ONLY: return "GL_READ_ONLY";
        case GL_WRITE_ONLY: return "GL_WRITE_ONLY";
        case GL_READ_WRITE: return "GL_READ_WRITE";
        case GL_BUFFER_SIZE: return "GL_BUFFER_SIZE";
        case GL_BUFFER_USAGE: return "GL_BUFFER_USAGE";
        case GL_BUFFER_ACCESS: return "GL_BUFFER_ACCESS";
        case GL_BUFFER_MAPPED: return "GL_BUFFER_MAPPED";
        case GL_BUFFER_MAP_POINTER: return "GL_BUFFER_MAP_POINTER";
        // case GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING: return "GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING";

        case GL_CURRENT_PROGRAM: return "GL_CURRENT_PROGRAM";
        case GL_SHADER_TYPE: return "GL_SHADER_TYPE";
        case GL_DELETE_STATUS: return "GL_DELETE_STATUS";
        case GL_COMPILE_STATUS: return "GL_COMPILE_STATUS";
        case GL_LINK_STATUS: return "GL_LINK_STATUS";
        case GL_VALIDATE_STATUS: return "GL_VALIDATE_STATUS";
        case GL_INFO_LOG_LENGTH: return "GL_INFO_LOG_LENGTH";
        case GL_ATTACHED_SHADERS: return "GL_ATTACHED_SHADERS";
        case GL_ACTIVE_UNIFORMS: return "GL_ACTIVE_UNIFORMS";
        case GL_ACTIVE_UNIFORM_MAX_LENGTH: return "GL_ACTIVE_UNIFORM_MAX_LENGTH";
        case GL_SHADER_SOURCE_LENGTH: return "GL_SHADER_SOURCE_LENGTH";
        case GL_FLOAT_VEC2: return "GL_FLOAT_VEC2";
        case GL_FLOAT_VEC3: return "GL_FLOAT_VEC3";
        case GL_FLOAT_VEC4: return "GL_FLOAT_VEC4";
        case GL_INT_VEC2: return "GL_INT_VEC2";
        case GL_INT_VEC3: return "GL_INT_VEC3";
        case GL_INT_VEC4: return "GL_INT_VEC4";
        case GL_BOOL: return "GL_BOOL";
        case GL_BOOL_VEC2: return "GL_BOOL_VEC2";
        case GL_BOOL_VEC3: return "GL_BOOL_VEC3";
        case GL_BOOL_VEC4: return "GL_BOOL_VEC4";
        case GL_FLOAT_MAT2: return "GL_FLOAT_MAT2";
        case GL_FLOAT_MAT3: return "GL_FLOAT_MAT3";
        case GL_FLOAT_MAT4: return "GL_FLOAT_MAT4";
        case GL_SAMPLER_1D: return "GL_SAMPLER_1D";
        case GL_SAMPLER_2D: return "GL_SAMPLER_2D";
        case GL_SAMPLER_3D: return "GL_SAMPLER_3D";
        case GL_SAMPLER_CUBE: return "GL_SAMPLER_CUBE";
        case GL_SAMPLER_1D_SHADOW: return "GL_SAMPLER_1D_SHADOW";
        case GL_SAMPLER_2D_SHADOW: return "GL_SAMPLER_2D_SHADOW";
        case GL_SHADING_LANGUAGE_VERSION: return "GL_SHADING_LANGUAGE_VERSION";
        case GL_VERTEX_SHADER: return "GL_VERTEX_SHADER";
        case GL_MAX_VERTEX_UNIFORM_COMPONENTS: return "GL_MAX_VERTEX_UNIFORM_COMPONENTS";
        case GL_MAX_VARYING_FLOATS: return "GL_MAX_VARYING_FLOATS";
        case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS: return "GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS";
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: return "GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS";
        case GL_ACTIVE_ATTRIBUTES: return "GL_ACTIVE_ATTRIBUTES";
        case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH: return "GL_ACTIVE_ATTRIBUTE_MAX_LENGTH";
        case GL_FRAGMENT_SHADER: return "GL_FRAGMENT_SHADER";
        case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS: return "GL_MAX_FRAGMENT_UNIFORM_COMPONENTS";
        case GL_FRAGMENT_SHADER_DERIVATIVE_HINT: return "GL_FRAGMENT_SHADER_DERIVATIVE_HINT";
        case GL_MAX_VERTEX_ATTRIBS: return "GL_MAX_VERTEX_ATTRIBS";
        case GL_VERTEX_ATTRIB_ARRAY_ENABLED: return "GL_VERTEX_ATTRIB_ARRAY_ENABLED";
        case GL_VERTEX_ATTRIB_ARRAY_SIZE: return "GL_VERTEX_ATTRIB_ARRAY_SIZE";
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE: return "GL_VERTEX_ATTRIB_ARRAY_STRIDE";
        case GL_VERTEX_ATTRIB_ARRAY_TYPE: return "GL_VERTEX_ATTRIB_ARRAY_TYPE";
        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED: return "GL_VERTEX_ATTRIB_ARRAY_NORMALIZED";
        case GL_CURRENT_VERTEX_ATTRIB: return "GL_CURRENT_VERTEX_ATTRIB";
        case GL_VERTEX_ATTRIB_ARRAY_POINTER: return "GL_VERTEX_ATTRIB_ARRAY_POINTER";
        case GL_VERTEX_PROGRAM_POINT_SIZE: return "GL_VERTEX_PROGRAM_POINT_SIZE";
        case GL_VERTEX_PROGRAM_TWO_SIDE: return "GL_VERTEX_PROGRAM_TWO_SIDE";
        case GL_MAX_TEXTURE_COORDS: return "GL_MAX_TEXTURE_COORDS";
        case GL_MAX_TEXTURE_IMAGE_UNITS: return "GL_MAX_TEXTURE_IMAGE_UNITS";
        case GL_MAX_DRAW_BUFFERS: return "GL_MAX_DRAW_BUFFERS";
        case GL_DRAW_BUFFER0: return "GL_DRAW_BUFFER0";
        case GL_DRAW_BUFFER1: return "GL_DRAW_BUFFER1";
        case GL_DRAW_BUFFER2: return "GL_DRAW_BUFFER2";
        case GL_DRAW_BUFFER3: return "GL_DRAW_BUFFER3";
        case GL_DRAW_BUFFER4: return "GL_DRAW_BUFFER4";
        case GL_DRAW_BUFFER5: return "GL_DRAW_BUFFER5";
        case GL_DRAW_BUFFER6: return "GL_DRAW_BUFFER6";
        case GL_DRAW_BUFFER7: return "GL_DRAW_BUFFER7";
        case GL_DRAW_BUFFER8: return "GL_DRAW_BUFFER8";
        case GL_DRAW_BUFFER9: return "GL_DRAW_BUFFER9";
        case GL_DRAW_BUFFER10: return "GL_DRAW_BUFFER10";
        case GL_DRAW_BUFFER11: return "GL_DRAW_BUFFER11";
        case GL_DRAW_BUFFER12: return "GL_DRAW_BUFFER12";
        case GL_DRAW_BUFFER13: return "GL_DRAW_BUFFER13";
        case GL_DRAW_BUFFER14: return "GL_DRAW_BUFFER14";
        case GL_DRAW_BUFFER15: return "GL_DRAW_BUFFER15";
        case GL_POINT_SPRITE: return "GL_POINT_SPRITE";
        case GL_COORD_REPLACE: return "GL_COORD_REPLACE";
        case GL_POINT_SPRITE_COORD_ORIGIN: return "GL_POINT_SPRITE_COORD_ORIGIN";
        case GL_LOWER_LEFT: return "GL_LOWER_LEFT";
        case GL_UPPER_LEFT: return "GL_UPPER_LEFT";
        case GL_STENCIL_BACK_FUNC: return "GL_STENCIL_BACK_FUNC";
        case GL_STENCIL_BACK_VALUE_MASK: return "GL_STENCIL_BACK_VALUE_MASK";
        case GL_STENCIL_BACK_REF: return "GL_STENCIL_BACK_REF";
        case GL_STENCIL_BACK_FAIL: return "GL_STENCIL_BACK_FAIL";
        case GL_STENCIL_BACK_PASS_DEPTH_FAIL: return "GL_STENCIL_BACK_PASS_DEPTH_FAIL";
        case GL_STENCIL_BACK_PASS_DEPTH_PASS: return "GL_STENCIL_BACK_PASS_DEPTH_PASS";
        case GL_STENCIL_BACK_WRITEMASK: return "GL_STENCIL_BACK_WRITEMASK";

        case GL_CURRENT_RASTER_SECONDARY_COLOR: return "GL_CURRENT_RASTER_SECONDARY_COLOR";
        case GL_PIXEL_PACK_BUFFER: return "GL_PIXEL_PACK_BUFFER";
        case GL_PIXEL_UNPACK_BUFFER: return "GL_PIXEL_UNPACK_BUFFER";
        case GL_PIXEL_PACK_BUFFER_BINDING: return "GL_PIXEL_PACK_BUFFER_BINDING";
        case GL_PIXEL_UNPACK_BUFFER_BINDING: return "GL_PIXEL_UNPACK_BUFFER_BINDING";
        case GL_FLOAT_MAT2x3: return "GL_FLOAT_MAT2x3";
        case GL_FLOAT_MAT2x4: return "GL_FLOAT_MAT2x4";
        case GL_FLOAT_MAT3x2: return "GL_FLOAT_MAT3x2";
        case GL_FLOAT_MAT3x4: return "GL_FLOAT_MAT3x4";
        case GL_FLOAT_MAT4x2: return "GL_FLOAT_MAT4x2";
        case GL_FLOAT_MAT4x3: return "GL_FLOAT_MAT4x3";
        case GL_SRGB: return "GL_SRGB";
        case GL_SRGB8: return "GL_SRGB8";
        case GL_SRGB_ALPHA: return "GL_SRGB_ALPHA";
        case GL_SRGB8_ALPHA8: return "GL_SRGB8_ALPHA8";
        case GL_SLUMINANCE_ALPHA: return "GL_SLUMINANCE_ALPHA";
        case GL_SLUMINANCE8_ALPHA8: return "GL_SLUMINANCE8_ALPHA8";
        case GL_SLUMINANCE: return "GL_SLUMINANCE";
        case GL_SLUMINANCE8: return "GL_SLUMINANCE8";
        case GL_COMPRESSED_SRGB: return "GL_COMPRESSED_SRGB";
        case GL_COMPRESSED_SRGB_ALPHA: return "GL_COMPRESSED_SRGB_ALPHA";
        case GL_COMPRESSED_SLUMINANCE: return "GL_COMPRESSED_SLUMINANCE";
        case GL_COMPRESSED_SLUMINANCE_ALPHA: return "GL_COMPRESSED_SLUMINANCE_ALPHA";
        default: return "<<unknown>>";
        }

#else
        return "<<unknown>>";
#endif
    }

}}


