// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TextureView.h"
#include "ObjectFactory.h"
#include "State.h"
#include "Format.h"

namespace RenderCore { namespace Metal_Vulkan
{
    const TextureViewWindow::SubResourceRange TextureViewWindow::All = SubResourceRange{0, Unlimited};

    static VkImageViewType AsImageViewType(TextureDesc::Dimensionality dims, bool isArray)
    {
        switch (dims) {
        case TextureDesc::Dimensionality::T1D:      return isArray?VK_IMAGE_VIEW_TYPE_1D_ARRAY:VK_IMAGE_VIEW_TYPE_1D;
        case TextureDesc::Dimensionality::T2D:      return isArray?VK_IMAGE_VIEW_TYPE_2D_ARRAY:VK_IMAGE_VIEW_TYPE_2D;
        case TextureDesc::Dimensionality::T3D:      return VK_IMAGE_VIEW_TYPE_3D;
        case TextureDesc::Dimensionality::CubeMap:  return isArray?VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:VK_IMAGE_VIEW_TYPE_CUBE;
        default:                                    return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        }
    }

    static VkImageViewCreateInfo MakeCreateInfo(TextureViewWindow window, VkImage image, bool isArray)
    {
        // Note that the arrayCount value is sometimes set to 1 when we want 
        // an array texture with a single array slice (as opposed to 0, meaning no array at all).
        // Current single array slice views become non-array views... But we could make "1" mean
        // an array view.
        isArray &= window._arrayLayerRange._count > 1;

        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.pNext = nullptr;
        view_info.image = image;
        view_info.viewType = AsImageViewType(window._dimensionality, isArray);
        view_info.format = AsVkFormat(window._format);
        view_info.components.r = VK_COMPONENT_SWIZZLE_R;
        view_info.components.g = VK_COMPONENT_SWIZZLE_G;
        view_info.components.b = VK_COMPONENT_SWIZZLE_B;
        view_info.components.a = VK_COMPONENT_SWIZZLE_A;
        view_info.subresourceRange.aspectMask = AsImageAspectMask(window._format);
        view_info.subresourceRange.baseMipLevel = window._mipRange._min;
        view_info.subresourceRange.levelCount = std::max(1u, (unsigned)window._mipRange._count);
        view_info.subresourceRange.baseArrayLayer = window._arrayLayerRange._min;
        view_info.subresourceRange.layerCount = std::max(1u, (unsigned)window._arrayLayerRange._count);
        return view_info;
    }

    TextureView::TextureView(const ObjectFactory& factory, VkImage image, const TextureViewWindow& window)
    {
        // We don't know anything about the "image" in this case. We need to rely on "image" containing all
        // of the relevant information.
        auto createInfo = MakeCreateInfo(window, image, true);
        _imageView = factory.CreateImageView(createInfo);
    }

	TextureView::TextureView(const ObjectFactory& factory, const ResourcePtr& image, const TextureViewWindow& window)
	{
		auto res = UnderlyingResourcePtr(image).get();
		// note --	some "buffer" objects can be used as ShaderResources... In those cases, we will arrive here,
		//			and the calling code is probably expecting use to create a VkBufferView
		if (res->GetImage()) {
			assert(res->GetDesc()._type == ResourceDesc::Type::Texture);
			assert(res->GetImage());
			const auto& tDesc = res->GetDesc()._textureDesc;
			auto adjWindow = window;

			// Some parts of the "TextureViewWindow" can be set to "undefined". In these cases,
			// we should fill them in with the detail from the resource.
			if (adjWindow._format == Format(0)) adjWindow._format = tDesc._format;
			if (adjWindow._dimensionality == TextureDesc::Dimensionality::Undefined)
				adjWindow._dimensionality = tDesc._dimensionality;
			if (adjWindow._mipRange._count == TextureViewWindow::Unlimited)
				adjWindow._mipRange._count = tDesc._mipCount - adjWindow._mipRange._min;
			if (adjWindow._arrayLayerRange._count == TextureViewWindow::Unlimited)
				adjWindow._arrayLayerRange._count = tDesc._arrayCount - adjWindow._arrayLayerRange._min;

			auto createInfo = MakeCreateInfo(adjWindow, res->GetImage(), true);
			_imageView = factory.CreateImageView(createInfo);
		}

        // keep a pointer to the "image" even if we couldn't construct a VkImageView
        _image = image;
	}

    TextureView::TextureView(VkImage image, const TextureViewWindow& window)
    : TextureView(GetObjectFactory(), image, window)
    {}

    TextureView::TextureView(const ResourcePtr& image, const TextureViewWindow& window)
    : TextureView(GetObjectFactory(), image, window)
    { 
    }

    TextureView::TextureView() {}
    TextureView::~TextureView() {}

}}

