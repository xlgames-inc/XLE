// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderPass.h"
#include "../Metal/FrameBuffer.h"
#include "../Metal/TextureView.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/ObjectFactory.h"

namespace RenderCore { namespace Techniques
{
    void            RenderPassInstance::NextSubpass()
    {
        BeginNextSubpass(*_attachedContext, *_frameBuffer);
    }

    void            RenderPassInstance::End()
    {
        EndRenderPass(*_attachedContext);
    }

    const Metal::RenderTargetView&  RenderPassInstance::GetAttachment(unsigned index)
    {
        // We can call this function during the render pass... However normally if we 
        // want to use a render target, we should do it after the render pass has been
        // ended (with RenderPassInstance::End())
        return _frameBuffer->GetViews().GetRTV(index);
    }

    RenderPassInstance::RenderPassInstance(
        Metal::DeviceContext& context,
        const FrameBufferDesc& layout,
        uint64 hashName,
        NamedResources& namedResources,
        Metal::FrameBufferCache& cache,
        const RenderPassBeginDesc& beginInfo)
    {
        // We need to allocate the particular frame buffer we're going to use
        // And then we'll call BeginRenderPass to begin the render pass

        std::vector<Metal::RenderTargetView> rtvs;
        std::vector<Metal::DepthStencilView> dsvs;
        auto attachments = layout.GetAttachments();
        rtvs.reserve(attachments.size());
        rtvs.reserve(attachments.size());
        
        for (const auto&a:attachments) {
            const auto* existingRTV = namedResources.GetRTV(a._viewName, a._resourceName, a._window);
            const auto* existingDSV = namedResources.GetDSV(a._viewName, a._resourceName, a._window);
            rtvs.push_back(existingRTV ? *existingRTV : Metal::RenderTargetView());
            dsvs.push_back(existingDSV ? *existingDSV : Metal::DepthStencilView());
        }

        _frameBuffer = cache.BuildFrameBuffer(
            context.GetFactory(), layout, 
            Metal::FrameBufferViews(std::move(rtvs), std::move(dsvs)), hashName);
        assert(_frameBuffer);

        Metal::BeginRenderPass(context, *_frameBuffer, layout, namedResources.GetFrameBufferProperties(), beginInfo._clearValues);
        _attachedContext = &context;
    }

    RenderPassInstance::RenderPassInstance(
        IThreadContext& context,
        const FrameBufferDesc& layout,
        uint64 hashName,
        NamedResources& namedResources,
        Metal::FrameBufferCache& cache,
        const RenderPassBeginDesc& beginInfo)
    : RenderPassInstance(
        *Metal::DeviceContext::Get(context),
        layout, hashName, namedResources,
        cache, beginInfo)
    {
    }
    
    RenderPassInstance::~RenderPassInstance() 
    {
        End();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static const unsigned s_maxBoundTargets = 64;

    class NamedResources::Pimpl
    {
    public:
        Metal::ShaderResourceView  _srv[s_maxBoundTargets];
        Metal::RenderTargetView    _rtv[s_maxBoundTargets];
        Metal::DepthStencilView    _dsv[s_maxBoundTargets];
        RenderCore::ResourcePtr         _resources[s_maxBoundTargets];
        AttachmentDesc                  _attachments[s_maxBoundTargets];
        AttachmentName                  _resNames[s_maxBoundTargets];

        FrameBufferProperties           _props;
        TextureSamples                  _samples;

        Metal::ObjectFactory*      _factory;

        void BuildAttachment(AttachmentName attach);
        void InvalidateAttachment(AttachmentName name);
    };

    static bool Equal(const AttachmentDesc& lhs, const AttachmentDesc& rhs)
    {
        return lhs._name == rhs._name
            && lhs._dimsMode == rhs._dimsMode
            && lhs._width == rhs._width
            && lhs._height == rhs._height
            && lhs._format == rhs._format
            && lhs._flags == rhs._flags
            ;
    }

    void NamedResources::Pimpl::BuildAttachment(AttachmentName attach)
    {
        assert(attach<s_maxBoundTargets);
        _resources[attach].reset();
        const auto& a = _attachments[attach];

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
            attachmentWidth = unsigned(std::floor(_props._outputWidth * a._width + 0.5f));
            attachmentHeight = unsigned(std::floor(_props._outputHeight * a._height + 0.5f));
        }

        auto desc = CreateDesc(
            0, 0, 0, 
            TextureDesc::Plain2D(attachmentWidth, attachmentHeight, a._format, 1, uint16(_props._outputLayers)),
            "attachment");

        if (a._flags & AttachmentDesc::Flags::Multisampled)
            desc._textureDesc._samples = _samples;

        // Look at how the attachment is used by the subpasses to figure out what the
        // bind flags should be.

        // todo --  Do we also need to consider what happens to the image after 
        //          the render pass has finished? Resources that are in "output", 
        //          "depthStencil", or "preserve" in the final subpass could be used
        //          in some other way afterwards. For example, one render pass could
        //          generate shadow textures for uses in future render passes?
        if (a._flags & AttachmentDesc::Flags::ShaderResource) {
            desc._bindFlags |= BindFlag::ShaderResource;
            desc._gpuAccess |= GPUAccess::Read;
        }

        if (a._flags & AttachmentDesc::Flags::RenderTarget) {
            desc._bindFlags |= BindFlag::RenderTarget;
            desc._gpuAccess |= GPUAccess::Write;
        }

        if (a._flags & AttachmentDesc::Flags::DepthStencil) {
            desc._bindFlags |= BindFlag::DepthStencil;
            desc._gpuAccess |= GPUAccess::Write;
        }

        if (a._flags & AttachmentDesc::Flags::TransferSource) {
            desc._bindFlags |= BindFlag::TransferSrc;
            desc._gpuAccess |= GPUAccess::Read;
        }

        // note -- it might be handy to have a cache of "device memory" that could be reused here?
        _resources[attach] = Metal::CreateResource(*_factory, desc);
    }

