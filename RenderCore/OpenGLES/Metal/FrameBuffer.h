// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TextureView.h"
#include "../../FrameBufferDesc.h"
#include "../../Types_Forward.h"
#include <memory>

namespace RenderCore { namespace Metal_OpenGLES
{
    class ObjectFactory;
    class RenderTargetView;
    class ShaderResourceView;
    class DepthStencilView;
    class DeviceContext;

    class INamedAttachments
    {
    public:
        virtual auto GetSRV(AttachmentName viewName, AttachmentName resName = ~0u, const TextureViewDesc& window = TextureViewDesc()) const -> const ShaderResourceView* = 0;
        virtual auto GetRTV(AttachmentName viewName, AttachmentName resName = ~0u, const TextureViewDesc& window = TextureViewDesc()) const -> const RenderTargetView* = 0;
        virtual auto GetDSV(AttachmentName viewName, AttachmentName resName = ~0u, const TextureViewDesc& window = TextureViewDesc()) const -> const DepthStencilView* = 0;
    };

    class FrameBuffer
	{
	public:
        void BindSubpass(DeviceContext& context, unsigned subpassIndex, IteratorRange<const ClearValue*> clearValues) const;

        RenderTargetView& GetRTV(unsigned index);
        DepthStencilView& GetDSV(unsigned index);

		FrameBuffer(
			ObjectFactory& factory,
            const FrameBufferDesc& desc,
            const INamedAttachments& namedResources);
		FrameBuffer();
		~FrameBuffer();
	private:
        static const unsigned s_maxMRTs = 4u;
        static const unsigned s_maxSubpasses = 4u;
        static const unsigned s_maxAttachments = 16u;
        
        RenderTargetView    _rtvs[s_maxAttachments];
        DepthStencilView    _dsvs[s_maxAttachments];
        unsigned            _attachmentCount;
        
        class Subpass
        {
        public:
            unsigned _rtvs[s_maxMRTs];
            unsigned _dsv;
            unsigned _rtvCount;

            AttachmentViewDesc::LoadStore _rtvLoad[s_maxMRTs];
            AttachmentViewDesc::LoadStore _rtvStore[s_maxMRTs];
            AttachmentViewDesc::LoadStore _dsvLoad;
            AttachmentViewDesc::LoadStore _dsvStore;

            intrusive_ptr<OpenGL::FrameBuffer> _frameBuffer;
        };
        Subpass     _subpasses[s_maxSubpasses];
        unsigned    _subpassCount;
	};

    /// <summary>Stores a set of retained frame buffers, which can be reused frame-to-frame</summary>
    /// Client code typically just wants to define the size and formats of frame buffers, without
    /// manually retaining and managing the objects themselves. It's a result of typical usage patterns
    /// of RenderPassInstance.
    ///
    /// This helper class allows client code to simply declare what it needs and the actual management 
    /// of the device objects will be handled within the cache.
    class FrameBufferPool
    {
    public:
        std::shared_ptr<FrameBuffer> BuildFrameBuffer(
			ObjectFactory& factory,
            const FrameBufferDesc& desc,
            const FrameBufferProperties& props,
            IteratorRange<const AttachmentDesc*> attachmentResources,
            const INamedAttachments& namedResources,
            uint64 hashName);

        FrameBufferPool();
        ~FrameBufferPool();
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
