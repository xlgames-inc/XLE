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
#include "../IThreadContext.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/ArithmeticUtils.h"
#include "../../Utility/StreamUtils.h"
#include <cmath>
#include <sstream>
#include <iostream>
#include <set>

namespace RenderCore
{
    inline std::ostream& operator<<(std::ostream& str, const AttachmentDesc& attachment)
    {
        str << "AttachmentDesc {"
            #if defined(_DEBUG)
                << (!attachment._name.empty()?attachment._name:std::string("<<no name>>")) << ", "
            #endif
            << AsString(attachment._format) << ", "
            << attachment._width << ", "
            << attachment._height << ", "
            << attachment._arrayLayerCount << ", "
            << attachment._defaultAspect << ", "
            << unsigned(attachment._dimsMode)
            << ", 0x" << std::hex << attachment._flags << std::dec << "}";
        return str;
    }

    inline std::ostream& operator<<(std::ostream& str, const SubpassDesc& subpass)
    {
        str << "SubpassDesc { "
            #if defined(_DEBUG)
                << (!subpass._name.empty()?subpass._name:std::string("<<no name>>")) << ", "
            #endif
            << "outputs [";
        for (unsigned c=0; c<subpass._output.size(); ++c) { if (c!=0) str << ", "; str << subpass._output[c]._resourceName; }
        str << "], DepthStencil: ";
        if (subpass._depthStencil._resourceName != ~0u) { str << subpass._depthStencil._resourceName; } else { str << "<<none>>"; }
        str << ", inputs [";
        for (unsigned c=0; c<subpass._input.size(); ++c) { if (c!=0) str << ", "; str << subpass._input[c]._resourceName; }
        str << "], preserve [";
        for (unsigned c=0; c<subpass._preserve.size(); ++c) { if (c!=0) str << ", "; str << subpass._preserve[c]._resourceName; }
        str << "], resolve [";
        for (unsigned c=0; c<subpass._resolve.size(); ++c) { if (c!=0) str << ", "; str << subpass._resolve[c]._resourceName; }
        str << "] }";
        return str;
    }
}

namespace RenderCore { namespace Techniques
{

    AttachmentName FrameBufferDescFragment::DefineAttachment(uint64_t semantic, const AttachmentDesc& request)
    {
        auto name = (AttachmentName)_attachments.size();
        _attachments.push_back({semantic, semantic, request});
        return name;
    }

    void FrameBufferDescFragment::AddSubpass(SubpassDesc&& subpass)
    {
        _subpasses.emplace_back(std::move(subpass));
    }

    FrameBufferDescFragment::FrameBufferDescFragment() {}
    FrameBufferDescFragment::~FrameBufferDescFragment() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    auto RenderPassFragment::GetInputAttachmentSRV(unsigned inputAttachmentSlot, const TextureViewDesc& window) const -> Metal::ShaderResourceView*
    {
        auto remapped = RemapToRPI(inputAttachmentSlot);
        if (remapped == ~0u) return nullptr;
        return _rpi->GetSRV(remapped, window);
    }

    auto RenderPassFragment::GetInputAttachmentDesc(unsigned inputAttachmentSlot) const -> const AttachmentDesc*
    {
        auto remapped = RemapToRPI(inputAttachmentSlot);
        if (remapped == ~0u) return nullptr;
        return _rpi->GetDesc(remapped);
    }

    auto RenderPassFragment::GetInputAttachmentResource(unsigned inputAttachmentSlot) const -> IResourcePtr
    {
        auto remapped = RemapToRPI(inputAttachmentSlot);
        if (remapped == ~0u) return nullptr;
        return _rpi->GetResource(remapped);
    }

    AttachmentName RenderPassFragment::RemapToRPI(unsigned inputAttachmentSlot) const
    {
        auto passIndex = _currentPassIndex;
        auto i = std::find_if(
            _mapping->_inputAttachmentMapping.begin(), _mapping->_inputAttachmentMapping.end(),
            [passIndex, inputAttachmentSlot](const std::pair<FrameBufferFragmentMapping::PassAndSlot, AttachmentName>& p) {
                return p.first == std::make_pair(passIndex, inputAttachmentSlot);
            });
        if (i == _mapping->_inputAttachmentMapping.end())
            return ~0u;
        return i->second;
    }

	auto RenderPassFragment::GetOutputAttachmentDesc(unsigned slot) const -> const AttachmentDesc*
    {
        auto passIndex = _currentPassIndex;
        auto i = std::find_if(
            _mapping->_outputAttachmentMapping.begin(), _mapping->_outputAttachmentMapping.end(),
            [passIndex, slot](const std::pair<FrameBufferFragmentMapping::PassAndSlot, AttachmentName>& p) {
                return p.first == std::make_pair(passIndex, slot);
            });
        if (i == _mapping->_outputAttachmentMapping.end())
            return nullptr;

        assert(_attachmentPool);
        return _attachmentPool->GetDesc(i->second);
    }

    void RenderPassFragment::NextSubpass()
    {
        _rpi->NextSubpass();
        ++_currentPassIndex;
    }

    RenderPassFragment::RenderPassFragment(
        RenderPassInstance& rpi,
        const FrameBufferFragmentMapping& mapping)
    : _rpi(&rpi), _mapping(&mapping)
    , _currentPassIndex(0)
    {
    }

	RenderPassFragment::RenderPassFragment()
	: _rpi(nullptr), _mapping(nullptr), _attachmentPool(nullptr), _currentPassIndex(~0u)
	{
	}

