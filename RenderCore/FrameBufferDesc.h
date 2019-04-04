// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Types_Forward.h"
#include "ResourceDesc.h"
#include "IDevice_Forward.h"
#include "../Utility/IteratorUtils.h"
#include <memory>

namespace RenderCore
{
    using AttachmentName = uint32;

	enum class LoadStore
	{
		DontCare, Retain, Clear,
		DontCare_RetainStencil, Retain_RetainStencil, Clear_RetainStencil,
		DontCare_ClearStencil, Retain_ClearStencil, Clear_ClearStencil
	};

    const char* AsString(LoadStore);

    /// <summary>Attachments are part of a frame buffer, and typically represent a rendering surface</summary>
    /// This description object can define an attachment. Typically the attachment is defined in terms of
    /// some global frame buffer properties (such as output dimensions and sample count).
    class AttachmentViewDesc
    {
    public:
        AttachmentName _resourceName = ~0u;

        LoadStore _loadFromPreviousPhase = LoadStore::Retain_RetainStencil;       ///< equivalent to "load op" in a Vulkan attachment
        LoadStore _storeToNextPhase = LoadStore::Retain_RetainStencil;            ///< equivalent to "store op" in a Vulkan attachment

		TextureViewDesc _window = {};
    };

    class AttachmentDesc
    {
    public:
        Format _format = Format(0);
        float _width = 1.0f, _height = 1.0f;
        unsigned _arrayLayerCount = 0u;

        enum class DimensionsMode
        {
            Absolute,                   ///< _width and _height define absolute pixel values
            OutputRelative              ///< _width and _height are multipliers applied to the defined "output" dimensions (ie, specify 1.f to create buffers the same size as the output)
        };
        DimensionsMode _dimsMode = DimensionsMode::OutputRelative;

        struct Flags
        {
            enum Enum
            {
                Multisampled		= 1<<0,     ///< use the current multisample settings (otherwise just set to single sampled mode)
                ShaderResource		= 1<<1,     ///< allow binding as a shader resource after the render pass has finished
                TransferSource		= 1<<2,     ///< allow binding as a transfer source after the render pass has finished
                RenderTarget		= 1<<3,
                DepthStencil		= 1<<4,
				PresentationSource	= 1<<5
            };
            using BitField = unsigned;
        };
        Flags::BitField _flags = 0u;

        static AttachmentDesc FromFlags(Flags::BitField flags)
        {
            AttachmentDesc result;
            result._flags = flags;
            return result;
        }

        #if defined(_DEBUG)
            mutable std::string _name = std::string();
            inline AttachmentDesc&& SetName(const std::string& name) const { this->_name = name; return std::move(const_cast<AttachmentDesc&>(*this)); }
            inline AttachmentDesc&& SetName(const std::string& name) { this->_name = name; return std::move(*this); }
        #else
            inline AttachmentDesc&& SetName(const std::string& name) const { return std::move(const_cast<AttachmentDesc&>(*this)); }
            inline AttachmentDesc&& SetName(const std::string& name) { return std::move(*this); }
        #endif
    };

    AttachmentDesc AsAttachmentDesc(const ResourceDesc&);

    /// <summary>Defines which attachments are used during a subpass (and ordering)</summary>
    /// Input attachments are read by shader stages. Output attachments are for color data written
    /// from pixel shaders. There can be 0 or 1 depth stencil attachments.
    /// Finally, "preserved" attachments are not used during this subpass, but their contents are
    /// preserved to be used in future subpasses.
    class SubpassDesc
    {
    public:
        std::vector<AttachmentViewDesc> _output;
		AttachmentViewDesc _depthStencil = Unused;
		std::vector<AttachmentViewDesc> _input = {};
		std::vector<AttachmentViewDesc> _preserve = {};
		std::vector<AttachmentViewDesc> _resolve = {};
        AttachmentViewDesc _depthStencilResolve = Unused;

		static const AttachmentViewDesc Unused;

        #if defined(_DEBUG)
            mutable std::string _name = std::string();
            inline SubpassDesc&& SetName(const std::string& name) const { this->_name = name; return std::move(const_cast<SubpassDesc&>(*this)); }
            inline SubpassDesc&& SetName(const std::string& name) { this->_name = name; return std::move(*this); }
        #else
            inline SubpassDesc&& SetName(const std::string& name) const { return std::move(const_cast<SubpassDesc&>(*this)); }
            inline SubpassDesc&& SetName(const std::string& name) { return std::move(*this); }
        #endif

