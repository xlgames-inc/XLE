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

#if 0
    namespace Internal
    {
        enum class AttachmentUsage : unsigned
        {
            Input = 1<<0, Output = 1<<1, DepthStencil = 1<<2
        };
    }

    static unsigned GetAttachmentUsage(const FrameBufferDesc& layout, AttachmentName attachment)
    {
        unsigned result = 0u;
        for (auto& s:layout.GetSubpasses()) {
            auto i = std::find(s._input.begin(), s._input.end(), attachment);
            if (i != s._input.end()) 
                result |= unsigned(Internal::AttachmentUsage::Input);

            auto o = std::find(s._output.begin(), s._output.end(), attachment);
            if (o != s._output.end()) 
                result |= unsigned(Internal::AttachmentUsage::Output);

            if (s._depthStencil == attachment)
                result |= unsigned(Internal::AttachmentUsage::DepthStencil);
        }
        return result;
    }

    static bool IsRetained(AttachmentViewDesc::LoadStore loadStore)
    {
        return  loadStore == AttachmentViewDesc::LoadStore::Retain
            ||  loadStore == AttachmentViewDesc::LoadStore::Retain_RetainStencil
            ||  loadStore == AttachmentViewDesc::LoadStore::Retain_ClearStencil
            ||  loadStore == AttachmentViewDesc::LoadStore::DontCare_RetainStencil
            ||  loadStore == AttachmentViewDesc::LoadStore::Clear_RetainStencil;
    }