    RenderPassFragment::~RenderPassFragment()
    {
        if (_mapping && _mapping->_subpassCount)
            _rpi->NextSubpass();
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    void RenderPassInstance::NextSubpass()
    {
        Metal::EndSubpass(*_attachedContext);
        Metal::BeginNextSubpass(*_attachedContext, *_frameBuffer);
    }

    void RenderPassInstance::End()
    {
        Metal::EndSubpass(*_attachedContext);
        Metal::EndRenderPass(*_attachedContext);
    }
    
    unsigned RenderPassInstance::GetCurrentSubpassIndex() const
    {
        return Metal::GetCurrentSubpassIndex(*_attachedContext);
    }

    auto RenderPassInstance::GetDesc(AttachmentName resName) const -> const AttachmentDesc*
    {
        assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetDesc(_attachmentPoolRemapping[resName]);
        return nullptr;
    }

    auto RenderPassInstance::GetResource(AttachmentName resName) const -> IResourcePtr
    {
        assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetResource(_attachmentPoolRemapping[resName]);
        return nullptr;
    }

    auto RenderPassInstance::GetSRV(AttachmentName resName, const TextureViewDesc& window) const -> Metal::ShaderResourceView*
    {
        assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetSRV(_attachmentPoolRemapping[resName], window);
        return nullptr;
    }

    class NamedAttachmentsWrapper : public INamedAttachments
    {
    public:
		virtual IResourcePtr GetResource(AttachmentName resName) const;
		virtual const AttachmentDesc* GetDesc(AttachmentName resName) const;

        NamedAttachmentsWrapper(
            AttachmentPool& pool,
            const std::vector<AttachmentName>& poolMapping);
        ~NamedAttachmentsWrapper();
    private:
        AttachmentPool* _pool;
        const std::vector<AttachmentName>* _poolMapping;
    };

    IResourcePtr NamedAttachmentsWrapper::GetResource(AttachmentName resName) const
    {
        assert(resName < _poolMapping->size());
        return _pool->GetResource((*_poolMapping)[resName]);
    }

    auto NamedAttachmentsWrapper::GetDesc(AttachmentName resName) const -> const AttachmentDesc*
    {
        assert(resName < _poolMapping->size());
        return _pool->GetDesc((*_poolMapping)[resName]);
    }

    NamedAttachmentsWrapper::NamedAttachmentsWrapper(
        AttachmentPool& pool,
        const std::vector<AttachmentName>& poolMapping)
    : _pool(&pool)
    , _poolMapping(&poolMapping) {}
    NamedAttachmentsWrapper::~NamedAttachmentsWrapper() {}

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
            std::vector<AttachmentName> _poolAttachmentsRemapping;
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
            if ((e._tickId + evictionRange) < _currentTickId) {
                e._fb.reset();
				e._hash = 0;
			}
        ++_currentTickId;
    }

    auto FrameBufferPool::BuildFrameBuffer(
        Metal::ObjectFactory& factory,
        const FrameBufferDesc& desc,
        AttachmentPool& attachmentPool) -> Result
    {
        auto poolAttachments = attachmentPool.Request(desc.GetAttachments());

        uint64_t hashValue = DefaultSeed64;
        for (const auto&a:poolAttachments)
            hashValue = HashCombine(attachmentPool.GetResource(a)->GetGUID(), hashValue);
        assert(hashValue != 0);     // using 0 has a sentinel, so this will cause some problems

        unsigned earliestEntry = 0;
        unsigned tickIdOfEarliestEntry = ~0u;
        for (unsigned c=0; c<dimof(_pimpl->_entries); ++c) {
            if (_pimpl->_entries[c]._hash == hashValue) {
                _pimpl->_entries[c]._tickId = _pimpl->_currentTickId;
                _pimpl->IncreaseTickId();
                assert(_pimpl->_entries[c]._fb != nullptr);
                return {
                    _pimpl->_entries[c]._fb,
                    MakeIteratorRange(_pimpl->_entries[earliestEntry]._poolAttachmentsRemapping)
                };
            }
            if (_pimpl->_entries[c]._tickId < tickIdOfEarliestEntry) {
                tickIdOfEarliestEntry = _pimpl->_entries[c]._tickId;
                earliestEntry = c;
            }
        }

        // Can't find it; we're just going to overwrite the oldest entry with a new one
        assert(earliestEntry < dimof(_pimpl->_entries));
//        if (_pimpl->_entries[earliestEntry]._fb) {
//            Log(Warning) << "Overwriting tail in FrameBufferPool(). There may be too many different framebuffers required from the same pool" << std::endl;
//        }

        auto namedAttachments = std::make_shared<NamedAttachmentsWrapper>(attachmentPool, poolAttachments);
        _pimpl->_entries[earliestEntry]._fb = std::make_shared<Metal::FrameBuffer>(
            factory,
            desc, *namedAttachments);
        _pimpl->_entries[earliestEntry]._tickId = _pimpl->_currentTickId;
        _pimpl->_entries[earliestEntry]._hash = hashValue;
        _pimpl->_entries[earliestEntry]._poolAttachmentsRemapping = std::move(poolAttachments);
        _pimpl->IncreaseTickId();
        return {
            _pimpl->_entries[earliestEntry]._fb,
            MakeIteratorRange(_pimpl->_entries[earliestEntry]._poolAttachmentsRemapping)
        };
    }

