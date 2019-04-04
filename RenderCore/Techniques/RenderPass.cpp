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
#include <unordered_map>
#include <set>

namespace RenderCore
{
    inline std::ostream& operator<<(std::ostream& str, const AttachmentDesc& attachment)
    {
        str << "AttachmentDesc { "
            #if defined(_DEBUG)
                << (!attachment._name.empty()?attachment._name:std::string("<<no name>>")) << ", "
            #endif
            << AsString(attachment._format) << ", "
            << attachment._width << ", "
            << attachment._height << ", "
            << attachment._arrayLayerCount << ", "
            << unsigned(attachment._dimsMode)
            << ", 0x" << std::hex << attachment._flags << std::dec << " }";
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
        str << "], resolveDepthStencil: ";
        if (subpass._depthStencilResolve._resourceName != ~0u) { str << subpass._depthStencilResolve._resourceName << " }"; }
        else { str << "<<none>> }"; }
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
        return _rpi->GetDesc(i->second);
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
	: _rpi(nullptr), _mapping(nullptr), _currentPassIndex(~0u)
	{
	}

    RenderPassFragment::~RenderPassFragment()
    {
        if (_mapping && _mapping->_subpassCount)
            _rpi->NextSubpass();
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    class NamedAttachmentsWrapper : public INamedAttachments
    {
    public:
		virtual IResourcePtr GetResource(AttachmentName resName) const;
		virtual const AttachmentDesc* GetDesc(AttachmentName resName) const;
		virtual const FrameBufferProperties& GetFrameBufferProperties() const;

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

	const FrameBufferProperties& NamedAttachmentsWrapper::GetFrameBufferProperties() const
	{
		return _pool->GetFrameBufferProperties();
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
            uint64_t _hash = ~0ull;
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
				e._hash = ~0ull;
			}
        ++_currentTickId;
    }

    auto FrameBufferPool::BuildFrameBuffer(
        Metal::ObjectFactory& factory,
        const FrameBufferDesc& desc,
        AttachmentPool& attachmentPool) -> Result
    {
        auto poolAttachments = attachmentPool.Request(desc.GetAttachments());
		assert(poolAttachments.size() == desc.GetAttachments().size());

        uint64_t hashValue = desc.GetHash();
        for (const auto&a:poolAttachments)
            hashValue = HashCombine(attachmentPool.GetResource(a)->GetGUID(), hashValue);
        assert(hashValue != ~0ull);     // using ~0ull has a sentinel, so this will cause some problems

        unsigned earliestEntry = 0;
        unsigned tickIdOfEarliestEntry = ~0u;
        for (unsigned c=0; c<dimof(_pimpl->_entries); ++c) {
            if (_pimpl->_entries[c]._hash == hashValue) {
                _pimpl->_entries[c]._tickId = _pimpl->_currentTickId;
				_pimpl->_entries[c]._poolAttachmentsRemapping = std::move(poolAttachments);	// update the mapping, because attachments map have moved
                _pimpl->IncreaseTickId();
                assert(_pimpl->_entries[c]._fb != nullptr);
                return {
                    _pimpl->_entries[c]._fb,
                    MakeIteratorRange(_pimpl->_entries[c]._poolAttachmentsRemapping)
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

        NamedAttachmentsWrapper namedAttachments(attachmentPool, poolAttachments);
        _pimpl->_entries[earliestEntry]._fb = std::make_shared<Metal::FrameBuffer>(
            factory,
            desc, namedAttachments);
        _pimpl->_entries[earliestEntry]._tickId = _pimpl->_currentTickId;
        _pimpl->_entries[earliestEntry]._hash = hashValue;
        _pimpl->_entries[earliestEntry]._poolAttachmentsRemapping = std::move(poolAttachments);
        _pimpl->IncreaseTickId();
        return {
            _pimpl->_entries[earliestEntry]._fb,
            MakeIteratorRange(_pimpl->_entries[earliestEntry]._poolAttachmentsRemapping)
        };
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

	void RenderPassInstance::NextSubpass()
    {
		if (_attachedContext) {
			assert(_frameBuffer);
			Metal::EndSubpass(*_attachedContext, *_frameBuffer);
			Metal::BeginNextSubpass(*_attachedContext, *_frameBuffer);
		}
    }

    void RenderPassInstance::End()
    {
		if (_attachedContext) {
			Metal::EndSubpass(*_attachedContext, *_frameBuffer);
			Metal::EndRenderPass(*_attachedContext);
		}
    }
    
    unsigned RenderPassInstance::GetCurrentSubpassIndex() const
    {
		if (_attachedContext) {
			return Metal::GetCurrentSubpassIndex(*_attachedContext);
		} else {
			return 0;
		}
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

	AttachmentName RenderPassInstance::RemapAttachmentName(AttachmentName resName) const
	{
		if (resName < _attachmentPoolRemapping.size())
			return _attachmentPoolRemapping[resName];
		return ~0u;
	}

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

	RenderPassInstance::RenderPassInstance(
        const FrameBufferDesc& layout,
        AttachmentPool& attachmentPool)
    {
		// This constructs a kind of "non-metal" RenderPassInstance
		// It allows us to use the RenderPassInstance infrastructure (for example, for remapping attachment requests)
		// without actually constructing a underlying metal renderpass.
		// This is used with compute pipelines sometimes -- since in Vulkan, those have some similarities with
		// graphics pipelines, but are incompatible with the vulkan render passes
		_attachedContext = nullptr;
		_attachmentPoolRemapping = attachmentPool.Request(layout.GetAttachments());
		assert(_attachmentPoolRemapping.size() == layout.GetAttachments().size());
		_attachmentPool = &attachmentPool;
	}
    
    RenderPassInstance::~RenderPassInstance() 
    {
		if (_attachedContext) {
			End();
		}
    }

    RenderPassInstance::RenderPassInstance(RenderPassInstance&& moveFrom) never_throws
    : _frameBuffer(std::move(moveFrom._frameBuffer))
    , _attachedContext(moveFrom._attachedContext)
    , _attachmentPool(moveFrom._attachmentPool)
    , _attachmentPoolRemapping(std::move(moveFrom._attachmentPoolRemapping))
    {
        moveFrom._attachedContext = nullptr;
        moveFrom._attachmentPool = nullptr;
    }

    RenderPassInstance& RenderPassInstance::operator=(RenderPassInstance&& moveFrom) never_throws
    {
		if (_attachedContext) {
			End();
		}

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
        ViewPool<Metal::ShaderResourceView> _srvPool;

        bool BuildAttachment(AttachmentName attach);
    };

    static std::shared_ptr<IResource> CreateFromAttachmentDesc(const AttachmentDesc& a, const FrameBufferProperties& props)
    {
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
            attachmentWidth = unsigned(std::floor(props._outputWidth * a._width));
            attachmentHeight = unsigned(std::floor(props._outputHeight * a._height));
        }

        if (!attachmentWidth || !attachmentHeight) return nullptr;

        auto desc = CreateDesc(
            0, 0, 0, 
            TextureDesc::Plain2D(attachmentWidth, attachmentHeight, a._format, 1, uint16(a._arrayLayerCount)),
            "attachment");

		desc._textureDesc._format = desc._textureDesc._format;

        if (a._flags & AttachmentDesc::Flags::Multisampled)
            desc._textureDesc._samples = props._samples;

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
        return Metal::CreateResource(Metal::GetObjectFactory(), desc);
    }

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

        attach->_resource = CreateFromAttachmentDesc(attach->_desc, _props);
        return attach->_resource != nullptr;
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
		assert(attach->_resource);
		auto defaultAspect = TextureViewDesc::Aspect::ColorLinear;
		auto formatComponents = GetComponents(attach->_desc._format);
		if (formatComponents == FormatComponents::Depth || formatComponents == FormatComponents::DepthStencil) {
			defaultAspect = TextureViewDesc::Aspect::Depth;	// can only choose depth or stencil -- so DepthStencil defaults to Depth
		} else if (formatComponents == FormatComponents::Stencil) {
			defaultAspect = TextureViewDesc::Aspect::Stencil;
		}
		auto completeView = CompleteTextureViewDesc(attach->_desc, window, defaultAspect);
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

    static unsigned GetArrayCount(const AttachmentDesc& lhs) { return (lhs._arrayLayerCount == 0) ? 1 : lhs._arrayLayerCount; }

    static bool MatchRequest(const AttachmentDesc& lhs, const AttachmentDesc& rhs, const FrameBufferProperties& props)
    {
        return
            GetArrayCount(lhs) == GetArrayCount(rhs)
            && (AsTypelessFormat(lhs._format) == AsTypelessFormat(rhs._format) || lhs._format == Format::Unknown || rhs._format == Format::Unknown)
            && DimsEqual(lhs, rhs, props)
            ;
    }

    std::vector<AttachmentName> AttachmentPool::Request(IteratorRange<const FrameBufferDesc::Attachment*> requests)
    {
        std::vector<bool> consumed(_pimpl->_attachments.size(), false);
        std::vector<bool> consumedSemantic(_pimpl->_semanticAttachments.size(), false);

        // Treat any attachments that are bound to semantic values as "consumed" already.
        // In other words, we can't give these attachments to requests without a semantic,
        // or using another semantic.
        for (unsigned c=0; c<_pimpl->_attachments.size(); ++c) {
            for (const auto&a:_pimpl->_semanticAttachments) {
                if (a._resource == _pimpl->_attachments[c]._resource) {
                    consumed[c] = true;
                    break;
                }
            }
        }

        AttachmentDesc::Flags::BitField relevantFlags = ~0u;
        if (_pimpl->_props._samples._sampleCount <= 1)
            relevantFlags &= ~AttachmentDesc::Flags::Multisampled;

        std::vector<AttachmentName> result;
        for (const auto&r:requests) {
            // If a semantic value is set, we should first check to see if the request can match
            // one of the bound attachments.
            bool foundMatch = false;
            if (r._semantic) {
                for (unsigned q=0; q<_pimpl->_semanticAttachments.size(); ++q) {
                    if (r._semantic == _pimpl->_semanticAttachments[q]._semantic && !consumedSemantic[q] && _pimpl->_semanticAttachments[q]._resource) {
                        #if defined(_DEBUG)
							if (!MatchRequest(r._desc, _pimpl->_semanticAttachments[q]._desc, _pimpl->_props)) {
                            	Log(Warning) << "Attachment bound to the pool for semantic (0x" << std::hex << r._semantic << std::dec << ") does not match the request for this semantic. Attempting to use it anyway. Request: "
                                	<< r._desc << ", Bound to pool: " << _pimpl->_semanticAttachments[q]._desc
                                	<< std::endl;
                        	}
						#endif

                        consumedSemantic[q] = true;
                        foundMatch = true;
                        result.push_back(q | (1u<<31u));
                        break;
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
                    // We must ensure that the attachment matches all of the flags in the request.
                    // However, we can ignore the "multisampled" flag if FrameBufferProps doesn't have any
                    // multisampling enabled
                    auto requestFlags = r._desc._flags & relevantFlags;
                    auto attachmentFlags = _pimpl->_attachments[q]._desc._flags;
                    if ((attachmentFlags&requestFlags) == requestFlags) {
                        consumed[q] = true;
                        result.push_back(q);
                        foundMatch = true;
                        break;
                    }
                }
            }

            if (!foundMatch) {
				// Prefer "typeless" formats when creating the actual attachments
				// This ensures that we can have complete freedom when we create views
				auto typelessDesc = r._desc;
                #if GFXAPI_TARGET != GFXAPI_OPENGLES        // OpenGLES can't handle the typeless formats current (and they are useless since there aren't "views" on OpenGL) -- so just skip this
				    typelessDesc._format = AsTypelessFormat(typelessDesc._format);
                #endif
                _pimpl->_attachments.push_back(
                    Pimpl::Attachment{nullptr, typelessDesc});
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
            if (existingBinding->_resource == resource)
                return;

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
		assert(existingBinding->_desc._format != Format::Unknown);
        existingBinding->_resource = resource;
    }

    void AttachmentPool::Unbind(const IResource& resource)
    {
        for (auto& binding:_pimpl->_semanticAttachments) {
            if (binding._resource.get() == &resource) {
                _pimpl->_srvPool.Erase(*binding._resource);
                binding._resource = nullptr;
            }
        }
    }

    void AttachmentPool::UnbindAll()
    {
        _pimpl->_semanticAttachments.clear();
    }

	auto AttachmentPool::GetBoundResource(uint64_t semantic) -> IResourcePtr
	{
		auto existingBinding = std::find_if(
            _pimpl->_semanticAttachments.begin(),
            _pimpl->_semanticAttachments.end(),
            [semantic](const Pimpl::SemanticAttachment& a) {
                return a._semantic == semantic;
            });
		if (existingBinding != _pimpl->_semanticAttachments.end())
			return existingBinding->_resource;
		return nullptr;
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

    void AttachmentPool::ResetActualized()
    {
        // Reset all actualized attachments. They will get recreated on demand
        for (auto&attach:_pimpl->_attachments)
            attach._resource.reset();
        _pimpl->_srvPool.Reset();
    }

    std::string AttachmentPool::GetMetrics() const
    {
        std::stringstream str;
        size_t totalByteCount = 0;
        str << "(" << _pimpl->_attachments.size() << ") attachments:" << std::endl;
        for (unsigned c=0; c<_pimpl->_attachments.size(); ++c) {
            auto& desc = _pimpl->_attachments[c]._desc;
            str << "    [" << c << "] " << desc;
            if (_pimpl->_attachments[c]._resource) {
                totalByteCount += ByteCount(_pimpl->_attachments[c]._resource->GetDesc());
                str << " (actualized)";
            } else {
                str << " (not actualized)";
            }
            str << std::endl;
        }

        str << "Total memory: (" << std::setprecision(4) << totalByteCount / (1024.f*1024.f) << "MiB)" << std::endl;
        str << "ViewPool count: (" << _pimpl->_srvPool.GetMetrics()._viewCount << ")" << std::endl;
        return str.str();
    }

    AttachmentPool::AttachmentPool()
    {
        _pimpl = std::make_unique<Pimpl>();
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

    static bool FormatCompatible(Format lhs, Format rhs)
    {
        if (lhs == rhs) return true;
        auto    lhsTypeless = AsTypelessFormat(lhs),
                rhsTypeless = AsTypelessFormat(rhs);
        return lhsTypeless == rhsTypeless;
    }

    bool IsCompatible(const AttachmentDesc& testAttachment, const AttachmentDesc& request, UInt2 dimensions)
    {
        return
            ( (FormatCompatible(testAttachment._format, request._format)) || (testAttachment._format == Format::Unknown) || (request._format == Format::Unknown) )
            && GetArrayCount(testAttachment) == GetArrayCount(request)
			&& DimsEqual(testAttachment, request, FrameBufferProperties{dimensions[0], dimensions[1]})
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
            RetainAfterLoad = 1<<2,
            Store = 1<<3
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

    static DirectionFlags::BitField GetDirectionFlags(const SubpassDesc& p, AttachmentName attachment)
    {
        DirectionFlags::BitField result = 0;
        for (const auto&a:p._output)
            if (a._resourceName == attachment) {
                result |= DirectionFlags::Reference;
                if (HasRetain(a._loadFromPreviousPhase))
                    result |= DirectionFlags::Load;
                if (HasRetain(a._storeToNextPhase))
                    result |= DirectionFlags::Store;
            }
        if (p._depthStencil._resourceName == attachment) {
            result |= DirectionFlags::Reference;
            if (HasRetain(p._depthStencil._loadFromPreviousPhase))
                result |= DirectionFlags::Load;
            if (HasRetain(p._depthStencil._storeToNextPhase))
                result |= DirectionFlags::Store;
        }
        for (const auto&a:p._input)
            if (a._resourceName == attachment) {
                result |= DirectionFlags::Reference | DirectionFlags::Load;
                if (HasRetain(a._storeToNextPhase))
                    result |= DirectionFlags::RetainAfterLoad;
            }
        for (const auto&a:p._resolve)
            if (a._resourceName == attachment) {
                result |= DirectionFlags::Reference | DirectionFlags::Store;
            }
        if (p._depthStencilResolve._resourceName == attachment) {
            result |= DirectionFlags::Reference | DirectionFlags::Store;
        }
        return result;
    }

    static bool ImplicitlyRequiresShaderResourceFlag(const FrameBufferDescFragment& fragment, unsigned attachmentName)
    {
        for (const auto&p:fragment._subpasses)
            for (const auto&a:p._input)
                if (a._resourceName == attachmentName)
                    return true;
        return false;
    }

    class WorkingAttachment
    {
    public:
        AttachmentName _name = ~0u;
        AttachmentDesc _desc;
        uint64_t _shouldReceiveDataForSemantic = 0;         // when looking for an attachment to write the data for this semantic, prefer this attachment
        uint64_t _containsDataForSemantic = 0;              // the data for this semantic is already written to this attachment
        uint64_t _firstReadSemantic = 0;
        uint64_t _lastWriteSemantic = 0;
        bool _isPredefinedAttachment = false;
        bool _forceShaderResourceFlag = false;
        PreregisteredAttachment::State _state = PreregisteredAttachment::State::Uninitialized;
        PreregisteredAttachment::State _stencilState = PreregisteredAttachment::State::Uninitialized;
    };

    static AttachmentName NextName(IteratorRange<const WorkingAttachment*> attachments0, IteratorRange<const WorkingAttachment*> attachments1)
    {
        // find the lowest name not used by any of the attachments
        uint64_t bitField = 0;
        for (const auto& a:attachments0) {
            if (a._name == ~0u) continue;
            assert(a._name < 64);
            assert(!(bitField & (1ull << uint64_t(a._name))));
            bitField |= 1ull << uint64_t(a._name);
        }
        for (const auto& a:attachments1) {
            if (a._name == ~0u) continue;
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
        str << "PreregisteredAttachment { 0x"
            << std::hex << attachment._semantic << std::dec << ", "
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
            << std::hex << "Contains: 0x" << attachment._containsDataForSemantic << ", "
            << "ShouldReceive: 0x" << attachment._shouldReceiveDataForSemantic << ", "
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

    MergeFragmentsResult MergeFragments(
        IteratorRange<const PreregisteredAttachment*> preregisteredInputs,
        IteratorRange<const FrameBufferDescFragment*> fragments,
		UInt2 dimensionsForCompatibilityTests)
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
        }
        shaderResourceSemantics.erase(
            std::unique(shaderResourceSemantics.begin(), shaderResourceSemantics.end()),
            shaderResourceSemantics.end());
        */
        
        std::vector<WorkingAttachment> workingAttachments;
        workingAttachments.reserve(preregisteredInputs.size());
        for (unsigned c=0; c<preregisteredInputs.size(); c++) {
            WorkingAttachment initialState;
            initialState._shouldReceiveDataForSemantic = preregisteredInputs[c]._semantic;
            if (    preregisteredInputs[c]._state == PreregisteredAttachment::State::Initialized
                ||  preregisteredInputs[c]._stencilState == PreregisteredAttachment::State::Initialized)
                initialState._containsDataForSemantic = preregisteredInputs[c]._semantic;
            initialState._desc = preregisteredInputs[c]._desc;
            initialState._state = preregisteredInputs[c]._state;
            initialState._stencilState = preregisteredInputs[c]._stencilState;
            initialState._isPredefinedAttachment = true;
            initialState._name = ~0u;       // start with no name (which equates to unused)
            workingAttachments.push_back(initialState);
        }

        for (auto f=fragments.begin(); f!=fragments.end(); ++f) {
            std::vector<WorkingAttachment> newWorkingAttachments;
            std::vector<std::pair<AttachmentName, AttachmentName>> attachmentRemapping;

            // Capture the default properties for each semantic on the interface now
            std::unordered_map<uint64_t, AttachmentDesc> defaultSemanticFormats;
            for (const auto& a:preregisteredInputs) {
                defaultSemanticFormats[a._semantic] = a._desc;
            }
            for (const auto& a:workingAttachments) {
                if (a._shouldReceiveDataForSemantic)
                    defaultSemanticFormats[a._shouldReceiveDataForSemantic] = a._desc;
                if (a._containsDataForSemantic)
                    defaultSemanticFormats[a._containsDataForSemantic] = a._desc;
            }

            #if defined(_DEBUG)
                debugInfo << "-------------------------------" << std::endl;
                debugInfo << "Fragment [" << std::distance(fragments.begin(), f) << "] " << *f;
            #endif

            /////////////////////////////////////////////////////////////////////////////
            using AttachmentAndDirection = std::pair<AttachmentName, DirectionFlags::BitField>;
            std::vector<AttachmentAndDirection> sortedInterfaceAttachments;
            for (auto interf = f->_attachments.begin(); interf != f->_attachments.end(); ++interf) {
                AttachmentName interfaceAttachmentName = (AttachmentName)std::distance(f->_attachments.begin(), interf);

                DirectionFlags::BitField firstUseDirection = 0;
                // Look through the load/store values in the subpasses to find the "direction" for
                // the first use of this attachment;
                for (auto p = f->_subpasses.begin(); p != f->_subpasses.end(); ++p) {
                    firstUseDirection = GetDirectionFlags(*p, interfaceAttachmentName);
                    if (firstUseDirection)
                        break;
                }
                assert(firstUseDirection != 0);

                sortedInterfaceAttachments.push_back({interfaceAttachmentName, firstUseDirection});
            }

            // sort so the attachment with "load" direction are handled first
            std::stable_sort(
                sortedInterfaceAttachments.begin(), sortedInterfaceAttachments.end(),
                [](const AttachmentAndDirection& lhs, const AttachmentAndDirection& rhs) {
                    return (lhs.second & DirectionFlags::Load) > (rhs.second & DirectionFlags::Load);
                });

            for (const auto&pair:sortedInterfaceAttachments) {
                const auto& interfaceAttachment = f->_attachments[pair.first];
                AttachmentName reboundName = ~0u;
                AttachmentName interfaceAttachmentName = pair.first;
                DirectionFlags::BitField firstUseDirection = pair.second;

                DirectionFlags::BitField lastUseDirection = 0;
                for (auto p = f->_subpasses.rbegin(); p != f->_subpasses.rend(); ++p) {
                    lastUseDirection = GetDirectionFlags(*p, interfaceAttachmentName);
                    if (lastUseDirection)
                        break;
				}

                if (firstUseDirection & DirectionFlags::Load) {
                    // We're expecting a buffer that already has some initialized contents. Look for
                    // something matching in our working attachments array
                    auto interfaceAttachmentNoFlag = interfaceAttachment;
                    interfaceAttachmentNoFlag._desc._flags &= ~AttachmentDesc::Flags::ShaderResource;
                    auto compat = std::find_if(
                        workingAttachments.begin(), workingAttachments.end(),
                        [&interfaceAttachmentNoFlag, dimensionsForCompatibilityTests](const WorkingAttachment& workingAttachment) {
                            return (workingAttachment._state == PreregisteredAttachment::State::Initialized)
                                && (workingAttachment._containsDataForSemantic == interfaceAttachmentNoFlag.GetInputSemanticBinding())
                                && IsCompatible(workingAttachment._desc, interfaceAttachmentNoFlag._desc, dimensionsForCompatibilityTests);
                        });

                    if (compat == workingAttachments.end()) {
                        if (    !interfaceAttachment.GetInputSemanticBinding()
                            ||  interfaceAttachment._desc._format == Format::Unknown) {
                            #if defined(_DEBUG)
                                auto uninitializedCheck = std::find_if(
                                    workingAttachments.begin(), workingAttachments.end(),
                                    [&interfaceAttachment, dimensionsForCompatibilityTests](const WorkingAttachment& workingAttachment) {
                                        return IsCompatible(workingAttachment._desc, interfaceAttachment._desc, dimensionsForCompatibilityTests);
                                    });
                                debugInfo << "      * Failed to find compatible initialized buffer for request: " << interfaceAttachment._desc << ". Semantic: 0x" << std::hex << interfaceAttachment.GetInputSemanticBinding() << std::dec << std::endl;
                                if (uninitializedCheck != workingAttachments.end())
                                    debugInfo << "      * Buffer " << std::distance(workingAttachments.begin(), uninitializedCheck) << " is compatible, but does not contain any initialized data (is there a missing Retain flag?)" << std::endl;
                                debugInfo << "      * Working attachments are: " << std::endl;
                                for (const auto& att : workingAttachments)
                                    debugInfo << att << std::endl;
                                auto debugInfoStr = debugInfo.str();
                                Log(Error) << "MergeFragments() failed. Details:" << std::endl << debugInfoStr << std::endl;
                                Throw(::Exceptions::BasicLabel("Couldn't bind renderpass fragment input request. Details:\n%s\n", debugInfoStr.c_str()));
                            #else
                                Throw(::Exceptions::BasicLabel("Couldn't bind renderpass fragment input request"));
                            #endif
                        }

                        // This is a new buffer that will be part of the input interface for the
                        // final fragment.
                        // Note that we don't allow an attachment with a "Unknown" format to be defined
                        // in this way -- just because that could start to get confusing to the caller.
                        reboundName = NextName(MakeIteratorRange(workingAttachments), MakeIteratorRange(newWorkingAttachments));

                        WorkingAttachment newState;
                        newState._desc = interfaceAttachment._desc;
                        newState._state = (lastUseDirection & (DirectionFlags::Store|DirectionFlags::RetainAfterLoad)) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized;
                        newState._stencilState = (lastUseDirection & (DirectionFlags::Store|DirectionFlags::RetainAfterLoad)) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized;

                        newState._firstReadSemantic = interfaceAttachment.GetInputSemanticBinding();
                        if (lastUseDirection & (DirectionFlags::Store|DirectionFlags::RetainAfterLoad)) {
                            newState._containsDataForSemantic = interfaceAttachment.GetOutputSemanticBinding();
                            newState._lastWriteSemantic = interfaceAttachment.GetOutputSemanticBinding();
                        }
                        newState._name = reboundName;
                        newWorkingAttachments.push_back(newState);
                    } else {
                        reboundName = compat->_name;
                        if (reboundName == ~0u)
                            reboundName = NextName(MakeIteratorRange(workingAttachments), MakeIteratorRange(newWorkingAttachments));

                        // Remove from the working attachments and push back in it's new state
                        // If we're not writing to this attachment, it will lose it's semantic here
                        auto newState = *compat;
                        newState._state = (lastUseDirection & (DirectionFlags::Store|DirectionFlags::RetainAfterLoad)) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized;
                        newState._stencilState = (lastUseDirection & (DirectionFlags::Store|DirectionFlags::RetainAfterLoad)) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized;
                        if ((interfaceAttachment._desc._flags & AttachmentDesc::Flags::ShaderResource) || ImplicitlyRequiresShaderResourceFlag(*f, interfaceAttachmentName))
                            newState._desc._flags |= AttachmentDesc::Flags::ShaderResource;

                        newState._shouldReceiveDataForSemantic = compat->_shouldReceiveDataForSemantic;
                        if (!newState._firstReadSemantic && compat->_isPredefinedAttachment) {    // (we only really care about first read for predefined attachments)
                            newState._firstReadSemantic = interfaceAttachment.GetInputSemanticBinding();
                        }
                        if (lastUseDirection & (DirectionFlags::Store|DirectionFlags::RetainAfterLoad)) {
                            newState._containsDataForSemantic =  interfaceAttachment.GetOutputSemanticBinding();
                            newState._lastWriteSemantic = interfaceAttachment.GetOutputSemanticBinding();
                        } else {
                            newState._containsDataForSemantic = 0;
                            newState._lastWriteSemantic = 0;
                        }
                        newState._isPredefinedAttachment = false;
                        newState._name = reboundName;
                        workingAttachments.erase(compat);
                        newWorkingAttachments.push_back(newState);
                    }
                } else {
                    // define a new output buffer, or reuse something that we can reuse
                    // Prefer a buffer that is uninitialized, but we can drop back to something that
                    // is initialized if we have to
                    auto compat = std::find_if(
                        workingAttachments.begin(), workingAttachments.end(),
                        [&interfaceAttachment, dimensionsForCompatibilityTests](const WorkingAttachment& workingAttachment) {
                            return (workingAttachment._shouldReceiveDataForSemantic == interfaceAttachment.GetOutputSemanticBinding())
                                && (workingAttachment._state == PreregisteredAttachment::State::Uninitialized)
                                && IsCompatible(workingAttachment._desc, interfaceAttachment._desc, dimensionsForCompatibilityTests);
                        });

                    if (compat == workingAttachments.end() && interfaceAttachment.GetOutputSemanticBinding()) {
                        // Look for a buffer with no assigned semantic. It will take on the semantic we
                        // give it
                        compat = std::find_if(
                            workingAttachments.begin(), workingAttachments.end(),
                            [&interfaceAttachment, dimensionsForCompatibilityTests](const WorkingAttachment& workingAttachment) {
                                return (workingAttachment._shouldReceiveDataForSemantic == 0)
                                    && (workingAttachment._state == PreregisteredAttachment::State::Uninitialized)
                                    && IsCompatible(workingAttachment._desc, interfaceAttachment._desc, dimensionsForCompatibilityTests);
                            });
                    }

                    if (compat == workingAttachments.end() && interfaceAttachment._desc._format != Format::Unknown) {
                        // Couldn't find a buffer in the "uninitialized" state. We're just going to
                        // find a initialized buffer and overwrite it's contents.
                        // We need this flexibility because some fragments use "retain" on their
                        // last read or write speculatively (maybe because they don't know whether the buffer
                        // will actually be read from again, or they have been mistakenly marked as
                        // retain)
                        // (this relies on us sorting the attachment list so load operations are processed
                        // first to work reliably)
						// Also, we shouldn't do this when the request format is "unknown" -- because then we
						// can match against almost anything
                        compat = std::find_if(
                            workingAttachments.begin(), workingAttachments.end(),
                            [&interfaceAttachment, dimensionsForCompatibilityTests](const WorkingAttachment& workingAttachment) {
                                return (workingAttachment._shouldReceiveDataForSemantic == interfaceAttachment.GetOutputSemanticBinding() || workingAttachment._shouldReceiveDataForSemantic == 0)
                                    && IsCompatible(workingAttachment._desc, interfaceAttachment._desc, dimensionsForCompatibilityTests);
                            });
                    }

                    if (compat == workingAttachments.end()) {
                        // Technically we could do a second pass looking for some initialized attachment
                        // we could write over -- but that would lead to other complications. Let's just
                        // create something new.

                        reboundName = NextName(MakeIteratorRange(workingAttachments), MakeIteratorRange(newWorkingAttachments));

                        // We can steal the settings from an existing attachment with the same semantic
                        // name, if necessary. We get these from a capture of the working attachments
                        // we made at the same of the fragment
                        auto desc = interfaceAttachment._desc;
                        auto sameSemantic = defaultSemanticFormats.find(interfaceAttachment.GetOutputSemanticBinding());
                        if (sameSemantic != defaultSemanticFormats.end()) {
                            if (desc._format == Format::Unknown) desc._format = sameSemantic->second._format;
                        } else {
                            if (desc._format == Format::Unknown) {
                                #if defined(_DEBUG)
                                    debugInfo << "      * Could not resolve correct format for attachment: " << interfaceAttachment._desc << ". Semantic: 0x" << std::hex << interfaceAttachment.GetInputSemanticBinding() << std::dec << std::endl;
                                    for (const auto& att : workingAttachments)
                                        debugInfo << att << std::endl;
                                    auto debugInfoStr = debugInfo.str();
                                    Log(Error) << "MergeFragments() failed. Details:" << std::endl << debugInfoStr << std::endl;
                                    Throw(::Exceptions::BasicLabel("Couldn't bind renderpass fragment input request. Details:\n%s\n", debugInfoStr.c_str()));
                                #else
                                    Throw(::Exceptions::BasicLabel("Couldn't bind renderpass fragment input request"));
                                #endif
                            }
                        }

                        WorkingAttachment newState;
                        newState._name = reboundName;
                        newState._desc = desc;
                        newState._state = (lastUseDirection & DirectionFlags::Store) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized;
                        newState._stencilState = (lastUseDirection & DirectionFlags::Store) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized;
                        if (lastUseDirection & DirectionFlags::Store) {
                            auto writeSemantic = interfaceAttachment.GetOutputSemanticBinding();
                            newState._containsDataForSemantic = writeSemantic;
                            newState._lastWriteSemantic = writeSemantic;

                            // If there are any other attachments with the same semantic, we have to clear them now
                            // 2 different subpasses can conceivably write to the same semantic data to 2 completely
                            // different attachments. In these cases, the last subpass should always win out
                            for (auto&a:workingAttachments)
                                if (a._containsDataForSemantic == writeSemantic)
                                    a._containsDataForSemantic = 0;
                            for (auto&a:newWorkingAttachments)
                                if (a._containsDataForSemantic == writeSemantic)
                                    a._containsDataForSemantic = 0;
                        }
                        newState._isPredefinedAttachment = false;
                        newWorkingAttachments.push_back(newState);

                        #if defined(_DEBUG)
                            debugInfo 
                                << "      * " 
                                << "Cannot find compatible buffer, creating #" << reboundName << ", " << newState << std::endl;
                        #endif
                    } else {
                        reboundName = compat->_name;
                        if (reboundName == ~0u)
                            reboundName = NextName(MakeIteratorRange(workingAttachments), MakeIteratorRange(newWorkingAttachments));

                        // remove from the working attachments and push back in it's new state
                        auto newState = *compat;
                        newState._state = (lastUseDirection & DirectionFlags::Store) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized;
                        newState._stencilState = (lastUseDirection & DirectionFlags::Store) ? PreregisteredAttachment::State::Initialized : PreregisteredAttachment::State::Uninitialized;
                        if (lastUseDirection & DirectionFlags::Store) {
                            newState._containsDataForSemantic = interfaceAttachment.GetOutputSemanticBinding();
                            newState._lastWriteSemantic = interfaceAttachment.GetOutputSemanticBinding();
                        }
                        newState._isPredefinedAttachment = false;
                        newState._name = reboundName;
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
            if (a._name == ~0u) continue;
            // The AttachmentNames in FrameBufferDescFragment are just indices into the attachment
            // list -- so we must ensure that we insert in order, and without gaps
            assert(a._name == result._attachments.size());
            FrameBufferDescFragment::Attachment r { a._firstReadSemantic, a._containsDataForSemantic, a._desc };
            result._attachments.push_back(r);
        }

        MergeFragmentsResult finalResult;
        finalResult._mergedFragment = std::move(result);
        finalResult._remapping = std::move(fragmentRemapping);

        for (auto& a:workingAttachments) {
            if (a._name == ~0u) continue;
            if (a._firstReadSemantic)
                finalResult._inputAttachments.push_back({a._firstReadSemantic, a._name});
            if (a._containsDataForSemantic)
                finalResult._outputAttachments.push_back({a._containsDataForSemantic, a._name});
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
            debugInfo << "Interface summary" << std::endl;
            for (unsigned c=0; c<finalResult._inputAttachments.size(); ++c)
                debugInfo << StreamIndent(4) << "Input [" << c << "] 0x" << std::hex << finalResult._inputAttachments[c].first << std::dec << " " << finalResult._inputAttachments[c].second << " (" << finalResult._mergedFragment._attachments[finalResult._inputAttachments[c].second]._desc << ")" << std::endl;
            for (unsigned c=0; c<finalResult._outputAttachments.size(); ++c)
                debugInfo << StreamIndent(4) << "Output [" << c << "] 0x" << std::hex << finalResult._outputAttachments[c].first << std::dec << " " << finalResult._outputAttachments[c].second << " (" << finalResult._mergedFragment._attachments[finalResult._outputAttachments[c].second]._desc << ")" << std::endl;
            debugInfo << "MergeFragments() finished." << std::endl;
            finalResult._log = debugInfo.str();
        #endif
        
        return finalResult;
    }


    static RenderCore::AttachmentViewDesc RemapAttachmentView(
        const RenderCore::AttachmentViewDesc& input,
        const RenderCore::Techniques::FrameBufferDescFragment& srcFragment,
        RenderCore::Techniques::FrameBufferDescFragment& dstFragment,
        std::vector<std::pair<RenderCore::AttachmentName, RenderCore::AttachmentName>>& remapping)
    {
        if (input._resourceName == ~0u) return input;

        auto existing = LowerBound(remapping, input._resourceName);
        if (existing == remapping.end() || existing->first != input._resourceName) {
            auto semantic = srcFragment._attachments[input._resourceName].GetInputSemanticBinding();
            auto newName = dstFragment.DefineAttachment(
                semantic,
                srcFragment._attachments[input._resourceName]._desc);
            existing = remapping.insert(existing, {input._resourceName, newName});
        }

        auto result = input;
        result._resourceName = existing->second;
        return result;
    }

    bool CanBeSimplified(
        const RenderCore::Techniques::FrameBufferDescFragment& inputFragment,
        IteratorRange<const RenderCore::Techniques::PreregisteredAttachment*> systemAttachments)
    {
        TRY
        {
            using namespace RenderCore;
            using Fragment = Techniques::FrameBufferDescFragment;
            std::vector<Fragment> testFragments;
            // Create a separate fragment for each subpass
            for (const auto&subpass:inputFragment._subpasses) {
                std::vector<std::pair<AttachmentName, AttachmentName>> remapping;
                Fragment separatedFragment;
                RenderCore::SubpassDesc remappedSubpass;
                for (const auto&a:subpass._output) remappedSubpass._output.push_back(RemapAttachmentView(a, inputFragment, separatedFragment, remapping));
                remappedSubpass._depthStencil = RemapAttachmentView(remappedSubpass._depthStencil, inputFragment, separatedFragment, remapping);
                for (const auto&a:subpass._input) remappedSubpass._input.push_back(RemapAttachmentView(a, inputFragment, separatedFragment, remapping));
                for (const auto&a:subpass._preserve) remappedSubpass._preserve.push_back(RemapAttachmentView(a, inputFragment, separatedFragment, remapping));
                for (const auto&a:subpass._resolve) remappedSubpass._resolve.push_back(RemapAttachmentView(a, inputFragment, separatedFragment, remapping));
                #if defined(_DEBUG)
                    remappedSubpass.SetName(subpass._name);
                #endif
                separatedFragment.AddSubpass(std::move(remappedSubpass));
                testFragments.emplace_back(std::move(separatedFragment));
            }
            auto collapsed = RenderCore::Techniques::MergeFragments(
                systemAttachments, MakeIteratorRange(testFragments));
            assert(collapsed._mergedFragment._attachments.size() <= inputFragment._attachments.size());
            if (collapsed._mergedFragment._attachments.size() < inputFragment._attachments.size()) {
                return true;
            }
            return false;
        } CATCH(const std::exception& e) {
            Log(Warning) << "Error during AnalyzeFragment while processing render step: " << e.what() << std::endl;
        } CATCH_END
        return false;
    }

    void MergeInOutputs(
        std::vector<PreregisteredAttachment>& workingSystemAttachments,
        const FrameBufferDescFragment& fragment)
    {
        for (const auto& fragAttachment:fragment._attachments) {
            if (fragAttachment.GetOutputSemanticBinding() == 0) continue;
            bool foundExisting = false;
            for (auto&dstAttachment:workingSystemAttachments)
                if (dstAttachment._semantic == fragAttachment.GetOutputSemanticBinding()) {
                    dstAttachment._state = PreregisteredAttachment::State::Initialized;
                    dstAttachment._stencilState = PreregisteredAttachment::State::Initialized;
                    // dstAttachment._desc = fragAttachment._desc;
                    foundExisting = true;
                    break;
                }
            if (!foundExisting) {
                workingSystemAttachments.push_back({
                    fragAttachment.GetOutputSemanticBinding(),
                    fragAttachment._desc,
                    PreregisteredAttachment::State::Initialized,
                    PreregisteredAttachment::State::Initialized});
            }
        }
    }

}}