    void NamedResources::Pimpl::InvalidateAttachment(AttachmentName name)
    {
        for (unsigned v=0; v<s_maxBoundTargets; ++v)
            if (_resNames[v] == name) {
                _srv[v] = Metal::ShaderResourceView();
                _rtv[v] = Metal::RenderTargetView();
                _dsv[v] = Metal::DepthStencilView();
                _resources[v].reset();
                _resNames[v] = ~0u;
            }
    }
    
    const Metal::ShaderResourceView*   NamedResources::GetSRV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const
    {
        if (resName == ~0u) resName = viewName;
        if (viewName >= s_maxBoundTargets || resName >= s_maxBoundTargets) return nullptr;
        if (_pimpl->_srv[viewName].IsGood()) {
            assert(_pimpl->_resNames[viewName] == resName);
            return &_pimpl->_srv[viewName];
        }

        // Attempt to build the SRV (if possible)
        const auto& attachDesc = _pimpl->_attachments[resName];
        if (!(attachDesc._flags & AttachmentDesc::Flags::ShaderResource)) return nullptr;
        
        if (!_pimpl->_resources[resName])
            _pimpl->BuildAttachment(resName);

        auto adjWindow = window;
        if (adjWindow._format._aspect == TextureViewWindow::UndefinedAspect)
            adjWindow._format._aspect = attachDesc._defaultAspect;
        _pimpl->_srv[viewName] = Metal::ShaderResourceView(*_pimpl->_factory, _pimpl->_resources[resName], adjWindow);
        _pimpl->_resNames[viewName] = resName;
        return _pimpl->_srv[viewName].IsGood() ? &_pimpl->_srv[viewName] : nullptr;
    }

    const Metal::RenderTargetView*     NamedResources::GetRTV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const
    {
        if (resName == ~0u) resName = viewName;
        if (viewName >= s_maxBoundTargets || resName >= s_maxBoundTargets) return nullptr;
        if (_pimpl->_rtv[viewName].IsGood()) {
            assert(_pimpl->_resNames[viewName] == resName);
            return &_pimpl->_rtv[viewName];
        }

        // Attempt to build the RTV (if possible)
        const auto& attachDesc = _pimpl->_attachments[resName];
        if (!(attachDesc._flags & AttachmentDesc::Flags::RenderTarget)) return nullptr;

        if (!_pimpl->_resources[resName])
            _pimpl->BuildAttachment(resName);
        
        auto adjWindow = window;
        if (adjWindow._format._aspect == TextureViewWindow::UndefinedAspect)
            adjWindow._format._aspect = attachDesc._defaultAspect;
        _pimpl->_rtv[viewName] = Metal::RenderTargetView(*_pimpl->_factory, _pimpl->_resources[resName], adjWindow);
        _pimpl->_resNames[viewName] = resName;
        return _pimpl->_rtv[viewName].IsGood() ? &_pimpl->_rtv[viewName] : nullptr;
    }

