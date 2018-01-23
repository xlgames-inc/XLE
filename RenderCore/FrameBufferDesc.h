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

    /// <summary>Attachments are part of a frame buffer, and typically represent a rendering surface</summary>
    /// This description object can define an attachment. Typically the attachment is defined in terms of
    /// some global frame buffer properties (such as output dimensions and sample count).
    class AttachmentViewDesc
    {
    public:
        AttachmentName _resourceName;
        AttachmentName _viewName;

        TextureViewWindow _window;

        enum class LoadStore
        {
            DontCare,                   Retain,                 Clear,
            DontCare_RetainStencil,     Retain_RetainStencil,   Clear_RetainStencil,
            DontCare_ClearStencil,      Retain_ClearStencil,    Clear_ClearStencil
        };
        LoadStore _loadFromPreviousPhase;       ///< equivalent to "load op" in a Vulkan attachment
        LoadStore _storeToNextPhase;            ///< equivalent to "store op" in a Vulkan attachment
    };

    class AttachmentDesc
    {
    public:
        AttachmentName _name;

        enum class DimensionsMode 
        {
            Absolute,                   ///< _width and _height define absolute pixel values
            OutputRelative              ///< _width and _height are multipliers applied to the defined "output" dimensions (ie, specify 1.f to create buffers the same size as the output)
        };
        DimensionsMode _dimsMode;
        float _width, _height;
        unsigned _arrayLayerCount;

        Format _format;
        TextureViewWindow::Aspect _defaultAspect;

        struct Flags
        {
            enum Enum
            {
                Multisampled    = 1<<0,     ///< use the current multisample settings (otherwise just set to single sampled mode)
                ShaderResource  = 1<<1,     ///< allow binding as a shader resource after the render pass has finished
                TransferSource  = 1<<2,     ///< allow binding as a transfer source after the render pass has finished
                RenderTarget    = 1<<3,
                DepthStencil    = 1<<4
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
        static const AttachmentName Unused = ~0u;
        std::vector<AttachmentName> _output = {};
        AttachmentName _depthStencil = Unused;
        std::vector<AttachmentName> _input = {};
        std::vector<AttachmentName> _preserve = {};
		std::vector<AttachmentName> _resolve = {};
    };

    class FrameBufferDesc
	{
	public:
        IteratorRange<const SubpassDesc*>           GetSubpasses() const    { return MakeIteratorRange(_subpasses); }
        IteratorRange<const AttachmentViewDesc*>    GetAttachments() const  { return MakeIteratorRange(_attachments); }
        uint64                                      GetHash() const         { return _hash; }

		FrameBufferDesc(
            IteratorRange<const SubpassDesc*> subpasses,
            IteratorRange<const AttachmentViewDesc*> attachments = IteratorRange<const AttachmentViewDesc*>());
		FrameBufferDesc(
			std::vector<SubpassDesc>&& subpasses,
			std::vector<AttachmentViewDesc>&& attachments = std::vector<AttachmentViewDesc>());
		FrameBufferDesc();
		~FrameBufferDesc();

	private:
        std::vector<SubpassDesc>        _subpasses;
        std::vector<AttachmentViewDesc> _attachments;
        uint64                          _hash;
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
}
