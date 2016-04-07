// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Format.h"
#include "../../../Core/Prefix.h"
#include <assert.h>

namespace RenderCore { namespace Metal_Vulkan
{
    static NativeFormat::Enum s_vkToGenericTable[VK_FORMAT_END_RANGE];
    static VkFormat s_genericToVkTable[NativeFormat::Max];
    static bool s_lookupTablesInitialized = false;

    static void BindFormat2(NativeFormat::Enum lhs, VkFormat rhs)
    {
        assert(unsigned(lhs) < dimof(s_genericToVkTable));
        assert(unsigned(rhs) < dimof(s_vkToGenericTable));
        s_genericToVkTable[unsigned(lhs)] = rhs;
        s_vkToGenericTable[unsigned(rhs)] = lhs;
    }

    static void BindFormat1(NativeFormat::Enum lhs, VkFormat rhs)
    {
        assert(unsigned(rhs) < dimof(s_vkToGenericTable));
        s_vkToGenericTable[unsigned(rhs)] = lhs;
    }

    static void BindFormatTypeless(NativeFormat::Enum lhs, VkFormat rhs)
    {
        assert(unsigned(lhs) < dimof(s_genericToVkTable));
        s_genericToVkTable[unsigned(lhs)] = rhs;
    }

    void InitFormatConversionTables()
    {
        if (s_lookupTablesInitialized) return;

        using namespace NativeFormat;
        BindFormat2(Unknown, VK_FORMAT_UNDEFINED);
        // BindFormat2(R4G4_UNORM_PACK8, VK_FORMAT_R4G4_UNORM_PACK8);
        // BindFormat2(R4G4B4A4_UNORM_PACK16, VK_FORMAT_R4G4B4A4_UNORM_PACK16);
        // BindFormat2(B4G4R4A4_UNORM_PACK16, VK_FORMAT_B4G4R4A4_UNORM_PACK16);
        // BindFormat2(R5G6B5_UNORM_PACK16, VK_FORMAT_R5G6B5_UNORM_PACK16);
        // BindFormat2(B5G6R5_UNORM_PACK16, VK_FORMAT_B5G6R5_UNORM_PACK16);
        // BindFormat2(R5G5B5A1_UNORM_PACK16, VK_FORMAT_R5G5B5A1_UNORM_PACK16);
        // BindFormat2(B5G5R5A1_UNORM_PACK16, VK_FORMAT_B5G5R5A1_UNORM_PACK16);
        // BindFormat2(A1R5G5B5_UNORM_PACK16, VK_FORMAT_A1R5G5B5_UNORM_PACK16);
        BindFormat2(R8_UNORM, VK_FORMAT_R8_UNORM);
        BindFormat2(R8_SNORM, VK_FORMAT_R8_SNORM);
        // BindFormat2(R8_USCALED, VK_FORMAT_R8_USCALED);
        // BindFormat2(R8_SSCALED, VK_FORMAT_R8_SSCALED);
        BindFormat2(R8_UINT, VK_FORMAT_R8_UINT);
        BindFormat2(R8_SINT, VK_FORMAT_R8_SINT);
        // BindFormat2(R8_UNORM_SRGB, VK_FORMAT_R8_SRGB);
        BindFormat2(R8G8_UNORM, VK_FORMAT_R8G8_UNORM);
        BindFormat2(R8G8_SNORM, VK_FORMAT_R8G8_SNORM);
        // BindFormat2(R8G8_USCALED, VK_FORMAT_R8G8_USCALED);
        // BindFormat2(R8G8_SSCALED, VK_FORMAT_R8G8_SSCALED);
        BindFormat2(R8G8_UINT, VK_FORMAT_R8G8_UINT);
        BindFormat2(R8G8_SINT, VK_FORMAT_R8G8_SINT);
        // BindFormat2(R8G8_UNORM_SRGB, VK_FORMAT_R8G8_SRGB);
        // BindFormat2(R8G8B8_UNORM, VK_FORMAT_R8G8B8_UNORM);
        // BindFormat2(R8G8B8_SNORM, VK_FORMAT_R8G8B8_SNORM);
        // BindFormat2(R8G8B8_USCALED, VK_FORMAT_R8G8B8_USCALED);
        // BindFormat2(R8G8B8_SSCALED, VK_FORMAT_R8G8B8_SSCALED);
        // BindFormat2(R8G8B8_UINT, VK_FORMAT_R8G8B8_UINT);
        // BindFormat2(R8G8B8_SINT, VK_FORMAT_R8G8B8_SINT);
        // BindFormat2(R8G8B8_UNORM_SRGB, VK_FORMAT_R8G8B8_SRGB);
        // BindFormat2(B8G8R8_UNORM, VK_FORMAT_B8G8R8_UNORM);
        // BindFormat2(B8G8R8_SNORM, VK_FORMAT_B8G8R8_SNORM);
        // BindFormat2(B8G8R8_USCALED, VK_FORMAT_B8G8R8_USCALED);
        // BindFormat2(B8G8R8_SSCALED, VK_FORMAT_B8G8R8_SSCALED);
        // BindFormat2(B8G8R8_UINT, VK_FORMAT_B8G8R8_UINT);
        // BindFormat2(B8G8R8_SINT, VK_FORMAT_B8G8R8_SINT);
        // BindFormat2(B8G8R8_UNORM_SRGB, VK_FORMAT_B8G8R8_SRGB);
        BindFormat2(R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM);
        BindFormat2(R8G8B8A8_SNORM, VK_FORMAT_R8G8B8A8_SNORM);
        // BindFormat2(R8G8B8A8_USCALED, VK_FORMAT_R8G8B8A8_USCALED);
        // BindFormat2(R8G8B8A8_SSCALED, VK_FORMAT_R8G8B8A8_SSCALED);
        BindFormat2(R8G8B8A8_UINT, VK_FORMAT_R8G8B8A8_UINT);
        BindFormat2(R8G8B8A8_SINT, VK_FORMAT_R8G8B8A8_SINT);
        BindFormat2(R8G8B8A8_UNORM_SRGB, VK_FORMAT_R8G8B8A8_SRGB);
        BindFormat2(B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM);
        // BindFormat2(B8G8R8A8_SNORM, VK_FORMAT_B8G8R8A8_SNORM);
        // BindFormat2(B8G8R8A8_USCALED, VK_FORMAT_B8G8R8A8_USCALED);
        // BindFormat2(B8G8R8A8_SSCALED, VK_FORMAT_B8G8R8A8_SSCALED);
        // BindFormat2(B8G8R8A8_UINT, VK_FORMAT_B8G8R8A8_UINT);
        // BindFormat2(B8G8R8A8_SINT, VK_FORMAT_B8G8R8A8_SINT);
        BindFormat2(B8G8R8A8_UNORM_SRGB, VK_FORMAT_B8G8R8A8_SRGB);
        // BindFormat2(A8B8G8R8_UNORM_PACK32, VK_FORMAT_A8B8G8R8_UNORM_PACK32);
        // BindFormat2(A8B8G8R8_SNORM_PACK32, VK_FORMAT_A8B8G8R8_SNORM_PACK32);
        // BindFormat2(A8B8G8R8_USCALED_PACK32, VK_FORMAT_A8B8G8R8_USCALED_PACK32);
        // BindFormat2(A8B8G8R8_SSCALED_PACK32, VK_FORMAT_A8B8G8R8_SSCALED_PACK32);
        // BindFormat2(A8B8G8R8_UINT_PACK32, VK_FORMAT_A8B8G8R8_UINT_PACK32);
        // BindFormat2(A8B8G8R8_SINT_PACK32, VK_FORMAT_A8B8G8R8_SINT_PACK32);
        // BindFormat2(A8B8G8R8_SRGB_PACK32, VK_FORMAT_A8B8G8R8_SRGB_PACK32);
        // BindFormat2(A2R10G10B10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32);
        // BindFormat2(A2R10G10B10_SNORM_PACK32, VK_FORMAT_A2R10G10B10_SNORM_PACK32);
        // BindFormat2(A2R10G10B10_USCALED_PACK32, VK_FORMAT_A2R10G10B10_USCALED_PACK32);
        // BindFormat2(A2R10G10B10_SSCALED_PACK32, VK_FORMAT_A2R10G10B10_SSCALED_PACK32);
        // BindFormat2(A2R10G10B10_UINT_PACK32, VK_FORMAT_A2R10G10B10_UINT_PACK32);
        // BindFormat2(A2R10G10B10_SINT_PACK32, VK_FORMAT_A2R10G10B10_SINT_PACK32);
        // BindFormat2(A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
        // BindFormat2(A2B10G10R10_SNORM_PACK32, VK_FORMAT_A2B10G10R10_SNORM_PACK32);
        // BindFormat2(A2B10G10R10_USCALED_PACK32, VK_FORMAT_A2B10G10R10_USCALED_PACK32);
        // BindFormat2(A2B10G10R10_SSCALED_PACK32, VK_FORMAT_A2B10G10R10_SSCALED_PACK32);
        // BindFormat2(A2B10G10R10_UINT_PACK32, VK_FORMAT_A2B10G10R10_UINT_PACK32);
        // BindFormat2(A2B10G10R10_SINT_PACK32, VK_FORMAT_A2B10G10R10_SINT_PACK32);
        BindFormat2(R16_UNORM, VK_FORMAT_R16_UNORM);
        BindFormat2(R16_SNORM, VK_FORMAT_R16_SNORM);
        // BindFormat2(R16_USCALED, VK_FORMAT_R16_USCALED);
        // BindFormat2(R16_SSCALED, VK_FORMAT_R16_SSCALED);
        BindFormat2(R16_UINT, VK_FORMAT_R16_UINT);
        BindFormat2(R16_SINT, VK_FORMAT_R16_SINT);
        BindFormat2(R16_FLOAT, VK_FORMAT_R16_SFLOAT);
        BindFormat2(R16G16_UNORM, VK_FORMAT_R16G16_UNORM);
        BindFormat2(R16G16_SNORM, VK_FORMAT_R16G16_SNORM);
        // BindFormat2(R16G16_USCALED, VK_FORMAT_R16G16_USCALED);
        // BindFormat2(R16G16_SSCALED, VK_FORMAT_R16G16_SSCALED);
        BindFormat2(R16G16_UINT, VK_FORMAT_R16G16_UINT);
        BindFormat2(R16G16_SINT, VK_FORMAT_R16G16_SINT);
        BindFormat2(R16G16_FLOAT, VK_FORMAT_R16G16_SFLOAT);
        // BindFormat2(R16G16B16_UNORM, VK_FORMAT_R16G16B16_UNORM);
        // BindFormat2(R16G16B16_SNORM, VK_FORMAT_R16G16B16_SNORM);
        // BindFormat2(R16G16B16_USCALED, VK_FORMAT_R16G16B16_USCALED);
        // BindFormat2(R16G16B16_SSCALED, VK_FORMAT_R16G16B16_SSCALED);
        // BindFormat2(R16G16B16_UINT, VK_FORMAT_R16G16B16_UINT);
        // BindFormat2(R16G16B16_SINT, VK_FORMAT_R16G16B16_SINT);
        // BindFormat2(R16G16B16_SFLOAT, VK_FORMAT_R16G16B16_SFLOAT);
        BindFormat2(R16G16B16A16_UNORM, VK_FORMAT_R16G16B16A16_UNORM);
        BindFormat2(R16G16B16A16_SNORM, VK_FORMAT_R16G16B16A16_SNORM);
        // BindFormat2(R16G16B16A16_USCALED, VK_FORMAT_R16G16B16A16_USCALED);
        // BindFormat2(R16G16B16A16_SSCALED, VK_FORMAT_R16G16B16A16_SSCALED);
        BindFormat2(R16G16B16A16_UINT, VK_FORMAT_R16G16B16A16_UINT);
        BindFormat2(R16G16B16A16_SINT, VK_FORMAT_R16G16B16A16_SINT);
        BindFormat2(R16G16B16A16_FLOAT, VK_FORMAT_R16G16B16A16_SFLOAT);
        BindFormat2(R32_UINT, VK_FORMAT_R32_UINT);
        BindFormat2(R32_SINT, VK_FORMAT_R32_SINT);
        BindFormat2(R32_FLOAT, VK_FORMAT_R32_SFLOAT);
        BindFormat2(R32G32_UINT, VK_FORMAT_R32G32_UINT);
        BindFormat2(R32G32_SINT, VK_FORMAT_R32G32_SINT);
        BindFormat2(R32G32_FLOAT, VK_FORMAT_R32G32_SFLOAT);
        BindFormat2(R32G32B32_UINT, VK_FORMAT_R32G32B32_UINT);
        BindFormat2(R32G32B32_SINT, VK_FORMAT_R32G32B32_SINT);
        BindFormat2(R32G32B32_FLOAT, VK_FORMAT_R32G32B32_SFLOAT);
        BindFormat2(R32G32B32A32_UINT, VK_FORMAT_R32G32B32A32_UINT);
        BindFormat2(R32G32B32A32_SINT, VK_FORMAT_R32G32B32A32_SINT);
        BindFormat2(R32G32B32A32_FLOAT, VK_FORMAT_R32G32B32A32_SFLOAT);
        // BindFormat2(R64_UINT, VK_FORMAT_R64_UINT);
        // BindFormat2(R64_SINT, VK_FORMAT_R64_SINT);
        // BindFormat2(R64_FLOAT, VK_FORMAT_R64_SFLOAT);
        // BindFormat2(R64G64_UINT, VK_FORMAT_R64G64_UINT);
        // BindFormat2(R64G64_SINT, VK_FORMAT_R64G64_SINT);
        // BindFormat2(R64G64_FLOAT, VK_FORMAT_R64G64_SFLOAT);
        // BindFormat2(R64G64B64_UINT, VK_FORMAT_R64G64B64_UINT);
        // BindFormat2(R64G64B64_SINT, VK_FORMAT_R64G64B64_SINT);
        // BindFormat2(R64G64B64_FLOAT, VK_FORMAT_R64G64B64_SFLOAT);
        // BindFormat2(R64G64B64A64_UINT, VK_FORMAT_R64G64B64A64_UINT);
        // BindFormat2(R64G64B64A64_SINT, VK_FORMAT_R64G64B64A64_SINT);
        // BindFormat2(R64G64B64A64_FLOAT, VK_FORMAT_R64G64B64A64_SFLOAT);
        // BindFormat2(B10G11R11_UFLOAT_PACK32, VK_FORMAT_B10G11R11_UFLOAT_PACK32);     R11G11B10_FLOAT?
        // BindFormat2(E5B9G9R9_UFLOAT_PACK32, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);
        BindFormat2(D16_UNORM, VK_FORMAT_D16_UNORM);
        // BindFormat2(X8_D24_UNORM_PACK32, VK_FORMAT_X8_D24_UNORM_PACK32);             R24_UNORM_X8_TYPELESS?
        BindFormat2(D32_FLOAT, VK_FORMAT_D32_SFLOAT);
        // BindFormat2(S8_UINT, VK_FORMAT_S8_UINT);
        // BindFormat2(D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT);
        BindFormat2(D24_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT);
        // BindFormat2(D32_FLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT);
        BindFormat1(BC1_UNORM, VK_FORMAT_BC1_RGB_UNORM_BLOCK);
        BindFormat1(BC1_UNORM_SRGB, VK_FORMAT_BC1_RGB_SRGB_BLOCK);
        BindFormat2(BC1_UNORM, VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
        BindFormat2(BC1_UNORM_SRGB, VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
        BindFormat2(BC2_UNORM, VK_FORMAT_BC2_UNORM_BLOCK);
        BindFormat2(BC2_UNORM_SRGB, VK_FORMAT_BC2_SRGB_BLOCK);
        BindFormat2(BC3_UNORM, VK_FORMAT_BC3_UNORM_BLOCK);
        BindFormat2(BC3_UNORM_SRGB, VK_FORMAT_BC3_SRGB_BLOCK);
        BindFormat2(BC4_UNORM, VK_FORMAT_BC4_UNORM_BLOCK);
        BindFormat2(BC4_SNORM, VK_FORMAT_BC4_SNORM_BLOCK);
        BindFormat2(BC5_UNORM, VK_FORMAT_BC5_UNORM_BLOCK);
        BindFormat2(BC5_SNORM, VK_FORMAT_BC5_SNORM_BLOCK);
        BindFormat2(BC6H_UF16, VK_FORMAT_BC6H_UFLOAT_BLOCK);
        BindFormat2(BC6H_SF16, VK_FORMAT_BC6H_SFLOAT_BLOCK);
        BindFormat2(BC7_UNORM, VK_FORMAT_BC7_UNORM_BLOCK);
        BindFormat2(BC7_UNORM_SRGB, VK_FORMAT_BC7_SRGB_BLOCK);
        // BindFormat2(ETC2_R8G8B8_UNORM, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
        // BindFormat2(ETC2_R8G8B8_UNORM_SRGB, VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
        // BindFormat2(ETC2_R8G8B8A1_UNORM, VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK);
        // BindFormat2(ETC2_R8G8B8A1_UNORM_SRGB, VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK);
        // BindFormat2(ETC2_R8G8B8A8_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
        // BindFormat2(ETC2_R8G8B8A8_UNORM_SRGB, VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
        // BindFormat2(EAC_R11_UNORM, VK_FORMAT_EAC_R11_UNORM_BLOCK);
        // BindFormat2(EAC_R11_SNORM, VK_FORMAT_EAC_R11_SNORM_BLOCK);
        // BindFormat2(EAC_R11G11_UNORM, VK_FORMAT_EAC_R11G11_UNORM_BLOCK);
        // BindFormat2(EAC_R11G11_SNORM, VK_FORMAT_EAC_R11G11_SNORM_BLOCK);
        // BindFormat2(ASTC_4x4_UNORM, VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
        // BindFormat2(ASTC_4x4_UNORM_SRGB, VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
        // BindFormat2(ASTC_5x4_UNORM, VK_FORMAT_ASTC_5x4_UNORM_BLOCK);
        // BindFormat2(ASTC_5x4_UNORM_SRGB, VK_FORMAT_ASTC_5x4_SRGB_BLOCK);
        // BindFormat2(ASTC_5x5_UNORM, VK_FORMAT_ASTC_5x5_UNORM_BLOCK);
        // BindFormat2(ASTC_5x5_UNORM_SRGB, VK_FORMAT_ASTC_5x5_SRGB_BLOCK);
        // BindFormat2(ASTC_6x5_UNORM, VK_FORMAT_ASTC_6x5_UNORM_BLOCK);
        // BindFormat2(ASTC_6x5_UNORM_SRGB, VK_FORMAT_ASTC_6x5_SRGB_BLOCK);
        // BindFormat2(ASTC_6x6_UNORM, VK_FORMAT_ASTC_6x6_UNORM_BLOCK);
        // BindFormat2(ASTC_6x6_UNORM_SRGB, VK_FORMAT_ASTC_6x6_SRGB_BLOCK);
        // BindFormat2(ASTC_8x5_UNORM, VK_FORMAT_ASTC_8x5_UNORM_BLOCK);
        // BindFormat2(ASTC_8x5_UNORM_SRGB, VK_FORMAT_ASTC_8x5_SRGB_BLOCK);
        // BindFormat2(ASTC_8x6_UNORM, VK_FORMAT_ASTC_8x6_UNORM_BLOCK);
        // BindFormat2(ASTC_8x6_UNORM_SRGB, VK_FORMAT_ASTC_8x6_SRGB_BLOCK);
        // BindFormat2(ASTC_8x8_UNORM, VK_FORMAT_ASTC_8x8_UNORM_BLOCK);
        // BindFormat2(ASTC_8x8_UNORM_SRGB, VK_FORMAT_ASTC_8x8_SRGB_BLOCK);
        // BindFormat2(ASTC_10x5_UNORM, VK_FORMAT_ASTC_10x5_UNORM_BLOCK);
        // BindFormat2(ASTC_10x5_UNORM_SRGB, VK_FORMAT_ASTC_10x5_SRGB_BLOCK);
        // BindFormat2(ASTC_10x6_UNORM, VK_FORMAT_ASTC_10x6_UNORM_BLOCK);
        // BindFormat2(ASTC_10x6_UNORM_SRGB, VK_FORMAT_ASTC_10x6_SRGB_BLOCK);
        // BindFormat2(ASTC_10x8_UNORM, VK_FORMAT_ASTC_10x8_UNORM_BLOCK);
        // BindFormat2(ASTC_10x8_UNORM_SRGB, VK_FORMAT_ASTC_10x8_SRGB_BLOCK);
        // BindFormat2(ASTC_10x10_UNORM, VK_FORMAT_ASTC_10x10_UNORM_BLOCK);
        // BindFormat2(ASTC_10x10_UNORM_SRGB, VK_FORMAT_ASTC_10x10_SRGB_BLOCK);
        // BindFormat2(ASTC_12x10_UNORM, VK_FORMAT_ASTC_12x10_UNORM_BLOCK);
        // BindFormat2(ASTC_12x10_UNORM_SRGB, VK_FORMAT_ASTC_12x10_SRGB_BLOCK);
        // BindFormat2(ASTC_12x12_UNORM, VK_FORMAT_ASTC_12x12_UNORM_BLOCK);
        // BindFormat2(ASTC_12x12_UNORM_SRGB, VK_FORMAT_ASTC_12x12_SRGB_BLOCK);

        // Vulkan has no "typeless" constant -- so we should bind the typeless
        // formats onto some default type. Note the defaults here are picked arbitrarily,
        // based on commonly used formats -- but with SRGB formats avoided.
        BindFormatTypeless(R32G32B32A32_TYPELESS, VK_FORMAT_R32G32B32A32_SFLOAT);
        BindFormatTypeless(R32G32B32_TYPELESS, VK_FORMAT_R32G32B32_SFLOAT);
        BindFormatTypeless(R16G16B16A16_TYPELESS, VK_FORMAT_R16G16B16A16_SFLOAT);
        BindFormatTypeless(R32G32_TYPELESS, VK_FORMAT_R32G32_SFLOAT);
        BindFormatTypeless(R8G8B8A8_TYPELESS, VK_FORMAT_R8G8B8A8_UNORM);
        BindFormatTypeless(R16G16_TYPELESS, VK_FORMAT_R16G16_SFLOAT);
        BindFormatTypeless(R32_TYPELESS, VK_FORMAT_R32_SFLOAT);
        BindFormatTypeless(R8G8_TYPELESS, VK_FORMAT_R8G8_UNORM);
        BindFormatTypeless(R16_TYPELESS, VK_FORMAT_R16_SFLOAT);
        BindFormatTypeless(R8_TYPELESS, VK_FORMAT_R8_UNORM);
        BindFormatTypeless(BC1_TYPELESS, VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
        BindFormatTypeless(BC2_TYPELESS, VK_FORMAT_BC2_UNORM_BLOCK);
        BindFormatTypeless(BC3_TYPELESS, VK_FORMAT_BC3_UNORM_BLOCK);
        BindFormatTypeless(BC4_TYPELESS, VK_FORMAT_BC4_UNORM_BLOCK);
        BindFormatTypeless(BC5_TYPELESS, VK_FORMAT_BC5_UNORM_BLOCK);
        BindFormatTypeless(BC6H_TYPELESS, VK_FORMAT_BC6H_SFLOAT_BLOCK);
        BindFormatTypeless(BC7_TYPELESS, VK_FORMAT_BC7_UNORM_BLOCK);
        BindFormatTypeless(B8G8R8A8_TYPELESS, VK_FORMAT_B8G8R8A8_UNORM);

        s_lookupTablesInitialized = true;
    }

    VkFormat AsVkFormat(NativeFormat::Enum input)
    {
        assert(s_lookupTablesInitialized);
        // Assuming NativeFormat::Enum and VkFormat are not the same thing, there's
        // just no way to do this conversion easily. We have to use a big lookup
        // table
        if (unsigned(input) > dimof(s_genericToVkTable))
            return VK_FORMAT_UNDEFINED;
        return s_genericToVkTable[unsigned(input)];
    }



    unsigned                    BitsPerPixel(NativeFormat::Enum format)
    {
        switch (format) {
        #define _EXP(X, Y, Z, U)    case NativeFormat::X##_##Y: return U;
            #include "../../Metal/Detail/DXGICompatibleFormats.h"
        #undef _EXP
        case NativeFormat::Matrix4x4: return 16 * sizeof(float) * 8;
        case NativeFormat::Matrix3x4: return 12 * sizeof(float) * 8;
        default: return 0;
        }
    }

}}

