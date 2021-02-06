// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TextureView.h"
#include "../../FrameBufferDesc.h"
#include "../../../Foreign/OCPtr/OCPtr.hpp"

@class MTLRenderPassDescriptor;

namespace RenderCore { namespace Metal_AppleMetal
{
    class ObjectFactory;
    class RenderTargetView;
    class ShaderResourceView;
    class DepthStencilView;
    class DeviceContext;

    class FrameBuffer
    {
    public:
        void BindSubpass(DeviceContext& context, unsigned subpassIndex, IteratorRange<const ClearValue*> clearValues) const;

        /* TODO -- Metal equivalent */ //OpenGL::FrameBuffer* GetSubpassUnderlyingFramebuffer(unsigned subpassIndex);
        unsigned GetSubpassCount() const { return (unsigned)_subpasses.size(); }

        FrameBuffer(
            ObjectFactory& factory,
            const FrameBufferDesc& fbDesc,
            const INamedAttachments& namedResources);
        FrameBuffer();
        ~FrameBuffer();
    private:
        static const unsigned s_maxMRTs = 4u;

        class Subpass
        {
        public:
            TBC::OCPtr<MTLRenderPassDescriptor> _renderPassDescriptor;

#if 0
            /* KenD -- Metal TODO -- clear values - the iterator has to be stored within the subpass;
             * when the subpass begins and is bound, the clear values will be set.
             * Skipping that implementation for now.
             */
            unsigned _rtvClearValue[s_maxMRTs];
            unsigned _dsvClearValue;
#endif

            unsigned _rasterCount;
        };
        std::vector<Subpass> _subpasses;
    };

    void BeginRenderPass(
        DeviceContext& context,
        FrameBuffer& frameBuffer,
        IteratorRange<const ClearValue*> clearValues = {});

    void BeginNextSubpass(DeviceContext& context, FrameBuffer& frameBuffer);
    void EndSubpass(DeviceContext& context, FrameBuffer& frameBuffer);
    void EndRenderPass(DeviceContext& context);
    unsigned GetCurrentSubpassIndex(DeviceContext& context);
}}
