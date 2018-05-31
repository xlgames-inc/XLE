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
#include "../Metal/Resource.h"
#include "../Format.h"
#include "../ResourceUtils.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/ArithmeticUtils.h"
#include <cmath>

#include <cmath>

namespace RenderCore { namespace Techniques
{

    AttachmentName FrameBufferDescFragment::DefineAttachment(
        uint64_t semantic,
        const AttachmentDesc& request)
    {
        auto name = _nextAttachment++;
        _attachments.push_back({name, {semantic, request}});
        return name;
    }

    void FrameBufferDescFragment::AddSubpass(SubpassDesc&& subpass)
    {
        _subpasses.emplace_back(std::move(subpass));
    }

    FrameBufferDescFragment::FrameBufferDescFragment() : _nextAttachment(0) {}
    FrameBufferDescFragment::~FrameBufferDescFragment() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    auto RenderPassFragment::GetSRV(unsigned slot) const -> Metal::ShaderResourceView*
    {
        auto passIndex = _currentPassIndex;
        auto i = std::find_if(
            _mapping->_inputAttachmentMapping.begin(), _mapping->_inputAttachmentMapping.end(),
            [passIndex, slot](const std::pair<FrameBufferFragmentMapping::PassAndSlot, AttachmentName>& p) {
                return p.first == std::make_pair(passIndex, slot);
            });
        if (i == _mapping->_inputAttachmentMapping.end())
            return {};

        assert(_attachmentPool);
        return _attachmentPool->GetSRV(i->second);
    }

    void RenderPassFragment::NextSubpass()
    {
        _rpi->NextSubpass();
        ++_currentPassIndex;
    }

    RenderPassFragment::RenderPassFragment(
        RenderPassInstance& rpi,
        const FrameBufferFragmentMapping& mapping,
        AttachmentPool& attachmentPool)
    : _rpi(&rpi), _mapping(&mapping), _attachmentPool(&attachmentPool)
    , _currentPassIndex(0)
    {
    }

