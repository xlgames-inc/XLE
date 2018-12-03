// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../ResourceDesc.h"        // needed for TextureViewDesc constructor
#include "../FrameBufferDesc.h"
#include "../IThreadContext_Forward.h"
#include "../Metal/Forward.h"
#include "../../Utility/IteratorUtils.h"
#include "../../Core/Types.h"
#include <memory>

namespace RenderCore 
{
    class FrameBufferDesc;
    class AttachmentDesc;
    class TextureViewDesc;
    class TextureSamples;
    class FrameBufferProperties;
    using AttachmentName = uint32;
    class IResource;
    using IResourcePtr = std::shared_ptr<IResource>;
}

namespace RenderCore { namespace Techniques
{
////////////////////////////////////////////////////////////////////////////////////////////////////

    class AttachmentPool
    {
    public:
        void DefineAttachment(AttachmentName, const AttachmentDesc& request);
        auto GetDesc(AttachmentName resName) const -> const AttachmentDesc*;

        void Bind(AttachmentName, const IResourcePtr& resource);
        void Unbind(AttachmentName);
        IResourcePtr GetResource(AttachmentName resName) const;
        Metal::ShaderResourceView* GetSRV(AttachmentName resName, const TextureViewDesc& window = {}) const;

        void Bind(FrameBufferProperties props);
        const FrameBufferProperties& GetFrameBufferProperties() const;
        IteratorRange<const AttachmentDesc*> GetDescriptions() const;

        AttachmentPool();
        ~AttachmentPool();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////

    class FrameBufferDescFragment
    {
    public:
        AttachmentName DefineAttachment(
            uint64_t semantic,
            const AttachmentDesc& request = {});
        AttachmentName DefineTemporaryAttachment(const AttachmentDesc& request) { return DefineAttachment(0, request); }
        void AddSubpass(SubpassDesc&& subpass);

        FrameBufferDescFragment();
        ~FrameBufferDescFragment();

        struct Attachment
        {
            uint64_t        _semantic;
            AttachmentDesc  _desc;
        };
        std::vector<std::pair<AttachmentName, Attachment>> _attachments;
        std::vector<SubpassDesc>    _subpasses;

    private:
        unsigned _nextAttachment;
    };

    FrameBufferDesc BuildFrameBufferDesc(
        AttachmentPool& namedResources,
        FrameBufferDescFragment&& fragment);

////////////////////////////////////////////////////////////////////////////////////////////////////

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
        std::shared_ptr<Metal::FrameBuffer> BuildFrameBuffer(
            Metal::ObjectFactory& factory,
            const FrameBufferDesc& desc,
            AttachmentPool& attachmentPool);

		std::shared_ptr<Metal::FrameBuffer> BuildFrameBuffer(
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

        RenderPassInstance(
            IThreadContext& context,
            const std::shared_ptr<Metal::FrameBuffer>& frameBuffer,
            const FrameBufferDesc& layout,
            AttachmentPool& namedResources,
            const RenderPassBeginDesc& beginInfo = RenderPassBeginDesc());

        ~RenderPassInstance();

        RenderPassInstance();
        RenderPassInstance(RenderPassInstance&& moveFrom);
        RenderPassInstance& operator=(RenderPassInstance&& moveFrom);

    private:
        std::shared_ptr<Metal::FrameBuffer> _frameBuffer;
        Metal::DeviceContext* _attachedContext;
    };

    std::shared_ptr<INamedAttachments> MakeNamedAttachmentsWrapper(AttachmentPool& namedRes);

////////////////////////////////////////////////////////////////////////////////////////////////////

    struct PreregisteredAttachment
    {
    public:
        AttachmentName _name;
        uint64_t _semantic;
        AttachmentDesc _desc;
        enum class State { Uninitialized, Initialized };
        State _state = State::Uninitialized;
    };

    class FrameBufferFragmentMapping
    {
    public:
        using PassAndSlot = std::pair<unsigned, unsigned>;
        std::vector<std::pair<PassAndSlot, AttachmentName>> _inputAttachmentMapping;
        unsigned _subpassCount;
    };

    std::pair<FrameBufferDescFragment, std::vector<FrameBufferFragmentMapping>>
        MergeFragments(
            IteratorRange<const PreregisteredAttachment*> preregisteredAttachments,
            IteratorRange<const FrameBufferDescFragment*> fragments,
            char *logBuffer = nullptr,
            size_t bufferLength = 0u);

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
        auto GetSRV(unsigned slot) const -> Metal::ShaderResourceView*;
        AttachmentPool& GetAttachmentPool() const { return *_attachmentPool; }

        void NextSubpass();

        RenderPassFragment(
            RenderPassInstance& rpi,
            const FrameBufferFragmentMapping& mapping,
            AttachmentPool& attachmentPool);
		RenderPassFragment();
        ~RenderPassFragment();

        RenderPassFragment(const RenderPassFragment&) = delete;
        RenderPassFragment& operator=(const RenderPassFragment&) = delete;
    private:
        RenderPassInstance* _rpi;
        const FrameBufferFragmentMapping* _mapping;
        AttachmentPool* _attachmentPool;
        unsigned _currentPassIndex;
    };

}}


