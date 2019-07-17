// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FrameBufferDesc.h"
#include "Format.h"
#include "../Utility/MemoryUtils.h"

namespace RenderCore
{
	const AttachmentViewDesc SubpassDesc::Unused = AttachmentViewDesc{};

	FrameBufferDesc FrameBufferDesc::s_empty { {}, {SubpassDesc{}} };

	FrameBufferDesc::FrameBufferDesc(
        std::vector<Attachment>&& attachments,
        std::vector<SubpassDesc>&& subpasses)
	: _attachments(std::move(attachments))
    , _subpasses(std::move(subpasses))
	{
        // Calculate the hash value for this description by combining
        // together the hashes of the members.
        _hash = Hash64(AsPointer(_attachments.begin()), AsPointer(_attachments.end()));
        _hash = Hash64(AsPointer(_subpasses.begin()), AsPointer(_subpasses.end()), _hash);
    }

	FrameBufferDesc::FrameBufferDesc()
    : _hash(0)
    {
    }

    FrameBufferDesc::~FrameBufferDesc() {}

	INamedAttachments::~INamedAttachments() {}

    AttachmentDesc AsAttachmentDesc(const ResourceDesc& desc)
    {
        return {
            desc._textureDesc._format,
            (float)desc._textureDesc._width, (float)desc._textureDesc._height,
            0u,
            RenderCore::AttachmentDesc::DimensionsMode::Absolute,
              ((desc._bindFlags & BindFlag::RenderTarget) ? AttachmentDesc::Flags::RenderTarget : 0u)
            | ((desc._bindFlags & BindFlag::ShaderResource) ? AttachmentDesc::Flags::ShaderResource : 0u)
            | ((desc._bindFlags & BindFlag::DepthStencil) ? AttachmentDesc::Flags::DepthStencil : 0u)
            | ((desc._bindFlags & BindFlag::TransferSrc) ? AttachmentDesc::Flags::TransferSource : 0u)
        };
    }

    const char* AsString(LoadStore input)
    {
        switch (input) {
        case LoadStore::DontCare: return "DontCare";
        case LoadStore::Retain: return "Retain";
        case LoadStore::Clear: return "Clear";
        case LoadStore::DontCare_RetainStencil: return "DontCare_RetainStencil";
        case LoadStore::Retain_RetainStencil: return "Retain_RetainStencil";
        case LoadStore::Clear_RetainStencil: return "Clear_RetainStencil";
        case LoadStore::DontCare_ClearStencil: return "DontCare_ClearStencil";
        case LoadStore::Retain_ClearStencil: return "Retain_ClearStencil";
        case LoadStore::Clear_ClearStencil: return "Clear_ClearStencil";
        default: return "<<unknown>>";
        }
    }

	TextureViewDesc CompleteTextureViewDesc(const AttachmentDesc& attachmentDesc, const TextureViewDesc& viewDesc, TextureViewDesc::Aspect defaultAspect)
	{
		TextureViewDesc result = viewDesc;
		if (result._format._aspect == TextureViewDesc::Aspect::UndefinedAspect)
			result._format._aspect = defaultAspect;
		return result;
	}

}

