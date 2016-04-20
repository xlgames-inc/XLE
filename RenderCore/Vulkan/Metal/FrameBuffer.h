// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "IncludeVulkan.h"
#include "../../ResourceDesc.h"
#include "../../../Utility/IteratorUtils.h"

namespace RenderCore { enum class Format; }
namespace RenderCore { namespace Metal_Vulkan
{
    class ObjectFactory;
    class RenderTargetView;
    class TextureView;
    class DeviceContext;

    /// <summary>Attachments are part of a frame buffer, and typically represent a rendering surface</summary>
    /// This description object can define an attachment. Typically the attachment is defined in terms of
    /// some global frame buffer properties (such as output dimensions and sample count).
    class AttachmentDesc
    {
    public:
        enum class DimensionsMode 
        {
            Absolute,                   ///< _width and _height define absolute pixel values
            OutputRelative              ///< _width and _height are multipliers applied to the defined "output" dimensions (ie, specify 1.f to create buffers the same size as the output)
        };
        DimensionsMode _dimsMode;
        float _width, _height;

        Format _format;

        enum class LoadStore { DontCare, Retain, Clear };
        LoadStore _loadFromPreviousPhase;       ///< equivalent to "load op" in a Vulkan attachment
        LoadStore _storeToNextPhase;            ///< equivalent to "store op" in a Vulkan attachment

        LoadStore _stencilLoad, _stencilStore;

        struct Flags
        {
            enum Enum
            {
                UsePresentationChainBuffer = 1<<0,  ///< use the output buffer from the presentation chain
                Multisampled = 1<<1,                ///< use the current multisample settings (otherwise just set to single sampled mode)
                ShaderResource = 1<<2,              ///< allow binding as a shader resource after the render pass has finished
                TransferSource = 1<<3               ///< allow binding as a transfer source after the render pass has finished
            };
            using BitField = unsigned;
        };
        Flags::BitField _flags;
    };

    /// <summary>Defines which attachments are used during a subpass (and ordering)</summary>
    /// Input attachments are read by shader stages. Output attachments are for color data written
    /// from pixel shaders. There can be 0 or 1 depth stencil attachments.
    /// Finally, "preserved" attachments are not used during this subpass, but their contents are
    /// preserved to be used in future subpasses.
    class SubpassDesc
    {
    public:
        static unsigned const Unused = ~0u;
        IteratorRange<unsigned*> _input;
        IteratorRange<unsigned*> _output;
        unsigned _depthStencil;
        IteratorRange<unsigned*> _preserve;

        SubpassDesc();
        SubpassDesc(
            std::initializer_list<unsigned> input, 
            std::initializer_list<unsigned> output,
            unsigned depthStencil = Unused,
            std::initializer_list<unsigned> preserve = {});
    };

    class FrameBufferProperties
    {
    public:
        unsigned _outputWidth, _outputHeight, _outputLayers;
        Format _outputFormat;
        TextureSamples _samples;
    };

    class FrameBufferLayout
	{
	public:
		VkRenderPass GetUnderlying() const { return _underlying.get(); }
        const VulkanSharedPtr<VkRenderPass>& ShareUnderlying() { return _underlying; }

        IteratorRange<const AttachmentDesc*> GetAttachments() const { return MakeIteratorRange(_attachments); }
        IteratorRange<const SubpassDesc*> GetSubpasses() const { return MakeIteratorRange(_subpasses); }

		FrameBufferLayout(
            const ObjectFactory& factory,
            const FrameBufferProperties& properties,        // (uses just the _outputFormat and _samples members)
            IteratorRange<AttachmentDesc*> attachments,
            IteratorRange<SubpassDesc*> subpasses);
		FrameBufferLayout();
		~FrameBufferLayout();

	private:
		VulkanSharedPtr<VkRenderPass>   _underlying;
        std::vector<AttachmentDesc>     _attachments;
        std::vector<SubpassDesc>        _subpasses;
        FrameBufferProperties           _properties;
	};
    
    class FrameBuffer
	{
	public:
		VkFramebuffer GetUnderlying() const { return _underlying.get(); }

		FrameBuffer(
			const ObjectFactory& factory,
			const FrameBufferLayout& layout,
			const FrameBufferProperties& props,
            const RenderTargetView& presentationChainTarget);
		FrameBuffer();
		~FrameBuffer();
	private:
		VulkanSharedPtr<VkFramebuffer> _underlying;
        std::vector<TextureView> _views;
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
        void            End();
        TextureView*    GetAttachment(unsigned index);

        class BeginInfo
        {
        public:
            VectorPattern<int, 2>           _offset;
            VectorPattern<unsigned, 2>      _extent;
            IteratorRange<VkClearValue*>    _clearValues;

            BeginInfo() : _offset(0,0), _extent(0,0), _clearValues(nullptr, nullptr) {}
        };

        RenderPassInstance(
            DeviceContext& context,
            const FrameBufferLayout& layout,
			const FrameBufferProperties& props,
            uint64 hashName,
            const BeginInfo& beginInfo = BeginInfo());
        ~RenderPassInstance();
    };
}}