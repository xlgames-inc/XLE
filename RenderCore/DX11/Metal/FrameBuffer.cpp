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

#include "IncludeDX11.h"

namespace RenderCore { namespace Metal_DX11
{
    static unsigned FindAttachmentIndex(IteratorRange<const AttachmentViewDesc*> attachments, AttachmentName name)
    {
        auto i = std::find_if(
            attachments.begin(), attachments.end(), 
            [name](const AttachmentViewDesc& desc) { return desc._viewName == name; });
        if (i != attachments.end())
            return (unsigned)std::distance(attachments.begin(), i);
        return ~0u;
    }

    FrameBufferViews::FrameBufferViews(
        std::vector<RenderTargetView>&& rtvs,
        std::vector<DepthStencilView>&& dsvs)
    : _rtvs(std::move(rtvs)), _dsvs(std::move(dsvs))
    {}
    FrameBufferViews::FrameBufferViews() {}
    FrameBufferViews::~FrameBufferViews() {}

    const RenderTargetView& FrameBufferViews::GetRTV(unsigned index) const
    {
        if (index >= _rtvs.size())
            Throw(::Exceptions::BasicLabel("Invalid attachment index passed to FrameBuffer::GetRTV()"));
        return _rtvs[index];
    }

    const DepthStencilView& FrameBufferViews::GetDSV(unsigned index) const
    {
        if (index >= _dsvs.size())
            Throw(::Exceptions::BasicLabel("Invalid attachment index passed to FrameBuffer::GetDSV()"));
        return _dsvs[index];
    }


    FrameBuffer::FrameBuffer(
        const ObjectFactory& factory,
        const FrameBufferDesc& fbDesc,
        const FrameBufferViews& views)
    : _views(views)
    {
        // We must create the frame buffer, including all resources and views required.
        // Here, some resources can come from the presentation chain. But other resources will
        // be created an attached to this object.
        auto attachments = fbDesc.GetAttachments();
        // _rtvs.reserve(attachments.size());
        // _dsvs.reserve(attachments.size());
        // 
        // for (const auto&a:attachments) {
        //     const auto* existingRTV = namedResources.GetRTV(a._viewName, a._resourceName, a._window);
        //     const auto* existingDSV = namedResources.GetDSV(a._viewName, a._resourceName, a._window);
        //     _rtvs.push_back(existingRTV ? *existingRTV : RenderTargetView());
        //     _dsvs.push_back(existingDSV ? *existingDSV : DepthStencilView());
        // }

        _subpasses.reserve(fbDesc.GetSubpasses().size());
        for (const auto&s:fbDesc.GetSubpasses()) {
            Subpass sp;
            sp._rtvCount = std::min((unsigned)s._output.size(), s_maxRTVS);
            for (unsigned r=0; r<sp._rtvCount; ++r)
                sp._rtvs[r] = FindAttachmentIndex(attachments, s._output[r]);
            for (unsigned r=sp._rtvCount; r<s_maxRTVS; ++r) sp._rtvs[r] = ~0u;
            sp._dsv = (s._depthStencil != SubpassDesc::Unused) ? FindAttachmentIndex(attachments, s._depthStencil) : ~0u;
            _subpasses.push_back(sp);
        }
    }

    void FrameBuffer::BindSubpass(DeviceContext& context, unsigned subpassIndex) const
    {
        if (subpassIndex >= _subpasses.size())
            Throw(::Exceptions::BasicLabel("Attempting to set invalid subpass"));

        const auto& s = _subpasses[subpassIndex];
        ID3D::RenderTargetView* bindRTVs[s_maxRTVS];
        ID3D::DepthStencilView* bindDSV = nullptr;
        for (unsigned c=0; c<s._rtvCount; ++c)
            bindRTVs[c] = _views.GetRTV(s._rtvs[c]).GetUnderlying();
        if (s._dsv != ~0u)
            bindDSV = _views.GetDSV(s._dsv).GetUnderlying();

        context.GetUnderlying()->OMSetRenderTargets(s._rtvCount, bindRTVs, bindDSV);
    }
    
