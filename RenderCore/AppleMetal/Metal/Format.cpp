// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Format.h"

#include "IncludeAppleMetal.h"

namespace RenderCore { namespace Metal_AppleMetal
{
    MTLPixelFormat AsMTLPixelFormat(RenderCore::Format fmt)
    {
        using namespace RenderCore;
        switch (fmt)
        {
            case Format::R32G32B32A32_FLOAT: return MTLPixelFormatRGBA32Float;
            case Format::R32G32B32A32_UINT: return MTLPixelFormatRGBA32Uint;
            case Format::R32G32B32A32_SINT: return MTLPixelFormatRGBA32Sint;

            case Format::R16G16B16A16_FLOAT: return MTLPixelFormatRGBA16Float;
            case Format::R16G16B16A16_UNORM: return MTLPixelFormatRGBA16Unorm;
            case Format::R16G16B16A16_UINT: return MTLPixelFormatRGBA16Uint;
            case Format::R16G16B16A16_SNORM: return MTLPixelFormatRGBA16Snorm;
            case Format::R16G16B16A16_SINT: return MTLPixelFormatRGBA16Sint;

            case Format::R32G32_FLOAT: return MTLPixelFormatRG32Float;
            case Format::R32G32_UINT: return MTLPixelFormatRG32Uint;
            case Format::R32G32_SINT: return MTLPixelFormatRG32Sint;

            case Format::R10G10B10A2_UNORM: return MTLPixelFormatRGB10A2Unorm;
            case Format::R10G10B10A2_UINT: return MTLPixelFormatRGB10A2Uint;
            case Format::R11G11B10_FLOAT: return MTLPixelFormatRG11B10Float;

            case Format::R8G8B8A8_UNORM: return MTLPixelFormatRGBA8Unorm;
            case Format::R8G8B8A8_UINT: return MTLPixelFormatRGBA8Uint;
            case Format::R8G8B8A8_SNORM: return MTLPixelFormatRGBA8Snorm;
            case Format::R8G8B8A8_SINT: return MTLPixelFormatRGBA8Sint;

            case Format::R16G16_FLOAT: return MTLPixelFormatRG16Float;
            case Format::R16G16_UNORM: return MTLPixelFormatRG16Unorm;
            case Format::R16G16_UINT: return MTLPixelFormatRG16Uint;
            case Format::R16G16_SNORM: return MTLPixelFormatRG16Snorm;
            case Format::R16G16_SINT: return MTLPixelFormatRG16Sint;

            case Format::D32_FLOAT: return MTLPixelFormatDepth32Float;
            case Format::R32_FLOAT: return MTLPixelFormatR32Float;
            case Format::R32_UINT: return MTLPixelFormatR32Uint;
            case Format::R32_SINT: return MTLPixelFormatR32Sint;

            case Format::R8G8_UNORM: return MTLPixelFormatRG8Unorm;
            case Format::R8G8_UINT: return MTLPixelFormatRG8Uint;
            case Format::R8G8_SNORM: return MTLPixelFormatRG8Snorm;
            case Format::R8G8_SINT: return MTLPixelFormatRG8Sint;

            case Format::R16_FLOAT: return MTLPixelFormatR16Float;
            case Format::R16_UNORM: return MTLPixelFormatR16Unorm;
            case Format::R16_UINT: return MTLPixelFormatR16Uint;
            case Format::R16_SNORM: return MTLPixelFormatR16Snorm;
            case Format::R16_SINT: return MTLPixelFormatR16Sint;

            case Format::R8_UNORM: return MTLPixelFormatR8Unorm;
            case Format::R8_UINT: return MTLPixelFormatR8Uint;
            case Format::R8_SNORM: return MTLPixelFormatR8Snorm;
            case Format::R8_SINT: return MTLPixelFormatR8Sint;
            case Format::A8_UNORM: return MTLPixelFormatA8Unorm;

            case Format::R9G9B9E5_SHAREDEXP: return MTLPixelFormatRGB9E5Float;
            case Format::D32_SFLOAT_S8_UINT: return MTLPixelFormatDepth32Float_Stencil8;

            case Format::R8G8B8A8_UNORM_SRGB: return MTLPixelFormatRGBA8Unorm_sRGB;
            case Format::B8G8R8A8_UNORM: return MTLPixelFormatBGRA8Unorm;
            case Format::B8G8R8A8_UNORM_SRGB: return MTLPixelFormatBGRA8Unorm_sRGB;

            //////////// missing formats ////////////
            case Format::R32G32B32_FLOAT:
            case Format::R32G32B32_UINT:
            case Format::R32G32B32_SINT:

            case Format::R8G8B8_UNORM:
            case Format::R8G8B8_UINT:
            case Format::R8G8B8_SNORM:
            case Format::R8G8B8_SINT:
            case Format::R8G8B8_UNORM_SRGB:

            case Format::RGB_ETC1_UNORM:
            case Format::RGB_ETC1_UNORM_SRGB:
            case Format::RGBA_ETC2_UNORM:
            case Format::RGBA_ETC2_UNORM_SRGB:

            default: break;
        }

#if TARGET_OS_OSX
        const bool OSXOnlyFormats = true;
        if (OSXOnlyFormats) {
            switch (fmt)
            {
            case Format::D16_UNORM: return MTLPixelFormatDepth16Unorm;
            case Format::D24_UNORM_S8_UINT: return MTLPixelFormatDepth24Unorm_Stencil8;

            case Format::BC1_UNORM: return MTLPixelFormatBC1_RGBA;
            case Format::BC1_UNORM_SRGB: return MTLPixelFormatBC1_RGBA_sRGB;
            case Format::BC2_UNORM: return MTLPixelFormatBC2_RGBA;
            case Format::BC2_UNORM_SRGB: return MTLPixelFormatBC2_RGBA_sRGB;
            case Format::BC3_UNORM: return MTLPixelFormatBC3_RGBA;
            case Format::BC3_UNORM_SRGB: return MTLPixelFormatBC3_RGBA_sRGB;

            case Format::BC4_UNORM: return MTLPixelFormatBC4_RUnorm;
            case Format::BC4_SNORM: return MTLPixelFormatBC4_RSnorm;
            case Format::BC5_UNORM: return MTLPixelFormatBC5_RGUnorm;
            case Format::BC5_SNORM: return MTLPixelFormatBC5_RGSnorm;

            case Format::BC6H_UF16: return MTLPixelFormatBC6H_RGBUfloat;
            case Format::BC6H_SF16: return MTLPixelFormatBC6H_RGBFloat;
            case Format::BC7_UNORM: return MTLPixelFormatBC7_RGBAUnorm;
            case Format::BC7_UNORM_SRGB: return MTLPixelFormatBC7_RGBAUnorm_sRGB;
            default: break;
            }
        }
#endif

#if TARGET_OS_IPHONE
        const bool IOSOnlyFormats = true;
        if (IOSOnlyFormats) {
            switch (fmt)
            {
            case Format::B5G6R5_UNORM: return MTLPixelFormatB5G6R5Unorm;
            case Format::B5G5R5A1_UNORM: return MTLPixelFormatA1BGR5Unorm;
            case Format::R4G4B4A4_UNORM: return MTLPixelFormatABGR4Unorm;

            case Format::RGB_PVRTC1_2BPP_UNORM: return MTLPixelFormatPVRTC_RGB_2BPP;
            case Format::RGB_PVRTC1_4BPP_UNORM: return MTLPixelFormatPVRTC_RGB_4BPP;
            case Format::RGBA_PVRTC1_2BPP_UNORM: /* DavidJ -- assuming compatibility */
            case Format::RGBA_PVRTC2_2BPP_UNORM: return MTLPixelFormatPVRTC_RGBA_2BPP;
            case Format::RGBA_PVRTC1_4BPP_UNORM: /* DavidJ -- assuming compatibility */
            case Format::RGBA_PVRTC2_4BPP_UNORM: return MTLPixelFormatPVRTC_RGBA_4BPP;

            case Format::RGB_PVRTC1_2BPP_UNORM_SRGB: return MTLPixelFormatPVRTC_RGB_2BPP_sRGB;
            case Format::RGB_PVRTC1_4BPP_UNORM_SRGB: return MTLPixelFormatPVRTC_RGB_4BPP_sRGB;
            case Format::RGBA_PVRTC1_2BPP_UNORM_SRGB: /* DavidJ -- assuming compatibility */
            case Format::RGBA_PVRTC2_2BPP_UNORM_SRGB: return MTLPixelFormatPVRTC_RGBA_2BPP_sRGB;
            case Format::RGBA_PVRTC1_4BPP_UNORM_SRGB: /* DavidJ -- assuming compatibility */
            case Format::RGBA_PVRTC2_4BPP_UNORM_SRGB: return MTLPixelFormatPVRTC_RGBA_4BPP_sRGB;

            case Format::RGB_ETC2_UNORM: return MTLPixelFormatETC2_RGB8;
            case Format::RGBA1_ETC2_UNORM: return MTLPixelFormatETC2_RGB8A1_sRGB;
            case Format::RGB_ETC2_UNORM_SRGB: return MTLPixelFormatETC2_RGB8_sRGB;
            case Format::RGBA1_ETC2_UNORM_SRGB: return MTLPixelFormatETC2_RGB8A1;

            default: break;
            }
        }
#endif

        // KenD -- Metal doesn't have a 24-bit format, which is quite unfortunate
        /*
#if DEBUG
        static NSMutableSet* missingFormats = [[NSMutableSet set] retain];
        NSUInteger c = missingFormats.count;
        [missingFormats addObject:[NSString stringWithCString:AsString(fmt) encoding:NSUTF8StringEncoding]];
        if (missingFormats.count > c) {
            NSLog(@"================> Missing RenderCore::Formats %@", missingFormats);
        }
#else
        assert(0); // aggressive failure for now
#endif
        */

        return MTLPixelFormatInvalid;
    }

