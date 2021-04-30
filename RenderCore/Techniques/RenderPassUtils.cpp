// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderPassUtils.h"
#include "RenderPass.h"
#include "ParsingContext.h"
#include "CommonBindings.h"
#include "Techniques.h"
#include "../IDevice.h"
#include "../IThreadContext.h"
#include "../Format.h"
#include "../../Utility/IteratorUtils.h"

namespace RenderCore { namespace Techniques
{
	RenderPassInstance RenderPassToPresentationTarget(
		IThreadContext& context,
        ParsingContext& parserContext,
		LoadStore loadOperation)
	{
		SubpassDesc subpass;
		subpass.AppendOutput( AttachmentViewDesc { 0, loadOperation, LoadStore::Retain, TextureViewDesc{ TextureViewDesc::Aspect::ColorSRGB } });
		FrameBufferDesc::Attachment attachment { AttachmentSemantics::ColorLDR };

		FrameBufferDesc fbDesc {
			std::vector<FrameBufferDesc::Attachment>{attachment},
			std::vector<SubpassDesc>{subpass},
			parserContext._fbProps
		};
        return RenderPassInstance{context, parserContext, fbDesc};
	}

	RenderPassInstance RenderPassToPresentationTarget(
		IThreadContext& context,
		const RenderCore::IResourcePtr& presentationTarget,
        ParsingContext& parserContext,
		LoadStore loadOperation)
	{
		parserContext.GetTechniqueContext()._attachmentPool->Bind(AttachmentSemantics::ColorLDR, presentationTarget);
		return RenderPassToPresentationTarget(context, parserContext, loadOperation);
	}

	RenderPassInstance RenderPassToPresentationTargetWithDepthStencil(
		IThreadContext& context,
        ParsingContext& parserContext,
		LoadStore loadOperation)
	{
		auto boundDepth = parserContext.GetTechniqueContext()._attachmentPool->GetBoundResource(AttachmentSemantics::MultisampleDepth);
		if (!boundDepth && loadOperation != LoadStore::Clear)
			return RenderPassToPresentationTarget(context, parserContext, loadOperation);

		SubpassDesc subpass;
		subpass.AppendOutput( AttachmentViewDesc { 0, loadOperation, LoadStore::Retain, TextureViewDesc{ TextureViewDesc::Aspect::ColorSRGB } });
		subpass.SetDepthStencil( AttachmentViewDesc { 1, loadOperation, LoadStore::Retain, TextureViewDesc{ TextureViewDesc::Aspect::DepthStencil } });
		FrameBufferDesc::Attachment colorAttachment { AttachmentSemantics::ColorLDR };

        AttachmentDesc depthDesc;
        if (boundDepth) {
            depthDesc = AsAttachmentDesc(boundDepth->GetDesc());
        } else {
            depthDesc = colorAttachment._desc;
            auto device = context.GetDevice();
            if (device->QueryFormatCapability(Format::D24_UNORM_S8_UINT, 0) == FormatCapability::Supported) {
                depthDesc._format = Format::D24_UNORM_S8_UINT;
            } else if (device->QueryFormatCapability(Format::D32_SFLOAT_S8_UINT, 0) == FormatCapability::Supported) {
                depthDesc._format = Format::D32_SFLOAT_S8_UINT;
            } else {
                assert(0);
            }
            depthDesc._flags = AttachmentDesc::Flags::Multisampled;
			depthDesc._bindFlagsForFinalLayout = BindFlag::DepthStencil;
        }
		FrameBufferDesc::Attachment depthAttachment { AttachmentSemantics::MultisampleDepth, depthDesc };

		FrameBufferDesc fbDesc {
			std::vector<FrameBufferDesc::Attachment>{colorAttachment, depthAttachment},
			std::vector<SubpassDesc>{std::move(subpass)},
			parserContext._fbProps
		};
        return RenderPassInstance{context, parserContext, fbDesc};
	}

	RenderPassInstance RenderPassToPresentationTargetWithDepthStencil(
		IThreadContext& context,
		const std::shared_ptr<RenderCore::IResource>& presentationTarget,
        ParsingContext& parserContext,
		LoadStore loadOperation)
	{
        parserContext.GetTechniqueContext()._attachmentPool->Bind(AttachmentSemantics::ColorLDR, presentationTarget);
		return RenderPassToPresentationTargetWithDepthStencil(context, parserContext, loadOperation);
	}
}}
