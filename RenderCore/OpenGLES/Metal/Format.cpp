// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Format.h"
#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    FormatCompressionType::Enum       GetCompressionType(NativeFormat::Enum format)
    {
        switch (format) {
        #undef _EXP
        #define _EXP(X, Y, Z, U)    case NativeFormat::X##_##Y: return FormatCompressionType::Z;
            #include "../../Metal/Detail/DXGICompatibleFormats.h"
        #undef _EXP
        default:
            return FormatCompressionType::None;
        }
    }

    namespace FormatPrefix
    {
        enum Enum 
        { 
            R32G32B32A32, R32G32B32, R16G16B16A16, R32G32, 
            R10G10B10A2, R11G11B10,
            R8G8B8A8, R16G16, R32, D32,
            R8G8, R16, D16, 
            R8, A8, A1, R1,
            R9G9B9E5, R8G8_B8G8, G8R8_G8B8,
            BC1, BC2, BC3, BC4, BC5, BC6H, BC7,
            B5G6R5, B5G5R5A1, B8G8R8A8, B8G8R8X8
        };
    }

    static FormatPrefix::Enum   GetPrefix(NativeFormat::Enum format)
    {
        switch (format) {
        #undef _EXP
        #define _EXP(X, Y, Z, U)    case NativeFormat::X##_##Y: return FormatPrefix::X;
            #include "../../Metal/Detail/DXGICompatibleFormats.h"
        #undef _EXP
        default: return FormatPrefix::R32G32B32A32;
        }
    }

    FormatComponents::Enum            GetComponents(NativeFormat::Enum format)
    {
        FormatPrefix::Enum prefix = GetPrefix(format);
        using namespace FormatPrefix;
        switch (prefix) {
        case A8:
        case A1:                return FormatComponents::Alpha;

        case D32:
        case D16:               return FormatComponents::Depth; 

        case R32:
        case R16: 
        case R8:
        case R1:                return FormatComponents::Luminance;

        case B5G5R5A1:
        case B8G8R8A8:
        case R8G8B8A8:
        case R10G10B10A2:
        case R16G16B16A16:
        case R32G32B32A32:      return FormatComponents::RGBAlpha;
        case B5G6R5:
        case B8G8R8X8:
        case R11G11B10:
        case R32G32B32:         return FormatComponents::RGB;

        case R9G9B9E5:          return FormatComponents::RGBE;
            
        case R32G32:
        case R16G16:
        case R8G8:              return FormatComponents::RG;
            
        
        case BC1:
        case BC6H:              return FormatComponents::RGB;
        case BC2:
        case BC3:
        case BC4: 
        case BC5:               
        case BC7:               return FormatComponents::RGBAlpha;

        case R8G8_B8G8: 
        case G8R8_G8B8:         return FormatComponents::RGB;

        default:                return FormatComponents::Unknown;
        }
    }

    FormatComponentType::Enum         GetComponentType(NativeFormat::Enum format)
    {
        enum InputComponentType
        {
            TYPELESS, FLOAT, UINT, SINT, UNORM, SNORM, UNORM_SRGB, SHAREDEXP, UF16, SF16
        };
        InputComponentType input;
        switch (format) {
            #undef _EXP
            #define _EXP(X, Y, Z, U)    case NativeFormat::X##_##Y: input = Y; break;
                #include "../../Metal/Detail/DXGICompatibleFormats.h"
            #undef _EXP
            case NativeFormat::Matrix4x4: input = FLOAT; break;
            case NativeFormat::Matrix3x4: input = FLOAT; break;
            default: input = TYPELESS;
        }
        switch (input) {
        default:
        case TYPELESS:      return FormatComponentType::Typeless;
        case FLOAT:         return FormatComponentType::Float;
        case UINT:          return FormatComponentType::UInt;
        case SINT:          return FormatComponentType::SInt;
        case UNORM:         return FormatComponentType::UNorm;
        case SNORM:         return FormatComponentType::SNorm;
        case UNORM_SRGB:    return FormatComponentType::UNorm_SRGB;
        case SHAREDEXP:     return FormatComponentType::Exponential;
        case UF16:          return FormatComponentType::UnsignedFloat16;
        case SF16:          return FormatComponentType::SignedFloat16;
        }
    }

    unsigned                    BitsPerPixel(NativeFormat::Enum format)
    {
        switch (format) {
        #undef _EXP
        #define _EXP(X, Y, Z, U)    case NativeFormat::X##_##Y: return U;
            #include "../../Metal/Detail/DXGICompatibleFormats.h"
        #undef _EXP
        case NativeFormat::Matrix4x4: return 16 * sizeof(float) * 8;
        case NativeFormat::Matrix3x4: return 12 * sizeof(float) * 8;
        default: return 0;
        }
    }

    unsigned                    AsGLComponents(FormatComponents::Enum components)
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

    unsigned                    AsGLCompressionType(FormatCompressionType::Enum compressionType)
    {
        return 0;   // (requires querying device for capabilities)
    }

    unsigned                    AsGLComponentWidths(NativeFormat::Enum format)
    {
        FormatPrefix::Enum prefix = GetPrefix(format);
        switch (prefix) {
        case FormatPrefix::R32G32B32A32:  
        case FormatPrefix::R32G32B32:       return GL_UNSIGNED_BYTE;
        default:                            return GL_UNSIGNED_BYTE;
        case FormatPrefix::B5G6R5:          return GL_UNSIGNED_SHORT_5_6_5;
        case FormatPrefix::B5G5R5A1:        return GL_UNSIGNED_SHORT_5_5_5_1;
        }
    }

    unsigned                    AsGLVertexComponentType(NativeFormat::Enum format)
    {
        const FormatComponentType::Enum type = GetComponentType(format);
        FormatPrefix::Enum prefix = GetPrefix(format);
        using namespace FormatPrefix;
        switch (prefix) {
        case A8:
        case R8:
        case B8G8R8A8:
        case R8G8B8A8:
        case B8G8R8X8:
        case R8G8:
        case R8G8_B8G8: 
        case G8R8_G8B8:         return (type==FormatComponentType::SInt || type==FormatComponentType::SNorm) ? GL_BYTE : GL_UNSIGNED_BYTE;

        case R16: 
        case D16:
        case R16G16:
        case R16G16B16A16:      return (type==FormatComponentType::SInt || type==FormatComponentType::SNorm) ? GL_SHORT : GL_UNSIGNED_SHORT;

        case D32:
        case R32:
        case R32G32:
        case R32G32B32:
        case R32G32B32A32:      return (type==FormatComponentType::Float) ? GL_FLOAT : GL_FIXED;

        case A1:
        case R1:
        case B5G5R5A1:
        case R10G10B10A2:
        case B5G6R5:
        case R11G11B10:
        case R9G9B9E5:
        case BC1:
        case BC2:
        case BC3:
        case BC4: 
        case BC5:
        default:                return 0;       // (invalid for vertex buffers)
        }
    }
}}


