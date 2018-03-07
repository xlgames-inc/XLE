// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FrameBuffer.h"
#include "DeviceContext.h"

#include "IncludeAppleMetal.h"

namespace RenderCore { namespace Metal_AppleMetal
{
    void FrameBuffer::BindSubpass(DeviceContext& context, unsigned subpassIndex, IteratorRange<const ClearValue*> clearValues) const
    {
        /* KenD -- Metal TODO -- modify attachment clear colors */

        /* KenD -- each subpass of the frame will have a RenderCommandEncoder with a different render pass descriptor. */
        context.CreateRenderCommandEncoder(_subpasses[subpassIndex]._renderPassDescriptor.get());
    }

    MTLLoadAction NonStencilLoadActionFromRenderCore(RenderCore::LoadStore load)
    {
        switch (load) {
            case RenderCore::LoadStore::DontCare:
            case RenderCore::LoadStore::DontCare_RetainStencil:
            case RenderCore::LoadStore::DontCare_ClearStencil:
                return MTLLoadActionDontCare;
            case RenderCore::LoadStore::Retain:
            case RenderCore::LoadStore::Retain_RetainStencil:
            case RenderCore::LoadStore::Retain_ClearStencil:
                return MTLLoadActionLoad;
            case RenderCore::LoadStore::Clear:
            case RenderCore::LoadStore::Clear_RetainStencil:
            case RenderCore::LoadStore::Clear_ClearStencil:
                return MTLLoadActionClear;
        }
    }

    MTLStoreAction NonStencilStoreActionFromRenderCore(RenderCore::LoadStore store)
    {
        switch (store) {
            case RenderCore::LoadStore::Retain:
            case RenderCore::LoadStore::Retain_RetainStencil:
            case RenderCore::LoadStore::Retain_ClearStencil:
                return MTLStoreActionStore;
            default:
                return MTLStoreActionDontCare;
        }
    }

    MTLLoadAction StencilLoadActionFromRenderCore(RenderCore::LoadStore load)
    {
        switch (load) {
            case RenderCore::LoadStore::Retain:
            case RenderCore::LoadStore::DontCare_RetainStencil:
            case RenderCore::LoadStore::Retain_RetainStencil:
            case RenderCore::LoadStore::Clear_RetainStencil:
                return MTLLoadActionLoad;
            case RenderCore::LoadStore::Clear:
            case RenderCore::LoadStore::DontCare_ClearStencil:
            case RenderCore::LoadStore::Retain_ClearStencil:
            case RenderCore::LoadStore::Clear_ClearStencil:
                return MTLLoadActionClear;
            case RenderCore::LoadStore::DontCare:
                return MTLLoadActionDontCare;
        }
    }

    MTLStoreAction StencilStoreActionFromRenderCore(RenderCore::LoadStore store)
    {
        switch (store) {
            case RenderCore::LoadStore::Retain:
            case RenderCore::LoadStore::DontCare_RetainStencil:
            case RenderCore::LoadStore::Retain_RetainStencil:
            case RenderCore::LoadStore::Clear_RetainStencil:
                return MTLStoreActionStore;
            default:
                return MTLStoreActionDontCare;
        }
    }