    RenderCore::Format AsRenderCoreFormat(MTLPixelFormat fmt)
    {
        using namespace RenderCore;
        switch (fmt)
        {
            case MTLPixelFormatRGBA32Float: return Format::R32G32B32A32_FLOAT;
            case MTLPixelFormatRGBA32Uint: return Format::R32G32B32A32_UINT;
            case MTLPixelFormatRGBA32Sint: return Format::R32G32B32A32_SINT;

            case MTLPixelFormatRGBA16Float: return Format::R16G16B16A16_FLOAT;
            case MTLPixelFormatRGBA16Unorm: return Format::R16G16B16A16_UNORM;
            case MTLPixelFormatRGBA16Uint: return Format::R16G16B16A16_UINT;
            case MTLPixelFormatRGBA16Snorm: return Format::R16G16B16A16_SNORM;
            case MTLPixelFormatRGBA16Sint: return Format::R16G16B16A16_SINT;

            case MTLPixelFormatRG32Float: return Format::R32G32_FLOAT;
            case MTLPixelFormatRG32Uint: return Format::R32G32_UINT;
            case MTLPixelFormatRG32Sint: return Format::R32G32_SINT;

            case MTLPixelFormatRGB10A2Unorm: return Format::R10G10B10A2_UNORM;
            case MTLPixelFormatRGB10A2Uint: return Format::R10G10B10A2_UINT;
            case MTLPixelFormatRG11B10Float: return Format::R11G11B10_FLOAT;

            case MTLPixelFormatRGBA8Unorm: return Format::R8G8B8A8_UNORM;
            case MTLPixelFormatRGBA8Uint: return Format::R8G8B8A8_UINT;
            case MTLPixelFormatRGBA8Snorm: return Format::R8G8B8A8_SNORM;
            case MTLPixelFormatRGBA8Sint: return Format::R8G8B8A8_SINT;

            case MTLPixelFormatRG16Float: return Format::R16G16_FLOAT;
            case MTLPixelFormatRG16Unorm: return Format::R16G16_UNORM;
            case MTLPixelFormatRG16Uint: return Format::R16G16_UINT;
            case MTLPixelFormatRG16Snorm: return Format::R16G16_SNORM;
            case MTLPixelFormatRG16Sint: return Format::R16G16_SINT;

            case MTLPixelFormatDepth32Float: return Format::D32_FLOAT;
            case MTLPixelFormatR32Float: return Format::R32_FLOAT;
            case MTLPixelFormatR32Uint: return Format::R32_UINT;
            case MTLPixelFormatR32Sint: return Format::R32_SINT;

            case MTLPixelFormatRG8Unorm: return Format::R8G8_UNORM;
            case MTLPixelFormatRG8Uint: return Format::R8G8_UINT;
            case MTLPixelFormatRG8Snorm: return Format::R8G8_SNORM;
            case MTLPixelFormatRG8Sint: return Format::R8G8_SINT;

            case MTLPixelFormatR16Float: return Format::R16_FLOAT;
            case MTLPixelFormatR16Unorm: return Format::R16_UNORM;
            case MTLPixelFormatR16Uint: return Format::R16_UINT;
            case MTLPixelFormatR16Snorm: return Format::R16_SNORM;
            case MTLPixelFormatR16Sint: return Format::R16_SINT;

            case MTLPixelFormatR8Unorm: return Format::R8_UNORM;
            case MTLPixelFormatR8Uint: return Format::R8_UINT;
            case MTLPixelFormatR8Snorm: return Format::R8_SNORM;
            case MTLPixelFormatR8Sint: return Format::R8_SINT;
            case MTLPixelFormatA8Unorm: return Format::A8_UNORM;

            case MTLPixelFormatRGB9E5Float: return Format::R9G9B9E5_SHAREDEXP;
            case MTLPixelFormatDepth32Float_Stencil8: return Format::D32_SFLOAT_S8_UINT;

            case MTLPixelFormatBGRA8Unorm: return Format::B8G8R8A8_UNORM;
            case MTLPixelFormatBGRA8Unorm_sRGB: return Format::B8G8R8A8_UNORM_SRGB;
            case MTLPixelFormatRGBA8Unorm_sRGB: return Format::R8G8B8A8_UNORM_SRGB;

#if 0
            case {GL_RGB, GL_UNSIGNED_BYTE, GL_RGB8}: return Format::R8G8B8_UNORM;
            case {GL_RGB_INTEGER, GL_UNSIGNED_BYTE, GL_RGB8UI}: return Format::R8G8B8_UINT;
            case {GL_RGB, GL_BYTE, GL_RGB8_SNORM}: return Format::R8G8B8_SNORM;
            case {GL_RGB_INTEGER, GL_BYTE, GL_RGB8I}: return Format::R8G8B8_SINT;

            case {GL_RGB, GL_FLOAT, GL_RGB32F}: return Format::R32G32B32_FLOAT;
            case {GL_RGB_INTEGER, GL_UNSIGNED_INT, GL_RGB32UI}: return Format::R32G32B32_UINT;
            case {GL_RGB_INTEGER, GL_INT, GL_RGB32I}: return Format::R32G32B32_SINT;

            case {GL_RGBA, GL_BYTE, GL_RGB8_SNORM}: return Format::R8G8B8_UNORM_SRGB;

            case {0, 0, GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG}: return Format::RGB_PVRTC1_2BPP_UNORM;
            case {0, 0, GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG}: return Format::RGBA_PVRTC1_2BPP_UNORM;
            case {0, 0, GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG}: return Format::RGB_PVRTC1_4BPP_UNORM;
            case {0, 0, GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG}: return Format::RGBA_PVRTC1_4BPP_UNORM;
            case {0, 0, GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG}: return Format::RGBA_PVRTC2_2BPP_UNORM;
            case {0, 0, GL_COMPRESSED_RGBA_PVRTC_4BPPV2_IMG}: return Format::RGBA_PVRTC2_4BPP_UNORM;
            case {0, 0, GL_ETC1_RGB8_OES}: return Format::RGB_ETC1_UNORM;

            case {0, 0, GL_COMPRESSED_RGB8_ETC2}: return Format::RGB_ETC2_UNORM;
            case {0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC}: return Format::RGBA_ETC2_UNORM;
            case {0, 0, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2}: return Format::RGBA1_ETC2_UNORM;

            case {0, 0, GL_COMPRESSED_SRGB8_ETC2}: return Format::RGB_ETC2_UNORM_SRGB;
            case {0, 0, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC}: return Format::RGBA_ETC2_UNORM_SRGB;
            case Format::RGBA1_ETC2_UNORM_SRGB: return {0, 0, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2};

            case Format::RGB_PVRTC1_2BPP_UNORM_SRGB:
            case Format::RGBA_PVRTC1_2BPP_UNORM_SRGB:
            case Format::RGB_PVRTC1_4BPP_UNORM_SRGB:
            case Format::RGBA_PVRTC1_4BPP_UNORM_SRGB:
            case Format::RGBA_PVRTC2_2BPP_UNORM_SRGB:
            case Format::RGBA_PVRTC2_4BPP_UNORM_SRGB:
            case Format::RGB_ETC1_UNORM_SRGB:
#endif

            default: break;
        }


#if TARGET_OS_OSX
        const bool OSXOnlyFormats = true;
        if (OSXOnlyFormats) {
            switch (fmt)
            {
            case MTLPixelFormatDepth16Unorm: return Format::D16_UNORM;
            case MTLPixelFormatDepth24Unorm_Stencil8: return Format::D24_UNORM_S8_UINT;

            case MTLPixelFormatBC1_RGBA: return Format::BC1_UNORM;
            case MTLPixelFormatBC1_RGBA_sRGB: return Format::BC1_UNORM_SRGB;
            case MTLPixelFormatBC2_RGBA: return Format::BC2_UNORM;
            case MTLPixelFormatBC2_RGBA_sRGB: return Format::BC2_UNORM_SRGB;
            case MTLPixelFormatBC3_RGBA: return Format::BC3_UNORM;
            case MTLPixelFormatBC3_RGBA_sRGB: return Format::BC3_UNORM_SRGB;

            case MTLPixelFormatBC4_RUnorm: return Format::BC4_UNORM;
            case MTLPixelFormatBC4_RSnorm: return Format::BC4_SNORM;
            case MTLPixelFormatBC5_RGUnorm: return Format::BC5_UNORM;
            case MTLPixelFormatBC5_RGSnorm: return Format::BC5_SNORM;

            case MTLPixelFormatBC6H_RGBUfloat: return Format::BC6H_UF16;
            case MTLPixelFormatBC6H_RGBFloat: return Format::BC6H_SF16;
            case MTLPixelFormatBC7_RGBAUnorm: return Format::BC7_UNORM;
            case MTLPixelFormatBC7_RGBAUnorm_sRGB: return Format::BC7_UNORM_SRGB;
            default: break;
            }
        }
#endif

#if TARGET_OS_IPHONE
        const bool IOSOnlyFormats = true;
        if (IOSOnlyFormats) {
            switch (fmt)
            {
            case MTLPixelFormatB5G6R5Unorm: return Format::B5G6R5_UNORM;
            case MTLPixelFormatA1BGR5Unorm: return Format::B5G5R5A1_UNORM;
            case MTLPixelFormatABGR4Unorm: return Format::R4G4B4A4_UNORM;

            case MTLPixelFormatPVRTC_RGB_2BPP: return Format::RGB_PVRTC1_2BPP_UNORM;
            case MTLPixelFormatPVRTC_RGB_4BPP: return Format::RGB_PVRTC1_4BPP_UNORM;
            case MTLPixelFormatPVRTC_RGBA_2BPP: return Format::RGBA_PVRTC2_2BPP_UNORM;
            case MTLPixelFormatPVRTC_RGBA_4BPP: return Format::RGBA_PVRTC2_4BPP_UNORM;

            case MTLPixelFormatPVRTC_RGB_2BPP_sRGB: return Format::RGB_PVRTC1_2BPP_UNORM_SRGB;
            case MTLPixelFormatPVRTC_RGB_4BPP_sRGB: return Format::RGB_PVRTC1_4BPP_UNORM_SRGB;
            case MTLPixelFormatPVRTC_RGBA_2BPP_sRGB: return Format::RGBA_PVRTC2_2BPP_UNORM_SRGB;
            case MTLPixelFormatPVRTC_RGBA_4BPP_sRGB: return Format::RGBA_PVRTC2_4BPP_UNORM_SRGB;

            case MTLPixelFormatETC2_RGB8: return Format::RGB_ETC2_UNORM;
            case MTLPixelFormatETC2_RGB8A1_sRGB: return Format::RGBA1_ETC2_UNORM;
            case MTLPixelFormatETC2_RGB8_sRGB: return Format::RGB_ETC2_UNORM_SRGB;
            case MTLPixelFormatETC2_RGB8A1: return Format::RGBA1_ETC2_UNORM_SRGB;
            default: break;
            }
        }
#endif

        /*
#if DEBUG
        static NSMutableSet* missingFormats = [[NSMutableSet set] retain];
        NSUInteger c = missingFormats.count;
        [missingFormats addObject:@(fmt)];
        if (missingFormats.count > c) {
            NSLog(@"================> Missing MTL formats %@", missingFormats);
        }
#else
        assert(0); // aggressive failure for now
#endif
        */

        return Format::Unknown;
    }