        void AppendOutput(
            AttachmentName attachment,
            LoadStore loadOp = LoadStore::Retain,
            LoadStore storeOp = LoadStore::Retain);

        void AppendInput(
            AttachmentName attachment,
            LoadStore loadOp = LoadStore::Retain,
            LoadStore storeOp = LoadStore::Retain);

        void SetDepthStencil(
            AttachmentName attachment,
            LoadStore loadOp = LoadStore::Retain_RetainStencil,
            LoadStore storeOp = LoadStore::Retain_RetainStencil);

        void AppendOutput(const AttachmentViewDesc& view);
        void AppendInput(const AttachmentViewDesc& view);
        void SetDepthStencil(const AttachmentViewDesc& view);
    };

    class FrameBufferDesc
	{
	public:
        auto	GetSubpasses() const -> IteratorRange<const SubpassDesc*> { return MakeIteratorRange(_subpasses); }

        struct Attachment
        {
            uint64_t        _semantic = 0;
			AttachmentDesc  _desc = {};
        };
        auto    GetAttachments() const -> IteratorRange<const Attachment*> { return MakeIteratorRange(_attachments); }

        uint64_t    GetHash() const { return _hash; }

		FrameBufferDesc(
            std::vector<Attachment>&& attachments,
            std::vector<SubpassDesc>&& subpasses);
		FrameBufferDesc();
		~FrameBufferDesc();

		static FrameBufferDesc s_empty;

	private:
        std::vector<Attachment>         _attachments;
        std::vector<SubpassDesc>        _subpasses;
        uint64_t                        _hash;
	};

    class FrameBufferProperties
    {
    public:
        unsigned _outputWidth, _outputHeight;
        TextureSamples _samples;
    };

    union ClearValue
    {
        float       _float[4];
        int         _int[4];
        unsigned    _uint[4];
        struct DepthStencilValue
        {
            float _depth;
            unsigned _stencil;
        };
        DepthStencilValue _depthStencil;
    };

	class INamedAttachments
	{
	public:
		virtual IResourcePtr GetResource(AttachmentName resName) const = 0;
		virtual const AttachmentDesc* GetDesc(AttachmentName resName) const = 0;
		virtual const FrameBufferProperties& GetFrameBufferProperties() const = 0;
		virtual ~INamedAttachments();
	};

	TextureViewDesc CompleteTextureViewDesc(const AttachmentDesc& attachmentDesc, const TextureViewDesc& viewDesc, TextureViewDesc::Aspect defaultAspect = TextureViewDesc::Aspect::UndefinedAspect);

////////////////////////////////////////////////////////////////////////////////////////////////////////////

    inline ClearValue MakeClearValue(const VectorPattern<float, 4>& v)
    {
        ClearValue result;
        for (unsigned c=0; c<4; ++c) result._float[c] = v[c];
        return result;
    }

    inline ClearValue MakeClearValue(const VectorPattern<int, 4>& v)
    {
        ClearValue result;
        for (unsigned c=0; c<4; ++c) result._int[c] = v[c];
        return result;
    }

    inline ClearValue MakeClearValue(const VectorPattern<unsigned, 4>& v)
    {
        ClearValue result;
        for (unsigned c=0; c<4; ++c) result._uint[c] = v[c];
        return result;
    }

    inline ClearValue MakeClearValue(float r, float g, float b, float a = 1.f)
    {
        ClearValue result;
        result._float[0] = r;
        result._float[1] = g;
        result._float[2] = b;
        result._float[3] = a;
        return result;
    }

    inline ClearValue MakeClearValue(int r, int g, int b, int a)
    {
        ClearValue result;
        result._int[0] = r;
        result._int[1] = g;
        result._int[2] = b;
        result._int[3] = a;
        return result;
    }

    inline ClearValue MakeClearValue(unsigned r, unsigned g, unsigned b, unsigned a)
    {
        ClearValue result;
        result._uint[0] = r;
        result._uint[1] = g;
        result._uint[2] = b;
        result._uint[3] = a;
        return result;
    }

    inline ClearValue MakeClearValue(float depth, unsigned stencil)
    {
        ClearValue result;
        result._depthStencil._depth = depth;
        result._depthStencil._stencil = stencil;
        return result;
    }


