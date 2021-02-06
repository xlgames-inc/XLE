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
        std::vector<SubpassDesc>&& subpasses,
        const FrameBufferProperties& props)
	: _attachments(std::move(attachments))
    , _subpasses(std::move(subpasses))
    , _props(props)
	{
        // Calculate the hash value for this description by combining
        // together the hashes of the members.
        _hash = DefaultSeed64;
        for (const auto&a:_attachments)
            _hash = HashCombine(_hash, HashCombine(a._semantic, a._desc.CalculateHash()));
        for (const auto&sp:_subpasses)
            _hash = HashCombine(_hash, sp.CalculateHash());
        _hash = HashCombine(_hash, _props.CalculateHash());
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

    uint64_t AttachmentDesc::CalculateHash() const
    {
        assert((unsigned(_format) & ((1<<8)-1)) == unsigned(_format));
        assert((_arrayLayerCount & ((1<<7)-1)) == _arrayLayerCount);

        uint64_t t0 =
                uint64_t(_format)
            |   (uint64_t(_arrayLayerCount) << 8ull)
            |   (uint64_t(_dimsMode) << 15ull)
            |   (uint64_t(_flags) << 16ull)
            ;
        uint64_t t1 =
                uint64_t(*(uint32_t*)&_width)
            |   (uint64_t(*(uint32_t*)&_height) << 32ull)
            ;
        return HashCombine(t0, t1);
    }

    uint64_t SubpassDesc::CalculateHash() const
    {
        uint64_t result = Hash64(_attachmentViewBuffer, &_attachmentViewBuffer[BufferSpaceUsed()]);
        result = Hash64(&_depthStencil, &_depthStencil+1, result);
        // result = Hash64(AsPointer(_preserve.begin()), AsPointer(_preserve.end()), result);
        result = Hash64(&_resolveDepthStencil, &_resolveDepthStencil+1, result);
        return result;
    }

    uint64_t FrameBufferProperties::CalculateHash() const
    {
        return uint64_t(_outputWidth) 
            ^ (uint64_t(_outputHeight) << 16ull)
            ^ (uint64_t(_samples._sampleCount) < 48ull)
            ^ (uint64_t(_samples._samplingQuality) < 56ull);
    }

	FrameBufferDesc SeparateSingleSubpass(const FrameBufferDesc& input, unsigned subpassIdx)
	{
		// Take out a single subpass from the input frame buffer desc.
		// Simplify the attachment list down so that it no longer contains any attachments that
		// are now not referenced.
		assert(subpassIdx < input.GetSubpasses().size());
		std::vector<SubpassDesc> newSubpasses;
		newSubpasses.push_back(input.GetSubpasses()[subpassIdx]);

		std::vector<AttachmentName> attachmentRemap;
		attachmentRemap.resize(input.GetAttachments().size(), ~0u);
		unsigned nextRemapIndex = 0;
		for (auto&a:MakeIteratorRange(newSubpasses[0]._attachmentViewBuffer, &newSubpasses[0]._attachmentViewBuffer[newSubpasses[0].BufferSpaceUsed()])) {
			if (attachmentRemap[a._resourceName] == ~0u)
				attachmentRemap[a._resourceName] = nextRemapIndex++;
			a._resourceName = attachmentRemap[a._resourceName];
		}

		if (newSubpasses[0]._depthStencil._resourceName != ~0u) {
			if (attachmentRemap[newSubpasses[0]._depthStencil._resourceName] == ~0u)
				attachmentRemap[newSubpasses[0]._depthStencil._resourceName] = nextRemapIndex++;
			newSubpasses[0]._depthStencil._resourceName = attachmentRemap[newSubpasses[0]._depthStencil._resourceName];
		}

		if (newSubpasses[0]._resolveDepthStencil._resourceName != ~0u) {
			if (attachmentRemap[newSubpasses[0]._resolveDepthStencil._resourceName] == ~0u)
				attachmentRemap[newSubpasses[0]._resolveDepthStencil._resourceName] = nextRemapIndex++;
			newSubpasses[0]._resolveDepthStencil._resourceName = attachmentRemap[newSubpasses[0]._resolveDepthStencil._resourceName];
		}

		// note -- ignoring the "preserve" bindings; because those make less sense with a single subpass

		std::vector<FrameBufferDesc::Attachment> newAttachments;
		newAttachments.resize(nextRemapIndex);
		for (unsigned c=0; c<input.GetAttachments().size(); ++c)
			if (attachmentRemap[c] != ~0u)
				newAttachments[attachmentRemap[c]] = input.GetAttachments()[c];

		return FrameBufferDesc(
			std::move(newAttachments),
			std::move(newSubpasses));
	}

}

