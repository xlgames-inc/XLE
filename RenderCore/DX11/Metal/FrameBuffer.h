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
    
#if 0
    class NamedResources
    {
    public:
        const ShaderResourceView*   GetSRV(AttachmentName name) const;
        const RenderTargetView*     GetRTV(AttachmentName name) const;
        const DepthStencilView*     GetDSV(AttachmentName name) const;

        void Bind(AttachmentName name, const ShaderResourceView& srv);
        void Bind(AttachmentName name, const RenderTargetView& rtv);
        void Bind(AttachmentName name, const DepthStencilView& dsv);
        void UnbindAll();

        NamedResources();
        ~NamedResources();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };
#endif

    class FrameBuffer
	{
	public:
        const RenderTargetView& GetRTV(unsigned index) const;
        const DepthStencilView& GetDSV(unsigned index) const;

        void BindSubpass(DeviceContext& context, unsigned subpassIndex) const;

		FrameBuffer(
			const ObjectFactory& factory,
            const FrameBufferDesc& desc,
            NamedResources& namedResources);
		FrameBuffer();
		~FrameBuffer();
	private:
        std::vector<RenderTargetView>   _rtvs;
        std::vector<DepthStencilView>   _dsvs;

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
            NamedResources& namedResources,
            uint64 hashName);

        FrameBufferCache();
        ~FrameBufferCache();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    /// <summary>Begins and ends a render pass on the given context</summary>
    /// Creates and begins a render pass using the given frame buffer layout. This will also automatically
    /// allocate the buffers required
    ///
    /// The "hashName" parameter to the constructor can be used to reuse buffers from previous instances
    /// of a similar render pass. For example, a render pass instance might be for rendering and resolving
    /// the lighting for a deferred lighting scheme. This will be rendered every frame, usually using the same
    /// parameters. Use the same hashName to ensure that the same cached frame buffer will be reused (when possible).
    ///
    /// If an output attachment is required after the render pass instance is finished, call GetAttachment().
    /// This can be used to retrieve the rendered results.
    class RenderPassInstance
    {
    public:
        void                    NextSubpass();
        void                    End();
        const RenderTargetView& GetAttachment(unsigned index);

        RenderPassInstance(
            DeviceContext& context,
            const FrameBufferDesc& layout,
            uint64 hashName,
            NamedResources& namedResources,
            FrameBufferCache& cache,
            const RenderPassBeginDesc& beginInfo = RenderPassBeginDesc());
        ~RenderPassInstance();

    private:
        std::shared_ptr<FrameBuffer> _frameBuffer;
        DeviceContext* _attachedContext;
        unsigned _activeSubpass;
    };
}}