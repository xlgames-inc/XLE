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

	FrameBufferDesc::FrameBufferDesc(std::vector<SubpassDesc>&& subpasses)
	: _subpasses(std::move(subpasses))
	{
        // Calculate the hash value for this description by combining
        // together the hashes of the members.
        _hash = Hash64(AsPointer(_subpasses.begin()), AsPointer(_subpasses.end()));
    }

    FrameBufferDesc::FrameBufferDesc(IteratorRange<const SubpassDesc*> subpasses)
    : FrameBufferDesc(std::vector<SubpassDesc>(subpasses.begin(), subpasses.end()))
    {}

	FrameBufferDesc::FrameBufferDesc(std::initializer_list<SubpassDesc> subpasses)
	: FrameBufferDesc(std::vector<SubpassDesc>(subpasses.begin(), subpasses.end()))
	{}

	FrameBufferDesc::FrameBufferDesc()
    : _hash(0)
    {
    }

    FrameBufferDesc::~FrameBufferDesc() {}

	INamedAttachments::~INamedAttachments() {}

	TextureViewDesc CompleteTextureViewDesc(const AttachmentDesc& attachmentDesc, const TextureViewDesc& viewDesc)
	{
		TextureViewDesc result = viewDesc;
		if (result._format._aspect == TextureViewDesc::Aspect::UndefinedAspect)
			result._format._aspect = attachmentDesc._defaultAspect;
		return result;
	}

}