	std::shared_ptr<Metal::FrameBuffer> FrameBufferPool::BuildFrameBuffer(
        const FrameBufferDesc& desc,
        AttachmentPool& attachmentPool)
	{
		return BuildFrameBuffer(Metal::GetObjectFactory(), desc, attachmentPool);
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
        const FrameBufferDesc& layout,
        FrameBufferPool& frameBufferPool,
        AttachmentPool& attachmentPool,
        const RenderPassBeginDesc& beginInfo)
    {
        auto fb = frameBufferPool.BuildFrameBuffer(
            Metal::GetObjectFactory(*context.GetDevice()),
            layout, attachmentPool);

        _attachedContext = Metal::DeviceContext::Get(context).get();
        _frameBuffer = std::move(fb._frameBuffer);
        _attachmentPoolRemapping = std::vector<AttachmentName>(fb._poolAttachmentsRemapping.begin(), fb._poolAttachmentsRemapping.end());
        _attachmentPool = &attachmentPool;
        Metal::BeginRenderPass(*_attachedContext, *_frameBuffer, layout, attachmentPool.GetFrameBufferProperties(), beginInfo._clearValues);
    }
    
    RenderPassInstance::~RenderPassInstance() 
    {
        End();
    }

    RenderPassInstance::RenderPassInstance(RenderPassInstance&& moveFrom)
    : _frameBuffer(std::move(moveFrom._frameBuffer))
    , _attachedContext(moveFrom._attachedContext)
    , _attachmentPool(moveFrom._attachmentPool)
    , _attachmentPoolRemapping(std::move(moveFrom._attachmentPoolRemapping))
    {
        moveFrom._attachedContext = nullptr;
        moveFrom._attachmentPool = nullptr;
    }

    RenderPassInstance& RenderPassInstance::operator=(RenderPassInstance&& moveFrom)
    {
        _frameBuffer = std::move(moveFrom._frameBuffer);
        _attachedContext = moveFrom._attachedContext;
        _attachmentPool = moveFrom._attachmentPool;
        _attachmentPoolRemapping = std::move(moveFrom._attachmentPoolRemapping);
        moveFrom._attachedContext = nullptr;
        moveFrom._attachmentPool = nullptr;
        return *this;
    }

    RenderPassInstance::RenderPassInstance()
    {
        _attachedContext = nullptr;
        _attachmentPool = nullptr;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class AttachmentPool::Pimpl
    {
    public:
        struct Attachment
        {
            IResourcePtr		    _resource;
            AttachmentDesc		    _desc;
        };
        std::vector<Attachment>                         _attachments;

        struct SemanticAttachment : public Attachment
        {
            uint64_t        _semantic;
        };
        std::vector<SemanticAttachment>    _semanticAttachments;

        FrameBufferProperties       _props;
        Metal::ObjectFactory*       _factory;
        ViewPool<Metal::ShaderResourceView> _srvPool;

        bool BuildAttachment(AttachmentName attach);
    };

    bool AttachmentPool::Pimpl::BuildAttachment(AttachmentName attachName)
    {
        Attachment* attach = nullptr;
        if (attachName & (1u<<31u)) {
            auto semanticAttachIdx = attachName & ~(1u<<31u);
            attach = &_semanticAttachments[semanticAttachIdx];
        } else {
            attach = &_attachments[attachName];
        }
        assert(attach);
        if (!attach) return false;

        const auto& a = attach->_desc;

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
            attachmentWidth = unsigned(std::floor(_props._outputWidth * a._width));
            attachmentHeight = unsigned(std::floor(_props._outputHeight * a._height));
        }

        if (!attachmentWidth || !attachmentHeight) return false;

        auto desc = CreateDesc(
            0, 0, 0, 
            TextureDesc::Plain2D(attachmentWidth, attachmentHeight, a._format, 1, uint16(a._arrayLayerCount)),
            "attachment");

		// Prefer a "typeless" format when we create the actual resource
		desc._textureDesc._format = AsTypelessFormat(desc._textureDesc._format);

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
        attach->_resource = Metal::CreateResource(*_factory, desc);
		assert(attach->_resource);
        return true;
    }

    auto AttachmentPool::GetDesc(AttachmentName attachName) const -> const AttachmentDesc*
    {
        Pimpl::Attachment* attach = nullptr;
        if (attachName & (1u<<31u)) {
            auto semanticAttachIdx = attachName & ~(1u<<31u);
            if (semanticAttachIdx >= _pimpl->_semanticAttachments.size()) return nullptr;
            attach = &_pimpl->_semanticAttachments[semanticAttachIdx];
        } else {
            if (attachName >= _pimpl->_attachments.size()) return nullptr;
            attach = &_pimpl->_attachments[attachName];
        }
        assert(attach);
        return &attach->_desc;
    }
    
    IResourcePtr AttachmentPool::GetResource(AttachmentName attachName) const
    {
        Pimpl::Attachment* attach = nullptr;
        if (attachName & (1u<<31u)) {
            auto semanticAttachIdx = attachName & ~(1u<<31u);
            if (semanticAttachIdx >= _pimpl->_semanticAttachments.size()) return nullptr;
            attach = &_pimpl->_semanticAttachments[semanticAttachIdx];
        } else {
            if (attachName >= _pimpl->_attachments.size()) return nullptr;
            attach = &_pimpl->_attachments[attachName];
        }
        assert(attach);
        if (!attach->_resource)
            _pimpl->BuildAttachment(attachName);
        return attach->_resource;
	}

