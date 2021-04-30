// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderPass.h"
#include "ParsingContext.h"
#include "Techniques.h"
#include "CommonBindings.h"     // (for semantic dehash)
#include "../Metal/FrameBuffer.h"
#include "../Metal/TextureView.h"
#include "../Metal/DeviceContext.h"
#include "../Metal/ObjectFactory.h"
#include "../Metal/State.h"
#include "../Metal/Resource.h"
#include "../Format.h"
#include "../ResourceUtils.h"
#include "../IThreadContext.h"
#include "../../OSServices/Log.h"
#include "../../Utility/ArithmeticUtils.h"
#include "../../Utility/StreamUtils.h"
#include <cmath>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <set>

namespace RenderCore
{
    static std::ostream& operator<<(std::ostream& str, const AttachmentDesc& attachment)
    {
        str << "AttachmentDesc { "
            #if defined(_DEBUG)
                << (!attachment._name.empty()?attachment._name:std::string("<<no name>>")) << ", "
            #endif
            << AsString(attachment._format) << ", "
            << attachment._width << ", "
            << attachment._height << ", "
            << attachment._arrayLayerCount
            << ", 0x" << std::hex << attachment._flags << std::dec
            << ", 0x" << std::hex << attachment._bindFlagsForFinalLayout << std::dec << " }";
        return str;
    }

    static std::ostream& operator<<(std::ostream& str, const SubpassDesc& subpass)
    {
        str << "SubpassDesc { "
            #if defined(_DEBUG)
                << (!subpass._name.empty()?subpass._name:std::string("<<no name>>")) << ", "
            #endif
            << "outputs [";
        for (unsigned c=0; c<subpass.GetOutputs().size(); ++c) { if (c!=0) str << ", "; str << subpass.GetOutputs()[c]._resourceName; }
        str << "], DepthStencil: ";
        if (subpass.GetDepthStencil()._resourceName != ~0u) { str << subpass.GetDepthStencil()._resourceName; } else { str << "<<none>>"; }
        str << ", inputs [";
        for (unsigned c=0; c<subpass.GetInputs().size(); ++c) { if (c!=0) str << ", "; str << subpass.GetInputs()[c]._resourceName; }
        /*str << "], preserve [";
        for (unsigned c=0; c<subpass._preserve.size(); ++c) { if (c!=0) str << ", "; str << subpass._preserve[c]._resourceName; }*/
        str << "], resolve [";
        for (unsigned c=0; c<subpass.GetResolveOutputs().size(); ++c) { if (c!=0) str << ", "; str << subpass.GetResolveOutputs()[c]._resourceName; }
        str << "], resolveDepthStencil: ";
        if (subpass.GetResolveDepthStencil()._resourceName != ~0u) { str << subpass.GetResolveDepthStencil()._resourceName << " }"; }
        else { str << "<<none>> }"; }
        return str;
    }

    static std::ostream& operator<<(std::ostream& str, const ResourceDesc& desc)
    {
        str << "ResourceDesc { ";
        if (desc._type == ResourceDesc::Type::Texture) {
            str << "[Texture] ";
            switch (desc._textureDesc._dimensionality) {
                case TextureDesc::Dimensionality::T1D: str << desc._textureDesc._width; break;
                case TextureDesc::Dimensionality::T2D: str << desc._textureDesc._width << "x" << desc._textureDesc._height; break;
                case TextureDesc::Dimensionality::T3D: str << desc._textureDesc._width << "x" << desc._textureDesc._height << "x" << desc._textureDesc._depth; break;
                case TextureDesc::Dimensionality::CubeMap: str << desc._textureDesc._width << "x" << desc._textureDesc._height << " cube"; break;
                default: str << "<<unknown dimensionality>>";
            }
            str << ", " << AsString(desc._textureDesc._format)
                << ", " << desc._textureDesc._arrayCount
                << ", " << desc._textureDesc._arrayCount
                << ", " << desc._textureDesc._arrayCount
                << ", 0x" << std::hex << desc._bindFlags << std::dec;
        } else {
            str << "[Buffer] " << Utility::ByteCount(desc._linearBufferDesc._sizeInBytes)
                << ", 0x" << std::hex << desc._bindFlags << std::dec;
        }
        return str;
    }
}

namespace RenderCore { namespace Techniques
{
    static BindFlag::BitField CalculateBindFlags(const FrameBufferDescFragment& fragment, unsigned attachmentName);
    struct AttachmentSemantic { uint64_t _value = 0; };
    static std::ostream& operator<<(std::ostream& str, AttachmentSemantic semantic)
    {
        auto dehash = AttachmentSemantics::TryDehash(semantic._value);
        if (dehash) str << dehash;
        else str << "0x" << std::hex << semantic._value << std::dec;
        return str;
    }

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

    class NamedAttachmentsWrapper : public INamedAttachments
    {
    public:
		virtual IResourcePtr GetResource(AttachmentName resName, const AttachmentDesc& requestDesc, const FrameBufferProperties& props) const override;
		// virtual const AttachmentDesc* GetDesc(AttachmentName resName) const;
		// virtual const FrameBufferProperties& GetFrameBufferProperties() const;

        NamedAttachmentsWrapper(
            AttachmentPool& pool,
            const std::vector<AttachmentName>& poolMapping);
        ~NamedAttachmentsWrapper();
    private:
        AttachmentPool* _pool;
        const std::vector<AttachmentName>* _poolMapping;
    };