    const Metal::DepthStencilView*     NamedResources::GetDSV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const
    {
        if (resName == ~0u) resName = viewName;
        if (viewName >= s_maxBoundTargets || resName >= s_maxBoundTargets) return nullptr;
        if (_pimpl->_dsv[viewName].IsGood()) {
            assert(_pimpl->_resNames[viewName] == resName);
            return &_pimpl->_dsv[viewName];
        }

        // Attempt to build the DSV (if possible)
        const auto& attachDesc = _pimpl->_attachments[resName];
        if (!(attachDesc._flags & AttachmentDesc::Flags::DepthStencil)) return nullptr;
        
        if (!_pimpl->_resources[resName])
            _pimpl->BuildAttachment(resName);

        auto adjWindow = window;
        if (adjWindow._format._aspect == TextureViewWindow::UndefinedAspect)
            adjWindow._format._aspect = attachDesc._defaultAspect;
        _pimpl->_dsv[viewName] = Metal::DepthStencilView(*_pimpl->_factory, _pimpl->_resources[resName], adjWindow);
        _pimpl->_resNames[viewName] = resName;
        return _pimpl->_dsv[viewName].IsGood() ? &_pimpl->_dsv[viewName] : nullptr;
    }

    void NamedResources::DefineAttachments(IteratorRange<const AttachmentDesc*> attachments)
    {
        for (const auto& a:attachments) {
            assert(a._name < s_maxBoundTargets);
            if (!Equal(_pimpl->_attachments[a._name], a)) {
                // Clear all of the views associated with this resource
                _pimpl->InvalidateAttachment(a._name);
                _pimpl->_resources[a._name].reset();
                _pimpl->_attachments[a._name] = a;
            }
        }
    }

    void NamedResources::Bind(AttachmentName resName, const ResourcePtr& resource)
    {
        assert(resName < s_maxBoundTargets);
        if (_pimpl->_resources[resName] == resource) return;

        _pimpl->InvalidateAttachment(resName);

        auto desc = Metal::ExtractDesc(resource);
        _pimpl->_attachments[resName] = 
            {
                0u, 
                RenderCore::AttachmentDesc::DimensionsMode::Absolute, 
                (float)desc._textureDesc._width, (float)desc._textureDesc._height,
                desc._textureDesc._format, 
                TextureViewWindow::UndefinedAspect,
                  ((desc._bindFlags & BindFlag::RenderTarget) ? AttachmentDesc::Flags::RenderTarget : 0u)
                | ((desc._bindFlags & BindFlag::ShaderResource) ? AttachmentDesc::Flags::ShaderResource : 0u)
                | ((desc._bindFlags & BindFlag::DepthStencil) ? AttachmentDesc::Flags::DepthStencil : 0u)
                | ((desc._bindFlags & BindFlag::TransferSrc) ? AttachmentDesc::Flags::TransferSource : 0u)
            };

        _pimpl->_resources[resName] = resource;
    }

    void NamedResources::Unbind(AttachmentName resName)
    {
        assert(resName < s_maxBoundTargets);
        _pimpl->InvalidateAttachment(resName);
    }

    void NamedResources::Bind(TextureSamples samples)
    {
        if (    _pimpl->_samples._sampleCount == samples._sampleCount
            &&  _pimpl->_samples._samplingQuality == samples._samplingQuality)
            return;

        // Invalidate all resources and views that depend on the multisample state
        for (unsigned c=0; c<s_maxBoundTargets; ++c)
            if (_pimpl->_attachments[c]._flags & AttachmentDesc::Flags::Multisampled)
                _pimpl->InvalidateAttachment(c);
        _pimpl->_samples = samples;
    }

    void NamedResources::Bind(FrameBufferProperties props)
    {
        bool xyChanged = 
               props._outputWidth != _pimpl->_props._outputWidth
            || props._outputHeight != _pimpl->_props._outputHeight;
        bool layersChanged = 
            props._outputLayers != _pimpl->_props._outputLayers;
        if (!xyChanged && !layersChanged) return;

        if (xyChanged) {
            for (unsigned c=0; c<s_maxBoundTargets; ++c)
                if (_pimpl->_attachments[c]._dimsMode == AttachmentDesc::DimensionsMode::OutputRelative)
                    _pimpl->InvalidateAttachment(c);
        }

        if (layersChanged) // currently all resources depend on the output layers value
            for (unsigned c=0; c<s_maxBoundTargets; ++c)
                _pimpl->InvalidateAttachment(c);
        _pimpl->_props = props;
    }

    const FrameBufferProperties& NamedResources::GetFrameBufferProperties() const
    {
        return _pimpl->_props;
    }

    NamedResources::NamedResources()
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_factory = &Metal::GetObjectFactory();
        for (unsigned c=0; c<s_maxBoundTargets; ++c)
            _pimpl->_resNames[c] = ~0u;
        _pimpl->_samples = TextureSamples::Create();
        _pimpl->_props = {0u, 0u, 0u};
    }

    NamedResources::~NamedResources()
    {}
    
}}