    static const Resource& AsResource(const IResourcePtr rp)
    {
        static Resource dummy;
        auto* res = (Resource*)rp->QueryInterface(typeid(Resource).hash_code());
        if (res)
            return *res;
        return dummy;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    FrameBuffer::FrameBuffer(
                             ObjectFactory& factory,
                             const FrameBufferDesc& fbDesc,
                             const INamedAttachments& namedResources)
    {
        auto subpasses = fbDesc.GetSubpasses();

        assert(subpasses.size() <= s_maxSubpasses);
        for (unsigned p=0; p<(unsigned)subpasses.size(); ++p) {
            _subpasses[p]._renderPassDescriptor = TBC::OCPtr<MTLRenderPassDescriptor>(TBC::moveptr([[MTLRenderPassDescriptor alloc] init]));
            auto* desc = _subpasses[p]._renderPassDescriptor.get();
            const auto& spDesc = subpasses[p];
            const unsigned maxColorAttachments = 4u;
            assert(spDesc._output.size() <= maxColorAttachments); // MTLRenderPassDescriptor supports up to four color attachments
            auto colorAttachmentsCount = (unsigned)std::min((unsigned)spDesc._output.size(), maxColorAttachments);
            for (unsigned o=0; o<colorAttachmentsCount; ++o) {
                const auto& attachmentView = spDesc._output[o];
                auto resource = namedResources.GetResource(attachmentView._resourceName);
                if (!resource)
                    Throw(::Exceptions::BasicLabel("Could not find attachment texture for color attachment in FrameBuffer::FrameBuffer"));
                // Configure MTLRenderPassColorAttachmentDescriptor
                desc.colorAttachments[o].texture = AsResource(resource).GetTexture();
                desc.colorAttachments[o].loadAction = NonStencilLoadActionFromRenderCore(attachmentView._loadFromPreviousPhase);
                desc.colorAttachments[o].storeAction = NonStencilStoreActionFromRenderCore(attachmentView._storeToNextPhase);
                /* KenD -- Metal TODO -- alter clearColor when binding */
                desc.colorAttachments[o].clearColor = MTLClearColorMake(0.0, 0.2, 0.2, 1.0);
            }

            if (spDesc._depthStencil._resourceName != ~0u) {
                auto resource = namedResources.GetResource(spDesc._depthStencil._resourceName);
                if (!resource)
                    Throw(::Exceptions::BasicLabel("Could not find attachment texture for depth/stencil attachment in FrameBuffer::FrameBuffer"));
                desc.depthAttachment.texture = AsResource(resource).GetTexture();
                desc.depthAttachment.loadAction = NonStencilLoadActionFromRenderCore(spDesc._depthStencil._loadFromPreviousPhase);
                desc.depthAttachment.storeAction = NonStencilStoreActionFromRenderCore(spDesc._depthStencil._storeToNextPhase);
                /* KenD -- Metal TODO -- alter clearDepth when binding */
                desc.depthAttachment.clearDepth = 1.0;

                desc.stencilAttachment.texture = AsResource(resource).GetTexture();
                desc.stencilAttachment.loadAction = StencilLoadActionFromRenderCore(spDesc._depthStencil._loadFromPreviousPhase);
                desc.stencilAttachment.storeAction = StencilStoreActionFromRenderCore(spDesc._depthStencil._storeToNextPhase);
                /* KenD -- Metal TODO -- alter clearStencil when binding */
                desc.stencilAttachment.clearStencil = 0;
            }

            /* KenD -- Metal TODO -- verify load and store actions */
        }
    }

    FrameBuffer::FrameBuffer() {}
    FrameBuffer::~FrameBuffer() {}

    class FrameBufferPool::Pimpl
    {
    };

    std::shared_ptr<FrameBuffer> FrameBufferPool::BuildFrameBuffer(
        ObjectFactory& factory,
        const FrameBufferDesc& desc,
        const FrameBufferProperties& props,
        const INamedAttachments& namedResources,
        uint64 hashName)
    {
        return std::make_shared<FrameBuffer>(factory, desc, namedResources);
    }

    FrameBufferPool::FrameBufferPool()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    FrameBufferPool::~FrameBufferPool()
    {
    }

    static unsigned s_nextSubpass = 0;
    static std::vector<ClearValue> s_clearValues;

    void BeginRenderPass(
        DeviceContext& context,
        FrameBuffer& frameBuffer,
        const FrameBufferDesc& layout,
        const FrameBufferProperties& props,
        IteratorRange<const ClearValue*> clearValues)
    {
        s_nextSubpass = 0;
        s_clearValues.clear();
        s_clearValues.insert(s_clearValues.end(), clearValues.begin(), clearValues.end());
        BeginNextSubpass(context, frameBuffer);
    }

    void BeginNextSubpass(
        DeviceContext& context,
        FrameBuffer& frameBuffer)
    {
        // Queue up the next render targets
        auto subpassIndex = s_nextSubpass;
        frameBuffer.BindSubpass(context, subpassIndex, MakeIteratorRange(s_clearValues));
        ++s_nextSubpass;
    }

    /* KenD -- Implementation note
     * At the end of a subpass, we should end encoding and destroy the encoder.
     * For now, in RenderPassInstance, when beginning a new subpass, we end the current subpass.
     * And, when ending the render pass, we end the subpass.
     *
     * Clients must balance creating and destroying the encoder.
     * They must also end encoding only once per encoder.
     */

    void EndSubpass(DeviceContext& context)
    {
        context.EndEncoding();
        context.DestroyRenderCommandEncoder();
    }

    void EndRenderPass(DeviceContext& context)
    {
        // For compatibility with Vulkan, it makes sense to unbind render targets here. This is important
        // if the render targets will be used as compute shader outputs in follow up steps. It also prevents
        // rendering outside of render passes. But sometimes it will produce redundant calls to OMSetRenderTargets().
    }
}}