#endif

    FrameBuffer::FrameBuffer(
        const ObjectFactory& factory,
        const FrameBufferDesc& fbDesc,
        NamedResources& namedResources)
    {
        // We must create the frame buffer, including all resources and views required.
        // Here, some resources can come from the presentation chain. But other resources will
        // be created an attached to this object.
        auto attachments = fbDesc.GetAttachments();
        _rtvs.reserve(attachments.size());
        _dsvs.reserve(attachments.size());

        for (const auto&a:attachments) {

            const auto* existingRTV = namedResources.GetRTV(a._viewName, a._resourceName, a._window);
            const auto* existingDSV = namedResources.GetDSV(a._viewName, a._resourceName, a._window);
            _rtvs.push_back(existingRTV ? *existingRTV : RenderTargetView());
            _dsvs.push_back(existingDSV ? *existingDSV : DepthStencilView());

            // auto usage = GetAttachmentUsage(fbDesc, a._resourceName);

            // RenderTargetView rtv;
            // DepthStencilView dsv;
            // ShaderResourceView srv;
            // 
            // const bool storeNamed = IsRetained(a._storeToNextPhase) || (usage & unsigned(Internal::AttachmentUsage::Input));
            // if (desc._bindFlags & BindFlag::RenderTarget) {
            //     TextureViewWindow rtvWindow = a._format;
            //     rtv = RenderTargetView(factory, image, rtvWindow);
            //     if (storeNamed) namedResources.Bind(a._name, rtv);
            // }
            // if (desc._bindFlags & BindFlag::DepthStencil) {
            //     TextureViewWindow dsvWindow = a._format;
            //     dsv = DepthStencilView(factory, image, dsvWindow);
            //     if (storeNamed) namedResources.Bind(a._name, dsv);
            // }
            // 
            // _rtvs.emplace_back(std::move(rtv));
            // _dsvs.emplace_back(std::move(dsv));
        }

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
            bindRTVs[c] = _rtvs[s._rtvs[c]].GetUnderlying();
        if (s._dsv != ~0u)
            bindDSV = _dsvs[s._dsv].GetUnderlying();

        context.GetUnderlying()->OMSetRenderTargets(s._rtvCount, bindRTVs, bindDSV);
    }

    const RenderTargetView& FrameBuffer::GetRTV(unsigned index) const
    {
        if (index >= _rtvs.size())
            Throw(::Exceptions::BasicLabel("Invalid attachment index passed to FrameBuffer::GetRTV()"));
        return _rtvs[index];
    }

    const DepthStencilView& FrameBuffer::GetDSV(unsigned index) const
    {
        if (index >= _dsvs.size())
            Throw(::Exceptions::BasicLabel("Invalid attachment index passed to FrameBuffer::GetDSV()"));
        return _dsvs[index];
    }

	FrameBuffer::FrameBuffer() {}
    FrameBuffer::~FrameBuffer() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void            RenderPassInstance::NextSubpass()
    {
        // queue up the next render targets
        _frameBuffer->BindSubpass(*_attachedContext, ++_activeSubpass);
    }

    void            RenderPassInstance::End()
    {
        // if (_attachedContext)
        //     _attachedContext->EndRenderPass();

        // For compatibility with Vulkan, it makes sense to unbind render targets here. This is important
        // if the render targets will be used as compute shader outputs in follow up steps. It also prevents
        // rendering outside of render passes. But sometimes it will produce redundant calls to OMSetRenderTargets().
        _attachedContext->Unbind<RenderTargetView>();
    }

    const RenderTargetView&  RenderPassInstance::GetAttachment(unsigned index)
    {
        // We can call this function during the render pass... However normally if we 
        // want to use a render target, we should do it after the render pass has been
        // ended (with RenderPassInstance::End())
        return _frameBuffer->GetRTV(index);
    }

    RenderPassInstance::RenderPassInstance(
        DeviceContext& context,
        const FrameBufferDesc& layout,
        uint64 hashName,
        NamedResources& namedResources,
        FrameBufferCache& cache,
        const RenderPassBeginDesc& beginInfo)
    {
        // We need to allocate the particular frame buffer we're going to use
        // And then we'll call BeginRenderPass to begin the render pass
        _frameBuffer = cache.BuildFrameBuffer(
            context.GetFactory(), layout, 
            namedResources, hashName);
        assert(_frameBuffer);

        // Do the clear operations here
        // Note that we could delay some clear operations until an attachment is first used... It's not clear
        // if that would really help...?
        for (unsigned attachmentIndex=0; attachmentIndex<(unsigned)layout.GetAttachments().size(); ++attachmentIndex) {
            const auto& a = layout.GetAttachments()[attachmentIndex];
            if (    a._loadFromPreviousPhase == AttachmentViewDesc::LoadStore::Clear
                ||  a._loadFromPreviousPhase == AttachmentViewDesc::LoadStore::Clear_RetainStencil
                ||  a._loadFromPreviousPhase == AttachmentViewDesc::LoadStore::Clear_ClearStencil) {

                auto& rtv = _frameBuffer->GetRTV(attachmentIndex);
                if (rtv.IsGood()) {
                    const auto& values = beginInfo._clearValues[attachmentIndex]._float;
                    context.Clear(rtv, {values[0], values[1], values[2], values[3]});
                } else {
                    auto& dsv = _frameBuffer->GetDSV(attachmentIndex);
                    if (dsv.IsGood()) {
                        bool clearStencil = a._loadFromPreviousPhase == AttachmentViewDesc::LoadStore::Clear_ClearStencil;
                        const auto& values = beginInfo._clearValues[attachmentIndex]._depthStencil;
                        context.Clear(dsv,
                            clearStencil ? DeviceContext::ClearFilter::Depth|DeviceContext::ClearFilter::Stencil : DeviceContext::ClearFilter::Depth,
                            values._depth, values._stencil);
                    }
                }
            } else if ( a._loadFromPreviousPhase == AttachmentViewDesc::LoadStore::DontCare_ClearStencil
                    ||  a._loadFromPreviousPhase == AttachmentViewDesc::LoadStore::Retain_ClearStencil) {
                auto& dsv = _frameBuffer->GetDSV(attachmentIndex);
                if (dsv.IsGood()) {
                    const auto& values = beginInfo._clearValues[attachmentIndex]._depthStencil;
                    context.Clear(dsv, DeviceContext::ClearFilter::Stencil, values._depth, values._stencil);
                }
            }
        }

        _frameBuffer->BindSubpass(context, 0);
        _attachedContext = &context;
        _activeSubpass = 0;
    }
    
    RenderPassInstance::~RenderPassInstance() 
    {
        End();
    }
    
///////////////////////////////////////////////////////////////////////////////////////////////////

    class FrameBufferCache::Pimpl 
    {
    public:
    };

    std::shared_ptr<FrameBuffer> FrameBufferCache::BuildFrameBuffer(
		const ObjectFactory& factory,
		const FrameBufferDesc& desc,
        NamedResources& namedResources,
        uint64 hashName)
    {
        return std::make_shared<FrameBuffer>(factory, desc, namedResources);
    }

    FrameBufferCache::FrameBufferCache()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    FrameBufferCache::~FrameBufferCache()
    {}



}}

