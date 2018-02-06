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
#include "../ResourceUtils.h"

namespace RenderCore { namespace Techniques
{

    AttachmentName FrameBufferDescFragment::DefineAttachment(
        Direction direction,
        uint64_t semantic,
        const AttachmentDesc& request)
    {
        auto name = _nextAttachment++;
        _attachments.push_back({name, {direction, semantic, request}});
        return name;
    }

    void FrameBufferDescFragment::AddSubpass(SubpassDesc&& subpass)
    {
        _subpasses.emplace_back(std::move(subpass));
    }

    FrameBufferDescFragment::FrameBufferDescFragment() : _nextAttachment(0) {}
    FrameBufferDescFragment::~FrameBufferDescFragment() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    auto PassFragment::GetSRV(const AttachmentPool& namedAttachments, unsigned passIndex, unsigned slot) const -> Metal::ShaderResourceView*
    {
        auto i = std::find_if(
            _inputAttachmentMapping.begin(), _inputAttachmentMapping.end(),
            [passIndex, slot](const std::pair<PassAndSlot, AttachmentName>& p) {
                return p.first == std::make_pair(passIndex, slot);
            });
        if (i == _inputAttachmentMapping.end())
            return {};

        return namedAttachments.GetSRV(i->second);
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

        NamedAttachmentsWrapper(AttachmentPool& namedRes);
        ~NamedAttachmentsWrapper();
    private:
        AttachmentPool* _namedRes;
    };

    IResourcePtr NamedAttachmentsWrapper::GetResource(AttachmentName resName) const
    {
        return _namedRes->GetResource(resName);
    }

    auto NamedAttachmentsWrapper::GetDesc(AttachmentName resName) const -> const AttachmentDesc*
    {
        return _namedRes->GetDesc(resName);
    }

    NamedAttachmentsWrapper::NamedAttachmentsWrapper(AttachmentPool& namedRes)
    : _namedRes(&namedRes) {}
    NamedAttachmentsWrapper::~NamedAttachmentsWrapper() {}

    std::shared_ptr<INamedAttachments> MakeNamedAttachmentsWrapper(AttachmentPool& namedRes)
    {
        return std::make_shared<NamedAttachmentsWrapper>(namedRes);
    }

    RenderPassInstance::RenderPassInstance(
        Metal::DeviceContext& context,
        const FrameBufferDesc& layout,
        uint64 hashName,
        AttachmentPool& namedResources,
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
        AttachmentPool& namedResources,
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

    class AttachmentPool::Pimpl
    {
    public:
        IResourcePtr		_attachments[s_maxBoundTargets];
        AttachmentDesc		_attachmentDescs[s_maxBoundTargets];
		
		ViewPool<Metal::ShaderResourceView> _srvPool;

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

    bool AttachmentPool::Pimpl::BuildAttachment(AttachmentName attach)
    {
        assert(attach<s_maxBoundTargets);
        _attachments[attach].reset();
        const auto& a = _attachmentDescs[attach];

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
        _attachments[attach] = Metal::CreateResource(*_factory, desc);
        return true;
    }

    auto AttachmentPool::GetDesc(AttachmentName resName) const -> const AttachmentDesc*
    {
        if (resName >= s_maxBoundTargets) return nullptr;
        return &_pimpl->_attachmentDescs[resName];
    }
    
    IResourcePtr AttachmentPool::GetResource(AttachmentName resName) const
    {
		if (resName >= s_maxBoundTargets) return nullptr;
		if (!_pimpl->_attachments[resName])
			_pimpl->BuildAttachment(resName);
		return _pimpl->_attachments[resName];
	}

	Metal::ShaderResourceView* AttachmentPool::GetSRV(AttachmentName resName, const TextureViewDesc& window) const
	{
		if (resName >= s_maxBoundTargets) return {};
		if (!_pimpl->_attachments[resName]) return {};
		return _pimpl->_srvPool.GetView(_pimpl->_attachments[resName], window);
	}

    void AttachmentPool::DefineAttachment(AttachmentName name, const AttachmentDesc& request)
    {
        assert(name < s_maxBoundTargets);
        if (!Equal(_pimpl->_attachmentDescs[name], request)) {
            _pimpl->_attachments[name].reset();
            _pimpl->_attachmentDescs[name] = request;
        }
    }

    void AttachmentPool::Bind(AttachmentName resName, const IResourcePtr& resource)
    {
        assert(resName < s_maxBoundTargets);
        if (_pimpl->_attachments[resName] == resource) return;

		_pimpl->_srvPool.Erase(*_pimpl->_attachments[resName]);

        auto desc = Metal::ExtractDesc(*resource);
        _pimpl->_attachmentDescs[resName] = 
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

        _pimpl->_attachments[resName] = resource;
    }

    void AttachmentPool::Unbind(AttachmentName resName)
    {
        assert(resName < s_maxBoundTargets);
		if (_pimpl->_attachments[resName])
			_pimpl->_srvPool.Erase(*_pimpl->_attachments[resName]);
    }

    void AttachmentPool::Bind(FrameBufferProperties props)
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
                if (_pimpl->_attachmentDescs[c]._dimsMode == AttachmentDesc::DimensionsMode::OutputRelative) {
					if (_pimpl->_attachments[c])
						_pimpl->_srvPool.Erase(*_pimpl->_attachments[c]);
					_pimpl->_attachments[c].reset();
				}

        if (samplesChanged) // Invalidate all resources and views that depend on the multisample state
            for (unsigned c=0; c<s_maxBoundTargets; ++c)
                if (_pimpl->_attachmentDescs[c]._flags & AttachmentDesc::Flags::Multisampled) {
                    if (_pimpl->_attachments[c])
						_pimpl->_srvPool.Erase(*_pimpl->_attachments[c]);
					_pimpl->_attachments[c].reset();
				}

        _pimpl->_props = props;
    }

    const FrameBufferProperties& AttachmentPool::GetFrameBufferProperties() const
    {
        return _pimpl->_props;
    }

    IteratorRange<const AttachmentDesc*> AttachmentPool::GetDescriptions() const
    {
        return MakeIteratorRange(_pimpl->_attachmentDescs);
    }

    AttachmentPool::AttachmentPool()
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_factory = &Metal::GetObjectFactory();
        for (unsigned c=0; c<s_maxBoundTargets; ++c) {
            _pimpl->_attachmentDescs[c] = {};
        }
        _pimpl->_props = {0u, 0u, TextureSamples::Create()};
    }

