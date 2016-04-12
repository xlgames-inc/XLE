// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "IncludeVulkan.h"
#include "../../ResourceDesc.h"
#include "../../../Utility/IteratorUtils.h"

namespace RenderCore { enum class Format; }
namespace RenderCore { namespace Metal_Vulkan
{
    class ObjectFactory;

    class FrameBufferLayout
	{
	public:
		enum class PreviousState { DontCare, Clear };

		class TargetInfo
		{
		public:
			Format          _format;
			TextureSamples  _samples;
			PreviousState   _previousState;

			TargetInfo(
				Format fmt = Format(0), const TextureSamples& samples = TextureSamples::Create(),
				PreviousState previousState = PreviousState::DontCare)
				: _format(fmt), _samples(samples), _previousState() {}
		};

		VkRenderPass GetUnderlying() { return _underlying.get(); }
        const VulkanSharedPtr<VkRenderPass>& ShareUnderlying() { return _underlying; }

		FrameBufferLayout(
			const ObjectFactory& factory,
			IteratorRange<TargetInfo*> rtvAttachments,
			TargetInfo dsvAttachment = TargetInfo());
		FrameBufferLayout();
		~FrameBufferLayout();

	private:
		VulkanSharedPtr<VkRenderPass> _underlying;
	};

    class FrameBuffer
	{
	public:
		VkFramebuffer GetUnderlying() { return _underlying.get(); }

		FrameBuffer(
			const Metal_Vulkan::ObjectFactory& factory,
			IteratorRange<VkImageView*> views,
			FrameBufferLayout& layout,
			unsigned width, unsigned height);
		FrameBuffer();
		~FrameBuffer();
	private:
		VulkanSharedPtr<VkFramebuffer> _underlying;
	};
}}