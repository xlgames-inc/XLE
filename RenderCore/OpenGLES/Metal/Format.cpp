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
        case Format::A8_UNORM: return {GL_ALPHA, GL_UNSIGNED_BYTE, GL_R8, FeatureSet::GLES200, FeatureSet::GLES300};

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

}}