	Metal::ShaderResourceView* AttachmentPool::GetSRV(AttachmentName attachName, const TextureViewDesc& window) const
	{
        Pimpl::Attachment* attach = nullptr;
        if (attachName & (1u<<31u)) {
            auto semanticAttachIdx = attachName & ~(1u<<31u);
            if (semanticAttachIdx >= _pimpl->_semanticAttachments.size()) return nullptr;
            attach = &_pimpl->_semanticAttachments[semanticAttachIdx];
        } else {
            if (attachName >= _pimpl->_attachments.size()) return nullptr;
            attach = &_pimpl->_attachments[attachName];
        }
        assert(attach);
		auto completeView = CompleteTextureViewDesc(_pimpl->_attachmentDescs[resName], window);
		return _pimpl->_srvPool.GetView(attach->_resource, completeView);
	}

    static bool DimsEqual(const AttachmentDesc& lhs, const AttachmentDesc& rhs, const FrameBufferProperties& props)
    {
        if (lhs._dimsMode == rhs._dimsMode)
            return lhs._width == rhs._width && lhs._height == rhs._height;

        unsigned lhsWidth, lhsHeight;
        unsigned rhsWidth, rhsHeight;

        if (lhs._dimsMode == AttachmentDesc::DimensionsMode::Absolute) {
            lhsWidth = unsigned(lhs._width);
            lhsHeight = unsigned(lhs._height);
        } else {
            lhsWidth = unsigned(std::floor(props._outputWidth * lhs._width));
            lhsHeight = unsigned(std::floor(props._outputHeight * lhs._height));
        }

        if (rhs._dimsMode == AttachmentDesc::DimensionsMode::Absolute) {
            rhsWidth = unsigned(rhs._width);
            rhsHeight = unsigned(rhs._height);
        } else {
            rhsWidth = unsigned(std::floor(props._outputWidth * rhs._width));
            rhsHeight = unsigned(std::floor(props._outputHeight * rhs._height));
        }

        return lhsWidth == rhsWidth && lhsHeight == rhsHeight;
    }

    static bool MatchRequest(const AttachmentDesc& lhs, const AttachmentDesc& rhs, const FrameBufferProperties& props)
    {
        return
            lhs._arrayLayerCount == rhs._arrayLayerCount
            && lhs._format == rhs._format
            && DimsEqual(lhs, rhs, props)
            ;
    }

    std::vector<AttachmentName> AttachmentPool::Request(IteratorRange<const FrameBufferDesc::Attachment*> requests)
    {
        std::vector<bool> consumed(_pimpl->_attachments.size(), false);
        std::vector<bool> consumedSemantic(_pimpl->_semanticAttachments.size(), false);
        std::vector<AttachmentName> result;
        for (const auto&r:requests) {
            // If a semantic value is set, we should first check to see if the request can match
            // one of the bound attachments.
            bool foundMatch = false;
            if (r._semantic) {
                for (unsigned q=0; q<_pimpl->_semanticAttachments.size(); ++q) {
                    if (r._semantic == _pimpl->_semanticAttachments[q]._semantic && !consumedSemantic[q]) {
                        if (MatchRequest(r._desc, _pimpl->_semanticAttachments[q]._desc, _pimpl->_props)
                            && _pimpl->_semanticAttachments[q]._resource) {
                            consumedSemantic[q] = true;
                            foundMatch = true;
                            result.push_back(q | (1u<<31u));
                            break;
                        } else {
                            Log(Warning) << "Attachment bound to the pool for semantic (0x" << std::hex << r._semantic << std::dec << ") does not match the request for this semantic. The bound attachment will be ignored. Request: "
                                << r._desc << ", Bound to pool: " << _pimpl->_semanticAttachments[q]._desc
                                << std::endl;
                        }
                    }
                }
                if (foundMatch) continue;

                // If we didn't find a match in one of our bound semantic attachments, we must flow
                // through and treat it as a temporary attachment.
            }

            // If we haven't found a match yet, we must treat the request as a temporary buffer
            // We will go through and either find an existing buffer or create a new one
            for (unsigned q=0; q<_pimpl->_attachments.size(); ++q) {
                if (MatchRequest(r._desc, _pimpl->_attachments[q]._desc, _pimpl->_props) && q < consumed.size() && !consumed[q]) {
                    consumed[q] = true;
                    result.push_back(q);
                    foundMatch = true;
                    break;
                }
            }

            if (!foundMatch) {
                _pimpl->_attachments.push_back(
                    Pimpl::Attachment{nullptr, r._desc});
                result.push_back((unsigned)(_pimpl->_attachments.size()-1));
            }
        }
        return result;
    }

    void AttachmentPool::Bind(uint64_t semantic, const IResourcePtr& resource)
    {
        assert(semantic != 0);      // using zero as a semantic is not supported; this is used as a sentinel for "no semantic"
        
        auto existingBinding = std::find_if(
            _pimpl->_semanticAttachments.begin(),
            _pimpl->_semanticAttachments.end(),
            [semantic](const Pimpl::SemanticAttachment& a) {
                return a._semantic == semantic;
            });
        if (existingBinding != _pimpl->_semanticAttachments.end()) {
		    if (existingBinding->_resource)
                _pimpl->_srvPool.Erase(*existingBinding->_resource);
        } else {
            Pimpl::SemanticAttachment newAttach;
            newAttach._semantic = semantic;
            existingBinding = _pimpl->_semanticAttachments.insert(
                _pimpl->_semanticAttachments.end(),
                newAttach);
        }

        existingBinding->_desc = AsAttachmentDesc(resource->GetDesc());
        existingBinding->_resource = resource;
    }

