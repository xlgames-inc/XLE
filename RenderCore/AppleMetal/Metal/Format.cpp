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

#if 0
            case Format::R32G32B32_FLOAT: return {GL_RGB, GL_FLOAT, GL_RGB32F};
            case Format::R32G32B32_UINT: return {GL_RGB_INTEGER, GL_UNSIGNED_INT, GL_RGB32UI};
            case Format::R32G32B32_SINT: return {GL_RGB_INTEGER, GL_INT, GL_RGB32I};
#endif

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

#if 0
            case Format::R8G8B8_UNORM: return {GL_RGB, GL_UNSIGNED_BYTE, GL_RGB8};
            case Format::R8G8B8_UINT: return {GL_RGB_INTEGER, GL_UNSIGNED_BYTE, GL_RGB8UI};
            case Format::R8G8B8_SNORM: return {GL_RGB, GL_BYTE, GL_RGB8_SNORM};
            case Format::R8G8B8_SINT: return {GL_RGB_INTEGER, GL_BYTE, GL_RGB8I};
#endif
            case Format::R8G8_UNORM: return MTLPixelFormatRG8Unorm;
            case Format::R8G8_UINT: return MTLPixelFormatRG8Uint;
            case Format::R8G8_SNORM: return MTLPixelFormatRG8Snorm;
            case Format::R8G8_SINT: return MTLPixelFormatRG8Sint;

            case Format::R16_FLOAT: return MTLPixelFormatR16Float;
#if !HACK_PLATFORM_IOS
            case Format::D16_UNORM: return MTLPixelFormatDepth16Unorm;
#endif
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
#if HACK_PLATFORM_IOS
            case Format::B5G6R5_UNORM: return MTLPixelFormatB5G6R5Unorm;
            case Format::B5G5R5A1_UNORM: return MTLPixelFormatA1BGR5Unorm;
            case Format::R4G4B4A4_UNORM: return MTLPixelFormatABGR4Unorm;
#endif

#if !HACK_PLATFORM_IOS
            case Format::D24_UNORM_S8_UINT: return MTLPixelFormatDepth24Unorm_Stencil8;
#endif
            case Format::D32_SFLOAT_S8_UINT: return MTLPixelFormatDepth32Float_Stencil8;
            case Format::R8G8B8A8_UNORM_SRGB: return MTLPixelFormatRGBA8Unorm_sRGB;
#if 0
            case Format::R8G8B8_UNORM_SRGB: return {GL_RGBA, GL_BYTE, GL_RGB8_SNORM};

            case Format::RGB_PVRTC1_2BPP_UNORM: return {0, 0, GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG};
            case Format::RGBA_PVRTC1_2BPP_UNORM: return {0, 0, GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG};
            case Format::RGB_PVRTC1_4BPP_UNORM: return {0, 0, GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG};
            case Format::RGBA_PVRTC1_4BPP_UNORM: return {0, 0, GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG};
            case Format::RGBA_PVRTC2_2BPP_UNORM: return {0, 0, GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG};
            case Format::RGBA_PVRTC2_4BPP_UNORM: return {0, 0, GL_COMPRESSED_RGBA_PVRTC_4BPPV2_IMG};
            case Format::RGB_ETC1_UNORM: return {0, 0, GL_ETC1_RGB8_OES};

            case Format::RGB_ETC2_UNORM: return {0, 0, GL_COMPRESSED_RGB8_ETC2};
            case Format::RGBA_ETC2_UNORM: return {0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC};
            case Format::RGBA1_ETC2_UNORM: return {0, 0, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2};

            case Format::RGB_ETC2_UNORM_SRGB: return {0, 0, GL_COMPRESSED_SRGB8_ETC2};
            case Format::RGBA_ETC2_UNORM_SRGB: return {0, 0, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC};
            case Format::RGBA1_ETC2_UNORM_SRGB: return {0, 0, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2};

            case Format::RGB_PVRTC1_2BPP_UNORM_SRGB:
            case Format::RGBA_PVRTC1_2BPP_UNORM_SRGB:
            case Format::RGB_PVRTC1_4BPP_UNORM_SRGB:
            case Format::RGBA_PVRTC1_4BPP_UNORM_SRGB:
            case Format::RGBA_PVRTC2_2BPP_UNORM_SRGB:
            case Format::RGBA_PVRTC2_4BPP_UNORM_SRGB:
            case Format::RGB_ETC1_UNORM_SRGB:
#endif

            case Format::B8G8R8A8_UNORM: return MTLPixelFormatBGRA8Unorm;

            default: break;
        }

        // KenD -- Metal doesn't have a 24-bit format, which is quite unfortunate
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

        return MTLPixelFormatBGRA8Unorm;
    }

    RenderCore::Format AsRenderCoreFormat(MTLPixelFormat fmt)
    {
        using namespace RenderCore;
        switch (fmt)
        {
            case MTLPixelFormatRGBA32Float: return Format::R32G32B32A32_FLOAT;
            case MTLPixelFormatRGBA32Uint: return Format::R32G32B32A32_UINT;
            case MTLPixelFormatRGBA32Sint: return Format::R32G32B32A32_SINT;

#if 0
            case {GL_RGB, GL_FLOAT, GL_RGB32F}: return Format::R32G32B32_FLOAT;
            case {GL_RGB_INTEGER, GL_UNSIGNED_INT, GL_RGB32UI}: return Format::R32G32B32_UINT;
            case {GL_RGB_INTEGER, GL_INT, GL_RGB32I}: return Format::R32G32B32_SINT;
#endif

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

#if 0
            case {GL_RGB, GL_UNSIGNED_BYTE, GL_RGB8}: return Format::R8G8B8_UNORM;
            case {GL_RGB_INTEGER, GL_UNSIGNED_BYTE, GL_RGB8UI}: return Format::R8G8B8_UINT;
            case {GL_RGB, GL_BYTE, GL_RGB8_SNORM}: return Format::R8G8B8_SNORM;
            case {GL_RGB_INTEGER, GL_BYTE, GL_RGB8I}: return Format::R8G8B8_SINT;
#endif
            case MTLPixelFormatRG8Unorm: return Format::R8G8_UNORM;
            case MTLPixelFormatRG8Uint: return Format::R8G8_UINT;
            case MTLPixelFormatRG8Snorm: return Format::R8G8_SNORM;
            case MTLPixelFormatRG8Sint: return Format::R8G8_SINT;

            case MTLPixelFormatR16Float: return Format::R16_FLOAT;
#if !HACK_PLATFORM_IOS
            case MTLPixelFormatDepth16Unorm: return Format::D16_UNORM;
#endif
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
#if HACK_PLATFORM_IOS
            case MTLPixelFormatB5G6R5Unorm: return Format::B5G6R5_UNORM;
            case MTLPixelFormatA1BGR5Unorm: return Format::B5G5R5A1_UNORM;
            case MTLPixelFormatABGR4Unorm: return Format::R4G4B4A4_UNORM;
#endif

#if !HACK_PLATFORM_IOS
            case MTLPixelFormatDepth24Unorm_Stencil8: return Format::D24_UNORM_S8_UINT;
#endif
            case MTLPixelFormatDepth32Float_Stencil8: return Format::D32_SFLOAT_S8_UINT;
            case MTLPixelFormatRGBA8Unorm_sRGB: return Format::R8G8B8A8_UNORM_SRGB;
#if 0
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

            case MTLPixelFormatBGRA8Unorm: return Format::B8G8R8A8_UNORM;

            default: break;
        }

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

        return Format::Unknown;
    }

    MTLVertexFormat AsMTLVertexFormat(RenderCore::Format fmt)
    {
        switch (fmt) {
            case Format::R32G32_FLOAT: return MTLVertexFormatFloat2;
            case Format::R32G32B32_FLOAT: return MTLVertexFormatFloat3;
            case Format::R32G32B32A32_FLOAT: return MTLVertexFormatFloat4;
            default: assert(0); return MTLVertexFormatInvalid;
        }
    }
}}
