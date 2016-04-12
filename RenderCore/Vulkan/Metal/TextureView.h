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

        Format                      _format;
        SubResourceRange            _mipRange;
        SubResourceRange            _arrayLayerRange;
        TextureDesc::Dimensionality _dimensionality;

        TextureViewWindow(
            Format format = Format(0),
            TextureDesc::Dimensionality dimensionality = TextureDesc::Dimensionality::Undefined,
            SubResourceRange mipRange = All,
            SubResourceRange arrayLayerRange = All
            ) : _format(format), _dimensionality(dimensionality), _mipRange(mipRange), _arrayLayerRange(arrayLayerRange) {}
    };

    class TextureView
    {
    public:
        TextureView(const ObjectFactory& factory, VkImage image, const TextureViewWindow& window = TextureViewWindow());
		TextureView(const ObjectFactory& factory, UnderlyingResourcePtr image, const TextureViewWindow& window = TextureViewWindow());
        TextureView(VkImage image, const TextureViewWindow& window = TextureViewWindow());
		TextureView(UnderlyingResourcePtr image, const TextureViewWindow& window = TextureViewWindow());
        TextureView();
        ~TextureView();

        using UnderlyingType = VkImageView;
		UnderlyingType      GetUnderlying() const { return _underlying.get(); }
		bool                IsGood() const { return _underlying != nullptr; }

        VkImageLayout _layout;

    private:
        VulkanSharedPtr<VkImageView> _underlying;
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
		ShaderResourceView(const ObjectFactory& factory, UnderlyingResourcePtr image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window) {}
        ShaderResourceView(VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
		ShaderResourceView(UnderlyingResourcePtr image, const TextureViewWindow& window = TextureViewWindow())
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
		RenderTargetView(const ObjectFactory& factory, UnderlyingResourcePtr image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window) {}
        RenderTargetView(VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
		RenderTargetView(UnderlyingResourcePtr image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
        RenderTargetView() {}
    };

    class DepthStencilView : public TextureView
    {
    public:
		// using TextureView::TextureView;

        DepthStencilView(const ObjectFactory& factory, VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window) {}
		DepthStencilView(const ObjectFactory& factory, UnderlyingResourcePtr image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window) {}
        DepthStencilView(VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
		DepthStencilView(UnderlyingResourcePtr image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
        DepthStencilView() {}
    };

    class UnorderedAccessView : public TextureView
    {
    public:
		// using TextureView::TextureView;

        UnorderedAccessView(const ObjectFactory& factory, VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window) {}
		UnorderedAccessView(const ObjectFactory& factory, UnderlyingResourcePtr image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(factory, image, window) {}
        UnorderedAccessView(VkImage image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
		UnorderedAccessView(UnderlyingResourcePtr image, const TextureViewWindow& window = TextureViewWindow())
            : TextureView(image, window) {}
        UnorderedAccessView() {}
    };
}}

