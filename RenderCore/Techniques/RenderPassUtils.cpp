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
        ParsingContext& parserContext)
	{
		SubpassDesc subpass;
		subpass.AppendOutput(0, LoadStore::Retain);
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
        ParsingContext& parserContext)
	{
		parserContext.GetNamedResources().Bind(AttachmentSemantics::ColorLDR, presentationTarget);
		return RenderPassToPresentationTarget(context, parserContext);
	}
}}
