// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderPassUtils.h"
#include "RenderPass.h"
#include "ParsingContext.h"
#include "CommonBindings.h"
#include "../IDevice.h"
#include "../../Utility/IteratorUtils.h"

namespace RenderCore { namespace Techniques
{
	RenderPassInstance RenderPassToPresentationTarget(
		IThreadContext& context,
        ParsingContext& parserContext,
		LoadStore loadOperation)
	{
		SubpassDesc subpass;
		subpass._output.push_back( AttachmentViewDesc { 0, loadOperation, LoadStore::Retain, TextureViewDesc{ TextureViewDesc::Aspect::ColorSRGB } });
		FrameBufferDesc::Attachment attachment { 
			AttachmentSemantics::ColorLDR,
			AsAttachmentDesc(parserContext.GetNamedResources().GetBoundResource(AttachmentSemantics::ColorLDR)->GetDesc())
		};

		FrameBufferDesc fbDesc {
			std::vector<FrameBufferDesc::Attachment>{attachment},
			std::vector<SubpassDesc>{subpass}
		};
        return RenderPassInstance(
            context, fbDesc,
			parserContext.GetFrameBufferPool(),
            parserContext.GetNamedResources());
	}

	RenderPassInstance RenderPassToPresentationTarget(
		IThreadContext& context,
		const RenderCore::IResourcePtr& presentationTarget,
        ParsingContext& parserContext,
		LoadStore loadOperation)
	{
		parserContext.GetNamedResources().Bind(AttachmentSemantics::ColorLDR, presentationTarget);
		return RenderPassToPresentationTarget(context, parserContext, loadOperation);
	}

	RenderPassInstance RenderPassToPresentationTargetWithDepthStencil(
		IThreadContext& context,
		const std::shared_ptr<RenderCore::IResource>& presentationTarget,
        ParsingContext& parserContext,
		LoadStore loadOperation)
	{
		auto boundDepth = parserContext.GetNamedResources().GetBoundResource(AttachmentSemantics::MultisampleDepth);
		if (!boundDepth) {
			return RenderPassToPresentationTarget(context, presentationTarget, parserContext, loadOperation);
		}

		SubpassDesc subpass;
		subpass._output.push_back( AttachmentViewDesc { 0, loadOperation, LoadStore::Retain, TextureViewDesc{ TextureViewDesc::Aspect::ColorSRGB } });
		subpass._depthStencil = AttachmentViewDesc { 1, loadOperation, LoadStore::Retain, TextureViewDesc{ TextureViewDesc::Aspect::DepthStencil } };
		FrameBufferDesc::Attachment colorAttachment { AttachmentSemantics::ColorLDR, AsAttachmentDesc(presentationTarget->GetDesc()) };
		FrameBufferDesc::Attachment depthAttachment { AttachmentSemantics::MultisampleDepth, AsAttachmentDesc(boundDepth->GetDesc()) };

		FrameBufferDesc fbDesc {
			std::vector<FrameBufferDesc::Attachment>{colorAttachment, depthAttachment},
			std::vector<SubpassDesc>{std::move(subpass)}
		};
        return RenderPassInstance(
            context, fbDesc,
			parserContext.GetFrameBufferPool(),
            parserContext.GetNamedResources());
	}
}}