    /** <summary>Add a "output" attachment to the given subpass</summary>
        This appends a given output attachment to the next available slot in the subpass.
        "Output attachment" is another name for a render target. Ie, this is the texture we're going
        to render onto.

        We can select the load/store operations to use when we do this. This determines whether we care
        about any previous contents in the buffer before this subpass, and whether we want to use the
        contents in future subpasses.

        For loadOp, we have these choices, all of which are commonly used:
        - **DontCare** -- it means we will never read from the previous contents
            of this buffer. The buffer will typically be filled with indeterminant noise. The subpass
            should write to every pixel in the buffer and not use blending (otherwise we will pass
            that noise down the pipeline). This is the most efficient option.
        - **Clear** -- clear the buffer before we start rendering. Because we're clearing the buffer
            entirely, anything written to it by previous subpasses will be lost.
        - **Retain** -- this is the default, keep the contents from previous subpasses. This is the
            least efficient (since the hardware has to reinitialize it's framebuffer memory).
            It's typically required when we're not going to write to every pixel in the render buffer,
            or when we want to use a blend mode to blend our output with something underneath.

        For storeOp, we will almost always use Retain. We can use DontCare, but that just instructs the
        system not to keep any of the data we write out.
    */
    inline void SubpassDesc::AppendOutput(AttachmentName attachment, LoadStore loadOp, LoadStore storeOp)
    {
        AttachmentViewDesc attachmentViewDesc = {};
        attachmentViewDesc._resourceName = attachment;
        attachmentViewDesc._loadFromPreviousPhase = loadOp;
        attachmentViewDesc._storeToNextPhase = storeOp;
        _output.push_back(attachmentViewDesc);
    }

    /** <summary>Add a "input" attachment to the given subpass</summary>
        This appends an input attachment to the given subpass. An input attachment is another word
        for a shader resource (or texture). They are attachments that have been written to by a previous
        attachment, and that we're going to bind as a shader resource to read from in this subpass.

        Note that the system doesn't automatically bind the attachment as a shader resource -- we still
        have to do that manually. This is because we may need to specify some parameters when creating
        the ShaderResourceView (which determines how the attachment is presented to the shader).
        Typically this involves RenderCore::Techniques::RenderPassFragment::GetInputAttachmentSRV.

        For loadOp, we will almost always use Retain. Technically we could use DontCare, but then we
        would just be reading noise, which seems a bit redundant.

        For storeOp, we have two options:
        - **DontCare** -- this means that the contents of the attachment will be discarded after this
            subpass. Typically this should be used when we know that will never need to read from the
            attachment ever again. Note that there are many different ways to read from an attachment
            -- as an input attachment, a resolve attachment, an "output" attachment, in a present operation
            or even from a map/ReadPixels operation.
        - **Retain** -- this means that we should keep the contents of the buffer, because it may be
            used again after the subpass is complete.
    */

    inline void SubpassDesc::AppendInput(AttachmentName attachment, LoadStore loadOp, LoadStore storeOp)
    {
        AttachmentViewDesc attachmentViewDesc = {};
        attachmentViewDesc._resourceName = attachment;
        attachmentViewDesc._loadFromPreviousPhase = loadOp;
        attachmentViewDesc._storeToNextPhase = storeOp;
        _input.push_back(attachmentViewDesc);
    }

    /** <summary>Set the depth/stencil attachment for the given subpass</summary>
        This sets the depth/stencil attachment. There can be only one attachment of this type,
        so it will overwrite anything that was previously set.
    */
    inline void SubpassDesc::SetDepthStencil(AttachmentName attachment, LoadStore loadOp, LoadStore storeOp)
    {
        AttachmentViewDesc attachmentViewDesc = {};
        attachmentViewDesc._resourceName = attachment;
        attachmentViewDesc._loadFromPreviousPhase = loadOp;
        attachmentViewDesc._storeToNextPhase = storeOp;
        _depthStencil = attachmentViewDesc;
    }

    inline void SubpassDesc::AppendOutput(const AttachmentViewDesc& view)
    {
        _output.push_back(view);
    }

    inline void SubpassDesc::AppendInput(const AttachmentViewDesc& view)
    {
        _input.push_back(view);
    }

    inline void SubpassDesc::SetDepthStencil(const AttachmentViewDesc& view)
    {
        _depthStencil = view;
    }

}