    IResourcePtr NamedAttachmentsWrapper::GetResource(AttachmentName resName, const AttachmentDesc& requestDesc, const FrameBufferProperties& props) const
    {
        assert(resName < _poolMapping->size());
        auto result = _pool->GetResource((*_poolMapping)[resName])._resource;

        #if defined(_DEBUG)
            // Validate that the "desc" for the returned resource matches what the caller was requesting
            auto resultDesc = result->GetDesc();

            unsigned requestAttachmentWidth, requestAttachmentHeight;
            if (!(requestDesc._flags & AttachmentDesc::Flags::OutputRelativeDimensions)) {
                requestAttachmentWidth = unsigned(requestDesc._width);
                requestAttachmentHeight = unsigned(requestDesc._height);
            } else {
                requestAttachmentWidth = unsigned(std::floor(props._outputWidth * requestDesc._width));
                requestAttachmentHeight = unsigned(std::floor(props._outputHeight * requestDesc._height));
            }

            assert(requestDesc._format == Format(0) || AsTypelessFormat(requestDesc._format) == AsTypelessFormat(resultDesc._textureDesc._format));
            assert(requestAttachmentWidth == resultDesc._textureDesc._width);
            assert(requestAttachmentHeight == resultDesc._textureDesc._height);
            assert(requestDesc._arrayLayerCount == resultDesc._textureDesc._arrayCount);
            assert((requestDesc._bindFlagsForFinalLayout & resultDesc._bindFlags) == requestDesc._bindFlagsForFinalLayout);
        #endif

        return result;
    }

    NamedAttachmentsWrapper::NamedAttachmentsWrapper(
        AttachmentPool& pool,
        const std::vector<AttachmentName>& poolMapping)
    : _pool(&pool)
    , _poolMapping(&poolMapping) {}
    NamedAttachmentsWrapper::~NamedAttachmentsWrapper() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    class FrameBufferPool
    {
    public:
        class Result
        {
        public:
            std::shared_ptr<Metal::FrameBuffer> _frameBuffer;
            IteratorRange<const AttachmentName*> _poolAttachmentsRemapping;
            const FrameBufferDesc* _completedDesc;
            std::vector<IResource*> _uncompletedInitializationResources;
        };
        Result BuildFrameBuffer(
            Metal::ObjectFactory& factory,
            const FrameBufferDesc& desc,
            AttachmentPool& attachmentPool,
            IteratorRange<const PreregisteredAttachment*> preregisteredAttachments);

        void Reset();

        FrameBufferPool();
        ~FrameBufferPool();
    private:
        class Entry
        {
        public:
            uint64_t _hash = ~0ull;
            unsigned _tickId = 0;
            std::shared_ptr<Metal::FrameBuffer> _fb;
            std::vector<AttachmentName> _poolAttachmentsRemapping;
            FrameBufferDesc _completedDesc;
        };
        Entry _entries[10];
        unsigned _currentTickId = 0;

        void IncreaseTickId();
    };

    void FrameBufferPool::IncreaseTickId()
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
        AttachmentPool& attachmentPool,
        IteratorRange<const PreregisteredAttachment*> preregisteredAttachments) -> Result
    {    
        auto poolAttachments = attachmentPool.Request(desc, preregisteredAttachments);
		assert(poolAttachments.size() == desc.GetAttachments().size());

        std::vector<FrameBufferDesc::Attachment> adjustedAttachments;
        adjustedAttachments.reserve(desc.GetAttachments().size());
        Result result;

        uint64_t hashValue = DefaultSeed64;
        for (unsigned c=0; c<desc.GetAttachments().size(); ++c) {
            auto matchedAttachment = attachmentPool.GetResource(poolAttachments[c]);
            assert(matchedAttachment._resource);
            hashValue = HashCombine(matchedAttachment._resource->GetGUID(), hashValue);

            if (matchedAttachment._needsCompleteResource)
                result._uncompletedInitializationResources.push_back(matchedAttachment._resource.get());

            // The attachment descriptions in the input FrameBufferDesc may not be 100% complete, however
            // in the process of matching them to the attachment pool, we will have filled in any missing
            // info (ie, either from the preregistered attachments or from existing attachments in the pool)
            // We must merge this updated information back into the FrameBufferDesc.
            // This also has the effect of normalizing the attachment desc information for the hash value,
            // which would help cases where functionality identical information produces different hash value
            auto completeAttachmentDesc = AsAttachmentDesc(matchedAttachment._resource->GetDesc());
            completeAttachmentDesc._bindFlagsForFinalLayout = desc.GetAttachments()[c]._desc._bindFlagsForFinalLayout;
            adjustedAttachments.push_back({desc.GetAttachments()[c]._semantic, completeAttachmentDesc});
        }

        FrameBufferDesc adjustedDesc(
            std::move(adjustedAttachments),
            {desc.GetSubpasses().begin(), desc.GetSubpasses().end()},
            desc.GetProperties()); 
        hashValue = HashCombine(adjustedDesc.GetHash(), hashValue);
        assert(hashValue != ~0ull);     // using ~0ull has a sentinel, so this will cause some problems

        unsigned earliestEntry = 0;
        unsigned tickIdOfEarliestEntry = ~0u;
        for (unsigned c=0; c<dimof(_entries); ++c) {
            if (_entries[c]._hash == hashValue) {
                _entries[c]._tickId = _currentTickId;
				_entries[c]._poolAttachmentsRemapping = std::move(poolAttachments);	// update the mapping, because attachments map have moved
                IncreaseTickId();
                assert(_entries[c]._fb != nullptr);
                return {
                    _entries[c]._fb,
                    MakeIteratorRange(_entries[c]._poolAttachmentsRemapping),
                    &_entries[c]._completedDesc
                };
            }
            if (_entries[c]._tickId < tickIdOfEarliestEntry) {
                tickIdOfEarliestEntry = _entries[c]._tickId;
                earliestEntry = c;
            }
        }

        // Can't find it; we're just going to overwrite the oldest entry with a new one
        assert(earliestEntry < dimof(_entries));
//        if (_pimpl->_entries[earliestEntry]._fb) {
//            Log(Warning) << "Overwriting tail in FrameBufferPool(). There may be too many different framebuffers required from the same pool" << std::endl;
//        }

        NamedAttachmentsWrapper namedAttachments(attachmentPool, poolAttachments);
        assert(adjustedDesc.GetSubpasses().size());
        _entries[earliestEntry]._fb = std::make_shared<Metal::FrameBuffer>(
            factory,
            adjustedDesc, namedAttachments);
        _entries[earliestEntry]._tickId = _currentTickId;
        _entries[earliestEntry]._hash = hashValue;
        _entries[earliestEntry]._poolAttachmentsRemapping = std::move(poolAttachments);
        _entries[earliestEntry]._completedDesc = std::move(adjustedDesc);
        IncreaseTickId();
        result._frameBuffer = _entries[earliestEntry]._fb;
        result._poolAttachmentsRemapping = MakeIteratorRange(_entries[earliestEntry]._poolAttachmentsRemapping);
        result._completedDesc = &_entries[earliestEntry]._completedDesc;
        return result;
    }

