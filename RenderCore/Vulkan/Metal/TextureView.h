// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Resource.h"
#include "VulkanCore.h"

namespace RenderCore { class Resource; enum class Format; }

namespace RenderCore { namespace Metal_Vulkan
{
    class ObjectFactory;
    class SamplerState;

    class TextureViewWindow
    {
    public:
        struct SubResourceRange { unsigned _min; unsigned _count; };
        static const unsigned Unlimited = ~0x0u;
        static const SubResourceRange All;

		struct Flags {
			enum Bits { AttachedCounter = 1<<0, AppendBuffer = 1<<1, ForceArray = 1<<2 };
			using BitField = unsigned;
		};

        Format                      _format;
        SubResourceRange            _mipRange;
        SubResourceRange            _arrayLayerRange;
        TextureDesc::Dimensionality _dimensionality;
		Flags::BitField				_flags;

        TextureViewWindow(
            Format format = Format(0),
            TextureDesc::Dimensionality dimensionality = TextureDesc::Dimensionality::Undefined,
            SubResourceRange mipRange = All,
            SubResourceRange arrayLayerRange = All,
			Flags::BitField flags = 0
            ) : _format(format), _dimensionality(dimensionality), _mipRange(mipRange), _arrayLayerRange(arrayLayerRange), _flags(flags) {}
    };

	/// <summary>Shared base class for texture view objects</summary>
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
		TextureView(const ObjectFactory& factory, const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow());
        explicit TextureView(const VkImage image, const TextureViewWindow& window = TextureViewWindow());
		explicit TextureView(const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow());
        TextureView();
        ~TextureView();

        using UnderlyingType = VkImageView;
		UnderlyingType			GetUnderlying() const { return _imageView.get(); }
		bool					IsGood() const { return _imageView != nullptr; }

		RenderCore::Resource*	GetResource() const { return _image.get(); }
		const ResourcePtr&		ShareResource() const { return _image; }

    private:
        VulkanSharedPtr<VkImageView>	_imageView;
		ResourcePtr						_image;
    };

    // note -- in Vulkan, ShaderResourceView, RenderTargetView, DepthStencilView and UnorderedAccessView
    // are the same. But we can use "using" statements, because we still use the types for matching with
    // Bind() functions

    class ShaderResourceView : public TextureView
    {
    public:
		// using TextureView::TextureView;

        ShaderResourceView(const ObjectFactory& factory, VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window) {}
		ShaderResourceView(const ObjectFactory& factory, const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window) {}
		explicit ShaderResourceView(VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
		explicit ShaderResourceView(const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
        ShaderResourceView() {}

		const SamplerState&     GetSampler() const;
		static void Cleanup();
    };

    class RenderTargetView : public TextureView
    {
    public:
		// using TextureView::TextureView;

        RenderTargetView(const ObjectFactory& factory, VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window) {}
		RenderTargetView(const ObjectFactory& factory, const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window) {}
		explicit RenderTargetView(VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
		explicit RenderTargetView(const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
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
            : TextureView(factory, image, window) {}
		explicit DepthStencilView(VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
		explicit DepthStencilView(const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
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
			: TextureView(factory, image, window) {}
		explicit UnorderedAccessView(VkImage image, const TextureViewWindow& window = TextureViewWindow())
			: TextureView(image, window) {}
		explicit UnorderedAccessView(const ResourcePtr& image, const TextureViewWindow& window = TextureViewWindow())
			: TextureView(image, window) {}
		UnorderedAccessView() {}
    };
}}

