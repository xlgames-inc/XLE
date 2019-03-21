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

    class FrameBuffer
	{
	public:
		VkFramebuffer GetUnderlying() const { return _underlying.get(); }
        VkRenderPass GetLayout() const { return _layout.get(); }

		FrameBuffer(
			const ObjectFactory& factory,
            const FrameBufferDesc& desc,
            const INamedAttachments& namedResources);
		FrameBuffer();
		~FrameBuffer();
	private:
		VulkanSharedPtr<VkFramebuffer> _underlying;
        VulkanSharedPtr<VkRenderPass> _layout;
	};

    void BeginRenderPass(
        DeviceContext& context,
        FrameBuffer& frameBuffer,
        const FrameBufferDesc& layout,
        const FrameBufferProperties& props,
        IteratorRange<const ClearValue*> clearValues);

    void BeginNextSubpass(DeviceContext& context, FrameBuffer& frameBuffer);
	void EndSubpass(DeviceContext& context);
    void EndRenderPass(DeviceContext& context);
	unsigned GetCurrentSubpassIndex(DeviceContext& context);

}}