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
#include "../Metal/State.h"

namespace RenderCore { namespace Techniques
{

    AttachmentName PassFragmentInterface::DefineAttachment(const AttachmentDesc& request)
    {
        return ~0u;
    }

    void PassFragmentInterface::BindColorAttachment(
        unsigned passIndex,
        unsigned slot,
        AttachmentName attachmentName,
        LoadStore loadOp,
        const ClearValue& clearValue)
    {
    }

    void PassFragmentInterface::BindDepthStencilAttachment(
        unsigned passIndex,
        AttachmentName attachmentName,
        LoadStore loadOp,
        const ClearValue& clearValue)
    {
    }

    void PassFragmentInterface::BindInputAttachment(
        unsigned passIndex,
        unsigned slot,
        AttachmentName attachmentName,
        LoadStore storeOp,
        TextureViewDesc::Aspect aspect)
    {
    }

    PassFragmentInterface::PassFragmentInterface() {}
    PassFragmentInterface::~PassFragmentInterface() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    auto PassFragment::GetSRV(const INamedAttachments& namedAttachments, unsigned passIndex, unsigned slot) const -> Metal::ShaderResourceView*
    {
        return nullptr;
    }

    Metal::ViewportDesc PassFragment::GetFullViewport() const
    {
        return {};
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    void            RenderPassInstance::NextSubpass()
    {
        Metal::BeginNextSubpass(*_attachedContext, *_frameBuffer);
    }

    void            RenderPassInstance::End()
    {
        Metal::EndRenderPass(*_attachedContext);
    }

    class NamedAttachmentsWrapper : public INamedAttachments
    {
    public:
		virtual IResourcePtr GetResource(AttachmentName resName) const;
		virtual const AttachmentDesc* GetDesc(AttachmentName resName) const;

        NamedAttachmentsWrapper(NamedAttachments& namedRes);
        ~NamedAttachmentsWrapper();
    private:
        NamedAttachments* _namedRes;
    };

    IResourcePtr NamedAttachmentsWrapper::GetResource(AttachmentName resName) const
    {
        return _namedRes->GetResource(resName);
    }

    auto NamedAttachmentsWrapper::GetDesc(AttachmentName resName) const -> const AttachmentDesc*
    {
        return _namedRes->GetDesc(resName);
    }

    NamedAttachmentsWrapper::NamedAttachmentsWrapper(NamedAttachments& namedRes)
    : _namedRes(&namedRes) {}
    NamedAttachmentsWrapper::~NamedAttachmentsWrapper() {}

    RenderPassInstance::RenderPassInstance(
        Metal::DeviceContext& context,
        const FrameBufferDesc& layout,
        uint64 hashName,
        NamedAttachments& namedResources,
        const RenderPassBeginDesc& beginInfo)
    {
        // We need to allocate the particular frame buffer we're going to use
        // And then we'll call BeginRenderPass to begin the render pass

        Metal::FrameBufferPool cache;
        _frameBuffer = cache.BuildFrameBuffer(
            Metal::GetObjectFactory(context), layout, 
            namedResources.GetFrameBufferProperties(),
            NamedAttachmentsWrapper(namedResources),
            hashName);
        assert(_frameBuffer);

        Metal::BeginRenderPass(context, *_frameBuffer, layout, namedResources.GetFrameBufferProperties(), beginInfo._clearValues);
        _attachedContext = &context;
    }

    RenderPassInstance::RenderPassInstance(
        IThreadContext& context,
        const FrameBufferDesc& layout,
        uint64 hashName,
        NamedAttachments& namedResources,
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

    class NamedAttachments::Pimpl
    {
    public:
        RenderCore::IResourcePtr    _resources[s_maxBoundTargets];
        AttachmentDesc              _attachments[s_maxBoundTargets];
        AttachmentName              _resNames[s_maxBoundTargets];

        FrameBufferProperties       _props;

        Metal::ObjectFactory*      _factory;

        bool BuildAttachment(AttachmentName attach);
    };

    static bool Equal(const AttachmentDesc& lhs, const AttachmentDesc& rhs)
    {
        return lhs._dimsMode == rhs._dimsMode
            && lhs._width == rhs._width
            && lhs._height == rhs._height
            && lhs._arrayLayerCount == rhs._arrayLayerCount
            && lhs._format == rhs._format
            && lhs._flags == rhs._flags
            ;
    }

    bool NamedAttachments::Pimpl::BuildAttachment(AttachmentName attach)
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

    auto NamedAttachments::GetDesc(AttachmentName resName) const -> const AttachmentDesc*
    {
        if (resName >= s_maxBoundTargets) return nullptr;
        return &_pimpl->_attachments[resName];
    }
    
    IResourcePtr NamedAttachments::GetResource(AttachmentName resName) const
    {
		return _pimpl->_resources[resName];
	}

    void NamedAttachments::DefineAttachment(AttachmentName name, const AttachmentDesc& request)
    {
        assert(name < s_maxBoundTargets);
        if (!Equal(_pimpl->_attachments[name], request)) {
            _pimpl->_resources[name].reset();
            _pimpl->_attachments[name] = request;
        }
    }

    void NamedAttachments::Bind(AttachmentName resName, const IResourcePtr& resource)
    {
        assert(resName < s_maxBoundTargets);
        if (_pimpl->_resources[resName] == resource) return;

        // _pimpl->InvalidateAttachment(resName);

        auto desc = Metal::ExtractDesc(*resource);
        _pimpl->_attachments[resName] = 
            {
                desc._textureDesc._format,
                (float)desc._textureDesc._width, (float)desc._textureDesc._height,
                0u,
                TextureViewDesc::UndefinedAspect,
                RenderCore::AttachmentDesc::DimensionsMode::Absolute,
                  ((desc._bindFlags & BindFlag::RenderTarget) ? AttachmentDesc::Flags::RenderTarget : 0u)
                | ((desc._bindFlags & BindFlag::ShaderResource) ? AttachmentDesc::Flags::ShaderResource : 0u)
                | ((desc._bindFlags & BindFlag::DepthStencil) ? AttachmentDesc::Flags::DepthStencil : 0u)
                | ((desc._bindFlags & BindFlag::TransferSrc) ? AttachmentDesc::Flags::TransferSource : 0u)
            };

        _pimpl->_resources[resName] = resource;
        _pimpl->_resNames[resName] = resName;
    }

    void NamedAttachments::Unbind(AttachmentName resName)
    {
        assert(resName < s_maxBoundTargets);
        // _pimpl->InvalidateAttachment(resName);
    }

    void NamedAttachments::Bind(FrameBufferProperties props)
    {
        bool xyChanged = 
               props._outputWidth != _pimpl->_props._outputWidth
            || props._outputHeight != _pimpl->_props._outputHeight;
        bool samplesChanged = 
                props._samples._sampleCount != _pimpl->_props._samples._sampleCount
            ||  props._samples._samplingQuality != _pimpl->_props._samples._samplingQuality;
        if (!xyChanged && !samplesChanged) return;

        /*
		if (xyChanged)
            for (unsigned c=0; c<s_maxBoundTargets; ++c)
                if (_pimpl->_attachments[c]._dimsMode == AttachmentDesc::DimensionsMode::OutputRelative)
                    _pimpl->InvalidateAttachment(c);

        if (samplesChanged) // Invalidate all resources and views that depend on the multisample state
            for (unsigned c=0; c<s_maxBoundTargets; ++c)
                if (_pimpl->_attachments[c]._flags & AttachmentDesc::Flags::Multisampled)
                    _pimpl->InvalidateAttachment(c);
		*/

        _pimpl->_props = props;
    }

    const FrameBufferProperties& NamedAttachments::GetFrameBufferProperties() const
    {
        return _pimpl->_props;
    }

    IteratorRange<const AttachmentDesc*> NamedAttachments::GetDescriptions() const
    {
        return MakeIteratorRange(_pimpl->_attachments);
    }

    NamedAttachments::NamedAttachments()
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_factory = &Metal::GetObjectFactory();
        for (unsigned c=0; c<s_maxBoundTargets; ++c) {
            _pimpl->_resNames[c] = ~0u;
            _pimpl->_attachments[c] = {};
        }
        _pimpl->_props = {0u, 0u, TextureSamples::Create()};
    }

    NamedAttachments::~NamedAttachments()
    {}
    
///////////////////////////////////////////////////////////////////////////////////////////////////

    FrameBufferDesc BuildFrameBufferDesc(
        /* in/out */ NamedAttachments& namedResources,
        /* out */ std::vector<PassFragment>& boundFragments,
        /* int */ IteratorRange<const PassFragmentInterface*> fragments)
    {
        return FrameBufferDesc {};
    }

}}

