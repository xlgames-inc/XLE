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
#include "../Utility/IteratorUtils.h"
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
    class Resource;
    using ResourcePtr = std::shared_ptr<Resource>;
}

namespace RenderCore { namespace Techniques
{
    class NamedResources
    {
    public:
        void DefineAttachments(IteratorRange<const AttachmentDesc*> attachments);

        auto GetSRV(AttachmentName viewName, AttachmentName resName = ~0u, const TextureViewWindow& window = TextureViewWindow()) const -> const Metal::ShaderResourceView*;
        auto GetRTV(AttachmentName viewName, AttachmentName resName = ~0u, const TextureViewWindow& window = TextureViewWindow()) const -> const Metal::RenderTargetView*;
        auto GetDSV(AttachmentName viewName, AttachmentName resName = ~0u, const TextureViewWindow& window = TextureViewWindow()) const -> const Metal::DepthStencilView*;

        void Bind(TextureSamples samples);
        void Bind(FrameBufferProperties props);
        void Bind(AttachmentName, const ResourcePtr& resource);
        void Unbind(AttachmentName);

        const FrameBufferProperties& GetFrameBufferProperties() const;
        TextureSamples GetSamples() const;
        IteratorRange<const AttachmentDesc*> GetDescriptions() const;

        NamedResources();
        ~NamedResources();
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
        const Metal::RenderTargetView& GetAttachment(unsigned index);

        RenderPassInstance(
            Metal::DeviceContext& context,
            const FrameBufferDesc& layout,
            uint64 hashName,
            NamedResources& namedResources,
            Metal::FrameBufferCache& cache,
            const RenderPassBeginDesc& beginInfo = RenderPassBeginDesc());

        RenderPassInstance(
            IThreadContext& context,
            const FrameBufferDesc& layout,
            uint64 hashName,
            NamedResources& namedResources,
            Metal::FrameBufferCache& cache,
            const RenderPassBeginDesc& beginInfo = RenderPassBeginDesc());
        ~RenderPassInstance();

    private:
        std::shared_ptr<Metal::FrameBuffer> _frameBuffer;
        Metal::DeviceContext* _attachedContext;
        unsigned _activeSubpass;
    };

}}