    void FrameBufferPool::Reset()
    {
        for (unsigned c=0; c<dimof(_entries); ++c)
            _entries[c] = {};
        _currentTickId = 0;
    }

    FrameBufferPool::FrameBufferPool()
    {
    }

    FrameBufferPool::~FrameBufferPool()
    {}

    std::shared_ptr<FrameBufferPool> CreateFrameBufferPool()
    {
        return std::make_shared<FrameBufferPool>();
    }

    void ResetFrameBufferPool(FrameBufferPool& fbPool)
    {
        fbPool.Reset();
    }

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
			_attachedContext = nullptr;
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

    auto RenderPassInstance::GetResourceForAttachmentName(AttachmentName resName) const -> IResourcePtr
    {
        assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetResource(_attachmentPoolRemapping[resName])._resource;
        return nullptr;
    }

    auto RenderPassInstance::GetSRVForAttachmentName(AttachmentName resName, const TextureViewDesc& window) const -> IResourceView*
    {
        assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetSRV(_attachmentPoolRemapping[resName], window);
        return nullptr;
    }

    auto RenderPassInstance::GetInputAttachmentResource(unsigned inputAttachmentSlot) const -> IResourcePtr
	{
		const auto& subPass = _layout.GetSubpasses()[GetCurrentSubpassIndex()];
		auto resName = subPass.GetInputs()[inputAttachmentSlot]._resourceName;
		assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetResource(_attachmentPoolRemapping[resName])._resource;
        return nullptr;
	}

    auto RenderPassInstance::GetInputAttachmentSRV(unsigned inputAttachmentSlot) const -> IResourceView*
	{
		const auto& subPass = _layout.GetSubpasses()[GetCurrentSubpassIndex()];
		auto resName = subPass.GetInputs()[inputAttachmentSlot]._resourceName;
		assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetSRV(_attachmentPoolRemapping[resName], subPass.GetInputs()[inputAttachmentSlot]._window);
        return nullptr;
	}
	
	auto RenderPassInstance::GetInputAttachmentSRV(unsigned inputAttachmentSlot, const TextureViewDesc& window) const -> IResourceView*
	{
		const auto& subPass = _layout.GetSubpasses()[GetCurrentSubpassIndex()];
		auto resName = subPass.GetInputs()[inputAttachmentSlot]._resourceName;
		assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetSRV(_attachmentPoolRemapping[resName], window);
        return nullptr;
	}

	auto RenderPassInstance::GetOutputAttachmentResource(unsigned inputAttachmentSlot) const -> IResourcePtr
	{
		const auto& subPass = _layout.GetSubpasses()[GetCurrentSubpassIndex()];
		auto resName = subPass.GetOutputs()[inputAttachmentSlot]._resourceName;
		assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetResource(_attachmentPoolRemapping[resName])._resource;
        return nullptr;
	}

    auto RenderPassInstance::GetOutputAttachmentSRV(unsigned inputAttachmentSlot) const -> IResourceView*
	{
		const auto& subPass = _layout.GetSubpasses()[GetCurrentSubpassIndex()];
		auto resName = subPass.GetOutputs()[inputAttachmentSlot]._resourceName;
		assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetSRV(_attachmentPoolRemapping[resName], subPass.GetInputs()[inputAttachmentSlot]._window);
        return nullptr;
	}
	
	auto RenderPassInstance::GetOutputAttachmentSRV(unsigned inputAttachmentSlot, const TextureViewDesc& window) const -> IResourceView*
	{
		const auto& subPass = _layout.GetSubpasses()[GetCurrentSubpassIndex()];
		auto resName = subPass.GetOutputs()[inputAttachmentSlot]._resourceName;
		assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetSRV(_attachmentPoolRemapping[resName], window);
        return nullptr;
	}

	auto RenderPassInstance::GetDepthStencilAttachmentSRV(const TextureViewDesc& window) const -> IResourceView*
	{
		const auto& subPass = _layout.GetSubpasses()[GetCurrentSubpassIndex()];
		auto resName = subPass.GetDepthStencil()._resourceName;
		assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetSRV(_attachmentPoolRemapping[resName], window);
        return nullptr;
	}

	auto RenderPassInstance::GetDepthStencilAttachmentResource(unsigned inputAttachmentSlot) const -> IResourcePtr
	{
		const auto& subPass = _layout.GetSubpasses()[GetCurrentSubpassIndex()];
		auto resName = subPass.GetDepthStencil()._resourceName;
		assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetResource(_attachmentPoolRemapping[resName])._resource;
        return nullptr;
	}

    auto RenderPassInstance::GetDepthStencilAttachmentSRV(unsigned inputAttachmentSlot) const -> IResourceView*
	{
		const auto& subPass = _layout.GetSubpasses()[GetCurrentSubpassIndex()];
		auto resName = subPass.GetDepthStencil()._resourceName;
		assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetSRV(_attachmentPoolRemapping[resName], subPass.GetInputs()[inputAttachmentSlot]._window);
        return nullptr;
	}
	
