// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Format.h"
#include "../../../Core/Prefix.h"
#include "../../../Utility/ParameterBox.h"
#include "../../../Utility/StringUtils.h"
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

///////////////////////////////////////////////////////////////////////////////////////////////////

    FormatCompressionType::Enum       GetCompressionType(NativeFormat::Enum format)
    {
        switch (format) {
        #define _EXP(X, Y, Z, U)    case NativeFormat::X##_##Y: return FormatCompressionType::Z;
            #include "../../Metal/Detail/DXGICompatibleFormats.h"
        #undef _EXP
        default:
            return FormatCompressionType::None;
        }
    }

    /// Container for FormatPrefix::Enum
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
            B5G6R5, B5G5R5A1, B8G8R8A8, B8G8R8X8,
            Unknown
        };
    }

    static FormatPrefix::Enum   GetPrefix(NativeFormat::Enum format)
    {
        switch (format) {
        #define _EXP(X, Y, Z, U)    case NativeFormat::X##_##Y: return FormatPrefix::X;
            #include "../../Metal/Detail/DXGICompatibleFormats.h"
        #undef _EXP
        default: return FormatPrefix::Unknown;
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
            #define _EXP(X, Y, Z, U)    case NativeFormat::X##_##Y: input = Y; break;
                #include "../../Metal/Detail/DXGICompatibleFormats.h"
            #undef _EXP
            case NativeFormat::Matrix4x4: input = FLOAT; break;
            case NativeFormat::Matrix3x4: input = FLOAT; break;
            default: input = TYPELESS; break;
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
        #define _EXP(X, Y, Z, U)    case NativeFormat::X##_##Y: return U;
            #include "../../Metal/Detail/DXGICompatibleFormats.h"
        #undef _EXP
        case NativeFormat::Matrix4x4: return 16 * sizeof(float) * 8;
        case NativeFormat::Matrix3x4: return 12 * sizeof(float) * 8;
        default: return 0;
        }
    }

    unsigned    GetComponentPrecision(NativeFormat::Enum format)
    {
        return BitsPerPixel(format) / GetComponentCount(GetComponents(format));
    }

    unsigned    GetDecompressedComponentPrecision(NativeFormat::Enum format)
    {
        FormatPrefix::Enum prefix = GetPrefix(format);
        using namespace FormatPrefix;
        switch (prefix) {
        case BC1:
        case BC2:
        case BC3:
        case BC4:
        case BC5:   return 8;
        case BC6H:  return 16;
        case BC7:   return 8;   // (can be used for higher precision data)
        default:
            return GetComponentPrecision(format);
        }
    }

    unsigned    GetComponentCount(FormatComponents::Enum components)
    {
        using namespace FormatComponents;
        switch (components) 
        {
        case Alpha:
        case Luminance: 
        case Depth: return 1;

        case LuminanceAlpha:
        case RG: return 2;
        
        case RGB: return 3;

        case RGBAlpha:
        case RGBE: return 4;

        default: return 0;
        }
    }

    NativeFormat::Enum FindFormat(
        FormatCompressionType::Enum compression, 
        FormatComponents::Enum components,
        FormatComponentType::Enum componentType,
        unsigned precision)
    {
        #define _EXP(X, Y, Z, U)                                                    \
            if (    compression == FormatCompressionType::Z                         \
                &&  components == GetComponents(NativeFormat::X##_##Y)              \
                &&  componentType == GetComponentType(NativeFormat::X##_##Y)        \
                &&  precision == GetComponentPrecision(NativeFormat::X##_##Y)) {    \
                return NativeFormat::X##_##Y;                                       \
            }                                                                       \
            /**/
            #include "../../Metal/Detail/DXGICompatibleFormats.h"
        #undef _EXP

        if (components == FormatComponents::RGB)
            return FindFormat(compression, FormatComponents::RGBAlpha, componentType, precision);

        return NativeFormat::Unknown;
    }


    NativeFormat::Enum AsSRGBFormat(NativeFormat::Enum inputFormat)
    {
        using namespace NativeFormat;
        switch (inputFormat) {
        case R8G8B8A8_TYPELESS:
        case R8G8B8A8_UNORM: return R8G8B8A8_UNORM_SRGB;
        case BC1_TYPELESS:
        case BC1_UNORM: return BC1_UNORM_SRGB;
        case BC2_TYPELESS:
        case BC2_UNORM: return BC2_UNORM_SRGB;
        case BC3_TYPELESS:
        case BC3_UNORM: return BC3_UNORM_SRGB;
        case BC7_TYPELESS:
        case BC7_UNORM: return BC7_UNORM_SRGB;

        case B8G8R8A8_TYPELESS:
        case B8G8R8A8_UNORM: return B8G8R8A8_UNORM_SRGB;
        case B8G8R8X8_TYPELESS:
        case B8G8R8X8_UNORM: return B8G8R8X8_UNORM_SRGB;
        }
        return inputFormat; // no linear/srgb version of this format exists
    }

    NativeFormat::Enum AsLinearFormat(NativeFormat::Enum inputFormat)
    {
        using namespace NativeFormat;
        switch (inputFormat) {
        case R8G8B8A8_TYPELESS:
        case R8G8B8A8_UNORM_SRGB: return R8G8B8A8_UNORM;
        case BC1_TYPELESS:
        case BC1_UNORM_SRGB: return BC1_UNORM;
        case BC2_TYPELESS:
        case BC2_UNORM_SRGB: return BC2_UNORM;
        case BC3_TYPELESS:
        case BC3_UNORM_SRGB: return BC3_UNORM;
        case BC7_TYPELESS:
        case BC7_UNORM_SRGB: return BC7_UNORM;

        case B8G8R8A8_TYPELESS:
        case B8G8R8A8_UNORM_SRGB: return B8G8R8A8_UNORM;
        case B8G8R8X8_TYPELESS:
        case B8G8R8X8_UNORM_SRGB: return B8G8R8X8_UNORM;
        }
        return inputFormat; // no linear/srgb version of this format exists
    }

    NativeFormat::Enum      AsTypelessFormat(NativeFormat::Enum inputFormat)
    {
            // note -- currently this only modifies formats that are also
            //          modified by AsSRGBFormat and AsLinearFormat. This is
            //          important, because this function is used to convert
            //          a pixel format for a texture that might be used by
            //          either a linear or srgb shader resource view.
            //          If this function changes formats aren't also changed
            //          by AsSRGBFormat and AsLinearFormat, it will cause some
            //          sources to fail to load correctly.
        using namespace NativeFormat;
        switch (inputFormat) {
        case R8G8B8A8_UNORM:
        case R8G8B8A8_UNORM_SRGB: return R8G8B8A8_TYPELESS;
        case BC1_UNORM:
        case BC1_UNORM_SRGB: return BC1_TYPELESS;
        case BC2_UNORM:
        case BC2_UNORM_SRGB: return BC2_TYPELESS;
        case BC3_UNORM:
        case BC3_UNORM_SRGB: return BC3_TYPELESS;
        case BC7_UNORM:
        case BC7_UNORM_SRGB: return BC7_TYPELESS;

        case B8G8R8A8_UNORM:
        case B8G8R8A8_UNORM_SRGB: return B8G8R8A8_TYPELESS;
        case B8G8R8X8_UNORM:
        case B8G8R8X8_UNORM_SRGB: return B8G8R8X8_TYPELESS;

        case D24_UNORM_S8_UINT:
        case R24_UNORM_X8_TYPELESS:
        case X24_TYPELESS_G8_UINT: return R24G8_TYPELESS;

        case R32_TYPELESS:
        case D32_FLOAT:
        case R32_FLOAT:
        case R32_UINT:
        case R32_SINT: return R32_TYPELESS;
        }
        return inputFormat; // no linear/srgb version of this format exists
    }

    bool HasLinearAndSRGBFormats(NativeFormat::Enum inputFormat)
    {
        using namespace NativeFormat;
        switch (inputFormat) {
        case R8G8B8A8_UNORM:
        case R8G8B8A8_UNORM_SRGB:
        case R8G8B8A8_TYPELESS:
        case BC1_UNORM:
        case BC1_UNORM_SRGB: 
        case BC1_TYPELESS:
        case BC2_UNORM:
        case BC2_UNORM_SRGB: 
        case BC2_TYPELESS:
        case BC3_UNORM:
        case BC3_UNORM_SRGB: 
        case BC3_TYPELESS:
        case BC7_UNORM:
        case BC7_UNORM_SRGB: 
        case BC7_TYPELESS:
        case B8G8R8A8_UNORM:
        case B8G8R8A8_UNORM_SRGB: 
        case B8G8R8A8_TYPELESS:
        case B8G8R8X8_UNORM:
        case B8G8R8X8_UNORM_SRGB: 
        case B8G8R8X8_TYPELESS:
            return true;

        default: return false;
        }
    }

    NativeFormat::Enum AsNativeFormat(
        const ImpliedTyping::TypeDesc& type,
        ShaderNormalizationMode::Enum norm)
    {
        using namespace NativeFormat;

        if (type._type == ImpliedTyping::TypeCat::Float) {
            if (type._arrayCount == 1) return R32_FLOAT;
            if (type._arrayCount == 2) return R32G32_FLOAT;
            if (type._arrayCount == 3) return R32G32B32_FLOAT;
            if (type._arrayCount == 4) return R32G32B32A32_FLOAT;
            return Unknown;
        }
        
        if (norm == ShaderNormalizationMode::Integer) {
            switch (type._type) {
            case ImpliedTyping::TypeCat::Int8:
                if (type._arrayCount == 1) return R8_SINT;
                if (type._arrayCount == 2) return R8G8_SINT;
                if (type._arrayCount == 4) return R8G8B8A8_SINT;
                break;

            case ImpliedTyping::TypeCat::UInt8:
                if (type._arrayCount == 1) return R8_UINT;
                if (type._arrayCount == 2) return R8G8_UINT;
                if (type._arrayCount == 4) return R8G8B8A8_UINT;
                break;

            case ImpliedTyping::TypeCat::Int16:
                if (type._arrayCount == 1) return R16_SINT;
                if (type._arrayCount == 2) return R16G16_SINT;
                if (type._arrayCount == 4) return R16G16B16A16_SINT;
                break;

            case ImpliedTyping::TypeCat::UInt16:
                if (type._arrayCount == 1) return R16_UINT;
                if (type._arrayCount == 2) return R16G16_UINT;
                if (type._arrayCount == 4) return R16G16B16A16_UINT;
                break;

            case ImpliedTyping::TypeCat::Int32:
                if (type._arrayCount == 1) return R32_SINT;
                if (type._arrayCount == 2) return R32G32_SINT;
                if (type._arrayCount == 3) return R32G32B32_SINT;
                if (type._arrayCount == 4) return R32G32B32A32_SINT;
                break;

            case ImpliedTyping::TypeCat::UInt32:
                if (type._arrayCount == 1) return R32_UINT;
                if (type._arrayCount == 2) return R32G32_UINT;
                if (type._arrayCount == 3) return R32G32B32_UINT;
                if (type._arrayCount == 4) return R32G32B32A32_UINT;
                break;
            }
        } else if (norm == ShaderNormalizationMode::Normalized) {
            switch (type._type) {
            case ImpliedTyping::TypeCat::Int8:
                if (type._arrayCount == 1) return R8_SNORM;
                if (type._arrayCount == 2) return R8G8_SNORM;
                if (type._arrayCount == 4) return R8G8B8A8_SNORM;
                break;

            case ImpliedTyping::TypeCat::UInt8:
                if (type._arrayCount == 1) return R8_UNORM;
                if (type._arrayCount == 2) return R8G8_UNORM;
                if (type._arrayCount == 4) return R8G8B8A8_UNORM;
                break;

            case ImpliedTyping::TypeCat::Int16:
                if (type._arrayCount == 1) return R16_SNORM;
                if (type._arrayCount == 2) return R16G16_SNORM;
                if (type._arrayCount == 4) return R16G16B16A16_SNORM;
                break;

            case ImpliedTyping::TypeCat::UInt16:
                if (type._arrayCount == 1) return R16_UNORM;
                if (type._arrayCount == 2) return R16G16_UNORM;
                if (type._arrayCount == 4) return R16G16B16A16_UNORM;
                break;
            }
        } else if (norm == ShaderNormalizationMode::Float) {
            switch (type._type) {
            case ImpliedTyping::TypeCat::Int16:
            case ImpliedTyping::TypeCat::UInt16:
                if (type._arrayCount == 1) return R16_FLOAT;
                if (type._arrayCount == 2) return R16G16_FLOAT;
                if (type._arrayCount == 4) return R16G16B16A16_FLOAT;
                break;
            }
        }

        return Unknown;
    }

    const char* AsString(NativeFormat::Enum format)
    {
        switch (format) {
            #define _EXP(X, Y, Z, U)    case NativeFormat::X##_##Y: return #X;
                #include "../../Metal/Detail/DXGICompatibleFormats.h"
            #undef _EXP
        case NativeFormat::Matrix4x4: return "Matrix4x4";
        case NativeFormat::Matrix3x4: return "Matrix3x4";
        default: return "Unknown";
        }
    }

    #define STRINGIZE(X) #X

    NativeFormat::Enum AsNativeFormat(const char name[])
    {
        #define _EXP(X, Y, Z, U)    if (XlEqStringI(name, STRINGIZE(X##_##Y))) return NativeFormat::X##_##Y;
            #include "../../Metal/Detail/DXGICompatibleFormats.h"
        #undef _EXP

        if (!XlEqStringI(name, "Matrix4x4")) return NativeFormat::Matrix4x4;
        if (!XlEqStringI(name, "Matrix3x4")) return NativeFormat::Matrix3x4;
        return NativeFormat::Unknown;
    }

}}

