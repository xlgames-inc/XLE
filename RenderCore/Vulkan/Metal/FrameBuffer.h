// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "../../Types_Forward.h"
#include "../../FrameBufferDesc.h"
#include <memory>

namespace RenderCore { namespace Metal_Vulkan
{
    class ObjectFactory;
    class RenderTargetView;
    class ShaderResourceView;
    class DepthStencilView;
    class TextureView;
    class DeviceContext;
    
    class INamedResources
    {
    public:
        virtual auto GetSRV(AttachmentName viewName, AttachmentName resName = ~0u, const TextureViewWindow& window = TextureViewWindow()) const -> ShaderResourceView* = 0;
        virtual auto GetRTV(AttachmentName viewName, AttachmentName resName = ~0u, const TextureViewWindow& window = TextureViewWindow()) const -> RenderTargetView* = 0;
        virtual auto GetDSV(AttachmentName viewName, AttachmentName resName = ~0u, const TextureViewWindow& window = TextureViewWindow()) const -> DepthStencilView* = 0;
        ~INamedResources();
    };

    class FrameBuffer
	{
	public:
		VkFramebuffer GetUnderlying() const { return _underlying.get(); }
        VkRenderPass GetLayout() const { return _layout; }

		FrameBuffer(
			const ObjectFactory& factory,
            const FrameBufferDesc& desc,
            const FrameBufferProperties& props,
            VkRenderPass layout,
            const INamedResources& namedResources);
		FrameBuffer();
		~FrameBuffer();
	private:
		VulkanSharedPtr<VkFramebuffer> _underlying;
        VkRenderPass _layout;
	};

    /// <summary>Stores a set of retained frame buffers, which can be reused frame-to-frame</summary>
    /// Client code typically just wants to define the size and formats of frame buffers, without
    /// manually retaining and managing the objects themselves. It's a result of typical usage patterns
    /// of RenderPassInstance.
    ///
    /// This helper class allows client code to simply declare what it needs and the actual management 
    /// of the device objects will be handled within the cache.
    class FrameBufferCache
    {
    public:
        std::shared_ptr<FrameBuffer> BuildFrameBuffer(
			const ObjectFactory& factory,
            const FrameBufferDesc& desc,
            const FrameBufferProperties& props,
            IteratorRange<const AttachmentDesc*> attachmentResources,
            const INamedResources& namedResources,
            uint64 hashName);

        VkRenderPass BuildFrameBufferLayout(
            const ObjectFactory& factory,
            const FrameBufferDesc& desc,
            IteratorRange<const AttachmentDesc*> attachmentResources,
            const TextureSamples& samples);

        FrameBufferCache();
        ~FrameBufferCache();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    void BeginRenderPass(
        DeviceContext& context,
        FrameBuffer& frameBuffer,
        const FrameBufferDesc& layout,
        const FrameBufferProperties& props,
        IteratorRange<const ClearValue*> clearValues);

    void BeginNextSubpass(DeviceContext& context, FrameBuffer& frameBuffer);
    void EndRenderPass(DeviceContext& context);
}}