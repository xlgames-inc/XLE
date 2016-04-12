// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FrameBuffer.h"
#include "Format.h"
#include "Resource.h"
#include "ObjectFactory.h"
#include "../../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{
    FrameBufferLayout::FrameBufferLayout(
        const Metal_Vulkan::ObjectFactory& factory,
        IteratorRange<TargetInfo*> rtvAttachments,
        TargetInfo dsvAttachment)
    {
        // The render targets and depth buffers slots are called "attachments"
        // In this case, we will create a render pass with a single subpass.
        // That subpass will reference all buffers.
        // This sets up the slots for rendertargets and depth buffers -- but it
        // doesn't assign the specific images.
      
        bool hasDepthBuffer = dsvAttachment._format != Format(0);

        VkAttachmentDescription attachmentsStatic[8];
        std::vector<VkAttachmentDescription> attachmentsOverflow;
        VkAttachmentReference colorReferencesStatic[8];
        std::vector<VkAttachmentReference> colorReferencesOverflow;

        VkAttachmentDescription* attachments = attachmentsStatic;
        auto attachmentCount = rtvAttachments.size() + unsigned(hasDepthBuffer);
        if (attachmentCount > dimof(attachmentsStatic)) {
            attachmentsOverflow.resize(attachmentCount);
            attachments = AsPointer(attachmentsOverflow.begin());
        }
        XlClearMemory(attachments, sizeof(VkAttachmentDescription) * attachmentCount);

        VkAttachmentReference* colorReferences = colorReferencesStatic;
        if (rtvAttachments.size() > dimof(colorReferencesStatic)) {
            colorReferencesOverflow.resize(rtvAttachments.size());
            colorReferences = AsPointer(colorReferencesOverflow.begin());
        }

        // note -- 
        //      Is it safe to set the initialLayout in the RenderPass to _UNDEFINED
        //      every time? If we're rendering to an image that we've touched before,
        //      does it mean we can start using the same value for initial and final layouts?

        auto* i = attachments;
        for (auto& rtv:rtvAttachments) {
            i->format = AsVkFormat(rtv._format);
            i->samples = AsSampleCountFlagBits(rtv._samples);
            i->loadOp = (rtv._previousState ==  PreviousState::DontCare) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            i->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            i->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            i->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            i->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            i->finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            i->flags = 0;

            colorReferences[i-attachments] = {};
            colorReferences[i-attachments].attachment = uint32_t(i-attachments);
            colorReferences[i-attachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            ++i;
        }

        VkAttachmentReference depth_reference = {};

        if (hasDepthBuffer) {
            i->format = AsVkFormat(dsvAttachment._format);
            i->samples = AsSampleCountFlagBits(dsvAttachment._samples);
            i->loadOp = (dsvAttachment._previousState ==  PreviousState::DontCare) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            i->storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            // note -- retaining stencil values frame to frame
            i->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            i->stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

            i->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            i->finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            i->flags = 0;

            depth_reference.attachment = uint32_t(i - attachments);
            depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            ++i;
        }

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.flags = 0;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = nullptr;
        subpass.colorAttachmentCount = uint32_t(rtvAttachments.size());
        subpass.pColorAttachments = colorReferences;
        subpass.pResolveAttachments = nullptr;
        subpass.pDepthStencilAttachment = hasDepthBuffer ? &depth_reference : nullptr;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = nullptr;

        VkRenderPassCreateInfo rp_info = {};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.pNext = nullptr;
        rp_info.attachmentCount = (uint32_t)attachmentCount;
        rp_info.pAttachments = attachments;
        rp_info.subpassCount = 1;
        rp_info.pSubpasses = &subpass;
        rp_info.dependencyCount = 0;
        rp_info.pDependencies = nullptr;

        _underlying = factory.CreateRenderPass(rp_info);
    }

	FrameBufferLayout::FrameBufferLayout() {}

    FrameBufferLayout::~FrameBufferLayout() {}



    FrameBuffer::FrameBuffer(
        const Metal_Vulkan::ObjectFactory& factory,
        IteratorRange<VkImageView*> views,
        FrameBufferLayout& layout,
        unsigned width, unsigned height)
    {
        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.pNext = nullptr;
        fb_info.renderPass = layout.GetUnderlying();
        fb_info.attachmentCount = (uint32_t)views.size();
        fb_info.pAttachments = views.begin();
        fb_info.width = width;
        fb_info.height = height;
        fb_info.layers = 1;
        _underlying = factory.CreateFramebuffer(fb_info);
    }

	FrameBuffer::FrameBuffer() {}

    FrameBuffer::~FrameBuffer() {}
}}

