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
        Metal::BeginNextSubpass(*_attachedContext, *_frameBuffer);
    }

    void            RenderPassInstance::End()
    {
        Metal::EndRenderPass(*_attachedContext);
    }

    class NamedResourcesWrapper : public Metal::INamedResources
    {
    public:
        virtual auto GetSRV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const -> Metal::ShaderResourceView*;
        virtual auto GetRTV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const -> Metal::RenderTargetView*;
        virtual auto GetDSV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const -> Metal::DepthStencilView*;
        virtual auto GetDesc(AttachmentName resName) const -> const AttachmentDesc*;

        NamedResourcesWrapper(NamedResources& namedRes);
        ~NamedResourcesWrapper();
    private:
        NamedResources* _namedRes;
    };

    auto NamedResourcesWrapper::GetSRV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const -> Metal::ShaderResourceView*
    {
        return _namedRes->GetSRV(viewName, resName, window);
    }

    auto NamedResourcesWrapper::GetRTV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const -> Metal::RenderTargetView*
    {
        return _namedRes->GetRTV(viewName, resName, window);
    }

    auto NamedResourcesWrapper::GetDSV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const -> Metal::DepthStencilView*
    {
        return _namedRes->GetDSV(viewName, resName, window);
    }

    auto NamedResourcesWrapper::GetDesc(AttachmentName resName) const -> const AttachmentDesc*
    {
        return _namedRes->GetDesc(resName);
    }

    NamedResourcesWrapper::NamedResourcesWrapper(NamedResources& namedRes)
    : _namedRes(&namedRes) {}
    NamedResourcesWrapper::~NamedResourcesWrapper() {}

    RenderPassInstance::RenderPassInstance(
        Metal::DeviceContext& context,
        const FrameBufferDesc& layout,
        uint64 hashName,
        NamedResources& namedResources,
        const RenderPassBeginDesc& beginInfo)
    {
        // We need to allocate the particular frame buffer we're going to use
        // And then we'll call BeginRenderPass to begin the render pass

        std::vector<Metal::RenderTargetView> rtvs;
        std::vector<Metal::DepthStencilView> dsvs;
        auto attachments = layout.GetAttachments();
        rtvs.reserve(attachments.size());
        dsvs.reserve(attachments.size());
        
        for (const auto&a:attachments) {
            const auto* existingRTV = namedResources.GetRTV(a._viewName, a._resourceName, a._window);
            const auto* existingDSV = namedResources.GetDSV(a._viewName, a._resourceName, a._window);
            rtvs.push_back(existingRTV ? *existingRTV : Metal::RenderTargetView());
            dsvs.push_back(existingDSV ? *existingDSV : Metal::DepthStencilView());
        }

        Metal::FrameBufferCache cache;
        _frameBuffer = cache.BuildFrameBuffer(
            context.GetFactory(), layout, 
            namedResources.GetFrameBufferProperties(),
            namedResources.GetDescriptions(),
            NamedResourcesWrapper(namedResources),
            hashName);
        assert(_frameBuffer);

        Metal::BeginRenderPass(context, *_frameBuffer, layout, namedResources.GetFrameBufferProperties(), beginInfo._clearValues);
        _attachedContext = &context;
    }

    RenderPassInstance::RenderPassInstance(
        IThreadContext& context,
        const FrameBufferDesc& layout,
        uint64 hashName,
        NamedResources& namedResources,
        const RenderPassBeginDesc& beginInfo)
    : RenderPassInstance(
        *Metal::DeviceContext::Get(context),
        layout, hashName, namedResources, beginInfo)
    {}
    
    RenderPassInstance::~RenderPassInstance() 
    {
        End();
    }

    RenderPassInstance::RenderPassInstance(RenderPassInstance&& moveFrom)
    : _frameBuffer(std::move(moveFrom._frameBuffer))
    , _attachedContext(moveFrom._attachedContext)
    , _activeSubpass(moveFrom._activeSubpass)
    {}

    RenderPassInstance& RenderPassInstance::operator=(RenderPassInstance&& moveFrom)
    {
        _frameBuffer = std::move(moveFrom._frameBuffer);
        _attachedContext = moveFrom._attachedContext;
        _activeSubpass = moveFrom._activeSubpass;
        return *this;
    }

    RenderPassInstance::RenderPassInstance()
    {
        _attachedContext = nullptr;
        _activeSubpass = 0u;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static const unsigned s_maxBoundTargets = 64;

    class NamedResources::Pimpl
    {
    public:
        Metal::ShaderResourceView   _srv[s_maxBoundTargets];
        Metal::RenderTargetView     _rtv[s_maxBoundTargets];
        Metal::DepthStencilView     _dsv[s_maxBoundTargets];
        RenderCore::ResourcePtr     _resources[s_maxBoundTargets];
        AttachmentDesc              _attachments[s_maxBoundTargets];
        AttachmentName              _resNames[s_maxBoundTargets];

        FrameBufferProperties       _props;

        Metal::ObjectFactory*      _factory;

        bool BuildAttachment(AttachmentName attach);
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

    bool NamedResources::Pimpl::BuildAttachment(AttachmentName attach)
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

        if (!attachmentWidth || !attachmentHeight) return false;

        auto desc = CreateDesc(
            0, 0, 0, 
            TextureDesc::Plain2D(attachmentWidth, attachmentHeight, a._format, 1, uint16(a._arrayLayerCount)),
            "attachment");

        if (a._flags & AttachmentDesc::Flags::Multisampled)
            desc._textureDesc._samples = _props._samples;

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
        return true;
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

    auto NamedResources::GetDesc(AttachmentName resName) const -> const AttachmentDesc*
    {
        if (resName >= s_maxBoundTargets) return nullptr;
        return &_pimpl->_attachments[resName];
    }
    
    Metal::ShaderResourceView*   NamedResources::GetSRV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const
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
            if (!_pimpl->BuildAttachment(resName))
                return nullptr;

        auto adjWindow = window;
        if (adjWindow._format._aspect == TextureViewWindow::UndefinedAspect)
            adjWindow._format._aspect = attachDesc._defaultAspect;
        _pimpl->_srv[viewName] = Metal::ShaderResourceView(*_pimpl->_factory, _pimpl->_resources[resName], adjWindow);
        _pimpl->_resNames[viewName] = resName;
        return _pimpl->_srv[viewName].IsGood() ? &_pimpl->_srv[viewName] : nullptr;
    }

    Metal::RenderTargetView*     NamedResources::GetRTV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const
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
            if (!_pimpl->BuildAttachment(resName))
                return nullptr;
        
        auto adjWindow = window;
        if (adjWindow._format._aspect == TextureViewWindow::UndefinedAspect)
            adjWindow._format._aspect = attachDesc._defaultAspect;
        _pimpl->_rtv[viewName] = Metal::RenderTargetView(*_pimpl->_factory, _pimpl->_resources[resName], adjWindow);
        _pimpl->_resNames[viewName] = resName;
        return _pimpl->_rtv[viewName].IsGood() ? &_pimpl->_rtv[viewName] : nullptr;
    }

    Metal::DepthStencilView*     NamedResources::GetDSV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const
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
            if (!_pimpl->BuildAttachment(resName))
                return nullptr;

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
                0u, desc._textureDesc._format, 
                TextureViewWindow::UndefinedAspect,
                  ((desc._bindFlags & BindFlag::RenderTarget) ? AttachmentDesc::Flags::RenderTarget : 0u)
                | ((desc._bindFlags & BindFlag::ShaderResource) ? AttachmentDesc::Flags::ShaderResource : 0u)
                | ((desc._bindFlags & BindFlag::DepthStencil) ? AttachmentDesc::Flags::DepthStencil : 0u)
                | ((desc._bindFlags & BindFlag::TransferSrc) ? AttachmentDesc::Flags::TransferSource : 0u)
            };

        _pimpl->_resources[resName] = resource;
        _pimpl->_resNames[resName] = resName;
    }

    void NamedResources::Unbind(AttachmentName resName)
    {
        assert(resName < s_maxBoundTargets);
        _pimpl->InvalidateAttachment(resName);
    }

    void NamedResources::Bind(FrameBufferProperties props)
    {
        bool xyChanged = 
               props._outputWidth != _pimpl->_props._outputWidth
            || props._outputHeight != _pimpl->_props._outputHeight;
        bool samplesChanged = 
                props._samples._sampleCount != _pimpl->_props._samples._sampleCount
            ||  props._samples._samplingQuality != _pimpl->_props._samples._samplingQuality;
        if (!xyChanged && !samplesChanged) return;

        if (xyChanged)
            for (unsigned c=0; c<s_maxBoundTargets; ++c)
                if (_pimpl->_attachments[c]._dimsMode == AttachmentDesc::DimensionsMode::OutputRelative)
                    _pimpl->InvalidateAttachment(c);

        if (samplesChanged) // Invalidate all resources and views that depend on the multisample state
            for (unsigned c=0; c<s_maxBoundTargets; ++c)
                if (_pimpl->_attachments[c]._flags & AttachmentDesc::Flags::Multisampled)
                    _pimpl->InvalidateAttachment(c);

        _pimpl->_props = props;
    }

    const FrameBufferProperties& NamedResources::GetFrameBufferProperties() const
    {
        return _pimpl->_props;
    }

    IteratorRange<const AttachmentDesc*> NamedResources::GetDescriptions() const
    {
        return MakeIteratorRange(_pimpl->_attachments);
    }

    NamedResources::NamedResources()
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_factory = &Metal::GetObjectFactory();
        for (unsigned c=0; c<s_maxBoundTargets; ++c) {
            _pimpl->_resNames[c] = ~0u;
            _pimpl->_attachments[c] = {};
        }
        _pimpl->_props = {0u, 0u, TextureSamples::Create()};
    }

    NamedResources::~NamedResources()
    {}
    
}}