    MTLVertexFormat AsMTLVertexFormat(RenderCore::Format fmt)
    {
        switch (fmt) {
            case Format::R32_FLOAT: return MTLVertexFormatFloat;
            case Format::R32G32_FLOAT: return MTLVertexFormatFloat2;
            case Format::R32G32B32_FLOAT: return MTLVertexFormatFloat3;
            case Format::R32G32B32A32_FLOAT: return MTLVertexFormatFloat4;

            case Format::R32_SINT: return MTLVertexFormatInt;
            case Format::R32G32_SINT: return MTLVertexFormatInt2;
            case Format::R32G32B32_SINT: return MTLVertexFormatInt3;
            case Format::R32G32B32A32_SINT: return MTLVertexFormatInt4;

            case Format::R32_UINT: return MTLVertexFormatUInt;
            case Format::R32G32_UINT: return MTLVertexFormatUInt2;
            case Format::R32G32B32_UINT: return MTLVertexFormatUInt3;
            case Format::R32G32B32A32_UINT: return MTLVertexFormatUInt4;

            case Format::R8G8_UNORM: return MTLVertexFormatUChar2Normalized;
            case Format::R8G8B8_UNORM: return MTLVertexFormatUChar3Normalized;
            case Format::R8G8B8A8_UNORM: return MTLVertexFormatUChar4Normalized;

            case Format::R8G8_SNORM: return MTLVertexFormatChar2Normalized;
            case Format::R8G8B8_SNORM: return MTLVertexFormatChar3Normalized;
            case Format::R8G8B8A8_SNORM: return MTLVertexFormatChar4Normalized;

            case Format::R16G16_UNORM: return MTLVertexFormatUShort2Normalized;
            case Format::R16G16B16A16_UNORM: return MTLVertexFormatUShort4Normalized;

            case Format::R16G16_SNORM: return MTLVertexFormatShort2Normalized;
            case Format::R16G16B16A16_SNORM: return MTLVertexFormatShort4Normalized;

            case Format::R16G16_FLOAT: return MTLVertexFormatHalf2;
            case Format::R16G16B16A16_FLOAT: return MTLVertexFormatHalf4;

            case Format::R16G16_UINT: return MTLVertexFormatUShort2;
            case Format::R16G16B16A16_UINT: return MTLVertexFormatUShort4;

            case Format::R16G16_SINT: return MTLVertexFormatShort2;
            case Format::R16G16B16A16_SINT: return MTLVertexFormatShort4;

            case Format::R8G8_UINT: return MTLVertexFormatUChar2;
            case Format::R8G8B8_UINT: return MTLVertexFormatUChar3;
            case Format::R8G8B8A8_UINT: return MTLVertexFormatUChar4;

            case Format::R8G8_SINT: return MTLVertexFormatChar2;
            case Format::R8G8B8_SINT: return MTLVertexFormatChar3;
            case Format::R8G8B8A8_SINT: return MTLVertexFormatChar4;

            default: break;
        }

#if DEBUG
        static NSMutableSet* missingFormats = [[NSMutableSet set] retain];
        NSUInteger c = missingFormats.count;
        [missingFormats addObject:[NSString stringWithCString:AsString(fmt) encoding:NSUTF8StringEncoding]];
        if (missingFormats.count > c) {
            NSLog(@"================> Missing MTLVertexFormat for RenderCore::Formats %@", missingFormats);
        }
#else
        assert(0); // aggressive failure for now
#endif

        return MTLVertexFormatInvalid;
    }


