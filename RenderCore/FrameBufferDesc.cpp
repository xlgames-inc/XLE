// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "FrameBufferDesc.h"
#include "Format.h"
#include "../Utility/MemoryUtils.h"
#include "DX11/Metal/TextureView.h"
#include "DX11/Metal/ObjectFactory.h"

namespace RenderCore
{
    SubpassDesc::SubpassDesc()
    : _depthStencil(Unused)
    {
    }

    SubpassDesc::SubpassDesc(
        IteratorRange<const AttachmentName*> output,
        AttachmentName depthStencil,
        IteratorRange<const AttachmentName*> input, 
        IteratorRange<const AttachmentName*> preserve,
        IteratorRange<const AttachmentName*> resolve)
    : _input(input.begin(), input.end())
    , _output(output.begin(), output.end())
    , _depthStencil(depthStencil)
    , _preserve(preserve.begin(), preserve.end())
    , _resolve(resolve.begin(), resolve.end())
    {
    }

    static bool HasAttachment(
        IteratorRange<const AttachmentViewDesc*> attachments,
        AttachmentName name)
    {
        for (const auto&i:attachments)
            if (i._viewName == name) return true;
        return false;
    }

    FrameBufferDesc::FrameBufferDesc(
        IteratorRange<const SubpassDesc*> subpasses,
        IteratorRange<const AttachmentViewDesc*> attachments)
    : _attachments(attachments.begin(), attachments.end())
    , _subpasses(subpasses.begin(), subpasses.end())
    {
        // We can also have "implied" attachments. These are attachments that are referenced but not explicitly
        // declared. These only color, depth/stencil and resolve attachments can be implied. We must make some 
        // assumptions about format, layout, etc.
        for (auto&p:subpasses) {
            for (auto& a:p._output)
                if (!HasAttachment(MakeIteratorRange(_attachments), a))
                    _attachments.push_back(
                        AttachmentViewDesc{
                            a, a,
                            TextureViewWindow(),
                            AttachmentViewDesc::LoadStore::Retain_RetainStencil,
                            AttachmentViewDesc::LoadStore::Retain_RetainStencil});

            if (    p._depthStencil != SubpassDesc::Unused 
                &&  !HasAttachment(MakeIteratorRange(_attachments), p._depthStencil))
                _attachments.push_back(
                    AttachmentViewDesc{
                        p._depthStencil, p._depthStencil,
                        TextureViewWindow(),
                        AttachmentViewDesc::LoadStore::Retain_RetainStencil,
                        AttachmentViewDesc::LoadStore::Retain_RetainStencil});

            for (auto& a:p._resolve)
                if (!HasAttachment(MakeIteratorRange(_attachments), a))
                    _attachments.push_back(
                        AttachmentViewDesc{
                            a, a,
                            TextureViewWindow(),
                            AttachmentViewDesc::LoadStore::Retain_RetainStencil,
                            AttachmentViewDesc::LoadStore::Retain_RetainStencil});
        }

        // Calculate the hash value for this description by combining
        // together the hashes of the members.
        _hash = DefaultSeed64;
        _hash = HashCombine(Hash64(AsPointer(_attachments.begin()), AsPointer(_attachments.end())), _hash);
        _hash = HashCombine(Hash64(AsPointer(_subpasses.begin()), AsPointer(_subpasses.end())), _hash);
    }

	FrameBufferDesc::FrameBufferDesc()
    : _hash(0)
    {
    }

    FrameBufferDesc::~FrameBufferDesc() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    static const unsigned s_maxBoundTargets = 64;

    class NamedResources::Pimpl
    {
    public:
        Metal_DX11::ShaderResourceView  _srv[s_maxBoundTargets];
        Metal_DX11::RenderTargetView    _rtv[s_maxBoundTargets];
        Metal_DX11::DepthStencilView    _dsv[s_maxBoundTargets];
        RenderCore::ResourcePtr         _resources[s_maxBoundTargets];
        AttachmentDesc                  _attachments[s_maxBoundTargets];
        AttachmentName                  _resNames[s_maxBoundTargets];

        FrameBufferProperties           _props;
        TextureSamples                  _samples;

        Metal_DX11::ObjectFactory*      _factory;

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
        _resources[attach] = Metal_DX11::CreateResource(*_factory, desc);
    }

    void NamedResources::Pimpl::InvalidateAttachment(AttachmentName name)
    {
        for (unsigned v=0; v<s_maxBoundTargets; ++v)
            if (_resNames[v] == name) {
                _srv[v] = Metal_DX11::ShaderResourceView();
                _rtv[v] = Metal_DX11::RenderTargetView();
                _dsv[v] = Metal_DX11::DepthStencilView();
                _resources[v].reset();
                _resNames[v] = ~0u;
            }
    }
    
    const Metal_DX11::ShaderResourceView*   NamedResources::GetSRV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const
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

        _pimpl->_srv[viewName] = Metal_DX11::ShaderResourceView(*_pimpl->_factory, _pimpl->_resources[resName], window);
        _pimpl->_resNames[viewName] = resName;
        return _pimpl->_srv[viewName].IsGood() ? &_pimpl->_srv[viewName] : nullptr;
    }

    const Metal_DX11::RenderTargetView*     NamedResources::GetRTV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const
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
        
        _pimpl->_rtv[viewName] = Metal_DX11::RenderTargetView(*_pimpl->_factory, _pimpl->_resources[resName], window);
        _pimpl->_resNames[viewName] = resName;
        return _pimpl->_rtv[viewName].IsGood() ? &_pimpl->_rtv[viewName] : nullptr;
    }

    const Metal_DX11::DepthStencilView*     NamedResources::GetDSV(AttachmentName viewName, AttachmentName resName, const TextureViewWindow& window) const
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

        _pimpl->_dsv[viewName] = Metal_DX11::DepthStencilView(*_pimpl->_factory, _pimpl->_resources[resName], window);
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
        _pimpl->_attachments[resName] = {};
        _pimpl->_resources[resName] = resource;
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

#if 0
    void NamedResources::Bind(AttachmentName name, const Metal_DX11::ShaderResourceView& srv)
    {
        if (name >= s_maxBoundTargets) return;
        _pimpl->_srv[name] = srv;
    }

    void NamedResources::Bind(AttachmentName name, const Metal_DX11::RenderTargetView& rtv)
    {
        if (name >= s_maxBoundTargets) return;
        _pimpl->_rtv[name] = rtv;
    }

    void NamedResources::Bind(AttachmentName name, const Metal_DX11::DepthStencilView& dsv)
    {
        if (name >= s_maxBoundTargets) return;
        _pimpl->_dsv[name] = dsv;
    }
#endif

    NamedResources::NamedResources()
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_factory = &Metal_DX11::GetObjectFactory();
        for (unsigned c=0; c<s_maxBoundTargets; ++c)
            _pimpl->_resNames[c] = ~0u;
    }

    NamedResources::~NamedResources()
    {}
}

