// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderResource.h"
#include "ObjectFactory.h"
#include "State.h"

namespace RenderCore { namespace Metal_Vulkan
{

    ShaderResourceView::ShaderResourceView(const ObjectFactory& factory, VkImage image, NativeFormat::Enum)
    {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.pNext = nullptr;
        view_info.image = image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        view_info.components.r = VK_COMPONENT_SWIZZLE_R;
        view_info.components.g = VK_COMPONENT_SWIZZLE_G;
        view_info.components.b = VK_COMPONENT_SWIZZLE_B;
        view_info.components.a = VK_COMPONENT_SWIZZLE_A;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        _underlying = factory.CreateImageView(view_info);

        _layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    ShaderResourceView::ShaderResourceView(VkImage image, NativeFormat::Enum format)
    : ShaderResourceView(GetObjectFactory(), image, format)
    {}

    ShaderResourceView::ShaderResourceView(UnderlyingResourcePtr res, NativeFormat::Enum)
    { 
        _layout = VK_IMAGE_LAYOUT_UNDEFINED; 
    }

    ShaderResourceView::ShaderResourceView() { _layout = VK_IMAGE_LAYOUT_UNDEFINED; }
    ShaderResourceView::~ShaderResourceView() {}


    const SamplerState&     ShaderResourceView::GetSampler() const
    {
        static SamplerState result;
        return result;
    }

}}