	FrameBuffer::FrameBuffer() {}
    FrameBuffer::~FrameBuffer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    static unsigned s_activeSubpass = 0;

    void BeginRenderPass(
        DeviceContext& context,
        FrameBuffer& frameBuffer,
        const FrameBufferDesc& layout,
        const FrameBufferProperties& props,
        IteratorRange<const ClearValue*> clearValues)
    {
        // Do the clear operations here
        // Note that we could delay some clear operations until an attachment is first used... It's not clear
        // if that would really help...?
        for (unsigned attachmentIndex=0; attachmentIndex<(unsigned)layout.GetAttachments().size(); ++attachmentIndex) {
            const auto& a = layout.GetAttachments()[attachmentIndex];
            if (    a._loadFromPreviousPhase == AttachmentViewDesc::LoadStore::Clear
                ||  a._loadFromPreviousPhase == AttachmentViewDesc::LoadStore::Clear_RetainStencil
                ||  a._loadFromPreviousPhase == AttachmentViewDesc::LoadStore::Clear_ClearStencil) {

                auto& rtv = frameBuffer.GetViews().GetRTV(attachmentIndex);
                if (rtv.IsGood()) {
                    const auto& values = clearValues[attachmentIndex]._float;
                    context.Clear(rtv, {values[0], values[1], values[2], values[3]});
                } else {
                    auto& dsv = frameBuffer.GetViews().GetDSV(attachmentIndex);
                    if (dsv.IsGood()) {
                        bool clearStencil = a._loadFromPreviousPhase == AttachmentViewDesc::LoadStore::Clear_ClearStencil;
                        const auto& values = clearValues[attachmentIndex]._depthStencil;
                        context.Clear(dsv,
                            clearStencil ? DeviceContext::ClearFilter::Depth|DeviceContext::ClearFilter::Stencil : DeviceContext::ClearFilter::Depth,
                            values._depth, values._stencil);
                    }
                }
            } else if ( a._loadFromPreviousPhase == AttachmentViewDesc::LoadStore::DontCare_ClearStencil
                    ||  a._loadFromPreviousPhase == AttachmentViewDesc::LoadStore::Retain_ClearStencil) {
                auto& dsv = frameBuffer.GetViews().GetDSV(attachmentIndex);
                if (dsv.IsGood()) {
                    const auto& values = clearValues[attachmentIndex]._depthStencil;
                    context.Clear(dsv, DeviceContext::ClearFilter::Stencil, values._depth, values._stencil);
                }
            }
        }

        frameBuffer.BindSubpass(context, 0);
        s_activeSubpass = 0;
    }

    void BeginNextSubpass(
        DeviceContext& context,
        FrameBuffer& frameBuffer)
    {
        // queue up the next render targets
        frameBuffer.BindSubpass(context, ++s_activeSubpass);
    }

    void EndRenderPass(DeviceContext& context)
    {
        // For compatibility with Vulkan, it makes sense to unbind render targets here. This is important
        // if the render targets will be used as compute shader outputs in follow up steps. It also prevents
        // rendering outside of render passes. But sometimes it will produce redundant calls to OMSetRenderTargets().
        context.Unbind<RenderTargetView>();
    }


///////////////////////////////////////////////////////////////////////////////////////////////////

    class FrameBufferCache::Pimpl 
    {
    public:
    };

    std::shared_ptr<FrameBuffer> FrameBufferCache::BuildFrameBuffer(
		const ObjectFactory& factory,
		const FrameBufferDesc& desc,
        const FrameBufferViews& views,
        uint64 hashName)
    {
        return std::make_shared<FrameBuffer>(factory, desc, views);
    }

    FrameBufferCache::FrameBufferCache()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    FrameBufferCache::~FrameBufferCache()
    {}



}}