	auto RenderPassInstance::GetDepthStencilAttachmentSRV(unsigned inputAttachmentSlot, const TextureViewDesc& window) const -> IResourceView*
	{
		const auto& subPass = _layout.GetSubpasses()[GetCurrentSubpassIndex()];
		auto resName = subPass.GetDepthStencil()._resourceName;
		assert(_attachmentPool);
        if (resName < _attachmentPoolRemapping.size())
            return _attachmentPool->GetSRV(_attachmentPoolRemapping[resName], window);
        return nullptr;
	}

    RenderPassInstance::RenderPassInstance(
        IThreadContext& context,
        const FrameBufferDesc& layout,
        FrameBufferPool& frameBufferPool,
        AttachmentPool& attachmentPool,
        IteratorRange<const PreregisteredAttachment*> preregisteredAttachments,
        const RenderPassBeginDesc& beginInfo)
    {
        _attachedContext = Metal::DeviceContext::Get(context).get();

        auto fb = frameBufferPool.BuildFrameBuffer(
            Metal::GetObjectFactory(*context.GetDevice()),
            layout, attachmentPool,
            preregisteredAttachments);

        if (!fb._uncompletedInitializationResources.empty())
            Metal::CompleteInitialization(*_attachedContext, MakeIteratorRange(fb._uncompletedInitializationResources));

        _frameBuffer = std::move(fb._frameBuffer);
        _attachmentPoolRemapping = std::vector<AttachmentName>(fb._poolAttachmentsRemapping.begin(), fb._poolAttachmentsRemapping.end());
        _attachmentPool = &attachmentPool;
        _layout = *fb._completedDesc;
        // todo -- we might need to pass offset & extent parameters to BeginRenderPass
        // this could be derived from _attachmentPool->GetFrameBufferProperties()?
        Metal::BeginRenderPass(*_attachedContext, *_frameBuffer, beginInfo._clearValues);
    }

    RenderPassInstance::RenderPassInstance(
        IThreadContext& context,
        ParsingContext& parsingContext,
        const FrameBufferDesc& layout,
        const RenderPassBeginDesc& beginInfo)
    : RenderPassInstance(
        context, layout, 
        *parsingContext.GetTechniqueContext()._frameBufferPool,
        *parsingContext.GetTechniqueContext()._attachmentPool,
        parsingContext._preregisteredAttachments,
        beginInfo)
    {
    }

	RenderPassInstance::RenderPassInstance(
        const FrameBufferDesc& layout,
        AttachmentPool& attachmentPool)
	: _layout(layout)
    {
		// This constructs a kind of "non-metal" RenderPassInstance
		// It allows us to use the RenderPassInstance infrastructure (for example, for remapping attachment requests)
		// without actually constructing a underlying metal renderpass.
		// This is used with compute pipelines sometimes -- since in Vulkan, those have some similarities with
		// graphics pipelines, but are incompatible with the vulkan render passes
		_attachedContext = nullptr;
		_attachmentPoolRemapping = attachmentPool.Request(layout);
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
	, _layout(std::move(moveFrom._layout))
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
		_layout = std::move(moveFrom._layout);
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
            ResourceDesc		    _desc;
        };
        std::vector<Attachment>                         _attachments;

        struct SemanticAttachment : public Attachment
        {
            uint64_t        _semantic;
        };
        std::vector<SemanticAttachment>    _semanticAttachments;

        ViewPool                    _srvPool;
        std::shared_ptr<IDevice>    _device;

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