    Utility::ImpliedTyping::TypeDesc AsTypeDesc(MTLDataType fmt)
    {
        switch(fmt)
        {
        case MTLDataTypeFloat: return { ImpliedTyping::TypeCat::Float };
        case MTLDataTypeFloat2: return { ImpliedTyping::TypeCat::Float, uint16(2), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeFloat3: return { ImpliedTyping::TypeCat::Float, uint16(3), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeFloat4: return { ImpliedTyping::TypeCat::Float, uint16(4), ImpliedTyping::TypeHint::Vector };

        case MTLDataTypeFloat2x2: return { ImpliedTyping::TypeCat::Float, uint16(4), ImpliedTyping::TypeHint::Matrix };
        case MTLDataTypeFloat2x3: return { ImpliedTyping::TypeCat::Float, uint16(6), ImpliedTyping::TypeHint::Matrix };
        case MTLDataTypeFloat2x4: return { ImpliedTyping::TypeCat::Float, uint16(8), ImpliedTyping::TypeHint::Matrix };

        case MTLDataTypeFloat3x2: return { ImpliedTyping::TypeCat::Float, uint16(6), ImpliedTyping::TypeHint::Matrix };
        case MTLDataTypeFloat3x3: return { ImpliedTyping::TypeCat::Float, uint16(9), ImpliedTyping::TypeHint::Matrix };
        case MTLDataTypeFloat3x4: return { ImpliedTyping::TypeCat::Float, uint16(12), ImpliedTyping::TypeHint::Matrix };

        case MTLDataTypeFloat4x2: return { ImpliedTyping::TypeCat::Float, uint16(8), ImpliedTyping::TypeHint::Matrix };
        case MTLDataTypeFloat4x3: return { ImpliedTyping::TypeCat::Float, uint16(12), ImpliedTyping::TypeHint::Matrix };
        case MTLDataTypeFloat4x4: return { ImpliedTyping::TypeCat::Float, uint16(16), ImpliedTyping::TypeHint::Matrix };

        case MTLDataTypeInt: return { ImpliedTyping::TypeCat::Int32 };
        case MTLDataTypeInt2: return { ImpliedTyping::TypeCat::Int32, uint16(2), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeInt3: return { ImpliedTyping::TypeCat::Int32, uint16(3), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeInt4: return { ImpliedTyping::TypeCat::Int32, uint16(4), ImpliedTyping::TypeHint::Vector };

        case MTLDataTypeUInt: return { ImpliedTyping::TypeCat::UInt32 };
        case MTLDataTypeUInt2: return { ImpliedTyping::TypeCat::UInt32, uint16(2), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeUInt3: return { ImpliedTyping::TypeCat::UInt32, uint16(3), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeUInt4: return { ImpliedTyping::TypeCat::UInt32, uint16(4), ImpliedTyping::TypeHint::Vector };

        case MTLDataTypeShort: return { ImpliedTyping::TypeCat::Int16 };
        case MTLDataTypeShort2: return { ImpliedTyping::TypeCat::Int16, uint16(2), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeShort3: return { ImpliedTyping::TypeCat::Int16, uint16(3), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeShort4: return { ImpliedTyping::TypeCat::Int16, uint16(4), ImpliedTyping::TypeHint::Vector };

        case MTLDataTypeUShort: return { ImpliedTyping::TypeCat::UInt16 };
        case MTLDataTypeUShort2: return { ImpliedTyping::TypeCat::UInt16, uint16(2), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeUShort3: return { ImpliedTyping::TypeCat::UInt16, uint16(3), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeUShort4: return { ImpliedTyping::TypeCat::UInt16, uint16(4), ImpliedTyping::TypeHint::Vector };

        case MTLDataTypeChar: return { ImpliedTyping::TypeCat::Int8 };
        case MTLDataTypeChar2: return { ImpliedTyping::TypeCat::Int8, uint16(2), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeChar3: return { ImpliedTyping::TypeCat::Int8, uint16(3), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeChar4: return { ImpliedTyping::TypeCat::Int8, uint16(4), ImpliedTyping::TypeHint::Vector };

        case MTLDataTypeUChar: return { ImpliedTyping::TypeCat::UInt8 };
        case MTLDataTypeUChar2: return { ImpliedTyping::TypeCat::UInt8, uint16(2), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeUChar3: return { ImpliedTyping::TypeCat::UInt8, uint16(3), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeUChar4: return { ImpliedTyping::TypeCat::UInt8, uint16(4), ImpliedTyping::TypeHint::Vector };

        case MTLDataTypeBool: return { ImpliedTyping::TypeCat::Bool };
        case MTLDataTypeBool2: return { ImpliedTyping::TypeCat::Bool, uint16(2), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeBool3: return { ImpliedTyping::TypeCat::Bool, uint16(3), ImpliedTyping::TypeHint::Vector };
        case MTLDataTypeBool4: return { ImpliedTyping::TypeCat::Bool, uint16(4), ImpliedTyping::TypeHint::Vector };

        default:
            return {};
        }
    }
}}
