// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../ResourceDesc.h"        // needed for TextureViewWindow constructor
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
    class TextureViewWindow;
    class TextureSamples;
    class FrameBufferProperties;
    using AttachmentName = uint32;
    class IResource;
    using IResourcePtr = std::shared_ptr<IResource>;
}

namespace RenderCore { namespace Techniques
{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class PassFragmentInterface
    {
    public:
        AttachmentName DefineAttachment(const AttachmentDesc& request);

        void BindColorAttachment(
            unsigned passIndex,
            unsigned slot, 
            AttachmentName attachmentName,
            AttachmentViewDesc::LoadStore loadOp = AttachmentViewDesc::LoadStore::Retain, 
            const ClearValue& clearValue = ClearValue{});

        void BindDepthStencilAttachment(
            unsigned passIndex,
            AttachmentName attachmentName,
            AttachmentViewDesc::LoadStore loadOp = AttachmentViewDesc::LoadStore::Retain, 
            const ClearValue& clearValue = ClearValue{});

        void BindInputAttachment(
            unsigned passIndex,
            unsigned slot, 
            AttachmentName attachmentName,
            AttachmentViewDesc::LoadStore storeOp = AttachmentViewDesc::LoadStore::Retain, 
            TextureViewWindow::Aspect aspect = TextureViewWindow::Aspect::UndefinedAspect);

        PassFragmentInterface();
        ~PassFragmentInterface();

        std::vector<std::pair<AttachmentName, AttachmentDesc>> _attachmentRequests;
    };

    class PassFragment
    {
    public:
        auto GetSRV(const Metal::INamedAttachments& namedAttachments, unsigned passIndex, unsigned slot) const -> Metal::ShaderResourceView*;
        Metal::ViewportDesc GetFullViewport() const;

        using PassAndSlot = std::pair<unsigned, unsigned>;
        std::vector<std::pair<PassAndSlot, AttachmentName>> _inputAttachmentMapping;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    class NamedAttachments
    {
    public:
        void DefineAttachment(AttachmentName, const AttachmentDesc& request);

        auto GetSRV(AttachmentName viewName, AttachmentName resName = ~0u, const TextureViewWindow& window = TextureViewWindow()) const -> Metal::ShaderResourceView*;
        auto GetRTV(AttachmentName viewName, AttachmentName resName = ~0u, const TextureViewWindow& window = TextureViewWindow()) const -> Metal::RenderTargetView*;
        auto GetDSV(AttachmentName viewName, AttachmentName resName = ~0u, const TextureViewWindow& window = TextureViewWindow()) const -> Metal::DepthStencilView*;
        auto GetDesc(AttachmentName resName) const -> const AttachmentDesc*;

        void Bind(FrameBufferProperties props);
        void Bind(AttachmentName, const IResourcePtr& resource);
        void Unbind(AttachmentName);

        const FrameBufferProperties& GetFrameBufferProperties() const;
        IteratorRange<const AttachmentDesc*> GetDescriptions() const;

        NamedAttachments();
        ~NamedAttachments();
    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    FrameBufferDesc BuildFrameBufferDesc(
        /* in/out */ NamedAttachments& namedResources,
        /* out */ std::vector<PassFragment>& boundFragments,
        /* int */ IteratorRange<const PassFragmentInterface*> fragments);

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
            std::initializer_list<ClearValue> clearValues = {})
        : _clearValues(clearValues.begin(), clearValues.end())
        {}
    };

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

        Metal::FrameBuffer& GetFrameBuffer() { return *_frameBuffer; }
        const Metal::INamedAttachments& GetNamedAttachments() const { return *_namedAttachments; }

        RenderPassInstance(
            Metal::DeviceContext& context,
            const FrameBufferDesc& layout,
            uint64 hashName,
            NamedAttachments& namedResources,
            const RenderPassBeginDesc& beginInfo = RenderPassBeginDesc());

        RenderPassInstance(
            IThreadContext& context,
            const FrameBufferDesc& layout,
            uint64 hashName,
            NamedAttachments& namedResources,
            const RenderPassBeginDesc& beginInfo = RenderPassBeginDesc());
        ~RenderPassInstance();

        RenderPassInstance();
        RenderPassInstance(RenderPassInstance&& moveFrom);
        RenderPassInstance& operator=(RenderPassInstance&& moveFrom);

    private:
        std::shared_ptr<Metal::FrameBuffer> _frameBuffer;
        std::shared_ptr<Metal::INamedAttachments> _namedAttachments;
        Metal::DeviceContext* _attachedContext;
        unsigned _activeSubpass;
    };

}}