        attach->_resource = _device->CreateResource(attach->_desc);
        return attach->_resource != nullptr;
    }
    
    auto AttachmentPool::GetResource(AttachmentName attachName) const -> GetResourceResult
    {
        Pimpl::Attachment* attach = nullptr;
        if (attachName & (1u<<31u)) {
            auto semanticAttachIdx = attachName & ~(1u<<31u);
            if (semanticAttachIdx >= _pimpl->_semanticAttachments.size()) return {nullptr};
            attach = &_pimpl->_semanticAttachments[semanticAttachIdx];
        } else {
            if (attachName >= _pimpl->_attachments.size()) return {nullptr};
            attach = &_pimpl->_attachments[attachName];
        }
        assert(attach);
        if (attach->_resource)
            return GetResourceResult { attach->_resource, false };
            
        _pimpl->BuildAttachment(attachName);
        return GetResourceResult { attach->_resource, true };
	}

    static TextureViewDesc CompleteTextureViewDesc(const TextureViewDesc& viewDesc, TextureViewDesc::Aspect defaultAspect)
	{
		TextureViewDesc result = viewDesc;
		if (result._format._aspect == TextureViewDesc::Aspect::UndefinedAspect)
			result._format._aspect = defaultAspect;
		return result;
	}

	IResourceView* AttachmentPool::GetSRV(AttachmentName attachName, const TextureViewDesc& window) const
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
		auto formatComponents = GetComponents(attach->_desc._textureDesc._format);
		if (formatComponents == FormatComponents::Depth || formatComponents == FormatComponents::DepthStencil) {
			defaultAspect = TextureViewDesc::Aspect::Depth;	// can only choose depth or stencil -- so DepthStencil defaults to Depth
		} else if (formatComponents == FormatComponents::Stencil) {
			defaultAspect = TextureViewDesc::Aspect::Stencil;
		}
		auto completeView = CompleteTextureViewDesc(window, defaultAspect);
		return _pimpl->_srvPool.GetTextureView(attach->_resource, BindFlag::ShaderResource, completeView).get();
	}

    static bool DimsEqual(const AttachmentDesc& lhs, const TextureDesc& rhs, const FrameBufferProperties& props)
    {
        bool lhsRelativeMode = !!(lhs._flags & AttachmentDesc::Flags::OutputRelativeDimensions);
        unsigned lhsWidth, lhsHeight;
        if (!lhsRelativeMode) {
            lhsWidth = unsigned(lhs._width);
            lhsHeight = unsigned(lhs._height);
        } else {
            lhsWidth = unsigned(std::floor(props._outputWidth * lhs._width));
            lhsHeight = unsigned(std::floor(props._outputHeight * lhs._height));
        }

        return lhsWidth == rhs._width && lhsHeight == rhs._height;
    }

    static unsigned GetArrayCount(const AttachmentDesc& lhs) { return (lhs._arrayLayerCount == 0) ? 1 : lhs._arrayLayerCount; }
    static unsigned GetArrayCount(const TextureDesc& lhs) { return (lhs._arrayCount == 0) ? 1 : lhs._arrayCount; }

    static bool MatchRequest(const AttachmentDesc& lhs, BindFlag::BitField lhsBindFlags, const ResourceDesc& rhs, const FrameBufferProperties& props)
    {
        assert(rhs._type == ResourceDesc::Type::Texture);
        auto requestSamples = lhs._flags & AttachmentDesc::Flags::Multisampled ? props._samples : TextureSamples::Create();
        return
            GetArrayCount(lhs) == GetArrayCount(rhs._textureDesc)
            && (AsTypelessFormat(lhs._format) == AsTypelessFormat(rhs._textureDesc._format) || lhs._format == Format::Unknown)
            && DimsEqual(lhs, rhs._textureDesc, props)
            && requestSamples == rhs._textureDesc._samples
            && (rhs._bindFlags & lhsBindFlags) == lhsBindFlags
            ;
    }

    static bool MatchRequest(const ResourceDesc& preregisteredDesc, const ResourceDesc& concreteObjectDesc)
    {
        assert(preregisteredDesc._type == ResourceDesc::Type::Texture && concreteObjectDesc._type == ResourceDesc::Type::Texture);
        return
            GetArrayCount(preregisteredDesc._textureDesc) == GetArrayCount(concreteObjectDesc._textureDesc)
            && (AsTypelessFormat(preregisteredDesc._textureDesc._format) == AsTypelessFormat(concreteObjectDesc._textureDesc._format) || preregisteredDesc._textureDesc._format == Format::Unknown)
            && preregisteredDesc._textureDesc._width == concreteObjectDesc._textureDesc._width
            && preregisteredDesc._textureDesc._height == concreteObjectDesc._textureDesc._height
            && preregisteredDesc._textureDesc._samples == concreteObjectDesc._textureDesc._samples
            && (concreteObjectDesc._bindFlags & preregisteredDesc._bindFlags) == preregisteredDesc._bindFlags
            ;

    }

    static ResourceDesc AsResourceDesc(const AttachmentDesc& attachmentDesc, BindFlag::BitField bindFlags, const FrameBufferProperties& props)
    {
        // Prefer "typeless" formats when creating the actual attachments
        // This ensures that we can have complete freedom when we create views
        TextureDesc tDesc;
        if (attachmentDesc._flags & AttachmentDesc::Flags::OutputRelativeDimensions) {
            tDesc._width = unsigned(std::floor(props._outputWidth * attachmentDesc._width));
            tDesc._height = unsigned(std::floor(props._outputHeight * attachmentDesc._height));
        } else {
            tDesc._width = (unsigned)attachmentDesc._width;
            tDesc._height = (unsigned)attachmentDesc._height;
        }
        tDesc._depth = 1;
        tDesc._format = attachmentDesc._format;
        #if (GFXAPI_TARGET != GFXAPI_OPENGLES) && (GFXAPI_TARGET != GFXAPI_APPLEMETAL)        // OpenGLES can't handle the typeless formats current (and they are useless since there aren't "views" on OpenGL) -- so just skip this
            tDesc._format = AsTypelessFormat(tDesc._format);
        #endif
        tDesc._dimensionality = TextureDesc::Dimensionality::T2D;
        tDesc._mipCount = 1;
        tDesc._arrayCount = 0;
        tDesc._samples = attachmentDesc._flags & AttachmentDesc::Flags::Multisampled ? props._samples : TextureSamples::Create();
        return CreateDesc(bindFlags, 0, 0, tDesc, "attachment-pool");
    }

    std::vector<AttachmentName> AttachmentPool::Request(const FrameBufferDesc& fbDesc, IteratorRange<const PreregisteredAttachment*> preregisteredAttachments)
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
        if (fbDesc.GetProperties()._samples._sampleCount <= 1)
            relevantFlags &= ~AttachmentDesc::Flags::Multisampled;

        std::vector<BindFlag::BitField> attachmentBindFlags(fbDesc.GetAttachments().size(), 0);
        for (const auto& spDesc:fbDesc.GetSubpasses()) {
            for (const auto& r:spDesc.GetOutputs())
                attachmentBindFlags[r._resourceName] |= BindFlag::RenderTarget;
			if (spDesc.GetDepthStencil()._resourceName != SubpassDesc::Unused._resourceName)
				attachmentBindFlags[spDesc.GetDepthStencil()._resourceName] |= BindFlag::DepthStencil;
			for (const auto& r:spDesc.GetInputs())
                // \todo -- shader resource or input attachment bind flag here?
				attachmentBindFlags[r._resourceName] |= BindFlag::ShaderResource;
        }
        for (unsigned c=0; c<fbDesc.GetAttachments().size(); ++c)
            attachmentBindFlags[c] |= fbDesc.GetAttachments()[c]._desc._bindFlagsForFinalLayout;

        std::vector<AttachmentName> result;
        auto bindFlagI = attachmentBindFlags.begin();
        for (auto request=fbDesc.GetAttachments().begin(); request!=fbDesc.GetAttachments().end(); ++request, ++bindFlagI) {
            auto requestDesc = request->_desc;
            requestDesc._flags &= relevantFlags;

            // If a semantic value is set, we should first check to see if the request can match
            // one of the bound attachments.
            bool foundMatch = false;
            if (request->_semantic) {
                for (unsigned q=0; q<_pimpl->_semanticAttachments.size(); ++q) {
                    if (request->_semantic == _pimpl->_semanticAttachments[q]._semantic && !consumedSemantic[q] && _pimpl->_semanticAttachments[q]._resource) {
                        #if defined(_DEBUG)
							if (!MatchRequest(requestDesc, *bindFlagI, _pimpl->_semanticAttachments[q]._desc, fbDesc.GetProperties())) {
                            	Log(Warning) << "Attachment bound to the pool for semantic (" << AttachmentSemantic{request->_semantic} << ", bind flag: 0x" << std::hex << *bindFlagI << std::dec << ") does not match the request for this semantic. Attempting to use it anyway. Request: "
                                	<< requestDesc << ", Bound to pool: " << _pimpl->_semanticAttachments[q]._desc
                                	<< std::endl;
                        	}
                            auto prereg = std::find_if(preregisteredAttachments.begin(), preregisteredAttachments.end(),
                                [sem = request->_semantic](const auto& c) { return c._semantic == sem; });
                            if (prereg != preregisteredAttachments.end() && !MatchRequest(prereg->_desc, _pimpl->_semanticAttachments[q]._desc))
                                Log(Warning) << "Attachment bound to the pool for semantic (" << AttachmentSemantic{request->_semantic} << " is not compatible with the preregistered attachments description. Attempting to use it anyway" << std::endl;
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
                if (MatchRequest(requestDesc, *bindFlagI, _pimpl->_attachments[q]._desc, fbDesc.GetProperties()) && q < consumed.size() && !consumed[q]) {
                    consumed[q] = true;
                    result.push_back(q);
                    foundMatch = true;

                    #if defined(_DEBUG)
                        if (request->_semantic) {
                            auto prereg = std::find_if(preregisteredAttachments.begin(), preregisteredAttachments.end(),
                                [sem = request->_semantic](const auto& c) { return c._semantic == sem; });
                            if (prereg != preregisteredAttachments.end() && !MatchRequest(prereg->_desc, _pimpl->_attachments[q]._desc))
                                Log(Warning) << "Attachment bound to the pool for semantic (" << AttachmentSemantic{request->_semantic} << " is not compatible with the preregistered attachments description. Attempting to use it anyway" << std::endl;
                        }
                    #endif
                    break;
                }
            }

            if (!foundMatch) {

                if (request->_semantic) {
                    auto prereg = std::find_if(preregisteredAttachments.begin(), preregisteredAttachments.end(),
                        [sem = request->_semantic](const auto& c) { return c._semantic == sem; });
                    if (prereg != preregisteredAttachments.end()) {
                        _pimpl->_attachments.push_back(Pimpl::Attachment{nullptr, prereg->_desc});
                        result.push_back((unsigned)(_pimpl->_attachments.size()-1));
                        foundMatch = true;
                    }
                }

                if (!foundMatch) {
                    _pimpl->_attachments.push_back(Pimpl::Attachment{nullptr, AsResourceDesc(requestDesc, *bindFlagI, fbDesc.GetProperties())});
                    result.push_back((unsigned)(_pimpl->_attachments.size()-1));
                }
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

        existingBinding->_desc = resource->GetDesc();
		assert(existingBinding->_desc._textureDesc._format != Format::Unknown);
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

    AttachmentPool::AttachmentPool(const std::shared_ptr<IDevice>& device)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_device = device;
    }

    AttachmentPool::~AttachmentPool()
    {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    FrameBufferDesc BuildFrameBufferDesc(
        FrameBufferDescFragment&& fragment,
        const FrameBufferProperties& props)
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
            /*if (inputFrag._desc._format == Format::Unknown)
                Throw(std::runtime_error("Cannot construct FrameBufferDesc because input fragment because an attachment doesn't have a fully specified format. Before we transform into the FrameBufferDesc version, we must have resolved the format to a concrete value."));*/
            fbAttachments.push_back({semantic, inputFrag._desc});
        }

        // Generate the final FrameBufferDesc by moving the subpasses out of the fragment
        // Usually this function is called as a final step when converting a number of fragments
        // into a final FrameBufferDesc, so it makes sense to move the subpasses from the input
        return FrameBufferDesc {
            std::move(fbAttachments),
            std::move(fragment._subpasses),
            props };
    }

    static bool FormatCompatible(Format lhs, Format rhs)
    {
        if (lhs == rhs) return true;
        auto    lhsTypeless = AsTypelessFormat(lhs),
                rhsTypeless = AsTypelessFormat(rhs);
        return lhsTypeless == rhsTypeless;
    }

    static bool DimsEqual(const AttachmentDesc& lhs, const AttachmentDesc& rhs, UInt2 dimensionsForSizeComparison)
    {
        bool lhsRelativeMode = !!(lhs._flags & AttachmentDesc::Flags::OutputRelativeDimensions);
        bool rhsRelativeMode = !!(rhs._flags & AttachmentDesc::Flags::OutputRelativeDimensions);
        if (lhsRelativeMode == rhsRelativeMode)
            return lhs._width == rhs._width && lhs._height == rhs._height;

        unsigned lhsWidth, lhsHeight;
        unsigned rhsWidth, rhsHeight;

        if (!lhsRelativeMode) {
            lhsWidth = unsigned(lhs._width);
            lhsHeight = unsigned(lhs._height);
        } else {
            lhsWidth = unsigned(std::floor(dimensionsForSizeComparison[0] * lhs._width));
            lhsHeight = unsigned(std::floor(dimensionsForSizeComparison[1] * lhs._height));
        }

        if (!rhsRelativeMode) {
            rhsWidth = unsigned(rhs._width);
            rhsHeight = unsigned(rhs._height);
        } else {
            rhsWidth = unsigned(std::floor(dimensionsForSizeComparison[0] * rhs._width));
            rhsHeight = unsigned(std::floor(dimensionsForSizeComparison[1] * rhs._height));
        }

        return lhsWidth == rhsWidth && lhsHeight == rhsHeight;
    }

    bool IsCompatible(const AttachmentDesc& testAttachment, const AttachmentDesc& request, UInt2 dimensionsForSizeComparison)
    {
        return
            ( (FormatCompatible(testAttachment._format, request._format)) || (testAttachment._format == Format::Unknown) || (request._format == Format::Unknown) )
            && GetArrayCount(testAttachment) == GetArrayCount(request)
			&& DimsEqual(testAttachment, request, dimensionsForSizeComparison)
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
        for (const auto&a:p.GetOutputs())
            if (a._resourceName == attachment) {
                result |= DirectionFlags::Reference;
                if (HasRetain(a._loadFromPreviousPhase))
                    result |= DirectionFlags::Load;
                if (HasRetain(a._storeToNextPhase))
                    result |= DirectionFlags::Store;
            }
        if (p.GetDepthStencil()._resourceName == attachment) {
            result |= DirectionFlags::Reference;
            if (HasRetain(p.GetDepthStencil()._loadFromPreviousPhase))
                result |= DirectionFlags::Load;
            if (HasRetain(p.GetDepthStencil()._storeToNextPhase))
                result |= DirectionFlags::Store;
        }
        for (const auto&a:p.GetInputs())
            if (a._resourceName == attachment) {
                result |= DirectionFlags::Reference | DirectionFlags::Load;
                if (HasRetain(a._storeToNextPhase))
                    result |= DirectionFlags::RetainAfterLoad;
            }
        for (const auto&a:p.GetResolveOutputs())
            if (a._resourceName == attachment) {
                result |= DirectionFlags::Reference | DirectionFlags::Store;
            }
        if (p.GetResolveDepthStencil()._resourceName == attachment) {
            result |= DirectionFlags::Reference | DirectionFlags::Store;
        }
        return result;
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

////////////////////////////////////////////////////////////////////////////////////////////////////

    static const char* AsString(PreregisteredAttachment::State state)
    {
        switch (state) {
        case PreregisteredAttachment::State::Uninitialized: return "Uninitialized";
        case PreregisteredAttachment::State::Initialized:   return "Initialized";
        default:                                            return "<<unknown>>";
        }
    }

    static std::ostream& operator<<(std::ostream& str, const PreregisteredAttachment& attachment)
    {
        str << "PreregisteredAttachment { "
            << AttachmentSemantic{attachment._semantic} << ", "
            << attachment._desc << ", "
            << AsString(attachment._state) << ", "
            << AsString(attachment._stencilState) << "}";
        return str;
    }

    static std::ostream& operator<<(std::ostream& str, const WorkingAttachment& attachment)
    {
        str << "WorkingAttachment {"
            << attachment._name << ", "
            << attachment._desc << ", "
            << std::hex << "Contains: " << AttachmentSemantic{attachment._containsDataForSemantic} << ", "
            << "ShouldReceive: " << AttachmentSemantic{attachment._shouldReceiveDataForSemantic} << ", "
            << "FirstRead: " << AttachmentSemantic{attachment._firstReadSemantic} << ", "
            << "LastWrite: " << AttachmentSemantic{attachment._lastWriteSemantic} << ", " << std::dec
            << attachment._forceShaderResourceFlag << ", "
            << AsString(attachment._state) << ", "
            << AsString(attachment._stencilState) << "}";
        return str;
    }

    static std::ostream& operator<<(std::ostream& str, const FrameBufferDescFragment& fragment)
    {
        str << "FrameBufferDescFragment with attachments: " << std::endl;
        for (unsigned c=0; c<fragment._attachments.size(); ++c) {
            str << StreamIndent(4) << "[" << c << "] "
                << AttachmentSemantic{fragment._attachments[c].GetInputSemanticBinding()} << ", " << AttachmentSemantic{fragment._attachments[c].GetOutputSemanticBinding()} << " : " << fragment._attachments[c]._desc
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

	static SubpassDesc RemapSubpassDesc(
		const SubpassDesc& input,
		const std::function<AttachmentName(AttachmentName)>& remapFunction)
	{
		SubpassDesc result;
		#if defined(_DEBUG)
			result.SetName(result._name);
		#endif
		for (auto remapped:input.GetOutputs()) {
			remapped._resourceName = remapFunction(remapped._resourceName);
			result.AppendOutput(remapped);
		}
		if (input.GetDepthStencil()._resourceName != ~0u) {
			auto remapped = input.GetDepthStencil();
			remapped._resourceName = remapFunction(remapped._resourceName);
			result.SetDepthStencil(remapped);
		}
		for (auto remapped:input.GetInputs()) {
			remapped._resourceName = remapFunction(remapped._resourceName);
			result.AppendInput(remapped);
		}
		for (auto remapped:input.GetResolveOutputs()) {
			remapped._resourceName = remapFunction(remapped._resourceName);
			result.AppendResolveOutput(remapped);
		}
		if (input.GetResolveDepthStencil()._resourceName != ~0u) {
			auto remapped = input.GetResolveDepthStencil();
			remapped._resourceName = remapFunction(remapped._resourceName);
			result.SetResolveDepthStencil(remapped);
		}
		return result;
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

        if (!fragments.size()) return { std::move(result) };
        
        std::vector<WorkingAttachment> workingAttachments;
        workingAttachments.reserve(preregisteredInputs.size());
        for (unsigned c=0; c<preregisteredInputs.size(); c++) {
            WorkingAttachment initialState;
            initialState._shouldReceiveDataForSemantic = preregisteredInputs[c]._semantic;
            if (    preregisteredInputs[c]._state == PreregisteredAttachment::State::Initialized
                ||  preregisteredInputs[c]._stencilState == PreregisteredAttachment::State::Initialized)
                initialState._containsDataForSemantic = preregisteredInputs[c]._semantic;
            initialState._desc = AsAttachmentDesc(preregisteredInputs[c]._desc);
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
                defaultSemanticFormats[a._semantic] = AsAttachmentDesc(a._desc);
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
                assert(firstUseDirection != 0);     // Note -- we can get here if we have an attachment that is defined, but never used

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
                    // interfaceAttachmentNoFlag._desc._flags &= ~AttachmentDesc::Flags::ShaderResource;
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
                                debugInfo << "      * Failed to find compatible initialized buffer for request: " << interfaceAttachment._desc << ". Semantic: " << AttachmentSemantic{interfaceAttachment.GetInputSemanticBinding()} << std::endl;
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
                        /*if ((interfaceAttachment._desc._flags & AttachmentDesc::Flags::ShaderResource) || ImplicitlyRequiresShaderResourceFlag(*f, interfaceAttachmentName))
                            newState._desc._flags |= AttachmentDesc::Flags::ShaderResource;*/

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
                                    debugInfo << "      * Could not resolve correct format for attachment: " << interfaceAttachment._desc << ". Semantic: " << AttachmentSemantic{interfaceAttachment.GetInputSemanticBinding()} << std::endl;
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

            for (unsigned p=0; p<(unsigned)f->_subpasses.size(); ++p) {
                SubpassDesc newSubpass = RemapSubpassDesc(
					f->_subpasses[p],
					std::bind(&Remap, std::ref(attachmentRemapping), std::placeholders::_1));

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
                debugInfo << StreamIndent(4) << "[" << c << "] "
                    << AttachmentSemantic{result._attachments[c].GetInputSemanticBinding()} << ", " << AttachmentSemantic{result._attachments[c].GetOutputSemanticBinding()}
                    << " : " << result._attachments[c]._desc << std::endl;
            debugInfo << "Final subpasses" << std::endl;
            for (unsigned c=0; c<result._subpasses.size(); ++c)
                debugInfo << StreamIndent(4) << "[" << c << "] " << result._subpasses[c] << std::endl;
            debugInfo << "Interface summary" << std::endl;
            for (unsigned c=0; c<finalResult._inputAttachments.size(); ++c)
                debugInfo << StreamIndent(4) << "Input [" << c << "] " << AttachmentSemantic{finalResult._inputAttachments[c].first} << " " << finalResult._inputAttachments[c].second << " (" << finalResult._mergedFragment._attachments[finalResult._inputAttachments[c].second]._desc << ")" << std::endl;
            for (unsigned c=0; c<finalResult._outputAttachments.size(); ++c)
                debugInfo << StreamIndent(4) << "Output [" << c << "] " << AttachmentSemantic{finalResult._outputAttachments[c].first} << " " << finalResult._outputAttachments[c].second << " (" << finalResult._mergedFragment._attachments[finalResult._outputAttachments[c].second]._desc << ")" << std::endl;
            debugInfo << "MergeFragments() finished." << std::endl;
            finalResult._log = debugInfo.str();
        #endif
        
        return finalResult;
    }

    static AttachmentName RemapAttachmentName(
        AttachmentName input,
        const RenderCore::Techniques::FrameBufferDescFragment& srcFragment,
        RenderCore::Techniques::FrameBufferDescFragment& dstFragment,
        std::vector<std::pair<RenderCore::AttachmentName, RenderCore::AttachmentName>>& remapping)
    {
        if (input == ~0u) return input;

        auto existing = LowerBound(remapping, input);
        if (existing == remapping.end() || existing->first != input) {
            auto semantic = srcFragment._attachments[input].GetInputSemanticBinding();
            auto newName = dstFragment.DefineAttachment(
                semantic,
                srcFragment._attachments[input]._desc);
            existing = remapping.insert(existing, {input, newName});
        }

        return existing->second;
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
                auto remappedSubpass = RemapSubpassDesc(
					subpass,
					std::bind(&RemapAttachmentName, std::placeholders::_1, std::ref(inputFragment), std::ref(separatedFragment), std::ref(remapping)));
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

    static BindFlag::BitField CalculateBindFlags(const FrameBufferDescFragment& fragment, unsigned attachmentName)
    {
        BindFlag::BitField result = fragment._attachments[attachmentName]._desc._bindFlagsForFinalLayout;
        for (const auto& spDesc:fragment._subpasses) {
            for (const auto& r:spDesc.GetOutputs())
                if (r._resourceName == attachmentName)
                    result |= BindFlag::RenderTarget;
			if (spDesc.GetDepthStencil()._resourceName == attachmentName)
				result |= BindFlag::DepthStencil;
			for (const auto& r:spDesc.GetInputs())
                result |= BindFlag::ShaderResource; // \todo -- shader resource or input attachment bind flag here?
        }
        return result;
    }

    void MergeInOutputs(
        std::vector<PreregisteredAttachment>& workingSystemAttachments,
        const FrameBufferDescFragment& fragment,
        const FrameBufferProperties& fbProps)
    {
        for (unsigned fragmentIdx=0; fragmentIdx<fragment._attachments.size(); ++fragmentIdx) {
            const auto& fragAttachment = fragment._attachments[fragmentIdx];
            if (fragAttachment.GetOutputSemanticBinding() == 0) continue;
            bool foundExisting = false;
            for (auto&dstAttachment:workingSystemAttachments)
                if (dstAttachment._semantic == fragAttachment.GetOutputSemanticBinding()) {
                    dstAttachment._state = PreregisteredAttachment::State::Initialized;
                    dstAttachment._stencilState = PreregisteredAttachment::State::Initialized;
                    dstAttachment._desc._bindFlags |= CalculateBindFlags(fragment, fragmentIdx);
                    foundExisting = true;
                    break;
                }
            if (!foundExisting) {
                workingSystemAttachments.push_back({
                    fragAttachment.GetOutputSemanticBinding(),
                    AsResourceDesc(fragAttachment._desc, CalculateBindFlags(fragment, fragmentIdx), fbProps),
                    PreregisteredAttachment::State::Initialized,
                    PreregisteredAttachment::State::Initialized});
            }
        }
    }

}}

