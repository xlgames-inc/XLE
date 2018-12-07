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
            TextureViewDesc::UndefinedAspect,
            RenderCore::AttachmentDesc::DimensionsMode::Absolute,
              ((desc._bindFlags & BindFlag::RenderTarget) ? AttachmentDesc::Flags::RenderTarget : 0u)
            | ((desc._bindFlags & BindFlag::ShaderResource) ? AttachmentDesc::Flags::ShaderResource : 0u)
            | ((desc._bindFlags & BindFlag::DepthStencil) ? AttachmentDesc::Flags::DepthStencil : 0u)
            | ((desc._bindFlags & BindFlag::TransferSrc) ? AttachmentDesc::Flags::TransferSource : 0u)
        };
    }

	TextureViewDesc CompleteTextureViewDesc(const AttachmentDesc& attachmentDesc, const TextureViewDesc& viewDesc)
	{
		TextureViewDesc result = viewDesc;
		if (result._format._aspect == TextureViewDesc::Aspect::UndefinedAspect)
			result._format._aspect = attachmentDesc._defaultAspect;
		return result;
	}

}