    AttachmentPool::~AttachmentPool()
    {}
    
///////////////////////////////////////////////////////////////////////////////////////////////////

    FrameBufferDesc BuildFrameBufferDesc(
        /* in/out */ AttachmentPool& namedResources,
        /* out */ std::vector<PassFragment>& boundFragments,
        /* int */ IteratorRange<const FrameBufferDescFragment*> fragments)
    {
        return {};
    }

    static bool IsCompatible(const AttachmentDesc& lhs, const AttachmentDesc& rhs)
    {
        return
            ((lhs._format == rhs._format) || (lhs._format == Format::Unknown) || (rhs._format == Format::Unknown))
            && lhs._width == rhs._width
            && lhs._height == rhs._height
            && lhs._arrayLayerCount == rhs._arrayLayerCount
            && lhs._defaultAspect == rhs._defaultAspect
            && lhs._dimsMode == rhs._dimsMode
            && lhs._flags == rhs._flags
            ;
    }

    static AttachmentName Remap(const std::vector<std::pair<AttachmentName, AttachmentName>>& remapping, AttachmentName name)
    {
        if (name == ~0u) return ~0u;
        auto i = LowerBound(remapping, name);
        assert(i!=remapping.end() && i->first == name);
        return i->second;
    }

    FrameBufferDescFragment MergeFragments(IteratorRange<const FrameBufferDescFragment*> fragments)
    {
        // Merge together the input fragments to create the final output
        // Each fragment defines an input/output interface. We need to bind these
        // together (along with the temporaries) to create a single cohesive render pass.
        // Where we can reuse the same temporary multiple times, we should do so
        FrameBufferDescFragment result;
        if (!fragments.size()) return result;

        struct UnconsumedInput
        {
            uint64_t        _semantic;
            AttachmentDesc  _desc;
            AttachmentName  _finalName;
        };
        std::vector<UnconsumedInput> originalInput;
        originalInput.push_back(
            {
                Hash64("color"),
                AttachmentDesc { Format::R8G8B8A8_UNORM },
                result.DefineAttachment(FrameBufferDescFragment::Direction::Temporary, Hash64("color"), AttachmentDesc { Format::R8G8B8A8_UNORM })
            });
        originalInput.push_back(
            {
                Hash64("depth"),
                AttachmentDesc { Format::D24_UNORM_S8_UINT },
                result.DefineAttachment(FrameBufferDescFragment::Direction::Temporary, Hash64("depth"), AttachmentDesc { Format::R8G8B8A8_UNORM })
            });

        std::vector<UnconsumedInput> unconsumedInputs = originalInput;

        struct AvailableTemporary
        {
            AttachmentDesc  _desc;
            AttachmentName  _finalName;
        };
        std::vector<AvailableTemporary> availableTemporaries;

        std::vector<PassFragment> fragmentRemapping;

        for (auto f=fragments.begin(); f!=fragments.end(); ++f) {
            std::vector<UnconsumedInput> newUnconsumedInputs;
            std::vector<AvailableTemporary> newAvailableTemporaries;
            std::vector<std::pair<AttachmentName, AttachmentName>> attachmentRemapping;

            /////////////////////////////////////////////////////////////////////////////
            for (const auto& interf:f->_attachments) {
                AttachmentName reboundName = ~0u;

                auto interfaceAttachment = interf.second;
                if (interfaceAttachment._direction == FrameBufferDescFragment::Direction::In || interfaceAttachment._direction == FrameBufferDescFragment::Direction::InOut) {
                    // find something compatible in the unconsumed inputs array
                    auto compat = std::find_if(
                        unconsumedInputs.begin(), unconsumedInputs.end(),
                        [&interfaceAttachment](const UnconsumedInput& input) {
                            return input._semantic == interfaceAttachment._semantic;
                        });
                    if (compat == unconsumedInputs.end())
                        Throw(::Exceptions::BasicLabel("Couldn't bind renderpass fragment input request"));

                    if (!IsCompatible(compat->_desc, interfaceAttachment._desc))
                        Throw(::Exceptions::BasicLabel("Incompatible attachment descriptions in renderpass fragment resolve"));

                    reboundName = compat->_finalName;
                    if (interfaceAttachment._direction != FrameBufferDescFragment::Direction::InOut) {
                        newAvailableTemporaries.push_back({compat->_desc, compat->_finalName});
                        unconsumedInputs.erase(compat);
                    }

                } else if (interfaceAttachment._direction == FrameBufferDescFragment::Direction::Out) {
                    // define a new output buffer, or reuse something that we can reuse
                    auto available = std::find_if(
                        availableTemporaries.begin(), availableTemporaries.end(),
                        [&interfaceAttachment](const AvailableTemporary& temp) {
                            return IsCompatible(temp._desc, interfaceAttachment._desc);
                        });
                    if (available != availableTemporaries.end()) {
                        reboundName = available->_finalName;
                        newUnconsumedInputs.push_back({interfaceAttachment._semantic, available->_desc, available->_finalName});
                    } else {
                        reboundName = result.DefineAttachment(FrameBufferDescFragment::Direction::Temporary, interfaceAttachment._semantic, interfaceAttachment._desc);
                        newUnconsumedInputs.push_back({interfaceAttachment._semantic, interfaceAttachment._desc, reboundName});
                    }
                } else if (interfaceAttachment._direction == FrameBufferDescFragment::Direction::Temporary) {
                    auto available = std::find_if(
                        availableTemporaries.begin(), availableTemporaries.end(),
                        [&interfaceAttachment](const AvailableTemporary& temp) {
                            return IsCompatible(temp._desc, interfaceAttachment._desc);
                        });
                    if (available != availableTemporaries.end()) {
                        reboundName = available->_finalName;
                        newAvailableTemporaries.push_back(*available);
                        availableTemporaries.erase(available);
                    } else {
                        reboundName = result.DefineAttachment(FrameBufferDescFragment::Direction::Temporary, interfaceAttachment._semantic, interfaceAttachment._desc);
                    }
                } else {
                    assert(0);
                }

                attachmentRemapping.push_back({interf.first, reboundName});
            }

            /////////////////////////////////////////////////////////////////////////////

                // setup the subpasses & PassFragment
            std::sort(attachmentRemapping.begin(), attachmentRemapping.end(), CompareFirst<AttachmentName, AttachmentName>());
            PassFragment passFragment;
            for (unsigned p=0; p<(unsigned)f->_subpasses.size(); ++p) {
                SubpassDesc newSubpass = f->_subpasses[p];
                for (auto&a:newSubpass._output)
                    a._resourceName = Remap(attachmentRemapping, a._resourceName);
                newSubpass._depthStencil._resourceName = Remap(attachmentRemapping, newSubpass._depthStencil._resourceName);
                for (unsigned c=0; c<(unsigned)newSubpass._input.size(); ++c) {
                    newSubpass._input[c]._resourceName = Remap(attachmentRemapping, newSubpass._input[c]._resourceName);
                    passFragment._inputAttachmentMapping.push_back({{p, c}, newSubpass._input[c]._resourceName});
                }
                for (auto&a:newSubpass._preserve)
                    a._resourceName = Remap(attachmentRemapping, a._resourceName);
                for (auto&a:newSubpass._resolve)
                    a._resourceName = Remap(attachmentRemapping, a._resourceName);
                result.AddSubpass(std::move(newSubpass));
            }

            /////////////////////////////////////////////////////////////////////////////

            unconsumedInputs.insert(unconsumedInputs.end(), newUnconsumedInputs.begin(), newUnconsumedInputs.end());
            availableTemporaries.insert(availableTemporaries.end(), newAvailableTemporaries.begin(), newAvailableTemporaries.end());

        }

        // The remaining "unconsumedInputs" are now the outputs
        // We need to set the direction flags on all of the defined attachments
        for (auto& attach:result._attachments) {
            auto i = std::find_if(
                originalInput.begin(), originalInput.end(),
                [&attach](const UnconsumedInput& e) { return e._finalName == attach.first; });
            bool isInput = i != originalInput.end();

            auto o = std::find_if(
                unconsumedInputs.begin(), unconsumedInputs.end(),
                [&attach](const UnconsumedInput& e) { return e._finalName == attach.first; });
            bool isOutput = o != unconsumedInputs.end();

            if (isInput && isOutput) attach.second._direction = FrameBufferDescFragment::Direction::InOut;
            else if (isInput) attach.second._direction = FrameBufferDescFragment::Direction::In;
            else if (isOutput) attach.second._direction = FrameBufferDescFragment::Direction::Out;
        }

        return result;
    }

}}

