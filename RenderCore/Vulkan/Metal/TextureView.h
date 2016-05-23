// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Resource.h"
#include "VulkanCore.h"
#include "../../Types_Forward.h"

namespace RenderCore { namespace Metal_Vulkan
{
    class ObjectFactory;

	/// <summary>Shared base class for various view objects</summary>
	/// In Vulkan, views of shader resources can be either VkImageViews or VkBufferViews.
	/// Both types represent an array of texels with a given format. VkBufferViews represent
	/// single dimensional linear array. VkImageViews arrange the texels in much more complex
	/// fashion (eg, 2D and 3D textures, with mipchains, etc).
	///
	/// Only VkImageViews can be used with render passes and frame buffers. But both image views
	/// and buffer views can be used as shader resources.
    class TextureView
    {
    public:
		using ResourcePtr = std::shared_ptr<RenderCore::Resource>;

        TextureView(const ObjectFactory& factory, VkImage image, const TextureViewWindow& window = TextureViewWindow());
		TextureView(const ObjectFactory& factory, const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow(), FormatUsage usage = FormatUsage::SRV);
        explicit TextureView(const VkImage image, const TextureViewWindow& window = TextureViewWindow());
		explicit TextureView(const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow(), FormatUsage usage = FormatUsage::SRV);
        TextureView();
        ~TextureView();

        using UnderlyingType = const TextureView*;
		UnderlyingType			GetUnderlying() const { return this; }
		bool					IsGood() const { return _imageView != nullptr; }

		RenderCore::Resource*	GetResource() const { return _image.get(); }
		const ResourcePtr&		ShareResource() const { return _image; }
        VkImageView             GetImageView() const { return _imageView.get(); }

    private:
        VulkanSharedPtr<VkImageView>	_imageView;
		ResourcePtr						_image;
    };

    // note -- in Vulkan, ShaderResourceView, RenderTargetView, DepthStencilView and UnorderedAccessView
    // are the same. But we can't just use "using" statements, because we need separate types to distinquish
    // between the variations of the Bind() functions

    class ShaderResourceView : public TextureView
    {
    public:
		// using TextureView::TextureView;

        ShaderResourceView(const ObjectFactory& factory, VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window) {}
		ShaderResourceView(const ObjectFactory& factory, const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window, FormatUsage::SRV) {}
		explicit ShaderResourceView(VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
		explicit ShaderResourceView(const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window, FormatUsage::SRV) {}
        ShaderResourceView() {}
    };

    class RenderTargetView : public TextureView
    {
    public:
		// using TextureView::TextureView;

        RenderTargetView(const ObjectFactory& factory, VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window) {}
		RenderTargetView(const ObjectFactory& factory, const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window, FormatUsage::RTV) {}
		explicit RenderTargetView(VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
		explicit RenderTargetView(const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window, FormatUsage::RTV) {}
		RenderTargetView(DeviceContext&) {}
        RenderTargetView() {}
    };

    class DepthStencilView : public TextureView
    {
    public:
		// using TextureView::TextureView;

        DepthStencilView(const ObjectFactory& factory, VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window) {}
		DepthStencilView(const ObjectFactory& factory, const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window, FormatUsage::DSV) {}
		explicit DepthStencilView(VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
		explicit DepthStencilView(const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window, FormatUsage::DSV) {}
		DepthStencilView(DeviceContext&) {}
		DepthStencilView() {}
    };

    class UnorderedAccessView : public TextureView
    {
    public:
		// using TextureView::TextureView;

		UnorderedAccessView(const ObjectFactory& factory, VkImage image, const TextureViewWindow& window = TextureViewWindow())
			: TextureView(factory, image, window) {}
		UnorderedAccessView(const ObjectFactory& factory, const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow())
			: TextureView(factory, image, window, FormatUsage::UAV) {}
		explicit UnorderedAccessView(VkImage image, const TextureViewWindow& window = TextureViewWindow())
			: TextureView(image, window) {}
		explicit UnorderedAccessView(const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow())
			: TextureView(image, window, FormatUsage::UAV) {}
		UnorderedAccessView() {}
    };
}}

