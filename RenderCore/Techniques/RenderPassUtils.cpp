// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderPassUtils.h"
#include "RenderPass.h"
#include "ParsingContext.h"

namespace RenderCore { namespace Techniques
{
	RenderPassInstance RenderPassToPresentationTarget(
		IThreadContext& context,
        ParsingContext& parserContext)
	{
		SubpassDesc subPasses[] = {{std::vector<AttachmentViewDesc>{AttachmentViewDesc{0}}}};
		FrameBufferDesc fbDesc = MakeIteratorRange(subPasses);
		auto fb = parserContext.GetFrameBufferPool().BuildFrameBuffer(fbDesc, parserContext.GetNamedResources());
        return RenderPassInstance(
            context, fb, fbDesc,
            parserContext.GetNamedResources());
	}
}}