    RenderPassFragment::~RenderPassFragment()
    {
        if (_mapping->_subpassCount)
            _rpi->NextSubpass();
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    void            RenderPassInstance::NextSubpass()
    {
        Metal::EndSubpass(*_attachedContext);
        Metal::BeginNextSubpass(*_attachedContext, *_frameBuffer);
    }

    void            RenderPassInstance::End()
    {
        Metal::EndSubpass(*_attachedContext);
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

////////////////////////////////////////////////////////////////////////////////////////////////////

    class FrameBufferPool::Pimpl
    {
    public:
        class Entry
        {
        public:
            uint64_t _hash = 0;
            unsigned _tickId = 0;
            std::shared_ptr<Metal::FrameBuffer> _fb;
        };
        Entry _entries[5];
        unsigned _currentTickId = 0;

        void IncreaseTickId();
    };

    void FrameBufferPool::Pimpl::IncreaseTickId()
    {
        // look for old FBs, and evict; then just increase the tick id
        const unsigned evictionRange = 10;
        for (auto&e:_entries)
            if ((e._tickId + evictionRange) < _currentTickId)
                e._fb.reset();
        ++_currentTickId;
    }

    std::shared_ptr<Metal::FrameBuffer> FrameBufferPool::BuildFrameBuffer(
        Metal::ObjectFactory& factory,
        const FrameBufferDesc& desc,
        AttachmentPool& attachmentPool)
    {
        // 1. Find the attachments referenced by the given framebuffer
        // 2. Ensure those attachments are instantiated in attachmentPool
        // 3. Build a hash from the GUIDs of the referenced attachments
        // 4. Find a framebuffer in the pool with the right hash, or create a new one
        std::vector<AttachmentName> uniqueAttachments;
        for (const auto&s:desc.GetSubpasses()) {
            for (const auto&a:s._output) uniqueAttachments.push_back(a._resourceName);
            for (const auto&a:s._input) uniqueAttachments.push_back(a._resourceName);
            for (const auto&a:s._preserve) uniqueAttachments.push_back(a._resourceName);
            for (const auto&a:s._resolve) uniqueAttachments.push_back(a._resourceName);
            if (s._depthStencil._resourceName != ~0u)
                uniqueAttachments.push_back(s._depthStencil._resourceName);
        }
        // (make unique set)
        uniqueAttachments.erase(
            std::unique(uniqueAttachments.begin(), uniqueAttachments.end()),
            uniqueAttachments.end());

        uint64_t hashValue = DefaultSeed64;
        for (const auto a:uniqueAttachments)
            hashValue = HashCombine(attachmentPool.GetResource(a)->GetGUID(), hashValue);
        assert(hashValue != 0);     // using 0 has a sentinel, so this will cause some problems

        unsigned earliestEntry = 0;
        unsigned tickIdOfEarliestEntry = ~0u;
        for (unsigned c=0; c<dimof(_pimpl->_entries); ++c) {
            if (_pimpl->_entries[c]._hash == hashValue) {
                _pimpl->_entries[c]._tickId = _pimpl->_currentTickId;
                _pimpl->IncreaseTickId();
                return _pimpl->_entries[c]._fb;
            }
            if (_pimpl->_entries[c]._tickId < tickIdOfEarliestEntry) {
                tickIdOfEarliestEntry = _pimpl->_entries[c]._tickId;
                earliestEntry = c;
            }
        }

        // Can't find it; we're just going to overwrite the oldest entry with a new one
        assert(earliestEntry < dimof(_pimpl->_entries));
        if (_pimpl->_entries[earliestEntry]._fb) {
            Log(Warning) << "Overwriting tail in FrameBufferPool(). There may be too many different framebuffers required from the same pool" << std::endl;
        }

        auto namedAttachments = MakeNamedAttachmentsWrapper(attachmentPool);
        _pimpl->_entries[earliestEntry]._fb = std::make_shared<Metal::FrameBuffer>(
            Metal::GetObjectFactory(),
            desc, *namedAttachments);
        _pimpl->_entries[earliestEntry]._tickId = _pimpl->_currentTickId;
        _pimpl->_entries[earliestEntry]._hash = hashValue;
        _pimpl->IncreaseTickId();
        return _pimpl->_entries[earliestEntry]._fb;
    }

    void FrameBufferPool::Reset()
    {
        // just destroy and recreate "Pimpl" to start again from scratch
        _pimpl.reset();
        _pimpl = std::make_unique<Pimpl>();
    }

    FrameBufferPool::FrameBufferPool()
    {
        _pimpl = std::make_unique<Pimpl>();
    }

    FrameBufferPool::~FrameBufferPool()
    {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    RenderPassInstance::RenderPassInstance(
        IThreadContext& context,
        const std::shared_ptr<Metal::FrameBuffer>& frameBuffer,
        const FrameBufferDesc& layout,
        AttachmentPool& namedResources,
        const RenderPassBeginDesc& beginInfo)
    {
        _attachedContext = Metal::DeviceContext::Get(context).get();
        _frameBuffer = frameBuffer;
        Metal::BeginRenderPass(*_attachedContext, *_frameBuffer, layout, namedResources.GetFrameBufferProperties(), beginInfo._clearValues);
    }
    
    RenderPassInstance::~RenderPassInstance() 
    {
        End();
    }

    RenderPassInstance::RenderPassInstance(RenderPassInstance&& moveFrom)
    : _frameBuffer(std::move(moveFrom._frameBuffer))
    , _attachedContext(moveFrom._attachedContext)
    {}

    RenderPassInstance& RenderPassInstance::operator=(RenderPassInstance&& moveFrom)
    {
        _frameBuffer = std::move(moveFrom._frameBuffer);
        _attachedContext = moveFrom._attachedContext;
        return *this;
    }

    RenderPassInstance::RenderPassInstance()
    {
        _attachedContext = nullptr;
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
		assert(_attachments[attach]);
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
		assert(_pimpl->_attachments[resName]);
		return _pimpl->_attachments[resName];
	}

	Metal::ShaderResourceView* AttachmentPool::GetSRV(AttachmentName resName, const TextureViewDesc& window) const
	{
		if (resName >= s_maxBoundTargets) return nullptr;
		if (!_pimpl->_attachments[resName]) return nullptr;
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

        auto& attach = _pimpl->_attachments[resName];
        if (attach) {
		    _pimpl->_srvPool.Erase(*attach);
            attach = nullptr;
        }

        auto desc = resource->GetDesc();
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

        attach = resource;
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
        AttachmentPool& namedResources,
        FrameBufferDescFragment&& fragment)
    {
        // Define all of the attachments in the attachment pool
        for (const auto& attachment:fragment._attachments)
            namedResources.DefineAttachment(attachment.first, attachment.second._desc);

        // Generate the final FrameBufferDesc by moving the subpasses out of the fragment
        // Usually this function is called as a final step when converting a number of fragments
        // into a final FrameBufferDesc, so it makes sense to move the subpasses from the input
        return FrameBufferDesc{std::move(fragment._subpasses)};
    }

    static bool IsCompatible(const AttachmentDesc& testAttachment, const AttachmentDesc& request)
    {
        return
            ( (testAttachment._format == request._format) || (testAttachment._format == Format::Unknown) || (request._format == Format::Unknown) )
            && testAttachment._width == request._width && testAttachment._height == request._height
            && testAttachment._arrayLayerCount == request._arrayLayerCount
            && ( (testAttachment._defaultAspect == request._defaultAspect) || (testAttachment._defaultAspect == TextureViewDesc::Aspect::UndefinedAspect) || (request._defaultAspect == TextureViewDesc::Aspect::UndefinedAspect) )
            && testAttachment._dimsMode == request._dimsMode
            && (testAttachment._flags & request._flags) == request._flags
            ;
    }

    static AttachmentName Remap(const std::vector<std::pair<AttachmentName, AttachmentName>>& remapping, AttachmentName name)
    {
        if (name == ~0u) return ~0u;
        auto i = LowerBound(remapping, name);
        assert(i!=remapping.end() && i->first == name);
        return i->second;
    }

    struct DirectionFlags
    {
        enum Bits
        {
            Reference = 1<<0,
            Load = 1<<1,
            Store = 1<<2
        };
        using BitField = unsigned;
    };

    static bool HasRetain(LoadStore loadStore)
    {
        return  loadStore == LoadStore::Retain
            ||  loadStore == LoadStore::DontCare_RetainStencil
            ||  loadStore == LoadStore::Retain_RetainStencil
            ||  loadStore == LoadStore::Clear_RetainStencil
            ||  loadStore == LoadStore::Retain_ClearStencil
            ;
    }

    static DirectionFlags::BitField GetLoadDirectionFlags(const SubpassDesc& p, AttachmentName attachment)
    {
        DirectionFlags::BitField result = 0;
        for (const auto&a:p._output)
            if (a._resourceName == attachment) {
                result |= DirectionFlags::Reference;
                if (HasRetain(a._loadFromPreviousPhase))
                    result |= DirectionFlags::Load;
            }
        if (p._depthStencil._resourceName == attachment) {
            result |= DirectionFlags::Reference;
            if (HasRetain(p._depthStencil._loadFromPreviousPhase))
                result |= DirectionFlags::Load;
        }
        for (const auto&a:p._input)
            if (a._resourceName == attachment)
                result |= DirectionFlags::Reference | DirectionFlags::Load;
        return result;
    }

    static DirectionFlags::BitField GetStoreDirectionFlags(const SubpassDesc& p, AttachmentName attachment)
    {
        DirectionFlags::BitField result = 0;
        for (const auto&a:p._output)
            if (a._resourceName == attachment) {
                result |= DirectionFlags::Reference;
                if (HasRetain(a._storeToNextPhase))
                    result |= DirectionFlags::Store;
            }
        if (p._depthStencil._resourceName == attachment) {
            result |= DirectionFlags::Reference;
            if (HasRetain(p._depthStencil._storeToNextPhase))
                result |= DirectionFlags::Store;
        }
        return result;
    }

    static AttachmentName NextName(IteratorRange<const PreregisteredAttachment*> attachments0, IteratorRange<const PreregisteredAttachment*> attachments1)
    {
        // find the lowest name not used by any of the attachments
        uint64_t bitField = 0;
        for (const auto& a:attachments0) {
            assert(a._name < 64);
            assert(!(bitField & (1ull << uint64_t(a._name))));
            bitField |= 1ull << uint64_t(a._name);
        }
        for (const auto& a:attachments1) {
            assert(a._name < 64);
            assert(!(bitField & (1ull << uint64_t(a._name))));
            bitField |= 1ull << uint64_t(a._name);
        }
        // Find the position of the least significant bit set in the inverse
        // That is the smallest number less than 64 that hasn't been used yet
        return xl_ctz8(~bitField);
    }

    static uint64_t GetSemantic(IteratorRange<const std::pair<AttachmentName, FrameBufferDescFragment::Attachment>*> attachments, RenderCore::AttachmentName attachment)
    {
        for (const auto& a:attachments)
            if (a.first == attachment)
                return a.second._semantic;
        return 0;
    }

    std::pair<FrameBufferDescFragment, std::vector<FrameBufferFragmentMapping>> MergeFragments(
        IteratorRange<const PreregisteredAttachment*> preregisteredInputs,
        IteratorRange<const FrameBufferDescFragment*> fragments)
    {
        // Merge together the input fragments to create the final output
        // Each fragment defines an input/output interface. We need to bind these
        // together (along with the temporaries) to create a single cohesive render pass.
        // Where we can reuse the same temporary multiple times, we should do so
        FrameBufferDescFragment result;
        std::vector<FrameBufferFragmentMapping> fragmentRemapping;

        if (!fragments.size()) return { std::move(result), std::move(fragmentRemapping) };

        // Some attachments will need the "ShaderResource" flag set, so that we can use them as an
        // input to the a shader. This propagates backwards, since once any data is written to a
        // non-shader resource attachment, it can never be moved to a shader resource attachment.
        // Hense we must ensure that any attachments that will ever be used as a shader resource
        // always have that flag set.
        std::vector<uint64_t> shaderResourceSemantics;
        for (const auto& f:fragments)
            for (const auto& p:f._subpasses)
                for (const auto& a:p._input)
                    if (HasRetain(a._loadFromPreviousPhase)) {
                        auto semantic = GetSemantic(MakeIteratorRange(f._attachments), a._resourceName);
                        if (semantic)
                            shaderResourceSemantics.push_back(semantic);
                    }
        std::sort(shaderResourceSemantics.begin(), shaderResourceSemantics.end());
        shaderResourceSemantics.erase(std::unique(shaderResourceSemantics.begin(), shaderResourceSemantics.end()), shaderResourceSemantics.end());

        std::vector<PreregisteredAttachment> workingAttachments { preregisteredInputs.begin(), preregisteredInputs.end() };
        for (auto f=fragments.begin(); f!=fragments.end(); ++f) {
            std::vector<PreregisteredAttachment> newWorkingAttachments;
            std::vector<std::pair<AttachmentName, AttachmentName>> attachmentRemapping;

            /////////////////////////////////////////////////////////////////////////////
            for (const auto& interf:f->_attachments) {
                AttachmentName reboundName = ~0u;

                DirectionFlags::BitField directionFlags = 0;
                // Look through the load/store values in the subpasses to find the "direction" for
                // this attachment. When deciding on the load flags, we must look for the first
                // subpass that references the attachment; for the store flags we look for the last
                // subpass
                for (auto p=f->_subpasses.begin(); p!=f->_subpasses.end(); ++p) {
                    auto subpassDirectionFlags = GetLoadDirectionFlags(*p, interf.first);
                    if (subpassDirectionFlags != 0) {
                        directionFlags |= subpassDirectionFlags;
                        break;
                    }
                }
                for (auto p=f->_subpasses.rbegin(); p!=f->_subpasses.rend(); ++p) {
                    auto subpassDirectionFlags = GetStoreDirectionFlags(*p, interf.first);
                    if (subpassDirectionFlags != 0) {
                        directionFlags |= subpassDirectionFlags;
                        break;
                    }
                }
                assert(directionFlags != 0);

                auto interfaceAttachment = interf.second;
                // toggle on the "ShaderResource" flag, if necessary
                if (std::find(shaderResourceSemantics.begin(), shaderResourceSemantics.end(), interfaceAttachment._semantic) != shaderResourceSemantics.end()) {
                    interfaceAttachment._desc._flags |= AttachmentDesc::Flags::Enum::ShaderResource;
                }

                if (directionFlags & DirectionFlags::Load) {

                    // We're expecting a buffer that already has some initialized contents. Look for
                    // something matching in our working attachments array
                    auto compat = std::find_if(
                        workingAttachments.begin(), workingAttachments.end(),
                        [&interfaceAttachment](const PreregisteredAttachment& input) {
                            return (input._state == PreregisteredAttachment::State::Initialized)
                                && (input._semantic == interfaceAttachment._semantic)
                                && IsCompatible(input._desc, interfaceAttachment._desc);
                        });
                    if (compat == workingAttachments.end())
                        Throw(::Exceptions::BasicLabel("Couldn't bind renderpass fragment input request"));

                    reboundName = compat->_name;

                    // remove from the working attachments and push back in it's new state
                    auto newState = *compat;
                    newState._state = (directionFlags & DirectionFlags::Store) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized;
                    workingAttachments.erase(compat);
                    newWorkingAttachments.push_back(newState);

                } else {

                    // define a new output buffer, or reuse something that we can reuse
                    auto compat = std::find_if(
                        workingAttachments.begin(), workingAttachments.end(),
                        [&interfaceAttachment](const PreregisteredAttachment& input) {
                            return (input._state == PreregisteredAttachment::State::Uninitialized)
                                && IsCompatible(input._desc, interfaceAttachment._desc);
                        });

                    if (compat == workingAttachments.end()) {
                        // Technically we could do a second pass looking for some initialized attachment
                        // we could write over -- but that would lead to other complications. Let's just
                        // create something new.

                        reboundName = NextName(MakeIteratorRange(workingAttachments), MakeIteratorRange(newWorkingAttachments));

                        // We can steal the settings from an existing attachment with the same semantic
                        // name, if necessary
                        auto desc = interfaceAttachment._desc;
                        auto sameSemantic = std::find_if(
                            workingAttachments.begin(), workingAttachments.end(),
                            [&interfaceAttachment](const PreregisteredAttachment& input) {
                                return (input._semantic == interfaceAttachment._semantic);
                            });
                        if (sameSemantic != workingAttachments.end()) {
                            if (desc._format == Format::Unknown) desc._format = sameSemantic->_desc._format;
                            if (desc._defaultAspect == TextureViewDesc::Aspect::UndefinedAspect) desc._defaultAspect = sameSemantic->_desc._defaultAspect;
                        }

                        PreregisteredAttachment newState {
                            reboundName,
                            interfaceAttachment._semantic,
                            desc,
                            (directionFlags & DirectionFlags::Store) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized
                        };
                        newWorkingAttachments.push_back(newState);

                    } else {

                        reboundName = compat->_name;

                        // remove from the working attachments and push back in it's new state
                        auto newState = *compat;
                        newState._state = (directionFlags & DirectionFlags::Store) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized;
                        workingAttachments.erase(compat);
                        newWorkingAttachments.push_back(newState);

                    }

                }

                attachmentRemapping.push_back({interf.first, reboundName});
            }

            /////////////////////////////////////////////////////////////////////////////

                // setup the subpasses & PassFragment
            std::sort(attachmentRemapping.begin(), attachmentRemapping.end(), CompareFirst<AttachmentName, AttachmentName>());
            FrameBufferFragmentMapping passFragment;
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

                #if defined(_DEBUG)
                    std::vector<AttachmentName> uniqueAttachments;
                    for (auto&a:newSubpass._output) uniqueAttachments.push_back(a._resourceName);
                    uniqueAttachments.push_back(newSubpass._depthStencil._resourceName);
                    for (auto&a:newSubpass._input) uniqueAttachments.push_back(a._resourceName);
                    for (auto&a:newSubpass._preserve) uniqueAttachments.push_back(a._resourceName);
                    for (auto&a:newSubpass._resolve) uniqueAttachments.push_back(a._resourceName);
                    std::sort(uniqueAttachments.begin(), uniqueAttachments.end());
                    assert(std::unique(uniqueAttachments.begin(), uniqueAttachments.end()) == uniqueAttachments.end()); // make sure the same attachment isn't used more than once
                #endif

                result.AddSubpass(std::move(newSubpass));
            }
            passFragment._subpassCount = (unsigned)f->_subpasses.size();
            fragmentRemapping.emplace_back(std::move(passFragment));

            /////////////////////////////////////////////////////////////////////////////

            workingAttachments.insert(workingAttachments.end(), newWorkingAttachments.begin(), newWorkingAttachments.end());

        }

        // The workingAttachments array is now the list of attachments that must go into
        // the output fragment;
        result._attachments.reserve(workingAttachments.size());
        for (auto& a:workingAttachments) {
            FrameBufferDescFragment::Attachment r { a._semantic, a._desc };
            result._attachments.push_back({a._name, r});
        }

        return { std::move(result), std::move(fragmentRemapping) };
    }

}}

