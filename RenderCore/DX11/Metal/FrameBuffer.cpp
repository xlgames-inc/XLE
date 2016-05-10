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
    static unsigned FindAttachmentIndex(IteratorRange<const AttachmentDesc*> attachments, AttachmentDesc::Name name)
    {
        auto i = std::find_if(
            attachments.begin(), attachments.end(), 
            [name](const AttachmentDesc& desc) { return desc._name == name; });
        if (i != attachments.end())
            return (unsigned)std::distance(attachments.begin(), i);
        return ~0u;
    }

    namespace Internal
    {
        enum class AttachmentUsage : unsigned
        {
            Input = 1<<0, Output = 1<<1, DepthStencil = 1<<2
        };
    }

    static unsigned GetAttachmentUsage(const FrameBufferDesc& layout, AttachmentDesc::Name attachment)
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

    static bool IsRetained(AttachmentDesc::LoadStore loadStore)
    {
        return  loadStore == AttachmentDesc::LoadStore::Retain
            ||  loadStore == AttachmentDesc::LoadStore::Retain_RetainStencil
            ||  loadStore == AttachmentDesc::LoadStore::Retain_ClearStencil
            ||  loadStore == AttachmentDesc::LoadStore::DontCare_RetainStencil
            ||  loadStore == AttachmentDesc::LoadStore::Clear_RetainStencil;
    }

    static Format AsSRVFormat(Format input)
    {
        return AsDepthAspectSRVFormat(input);
    }

    FrameBuffer::FrameBuffer(
        const ObjectFactory& factory,
        const FrameBufferDesc& fbDesc,
		const FrameBufferProperties& props,
        NamedResources& namedResources)
    {
        // We must create the frame buffer, including all resources and views required.
        // Here, some resources can come from the presentation chain. But other resources will
        // be created an attached to this object.
        auto attachments = fbDesc.GetAttachments();
        _rtvs.reserve(attachments.size());
        _dsvs.reserve(attachments.size());
        _srvs.reserve(attachments.size());

        for (const auto&a:attachments) {

            // First, look for an existing resource that is bound with this name.
            const auto* existingRTV = namedResources.GetRTV(a._name);
            const auto* existingSRV = namedResources.GetSRV(a._name);
            const auto* existingDSV = namedResources.GetDSV(a._name);
            if (existingRTV != nullptr || existingSRV != nullptr || existingDSV != nullptr) {
                _rtvs.push_back(existingRTV ? *existingRTV : RenderTargetView());
                _dsvs.push_back(existingDSV ? *existingDSV : DepthStencilView());
                _srvs.push_back(existingSRV ? *existingSRV : ShaderResourceView());
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
                TextureDesc::Plain2D(attachmentWidth, attachmentHeight, AsTypelessFormat(a._format), 1, uint16(props._outputLayers)),
                "attachment");

            if (a._flags & AttachmentDesc::Flags::Multisampled)
                desc._textureDesc._samples = fbDesc.GetSamples();

            // Look at how the attachment is used by the subpasses to figure out what the
            // bind flags should be.

            // todo --  Do we also need to consider what happens to the image after 
            //          the render pass has finished? Resources that are in "output", 
            //          "depthStencil", or "preserve" in the final subpass could be used
            //          in some other way afterwards. For example, one render pass could
            //          generate shadow textures for uses in future render passes?
            auto usage = GetAttachmentUsage(fbDesc, a._name);
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
            auto image = CreateResource(factory, desc);

            RenderTargetView rtv;
            DepthStencilView dsv;
            ShaderResourceView srv;

            const bool storeNamed = IsRetained(a._storeToNextPhase) || (usage & unsigned(Internal::AttachmentUsage::Input));
            if (desc._bindFlags & BindFlag::RenderTarget) {
                TextureViewWindow rtvWindow = a._format;
                rtv = RenderTargetView(factory, image, rtvWindow);
                if (storeNamed) namedResources.Bind(a._name, rtv);
            }
            if (desc._bindFlags & BindFlag::DepthStencil) {
                TextureViewWindow dsvWindow = a._format;
                dsv = DepthStencilView(factory, image, dsvWindow);
                if (storeNamed) namedResources.Bind(a._name, dsv);
            }
            if (desc._bindFlags & BindFlag::ShaderResource) {
                TextureViewWindow srvWindow = AsSRVFormat(a._format);
                srv = ShaderResourceView(factory, image, srvWindow);
                if (storeNamed) namedResources.Bind(a._name, srv);
            }

            _rtvs.emplace_back(std::move(rtv));
            _srvs.emplace_back(std::move(srv));
            _dsvs.emplace_back(std::move(dsv));
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

        // float clear[4] = {0.f, 0.f, 0.f, 1.f};
        // for (unsigned c=0; c<s._rtvCount; ++c)
        //     context.GetUnderlying()->ClearRenderTargetView(bindRTVs[c], clear);
        // if (bindDSV)
        //     context.GetUnderlying()->ClearDepthStencilView(bindDSV, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.f, 0);

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
		const FrameBufferProperties& props,
        uint64 hashName,
        FrameBufferCache& cache,
        const RenderPassBeginDesc& beginInfo)
    {
        // We need to allocate the particular frame buffer we're going to use
        // And then we'll call BeginRenderPass to begin the render pass
        _frameBuffer = cache.BuildFrameBuffer(
            context.GetFactory(), layout, 
            props, context.GetNamedResources(), hashName);
        assert(_frameBuffer);

        // Do the clear operations here
        // Note that we could delay some clear operations until an attachment is first used... It's not clear
        // if that would really help...?
        for (unsigned attachmentIndex=0; attachmentIndex<(unsigned)layout.GetAttachments().size(); ++attachmentIndex) {
            const auto& a = layout.GetAttachments()[attachmentIndex];
            if (    a._loadFromPreviousPhase == AttachmentDesc::LoadStore::Clear
                ||  a._loadFromPreviousPhase == AttachmentDesc::LoadStore::Clear_RetainStencil
                ||  a._loadFromPreviousPhase == AttachmentDesc::LoadStore::Clear_ClearStencil) {

                auto& rtv = _frameBuffer->GetRTV(attachmentIndex);
                if (rtv.IsGood()) {
                    const auto& values = beginInfo._clearValues[attachmentIndex]._float;
                    context.Clear(rtv, {values[0], values[1], values[2], values[3]});
                } else {
                    auto& dsv = _frameBuffer->GetDSV(attachmentIndex);
                    if (dsv.IsGood()) {
                        bool clearStencil = a._loadFromPreviousPhase == AttachmentDesc::LoadStore::Clear_ClearStencil;
                        const auto& values = beginInfo._clearValues[attachmentIndex]._depthStencil;
                        context.Clear(dsv,
                            clearStencil ? DeviceContext::ClearFilter::Depth|DeviceContext::ClearFilter::Stencil : DeviceContext::ClearFilter::Depth,
                            values._depth, values._stencil);
                    }
                }
            } else if ( a._loadFromPreviousPhase == AttachmentDesc::LoadStore::DontCare_ClearStencil
                    ||  a._loadFromPreviousPhase == AttachmentDesc::LoadStore::Retain_ClearStencil) {
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
		const FrameBufferProperties& props,
        NamedResources& namedResources,
        uint64 hashName)
    {
        return std::make_shared<FrameBuffer>(factory, desc, props, namedResources);
    }

    FrameBufferCache::FrameBufferCache()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    FrameBufferCache::~FrameBufferCache()
    {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    static const unsigned s_maxBoundTargets = 64;

    class NamedResources::Pimpl
    {
    public:
        ShaderResourceView  _srv[s_maxBoundTargets];
        RenderTargetView    _rtv[s_maxBoundTargets];
        DepthStencilView    _dsv[s_maxBoundTargets];
    };
    
    const ShaderResourceView*   NamedResources::GetSRV(AttachmentDesc::Name name) const
    {
        if (name >= s_maxBoundTargets) return nullptr;
        if (!_pimpl->_srv[name].IsGood()) return nullptr;
        return &_pimpl->_srv[name];
    }

    const RenderTargetView*     NamedResources::GetRTV(AttachmentDesc::Name name) const
    {
        if (name >= s_maxBoundTargets) return nullptr;
        if (!_pimpl->_rtv[name].IsGood()) return nullptr;
        return &_pimpl->_rtv[name];
    }

    const DepthStencilView*     NamedResources::GetDSV(AttachmentDesc::Name name) const
    {
        if (name >= s_maxBoundTargets) return nullptr;
        if (!_pimpl->_dsv[name].IsGood()) return nullptr;
        return &_pimpl->_dsv[name];
    }

    void NamedResources::Bind(AttachmentDesc::Name name, const ShaderResourceView& srv)
    {
        if (name >= s_maxBoundTargets) return;
        _pimpl->_srv[name] = srv;
    }

    void NamedResources::Bind(AttachmentDesc::Name name, const RenderTargetView& rtv)
    {
        if (name >= s_maxBoundTargets) return;
        _pimpl->_rtv[name] = rtv;
    }

    void NamedResources::Bind(AttachmentDesc::Name name, const DepthStencilView& dsv)
    {
        if (name >= s_maxBoundTargets) return;
        _pimpl->_dsv[name] = dsv;
    }

    void NamedResources::UnbindAll()
    {
        for (unsigned c=0; c<s_maxBoundTargets; ++c) {
            _pimpl->_srv[c] = ShaderResourceView();
            _pimpl->_rtv[c] = RenderTargetView();
            _pimpl->_dsv[c] = DepthStencilView();
        }
    }

    NamedResources::NamedResources()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    NamedResources::~NamedResources()
    {}

}}

