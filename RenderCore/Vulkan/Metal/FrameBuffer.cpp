// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FrameBuffer.h"
#include "Format.h"
#include "Resource.h"
#include "TextureView.h"
#include "DeviceContext.h"
#include "ObjectFactory.h"
#include "../../Format.h"
#include "../../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{
    static VkAttachmentLoadOp AsLoadOp(AttachmentDesc::LoadStore loadStore)
    {
        switch (loadStore)
        {
        default:
        case AttachmentDesc::LoadStore::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        case AttachmentDesc::LoadStore::Retain: return VK_ATTACHMENT_LOAD_OP_LOAD;
        case AttachmentDesc::LoadStore::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
        }
    }

    static VkAttachmentStoreOp AsStoreOp(AttachmentDesc::LoadStore loadStore)
    {
        switch (loadStore)
        {
        default:
        case AttachmentDesc::LoadStore::Clear: 
        case AttachmentDesc::LoadStore::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case AttachmentDesc::LoadStore::Retain: return VK_ATTACHMENT_STORE_OP_STORE;
        }
    }

    static bool IsDepthStencilFormat(Format fmt)
    {
        auto comp = GetComponents(fmt);
        return comp == FormatComponents::Depth || comp == FormatComponents::DepthStencil;
    }

    static VkImageLayout AsShaderReadLayout(VkImageLayout layout)
    {
        switch (layout)
        {
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case VK_IMAGE_LAYOUT_GENERAL:
            return VK_IMAGE_LAYOUT_GENERAL;
        default:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    }

    FrameBufferLayout::FrameBufferLayout(
        const Metal_Vulkan::ObjectFactory& factory,
        IteratorRange<AttachmentDesc*> attachments,
        IteratorRange<SubpassDesc*> subpasses,
        Format outputFormat, const TextureSamples& samples)
    : _attachments(attachments.begin(), attachments.end())
    , _subpasses(subpasses.begin(), subpasses.end())
    , _samples(samples)
    {
        std::vector<VkAttachmentDescription> attachmentDesc;
        attachmentDesc.reserve(attachments.size());
        for (auto&a:attachments) {
            VkAttachmentDescription desc;
            desc.flags = 0;
            desc.format = AsVkFormat(a._format);
            desc.samples = VK_SAMPLE_COUNT_1_BIT;
            desc.loadOp = AsLoadOp(a._loadFromPreviousPhase);
            desc.storeOp = AsStoreOp(a._storeToNextPhase);
            desc.stencilLoadOp = AsLoadOp(a._stencilLoad);
            desc.stencilStoreOp = AsStoreOp(a._stencilStore);

            desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            bool isDepthStencil = IsDepthStencilFormat(a._format);
            if (isDepthStencil) {
                desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }

            desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            desc.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            if (a._flags & AttachmentDesc::Flags::UsePresentationChainBuffer) {
                assert(!isDepthStencil);
                desc.format = AsVkFormat(outputFormat);
                desc.samples = AsSampleCountFlagBits(samples);
                desc.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            } else if (a._flags & AttachmentDesc::Flags::Multisampled) {
                desc.samples = AsSampleCountFlagBits(samples);
            }

            // note --  do we need to set VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL or 
            //          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL as appropriate for input attachments
            //          (we should be able to tell this from the subpasses)?

            attachmentDesc.push_back(desc);
        }

        std::vector<VkAttachmentReference> attachReferences;
        std::vector<uint32_t> preserveAttachments;

        std::vector<VkSubpassDescription> subpassDesc;
        subpassDesc.reserve(subpasses.size());
        for (auto&p:subpasses) {
            VkSubpassDescription desc;
            desc.flags = 0;
            desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

            // Input attachments are going to be difficult, because they must be bound both
            // by the sub passes and by the descriptor set! (and they must be explicitly listed as
            // input attachments in the shader). Holy cow, the render pass, frame buffer, pipeline
            // layout, descriptor set and shader must all agree!
            auto beforeInputs = attachReferences.size();
            for (auto& a:p._input) {
                // presumably we want to shader the shader read only layout modes in these cases.
                if (a != SubpassDesc::Unused) {
                    assert(a < attachmentDesc.size());
                    auto layout = AsShaderReadLayout(attachmentDesc[a].finalLayout);
                    attachReferences.push_back(VkAttachmentReference{a, layout});
                } else {
                    attachReferences.push_back(VkAttachmentReference{VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED});
                }
            }
            desc.pInputAttachments = (const VkAttachmentReference*)(beforeInputs+1);
            desc.inputAttachmentCount = uint32_t(attachReferences.size() - beforeInputs);

            auto beforeOutputs = attachReferences.size();
            for (auto& a:p._output) {
                if (a != SubpassDesc::Unused) {
                    assert(a < attachmentDesc.size());
                    auto layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;        // basically should be VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL, I guess?
                    attachReferences.push_back(VkAttachmentReference{a, layout});
                } else {
                    attachReferences.push_back(VkAttachmentReference{VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED});
                }
            }
            desc.pColorAttachments = (const VkAttachmentReference*)(beforeOutputs+1);
            desc.colorAttachmentCount = uint32_t(attachReferences.size() - beforeOutputs);
            desc.pResolveAttachments = nullptr; // not supported

            if (p._depthStencil != SubpassDesc::Unused) {
                desc.pDepthStencilAttachment = (const VkAttachmentReference*)(attachReferences.size()+1);
                attachReferences.push_back(VkAttachmentReference{p._depthStencil, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
            } else {
                desc.pDepthStencilAttachment = nullptr;
            }

            auto beforePreserve = preserveAttachments.size();
            for (auto&a:p._preserve) {
                assert(a < attachmentDesc.size());
                preserveAttachments.push_back(a);
            }
            desc.pPreserveAttachments = (const uint32_t*)(beforePreserve+1);
            desc.preserveAttachmentCount = uint32_t(preserveAttachments.size() - beforePreserve);
            subpassDesc.push_back(desc);
        }

        // we need to do a fixup pass over all of the subpasses to generate correct pointers
        for (auto&p:subpassDesc) {
            if (p.pInputAttachments)
                p.pInputAttachments = AsPointer(attachReferences.begin()) + size_t(p.pInputAttachments)-1;
            if (p.pColorAttachments)
                p.pColorAttachments = AsPointer(attachReferences.begin()) + size_t(p.pColorAttachments)-1;
            if (p.pResolveAttachments)
                p.pResolveAttachments = AsPointer(attachReferences.begin()) + size_t(p.pResolveAttachments)-1;
            if (p.pDepthStencilAttachment)
                p.pDepthStencilAttachment = AsPointer(attachReferences.begin()) + size_t(p.pDepthStencilAttachment)-1;
            if (p.pPreserveAttachments)
                p.pPreserveAttachments = AsPointer(preserveAttachments.begin()) + size_t(p.pPreserveAttachments)-1;
        }

        VkRenderPassCreateInfo rp_info = {};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.pNext = nullptr;
        rp_info.attachmentCount = (uint32_t)attachmentDesc.size();
        rp_info.pAttachments = AsPointer(attachmentDesc.begin());
        rp_info.subpassCount = (uint32_t)subpassDesc.size();
        rp_info.pSubpasses = AsPointer(subpassDesc.begin());
        rp_info.dependencyCount = 0;
        rp_info.pDependencies = nullptr;

        _underlying = factory.CreateRenderPass(rp_info);
    }

	FrameBufferLayout::FrameBufferLayout() {}

    FrameBufferLayout::~FrameBufferLayout() {}


    namespace Internal
    {
        enum class AttachmentUsage : unsigned
        {
            Input = 1<<0, Output = 1<<1, DepthStencil = 1<<2
        };
    }
    static unsigned GetAttachmentUsage(const FrameBufferLayout& layout, unsigned attachmentIndex)
    {
        unsigned result = 0u;
        for (auto& s:layout.GetSubpasses()) {
            auto i = std::find(s._input.begin(), s._input.end(), attachmentIndex);
            if (i != s._input.end()) 
                result |= unsigned(Internal::AttachmentUsage::Input);

            auto o = std::find(s._output.begin(), s._output.end(), attachmentIndex);
            if (o != s._input.end()) 
                result |= unsigned(Internal::AttachmentUsage::Output);

            if (s._depthStencil == attachmentIndex)
                result |= unsigned(Internal::AttachmentUsage::DepthStencil);
        }
        return result;
    }

    FrameBuffer::FrameBuffer(
        const Metal_Vulkan::ObjectFactory& factory,
		const FrameBufferLayout& layout,
		const FrameBufferProperties& props,
        const RenderTargetView* presentationChainTarget)
    {
        // We must create the frame buffer, including all resources and views required.
        // Here, some resources can come from the presentation chain. But other resources will
        // be created an attached to this object.
        auto attachments = layout.GetAttachments();
        _views.reserve(attachments.size());
        std::vector<VkImageView> rawViews;
        rawViews.reserve(attachments.size());
        for (unsigned aIndex = 0; aIndex < attachments.size(); ++aIndex) {
            const auto& a = attachments[aIndex];

            // Often, we will end up binding the presentation chain image as one our our attachments
            if (a._flags & AttachmentDesc::Flags::UsePresentationChainBuffer) {
                if (!presentationChainTarget)
                    Throw(::Exceptions::BasicLabel("Frame buffer layout expects a presentation chain target, but none was provided"));
                rawViews.push_back(presentationChainTarget->GetUnderlying());
                _views.push_back(*presentationChainTarget);
                continue;
            }

            // We need to calculate the dimensions, format, samples and bind flags for this
            // attachment. All of the information we need should be defined as part of the frame
            // buffer layout description.

            // note -- how do the frame buffer dimensions relate to the actual image dimensions?
            //          the documentation suggest that the frame buffer dims should always be equal
            //          or smaller to the image views...?
            unsigned attachmentWidth, attachmentHeight;
            if (a._dimsMode == AttachmentDesc::DimensionsMode::Absolute) {
                attachmentWidth = unsigned(a._width);
                attachmentHeight = unsigned(a._height);
            } else {
                attachmentWidth = unsigned(std::floor(props._outputWidth * a._width + 0.5f));
                attachmentHeight = unsigned(std::floor(props._outputHeight * a._height + 0.5f));
            }

            auto desc = CreateDesc(
                0, 0, 0, 
                TextureDesc::Plain2D(attachmentWidth, attachmentHeight, a._format, 1, uint16(props._outputLayers)),
                "attachment");

            if (a._flags & AttachmentDesc::Flags::Multisampled)
                desc._textureDesc._samples = layout.GetSamples();

            // Look at how the attachment is used by the subpasses to figure out what the
            // bind flags should be.

            // todo --  Do we also need to consider what happens to the image after 
            //          the render pass has finished? Resources that are in "output", 
            //          "depthStencil", or "preserve" in the final subpass could be used
            //          in some other way afterwards. For example, one render pass could
            //          generate shadow textures for uses in future render passes?
            auto usage = GetAttachmentUsage(layout, aIndex);
            if (usage & unsigned(Internal::AttachmentUsage::Input)
                || a._flags & AttachmentDesc::Flags::ShaderResource) {
                desc._bindFlags |= BindFlag::ShaderResource;
                desc._gpuAccess |= GPUAccess::Read;
            }

            if (usage & unsigned(Internal::AttachmentUsage::Output)) {
                desc._bindFlags |= BindFlag::RenderTarget;
                desc._gpuAccess |= GPUAccess::Write;
            }

            if (usage & unsigned(Internal::AttachmentUsage::DepthStencil)) {
                desc._bindFlags |= BindFlag::DepthStencil;
                desc._gpuAccess |= GPUAccess::Write;
            }

            if (a._flags & AttachmentDesc::Flags::TransferSource) {
                desc._bindFlags |= BindFlag::TransferSrc;
                desc._gpuAccess |= GPUAccess::Read;
            }

            // note -- it might be handy to have a cache of "device memory" that could be reused here?
            auto image = Resource::Allocate(factory, desc);
            TextureView view(factory, image);
            rawViews.push_back(view.GetUnderlying());
            _views.emplace_back(std::move(view));
        }

        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.pNext = nullptr;
        fb_info.renderPass = layout.GetUnderlying();
        fb_info.attachmentCount = (uint32_t)rawViews.size();
        fb_info.pAttachments = AsPointer(rawViews.begin());
        fb_info.width = props._outputWidth;
        fb_info.height = props._outputHeight;
        fb_info.layers = std::max(1u, props._outputLayers);
        _underlying = factory.CreateFramebuffer(fb_info);

        // todo --  do we need to create a "patch up" command buffer to assign the starting image layouts
        //          for all of the images we created?
    }

    const TextureView& FrameBuffer::GetAttachment(unsigned index) const
    {
        if (index >= _views.size())
            Throw(::Exceptions::BasicLabel("Invalid attachment index passed to FrameBuffer::GetAttachment()"));
        return _views[index];
    }

	FrameBuffer::FrameBuffer() {}

    FrameBuffer::~FrameBuffer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void            RenderPassInstance::End()
    {
        if (_attachedContext)
            _attachedContext->EndRenderPass();
    }

    const TextureView&  RenderPassInstance::GetAttachment(unsigned index)
    {
        // We can call this function during the render pass... However normally if we 
        // want to use a render target, we should do it after the render pass has been
        // ended (with RenderPassInstance::End())
        return _frameBuffer->GetAttachment(index);
    }

    RenderPassInstance::RenderPassInstance(
        DeviceContext& context,
        const FrameBufferLayout& layout,
		const FrameBufferProperties& props,
        uint64 hashName,
        FrameBufferCache& cache,
        const BeginInfo& beginInfo)
    {
        // We need to allocate the particular frame buffer we're going to use
        // And then we'll call BeginRenderPass to begin the render pass
        _frameBuffer = cache.BuildFrameBuffer(context.GetFactory(), layout, props, context.GetPresentationTarget(), hashName);
        assert(_frameBuffer);
        auto ext = beginInfo._extent;
        if (ext[0] == 0 && ext[1] == 0) {
            ext[0] = props._outputWidth - beginInfo._offset[0];
            ext[1] = props._outputHeight - beginInfo._offset[1];
        }
        context.BeginRenderPass(layout, *_frameBuffer, beginInfo._offset, ext, beginInfo._clearValues);
        _attachedContext = &context;
    }
    
    RenderPassInstance::~RenderPassInstance() 
    {
        End();
    }
    
///////////////////////////////////////////////////////////////////////////////////////////////////

    class FrameBufferCache::Pimpl {};

    std::shared_ptr<FrameBuffer> FrameBufferCache::BuildFrameBuffer(
		const ObjectFactory& factory,
		const FrameBufferLayout& layout,
		const FrameBufferProperties& props,
        const RenderTargetView* presentationChainTarget,
        uint64 hashName)
    {
        return std::make_shared<FrameBuffer>(factory, layout, props, presentationChainTarget);
    }

    FrameBufferCache::FrameBufferCache()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    FrameBufferCache::~FrameBufferCache()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    SubpassDesc::SubpassDesc()
    : _depthStencil(Unused)
    {
    }

    SubpassDesc::SubpassDesc(
        std::initializer_list<unsigned> input, 
        std::initializer_list<unsigned> output,
        unsigned depthStencil,
        std::initializer_list<unsigned> preserve)
    : _input(input.begin(), input.end())
    , _output(output.begin(), output.end())
    , _depthStencil(depthStencil)
    , _preserve(preserve.begin(), preserve.end())
    {
    }


}}

