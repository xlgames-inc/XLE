// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../FrameBufferDesc.h"
#include <memory>

namespace RenderCore { enum class Format; }
namespace RenderCore { namespace Metal_DX11
{
    class ObjectFactory;
    class RenderTargetView;
    class ShaderResourceView;
    class DepthStencilView;
    class DeviceContext;

    class FrameBufferViews
    {
    public:
        const RenderTargetView& GetRTV(unsigned index) const;
        const DepthStencilView& GetDSV(unsigned index) const;

        FrameBufferViews(
            std::vector<RenderTargetView>&& rtvs,
            std::vector<DepthStencilView>&& dsvs);
        FrameBufferViews();
        ~FrameBufferViews();
    private:
        std::vector<RenderTargetView>   _rtvs;
        std::vector<DepthStencilView>   _dsvs;
    };
    
    class FrameBuffer
	{
	public:
        void BindSubpass(DeviceContext& context, unsigned subpassIndex) const;
        const FrameBufferViews& GetViews() const { return _views; }

		FrameBuffer(
			const ObjectFactory& factory,
            const FrameBufferDesc& desc,
            const FrameBufferViews& views);
		FrameBuffer();
		~FrameBuffer();
	private:
        FrameBufferViews _views;

        static const unsigned s_maxRTVS = 4u;
        class Subpass
        {
        public:
            unsigned _rtvs[s_maxRTVS];
            unsigned _dsv;
            unsigned _rtvCount;
        };
        std::vector<Subpass> _subpasses;
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
            const FrameBufferViews& views,
            const FrameBufferProperties& props,
            IteratorRange<const AttachmentDesc*> attachmentResources,
            const TextureSamples& samples,
            uint64 hashName);

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