    void AttachmentPool::Unbind(const IResource& resource)
    {
        auto existingBinding = std::find_if(
            _pimpl->_semanticAttachments.begin(),
            _pimpl->_semanticAttachments.end(),
            [&resource](const Pimpl::SemanticAttachment& a) {
                return a._resource.get() == &resource;
            });
        if (existingBinding != _pimpl->_semanticAttachments.end()) {
            if (existingBinding->_resource)
                _pimpl->_srvPool.Erase(*existingBinding->_resource);
            existingBinding->_resource = nullptr;
        }
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
            for (auto& r:_pimpl->_attachments)
                if (r._desc._dimsMode == AttachmentDesc::DimensionsMode::OutputRelative) {
					if (r._resource)
						_pimpl->_srvPool.Erase(*r._resource);
					r._resource.reset();
				}

        if (samplesChanged) // Invalidate all resources and views that depend on the multisample state
            for (auto& r:_pimpl->_attachments)
                if (r._desc._flags & AttachmentDesc::Flags::Multisampled) {
                    if (r._resource)
                        _pimpl->_srvPool.Erase(*r._resource);
                    r._resource.reset();
				}

        _pimpl->_props = props;
    }

    const FrameBufferProperties& AttachmentPool::GetFrameBufferProperties() const
    {
        return _pimpl->_props;
    }

    AttachmentPool::AttachmentPool()
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_factory = &Metal::GetObjectFactory();
        _pimpl->_props = {0u, 0u, TextureSamples::Create()};
    }

    AttachmentPool::~AttachmentPool()
    {}
    
////////////////////////////////////////////////////////////////////////////////////////////////////

    FrameBufferDesc BuildFrameBufferDesc(
        FrameBufferDescFragment&& fragment)
    {
        //
        // Convert a frame buffer fragment to a discrete FrameBufferDesc. We actually don't need
        // to do much conversion. The SubpassDescs can just be copied across as is.
        // Attachments need to be transformed slightly. Here we also look for "split semantics".
        // This occurs when the input attachment for a semantic is different from the output
        // attachment for a semantic. This is illegal because in the FrameBufferDesc form, we can
        // only bind a single buffer to each semantic (this is because we can only bind a single
        // attachment to each semantic in the AttachmentPool -- this restriction was intended to
        // make the interface a little simplier and clearer)
        //
        // Split semantics typically happen when we want a RenderPass that reads from one attachment and
        // then writes back to the same attachment at the end (for example, that attachment might be
        // the main color buffer, and we want to do some post processing operation on it).
        //
        // Sometimes MergeFragments can't find a solution that can achieve this without adding extra
        // subpasses. So it creates this split semantic case. There are two options to solve this:
        // * use 2 different semantics for the operation in question (for example, a tonemap operation
        //      might take in a "hdrcolor" semantic and write to a "lhdrcolor" semantic)
        // * add an extra "copy" subpass -- this should just copy to the expected output buffer
        //
        std::vector<FrameBufferDesc::Attachment> fbAttachments;
        fbAttachments.reserve(fragment._attachments.size());
        for (const auto& inputFrag:fragment._attachments) {
            uint64_t semantic = 0;
            if (inputFrag._inputSemanticBinding != 0 || inputFrag._outputSemanticBinding != 0) {
                if (    inputFrag._inputSemanticBinding != 0 && inputFrag._outputSemanticBinding != 0
                    &&  inputFrag._inputSemanticBinding != inputFrag._outputSemanticBinding)
                    Throw(std::runtime_error("Cannot construct FrameBufferDesc because input fragment has an attachment with two different semantics for input and output. This isn't supported; we must instead use two different attachments, one for each semantic."));

                for (auto compareFrag=fbAttachments.begin(); compareFrag!=fbAttachments.end(); ++compareFrag) {
                    if (    ((inputFrag._inputSemanticBinding != 0) && compareFrag->_semantic == inputFrag._inputSemanticBinding)
                        ||  ((inputFrag._outputSemanticBinding != 0) && compareFrag->_semantic == inputFrag._outputSemanticBinding)) {
                        // Hit a duplicate semantic binding case. Let's check if it's a "split semantic" case
                        // If the two attachments in question have non-coinciding zeroes for their semantic interface
                        // (and we already know that the non-zero semantics are equal), then it must be the split
                        // semantic case
                        auto& suspect = fragment._attachments[std::distance(fbAttachments.begin(), compareFrag)];
                        if (    (suspect._inputSemanticBinding == 0 && inputFrag._outputSemanticBinding == 0)
                            ||  (suspect._outputSemanticBinding == 0 && inputFrag._inputSemanticBinding == 0)) {
                            Throw(std::runtime_error("Cannot construct FrameBufferDesc because input fragment has a \"split semantic\" attachment. This can occur when an extra copy subpass is required"));
                        } else {
                            Throw(std::runtime_error("Cannot construct FrameBufferDesc because input fragment has more than one buffer assigned to the same semantic. This isn't supported because the AttachmentPool can only bind a single attachment to each semantic."));
                        }
                    }
                }
                semantic = (inputFrag._inputSemanticBinding != 0) ? inputFrag._inputSemanticBinding : inputFrag._outputSemanticBinding;
            }
            if (inputFrag._desc._format == Format::Unknown)
                Throw(std::runtime_error("Cannot construct FrameBufferDesc because input fragment because an attachment doesn't have a fully specified format. Before we transform into the FrameBufferDesc version, we must have resolved the format to a concrete value."));
            fbAttachments.push_back({semantic, inputFrag._desc});
        }

        // Generate the final FrameBufferDesc by moving the subpasses out of the fragment
        // Usually this function is called as a final step when converting a number of fragments
        // into a final FrameBufferDesc, so it makes sense to move the subpasses from the input
        return FrameBufferDesc {
            std::move(fbAttachments),
            std::move(fragment._subpasses) };
    }

