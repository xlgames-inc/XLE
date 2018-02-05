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
#include "../../ResourceUtils.h"
#include "../../../Utility/MemoryUtils.h"

#include "IncludeDX11.h"

namespace RenderCore { namespace Metal_DX11
{
    FrameBuffer::FrameBuffer(
        const FrameBufferDesc& fbDesc,
        const INamedAttachments& namedResources)
    {
        // We must create the frame buffer, including all resources and views required.
        // Here, some resources can come from the presentation chain. But other resources will
        // be created an attached to this object.
        auto subpasses = fbDesc.GetSubpasses();

		ViewPool<RenderTargetView> rtvPool;
		ViewPool<DepthStencilView> dsvPool;
		unsigned clearValueIterator = 0;
        
        assert(subpasses.size() < s_maxSubpasses);
        for (unsigned c=0; c<(unsigned)subpasses.size(); ++c) {
            const auto& spDesc = subpasses[c];
            auto& sp = _subpasses[c];
            sp._rtvCount = std::min((unsigned)spDesc._output.size(), s_maxMRTs);
            for (unsigned r=0; r<sp._rtvCount; ++r) {
				const auto& attachmentView = spDesc._output[r];
				auto resource = namedResources.GetResource(attachmentView._resourceName);
				if (!resource)
					Throw(::Exceptions::BasicLabel("Could not find attachment resource for RTV in FrameBuffer::FrameBuffer"));
                sp._rtvs[r] = rtvPool.GetView(resource, attachmentView._window);
				sp._rtvLoad[r] = attachmentView._loadFromPreviousPhase;
				sp._rtvClearValue[r] = clearValueIterator++;
			}

			if (spDesc._depthStencil._resourceName != ~0u) {
				auto resource = namedResources.GetResource(spDesc._depthStencil._resourceName);
				if (!resource)
					Throw(::Exceptions::BasicLabel("Could not find attachment resource for DSV in FrameBuffer::FrameBuffer"));
				sp._dsv = dsvPool.GetView(resource, spDesc._depthStencil._window);
				sp._dsvLoad = spDesc._depthStencil._loadFromPreviousPhase;
				sp._dsvClearValue = clearValueIterator++;
			}
        }
        _subpassCount = (unsigned)subpasses.size();
    }

    void FrameBuffer::BindSubpass(DeviceContext& context, unsigned subpassIndex, IteratorRange<const ClearValue*> clearValues) const
    {
        if (subpassIndex >= _subpassCount)
            Throw(::Exceptions::BasicLabel("Attempting to set invalid subpass"));

        const auto& s = _subpasses[subpassIndex];
		for (unsigned c = 0; c < s._rtvCount; ++c) {
			if (s._rtvLoad[c] == LoadStore::Clear)
				context.Clear(s._rtvs[c], clearValues[s._rtvClearValue[c]]._float);
		}

		if (s._dsvLoad == LoadStore::Clear_ClearStencil) {
			context.Clear(
				s._dsv, DeviceContext::ClearFilter::Depth | DeviceContext::ClearFilter::Stencil,
				clearValues[s._dsvClearValue]._depthStencil._depth, clearValues[s._dsvClearValue]._depthStencil._stencil);
        } else if (s._dsvLoad == LoadStore::Clear || s._dsvLoad == LoadStore::Clear_RetainStencil) {
			context.Clear(
				s._dsv, DeviceContext::ClearFilter::Depth,
				clearValues[s._dsvClearValue]._depthStencil._depth, clearValues[s._dsvClearValue]._depthStencil._stencil);
        } else if (s._dsvLoad == LoadStore::DontCare_ClearStencil || s._dsvLoad == LoadStore::Retain_ClearStencil) {
			context.Clear(
				s._dsv, DeviceContext::ClearFilter::Stencil,
				clearValues[s._dsvClearValue]._depthStencil._depth, clearValues[s._dsvClearValue]._depthStencil._stencil);
        }

		ID3D::RenderTargetView* bindRTVs[s_maxMRTs];
		ID3D::DepthStencilView* bindDSV = nullptr;
		for (unsigned c = 0; c<s._rtvCount; ++c)
			bindRTVs[c] = s._rtvs[c].GetUnderlying();
		bindDSV = s._dsv.GetUnderlying();
        context.GetUnderlying()->OMSetRenderTargets(s._rtvCount, bindRTVs, bindDSV);
    }

	FrameBuffer::FrameBuffer() {}
    FrameBuffer::~FrameBuffer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

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

    void EndRenderPass(DeviceContext& context)
    {
        // For compatibility with Vulkan, it makes sense to unbind render targets here. This is important
        // if the render targets will be used as compute shader outputs in follow up steps. It also prevents
        // rendering outside of render passes. But sometimes it will produce redundant calls to OMSetRenderTargets().
        context.Unbind<RenderTargetView>();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class FrameBufferPool::Pimpl 
    {
    public:
    };

    std::shared_ptr<FrameBuffer> FrameBufferPool::BuildFrameBuffer(
		const ObjectFactory& factory,
		const FrameBufferDesc& desc,
        const FrameBufferProperties& props,
        const INamedAttachments& namedResources,
        uint64 hashName)
    {
        return std::make_shared<FrameBuffer>(desc, namedResources);
    }

    FrameBufferPool::FrameBufferPool()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    FrameBufferPool::~FrameBufferPool()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	

}}

