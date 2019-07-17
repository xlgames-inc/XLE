// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../ResourceDesc.h"        // needed for TextureViewDesc constructor
#include "../Types.h"
#include "../FrameBufferDesc.h"
#include "../IThreadContext_Forward.h"
#include "../Metal/Forward.h"
#include "../../Math/Vector.h"
#include "../../Utility/IteratorUtils.h"
#include <memory>

namespace RenderCore 
{
    class FrameBufferDesc;
    class AttachmentDesc;
    class TextureViewDesc;
    class TextureSamples;
    class FrameBufferProperties;
    using AttachmentName = uint32_t;
    class IResource;
    using IResourcePtr = std::shared_ptr<IResource>;
}

namespace RenderCore { namespace Techniques
{

////////////////////////////////////////////////////////////////////////////////////////////////////

    class FrameBufferDescFragment
    {
    public:
        AttachmentName DefineAttachment(uint64_t semantic, const AttachmentDesc& request = {});
        AttachmentName DefineTemporaryAttachment(const AttachmentDesc& request) { return DefineAttachment(0, request); }
        void AddSubpass(SubpassDesc&& subpass);

        FrameBufferDescFragment();
        ~FrameBufferDescFragment();

        struct Attachment
        {
            uint64_t        _inputSemanticBinding;
            uint64_t        _outputSemanticBinding;
            AttachmentDesc  _desc;

            uint64_t GetInputSemanticBinding() const { return _inputSemanticBinding; }
            uint64_t GetOutputSemanticBinding() const { return _outputSemanticBinding; }
        };
        std::vector<Attachment>     _attachments;
        std::vector<SubpassDesc>    _subpasses;
		PipelineType				_pipelineType = PipelineType::Graphics;
    };

    FrameBufferDesc BuildFrameBufferDesc(
        FrameBufferDescFragment&& fragment);

////////////////////////////////////////////////////////////////////////////////////////////////////

    class AttachmentPool
    {
    public:
        void Bind(uint64_t semantic, const IResourcePtr& resource);
        void Unbind(const IResource& resource);
        void UnbindAll();
		auto GetBoundResource(uint64_t semantic) -> IResourcePtr;
		auto GetBoundResourceDesc(uint64_t semantic) -> const AttachmentDesc*;

        std::vector<AttachmentName> Request(IteratorRange<const FrameBufferDesc::Attachment*> requests);

        void Bind(FrameBufferProperties props);
        const FrameBufferProperties& GetFrameBufferProperties() const;

        auto GetDesc(AttachmentName resName) const -> const AttachmentDesc*;
        auto GetResource(AttachmentName resName) const -> IResourcePtr;
        auto GetSRV(AttachmentName resName, const TextureViewDesc& window = {}) const -> Metal::ShaderResourceView*;

        void ResetActualized();
        std::string GetMetrics() const;

        AttachmentPool();
        ~AttachmentPool();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    class RenderPassBeginDesc
    {
    public:
        IteratorRange<const ClearValue*>    _clearValues;
        //      (Vulkan supports offset and extent here. But there there is
        //      no clean equivalent in D3D. Let's avoid exposing it until
        //      it's really needed)
        // VectorPattern<int, 2>               _offset;
        // VectorPattern<unsigned, 2>          _extent;

        RenderPassBeginDesc(
            IteratorRange<const ClearValue*> clearValues = {})
        : _clearValues(clearValues)
        {}
    };

    /// <summary>Stores a set of retained frame buffers, which can be reused frame-to-frame</summary>
    /// Client code typically just wants to define the size and formats of frame buffers, without
    /// manually retaining and managing the objects themselves. It's a result of typical usage patterns
    /// of RenderPassInstance.
    ///
    /// This helper class allows client code to simply declare what it needs and the actual management
    /// of the device objects will be handled within the cache.
    class FrameBufferPool
    {
    public:
        class Result
        {
        public:
            std::shared_ptr<Metal::FrameBuffer> _frameBuffer;
            IteratorRange<const AttachmentName*> _poolAttachmentsRemapping;
        };
        Result BuildFrameBuffer(
            Metal::ObjectFactory& factory,
            const FrameBufferDesc& desc,
            AttachmentPool& attachmentPool);

        void Reset();

        FrameBufferPool();
        ~FrameBufferPool();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    /// <summary>Begins and ends a render pass on the given context</summary>
    /// Creates and begins a render pass using the given frame buffer layout. This will also automatically
    /// allocate the buffers required
    ///
    /// The "hashName" parameter to the constructor can be used to reuse buffers from previous instances
    /// of a similar render pass. For example, a render pass instance might be for rendering and resolving
    /// the lighting for a deferred lighting scheme. This will be rendered every frame, usually using the same
    /// parameters. Use the same hashName to ensure that the same cached frame buffer will be reused (when possible).
    ///
    /// If an output attachment is required after the render pass instance is finished, call GetAttachment().
    /// This can be used to retrieve the rendered results.
    class RenderPassInstance
    {
    public:
        void NextSubpass();
        void End();
        unsigned GetCurrentSubpassIndex() const;

        Metal::FrameBuffer& GetFrameBuffer() { return *_frameBuffer; }
        const Metal::FrameBuffer& GetFrameBuffer() const { return *_frameBuffer; }

        // The "AttachmentNames" here map onto the names used by the FrameBufferDesc used to initialize this RPI
        auto GetDesc(AttachmentName resName) const -> const AttachmentDesc*;
        auto GetResource(AttachmentName resName) const -> IResourcePtr;
        auto GetSRV(AttachmentName resName, const TextureViewDesc& window = {}) const -> Metal::ShaderResourceView*;
		AttachmentName RemapAttachmentName(AttachmentName inputName) const;