    static bool IsCompatible(const AttachmentDesc& testAttachment, const AttachmentDesc& request)
    {
        return
            ( (testAttachment._format == request._format) || (testAttachment._format == Format::Unknown) || (request._format == Format::Unknown) )
            && testAttachment._arrayLayerCount == request._arrayLayerCount
            && ( (testAttachment._defaultAspect == request._defaultAspect) || (testAttachment._defaultAspect == TextureViewDesc::Aspect::UndefinedAspect) || (request._defaultAspect == TextureViewDesc::Aspect::UndefinedAspect) )
			&& testAttachment._width == request._width && testAttachment._height == request._height
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

    class WorkingAttachment
    {
    public:
        AttachmentName _name;
        AttachmentDesc _desc;
        uint64_t _inputSemanticBinding = 0;
        uint64_t _outputSemanticBinding = 0;
        uint64_t _firstReadSemantic = 0;
        uint64_t _lastWriteSemantic = 0;
        bool _forceShaderResourceFlag = false;
        PreregisteredAttachment::State _state = PreregisteredAttachment::State::Uninitialized;
        PreregisteredAttachment::State _stencilState = PreregisteredAttachment::State::Uninitialized;
    };

    static AttachmentName NextName(IteratorRange<const WorkingAttachment*> attachments0, IteratorRange<const WorkingAttachment*> attachments1)
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

    /*static uint64_t GetSemantic(IteratorRange<const std::pair<AttachmentName, FrameBufferDescFragment::Attachment>*> attachments, RenderCore::AttachmentName attachment)
    {
        for (const auto& a:attachments)
            if (a.first == attachment)
                return a.second._semantic;
        return 0;
    }*/

////////////////////////////////////////////////////////////////////////////////////////////////////

    inline const char* AsString(PreregisteredAttachment::State state)
    {
        switch (state) {
        case PreregisteredAttachment::State::Uninitialized: return "Uninitialized";
        case PreregisteredAttachment::State::Initialized:   return "Initialized";
        default:                                            return "<<unknown>>";
        }
    }

    inline std::ostream& operator<<(std::ostream& str, const PreregisteredAttachment& attachment)
    {
        str << "PreregisteredAttachment {"
            << attachment._semantic << ", "
            << attachment._desc << ", "
            << AsString(attachment._state) << ", "
            << AsString(attachment._stencilState) << "}";
        return str;
    }

    inline std::ostream& operator<<(std::ostream& str, const WorkingAttachment& attachment)
    {
        str << "WorkingAttachment {"
            << attachment._name << ", "
            << attachment._desc << ", "
            << std::hex << "Input: 0x" << attachment._inputSemanticBinding << ", "
            << "Output: 0x" << attachment._outputSemanticBinding << ", "
            << "FirstRead: 0x" << attachment._firstReadSemantic << ", "
            << "LastWrite: 0x" << attachment._lastWriteSemantic << ", " << std::dec
            << attachment._forceShaderResourceFlag << ", "
            << AsString(attachment._state) << ", "
            << AsString(attachment._stencilState) << "}";
        return str;
    }

    inline std::ostream& operator<<(std::ostream& str, const FrameBufferDescFragment& fragment)
    {
        str << "FrameBufferDescFragment with attachments: " << std::endl;
        for (unsigned c=0; c<fragment._attachments.size(); ++c) {
            str << StreamIndent(4) << "[" << c << "] "
                << std::hex << " 0x" << fragment._attachments[c].GetInputSemanticBinding() << ", 0x" << fragment._attachments[c].GetOutputSemanticBinding() << std::dec << " : " << fragment._attachments[c]._desc
                << std::endl;
        }
        str << "Subpasses: " << std::endl;
        for (unsigned c=0; c<fragment._subpasses.size(); ++c) {
            str << StreamIndent(4) << "[" << c << "] " << fragment._subpasses[c] << std::endl;
        }
        return str;
    }

    static bool CompareAttachmentName(const WorkingAttachment& lhs, const WorkingAttachment& rhs)
    {
        return lhs._name < rhs._name;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    std::pair<FrameBufferDescFragment, std::vector<FrameBufferFragmentMapping>> MergeFragments(
        IteratorRange<const PreregisteredAttachment*> preregisteredInputs,
        IteratorRange<const FrameBufferDescFragment*> fragments,
        char *logBuffer, size_t bufferLength)
    {
        #if defined(_DEBUG)
            std::stringstream debugInfo;
            debugInfo << "Preregistered Inputs:" << std::endl;
            for (auto a = preregisteredInputs.begin(); a != preregisteredInputs.end(); ++a) {
                debugInfo << "[" << std::distance(preregisteredInputs.begin(), a) << "] " << *a << std::endl;
            }
        #endif
        
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

        /*
        std::vector<uint64_t> shaderResourceSemantics;
        for (auto f = fragments.begin(); f != fragments.end(); ++f) {
            for (auto p = f->_subpasses.begin(); p != f->_subpasses.end(); ++p) {
                for (auto a = p->_input.begin(); a != p->_input.end(); ++a) {
                    if (HasRetain(a->_loadFromPreviousPhase)) {
                        auto semantic = GetSemantic(MakeIteratorRange(f->_attachments), a->_resourceName);
                        if (semantic) {
                            shaderResourceSemantics.push_back(semantic);
                        }
                    }
                }
            }
        }
        std::sort(shaderResourceSemantics.begin(), shaderResourceSemantics.end());
        shaderResourceSemantics.erase(
            std::unique(shaderResourceSemantics.begin(), shaderResourceSemantics.end()),
            shaderResourceSemantics.end());
        */
        
        std::vector<WorkingAttachment> workingAttachments;
        workingAttachments.reserve(preregisteredInputs.size());
        for (unsigned c=0; c<preregisteredInputs.size(); c++) {
            WorkingAttachment initialState;
            initialState._outputSemanticBinding = preregisteredInputs[c]._semantic;
            initialState._desc = preregisteredInputs[c]._desc;
            initialState._state = preregisteredInputs[c]._state;
            initialState._stencilState = preregisteredInputs[c]._stencilState;
        }
            // workingAttachments.push_back({preregisteredInputs[c], c});

        for (auto f=fragments.begin(); f!=fragments.end(); ++f) {
            std::vector<WorkingAttachment> newWorkingAttachments;
            std::vector<std::pair<AttachmentName, AttachmentName>> attachmentRemapping;
            /////////////////////////////////////////////////////////////////////////////
            for (auto interf = f->_attachments.begin(); interf != f->_attachments.end(); ++interf) {
                const auto& interfaceAttachment = *interf;
                AttachmentName reboundName = ~0u;
                AttachmentName interfaceAttachmentName = (AttachmentName)std::distance(f->_attachments.begin(), interf);
                DirectionFlags::BitField directionFlags = 0;
                // Look through the load/store values in the subpasses to find the "direction" for
                // this attachment. When deciding on the load flags, we must look for the first
                // subpass that references the attachment; for the store flags we look for the last
                // subpass that writes to it
                for (auto p = f->_subpasses.begin(); p != f->_subpasses.end(); ++p) {
                    auto subpassDirectionFlags = GetLoadDirectionFlags(*p, interfaceAttachmentName);
                    if (subpassDirectionFlags != 0) {
                        directionFlags |= subpassDirectionFlags;
                        break;
                    }
                }
                for (auto p = f->_subpasses.rbegin(); p != f->_subpasses.rend(); ++p) {
                    auto subpassDirectionFlags = GetStoreDirectionFlags(*p, interfaceAttachmentName);
                    if (subpassDirectionFlags != 0) {
                        directionFlags |= subpassDirectionFlags;
                        break;
                    }
                }
                assert(directionFlags != 0);

                // toggle on the "ShaderResource" flag, if necessary
                /*if (std::find(shaderResourceSemantics.begin(), shaderResourceSemantics.end(), interfaceAttachment._semantic) != shaderResourceSemantics.end()) {
                    interfaceAttachment._desc._flags |= AttachmentDesc::Flags::Enum::ShaderResource;
                }*/

                if (directionFlags & DirectionFlags::Load) {
                    // We're expecting a buffer that already has some initialized contents. Look for
                    // something matching in our working attachments array
                    auto compat = std::find_if(
                        workingAttachments.begin(), workingAttachments.end(),
                        [&interfaceAttachment](const WorkingAttachment& input) {
                            return (input._state == PreregisteredAttachment::State::Initialized)
                                && (input._outputSemanticBinding == interfaceAttachment.GetInputSemanticBinding())
                                && IsCompatible(input._desc, interfaceAttachment._desc);
                        });
                    if (compat == workingAttachments.end()) {
                        #if defined(_DEBUG)
                            debugInfo << "      * Failed to find compatible buffer, cannot proceed. Working attachments are: " << std::endl;
                            for (auto att : workingAttachments)
                                debugInfo << att << std::endl;
                            auto debugInfoStr = debugInfo.str();
                            Log(Error) << "MergeFragments() failed. Details:" << std::endl << debugInfoStr << std::endl;
                            Throw(::Exceptions::BasicLabel("Couldn't bind renderpass fragment input request. Details:\n%s\n", debugInfoStr.c_str()));
                        #else
                            Throw(::Exceptions::BasicLabel("Couldn't bind renderpass fragment input request"));
                        #endif
                    }
                    
                    reboundName = compat->_name;

                    // Remove from the working attachments and push back in it's new state
                    // If we're not writing to this attachment, it will lose it's semantic here
                    auto newState = *compat;
                    newState._state = (directionFlags & DirectionFlags::Store) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized;
                    newState._inputSemanticBinding = 0;
                    newState._outputSemanticBinding = (directionFlags & DirectionFlags::Store) ? interfaceAttachment.GetOutputSemanticBinding() : 0;
                    if (!newState._firstReadSemantic)
                        newState._firstReadSemantic = interfaceAttachment.GetInputSemanticBinding();
                    if (directionFlags & DirectionFlags::Store)
                        newState._lastWriteSemantic = interfaceAttachment.GetOutputSemanticBinding();
                    workingAttachments.erase(compat);
                    newWorkingAttachments.push_back(newState);
                } else {
                    // define a new output buffer, or reuse something that we can reuse
                    auto compat = std::find_if(
                        workingAttachments.begin(), workingAttachments.end(),
                        [&interfaceAttachment](const WorkingAttachment& input) {
                            return (input._state == PreregisteredAttachment::State::Uninitialized)
                                && (input._inputSemanticBinding == interfaceAttachment.GetOutputSemanticBinding())
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
                            preregisteredInputs.begin(), preregisteredInputs.end(),
                            [&interfaceAttachment](const PreregisteredAttachment& input) {
                                return (input._semantic == interfaceAttachment.GetOutputSemanticBinding());
                            });
                        if (sameSemantic != preregisteredInputs.end()) {
                            if (desc._format == Format::Unknown) desc._format = sameSemantic->_desc._format;
                            if (desc._defaultAspect == TextureViewDesc::Aspect::UndefinedAspect) desc._defaultAspect = sameSemantic->_desc._defaultAspect;
                        } else {
                            assert(desc._format != Format::Unknown);
                        }

                        WorkingAttachment newState;
                        newState._name = reboundName;
                        newState._desc = desc;
                        newState._state = (directionFlags & DirectionFlags::Store) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized;
                        newState._stencilState = (directionFlags & DirectionFlags::Store) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized;
                        if (directionFlags & DirectionFlags::Store) {
                            newState._outputSemanticBinding = interfaceAttachment.GetOutputSemanticBinding();
                            newState._lastWriteSemantic = interfaceAttachment.GetOutputSemanticBinding();
                        }
                        newWorkingAttachments.push_back(newState);

                        #if defined(_DEBUG)
                            debugInfo 
                                << "      * " 
                                << "Cannot find compatible buffer, creating #" << reboundName << ", " << newState << std::endl;
                        #endif
                    } else {
                        reboundName = compat->_name;

                        // remove from the working attachments and push back in it's new state
                        auto newState = *compat;
                        newState._state = (directionFlags & DirectionFlags::Store) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized;
                        if (directionFlags & DirectionFlags::Store) {
                            newState._outputSemanticBinding = interfaceAttachment.GetOutputSemanticBinding();
                            newState._lastWriteSemantic = interfaceAttachment.GetOutputSemanticBinding();
                        }
                        workingAttachments.erase(compat);
                        newWorkingAttachments.push_back(newState);
                    }
                }

                attachmentRemapping.push_back({interfaceAttachmentName, reboundName});
            }

            /////////////////////////////////////////////////////////////////////////////

                // setup the subpasses & PassFragment
            std::sort(attachmentRemapping.begin(), attachmentRemapping.end(), CompareFirst<AttachmentName, AttachmentName>());

            FrameBufferFragmentMapping passFragment;
            for (unsigned p=0; p<(unsigned)f->_subpasses.size(); ++p) {
                SubpassDesc newSubpass = f->_subpasses[p];
                for (unsigned c=0; c<(unsigned)newSubpass._output.size(); ++c) {
                    newSubpass._output[c]._resourceName = Remap(attachmentRemapping, newSubpass._output[c]._resourceName);
					passFragment._outputAttachmentMapping.push_back({{p, c}, newSubpass._output[c]._resourceName});
				}
                newSubpass._depthStencil._resourceName = Remap(attachmentRemapping, newSubpass._depthStencil._resourceName);
                for (unsigned c=0; c<(unsigned)newSubpass._input.size(); ++c) {
                    newSubpass._input[c]._resourceName = Remap(attachmentRemapping, newSubpass._input[c]._resourceName);
                    passFragment._inputAttachmentMapping.push_back({{p, c}, newSubpass._input[c]._resourceName});
                }
                for (auto&a:newSubpass._preserve)
                    a._resourceName = Remap(attachmentRemapping, a._resourceName);
                for (auto&a:newSubpass._resolve)
                    a._resourceName = Remap(attachmentRemapping, a._resourceName);

				// DavidJ -- in some interesting cases, a single attachment can be used mutliple times in the same subpass
				//		but I think this should only happen if different "aspects" of the resource are used each time
				//		for exampling, binding the stencil aspect of a DepthStencil buffer in the DepthStencil binding,
				//		and then binding the depth aspect as in input binding
                #if 0 // defined(_DEBUG)
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

            #if defined(_DEBUG)
                debugInfo << "-------------------------------" << std::endl;
                debugInfo << "Fragment [" << std::distance(fragments.begin(), f) << "] " << *f;
                debugInfo << "Merge calculated this attachment remapping:" << std::endl;
                for (const auto&r:attachmentRemapping)
                    debugInfo << StreamIndent(4) << "[" << r.first << "] remapped to " << r.second << " ("
                        << f->_attachments[r.first]._desc
                        << ")" << std::endl;
                debugInfo << "Current fragment interface:" << std::endl;
                for (const auto&w:workingAttachments)
                    debugInfo << StreamIndent(4) << w << std::endl;
            #endif
        }

        // The workingAttachments array is now the list of attachments that must go into
        // the output fragment;
        result._attachments.reserve(workingAttachments.size());
        std::sort(workingAttachments.begin(), workingAttachments.end(), CompareAttachmentName);
        for (auto& a:workingAttachments) {
            // The AttachmentNames in FrameBufferDescFragment are just indices into the attachment
            // list -- so we must ensure that we insert in order, and without gaps
            assert(a._name == result._attachments.size());
            FrameBufferDescFragment::Attachment r { a._firstReadSemantic, a._lastWriteSemantic, a._desc };
            result._attachments.push_back(r);
        }

        #if defined(_DEBUG)
            debugInfo << "-------------------------------" << std::endl;
            debugInfo << "Final attachments" << std::endl;
            for (unsigned c=0; c<result._attachments.size(); ++c)
                debugInfo << StreamIndent(4) << "[" << c << "] 0x"
                    << std::hex << result._attachments[c].GetInputSemanticBinding() << ", 0x" << result._attachments[c].GetOutputSemanticBinding() << std::dec
                    << " : " << result._attachments[c]._desc << std::endl;
            debugInfo << "Final subpasses" << std::endl;
            for (unsigned c=0; c<result._subpasses.size(); ++c)
                debugInfo << StreamIndent(4) << "[" << c << "] " << result._subpasses[c] << std::endl;
            debugInfo << "MergeFragments() finished." << std::endl;
            if (logBuffer != nullptr) XlCopyString(logBuffer, bufferLength, StringSection<>(debugInfo.str()));
        #endif
        
        return { std::move(result), std::move(fragmentRemapping) };
    }

}}