        RenderPassInstance(
            IThreadContext& context,
            const FrameBufferDesc& layout,
            FrameBufferPool& frameBufferPool,
            AttachmentPool& attachmentPool,
            const RenderPassBeginDesc& beginInfo = RenderPassBeginDesc());
		RenderPassInstance(
			const FrameBufferDesc& layout,
			AttachmentPool& attachmentPool);

        ~RenderPassInstance();

        RenderPassInstance();
        RenderPassInstance(RenderPassInstance&& moveFrom) never_throws;
        RenderPassInstance& operator=(RenderPassInstance&& moveFrom) never_throws;

    private:
        std::shared_ptr<Metal::FrameBuffer> _frameBuffer;
        Metal::DeviceContext* _attachedContext;
        AttachmentPool* _attachmentPool;
        std::vector<AttachmentName> _attachmentPoolRemapping;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    struct PreregisteredAttachment
    {
    public:
        uint64_t _semantic;
        AttachmentDesc _desc;
        enum class State { Uninitialized, Initialized };
        State _state = State::Uninitialized;
        State _stencilState = State::Uninitialized;
    };

    class FrameBufferFragmentMapping
    {
    public:
        using PassAndSlot = std::pair<unsigned, unsigned>;
        std::vector<std::pair<PassAndSlot, AttachmentName>> _outputAttachmentMapping;
		std::vector<std::pair<PassAndSlot, AttachmentName>> _inputAttachmentMapping;
        unsigned _subpassCount;
    };

    class MergeFragmentsResult
    {
    public:
        FrameBufferDescFragment _mergedFragment;
        std::vector<FrameBufferFragmentMapping> _remapping;
        std::vector<std::pair<uint64_t, AttachmentName>> _inputAttachments;
        std::vector<std::pair<uint64_t, AttachmentName>> _outputAttachments;
        std::string _log;
    };

    MergeFragmentsResult MergeFragments(
        IteratorRange<const PreregisteredAttachment*> preregisteredAttachments,
        IteratorRange<const FrameBufferDescFragment*> fragments,
		UInt2 dimenionsForCompatibilityTests = UInt2(1024, 1024));

    /// <summary>Like RenderPassInstance, but works with a single fragment</summary>
    /// RenderPasses are often generated from many "fragments" -- which are merged together into a
    /// single render pass using MergeFragments.
    ///
    /// Often we want to then iterate through the entire renderpass, one fragment at a time
    /// This class is a useful utility that allows us to work with a single fragment at a time, even
    /// after many fragments have been combined into a single uber-renderpass.
    class RenderPassFragment
    {
    public:
        const RenderPassInstance& GetRenderPassInstance() const { return *_rpi; }

        // The indicies here refer to the slot of the input attachment
        auto GetInputAttachmentDesc(unsigned inputAttachmentSlot) const -> const AttachmentDesc*;
        auto GetInputAttachmentResource(unsigned inputAttachmentSlot) const -> IResourcePtr;
        auto GetInputAttachmentSRV(unsigned inputAttachmentSlot, const TextureViewDesc& window = {}) const -> Metal::ShaderResourceView*;
		auto GetOutputAttachmentDesc(unsigned slot) const -> const AttachmentDesc*;

        void NextSubpass();

        RenderPassFragment(
            RenderPassInstance& rpi,
            const FrameBufferFragmentMapping& mapping);
        RenderPassFragment();
		~RenderPassFragment();

        RenderPassFragment(const RenderPassFragment&) = delete;
        RenderPassFragment& operator=(const RenderPassFragment&) = delete;
    private:
        RenderPassInstance* _rpi;
        const FrameBufferFragmentMapping* _mapping;
        unsigned _currentPassIndex;

        AttachmentName RemapToRPI(unsigned inputAttachmentSlot) const;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

	class SemanticNamedAttachments : public RenderCore::INamedAttachments
    {
    public:
        virtual IResourcePtr GetResource(AttachmentName resName) const;
        virtual const AttachmentDesc* GetDesc(AttachmentName resName) const;

        SemanticNamedAttachments(
			AttachmentPool& pool, 
			IteratorRange<const uint64_t*> semanticMapping);
        ~SemanticNamedAttachments();
    private:
        AttachmentPool* _pool;
        std::vector<uint64_t> _semanticMapping;
    };

    /// <summary>Tests to see if the attachment usage of the given fragment can be optimized</summary>
    /// Sometimes of the number of attachments used by a fragment can be reduced by reusing
    /// an existing attachment, instead of defining a new one. The common "ping-pong" rendering
    /// pattern is an example of this (in this pattern we reuse 2 attachments for many subpasses,
    /// instead of defining new attachments for each sub pass.
    ///
    /// If there are cases this like that, this function can detect them. Returns true iff there are
    /// optimizations detected.
    ///
    /// Internally, it calls MergeFragments, and to get some context on the solution it found,
    /// you can look at the "_log" member of the calculated MergeFragmentsResult.
    ///
    /// MergeInOutputs can be used to chain multiple calls to this function by merging in the
    /// outputs from each subsequent fragment into the systemAttachments array.
    bool CanBeSimplified(
        const FrameBufferDescFragment& inputFragment,
        IteratorRange<const PreregisteredAttachment*> systemAttachments);

    void MergeInOutputs(
        std::vector<PreregisteredAttachment>& workingSystemAttachments,
        const FrameBufferDescFragment& fragment);

    bool IsCompatible(const AttachmentDesc& testAttachment, const AttachmentDesc& request, UInt2 dimensions);

}}


