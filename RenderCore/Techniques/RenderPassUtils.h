// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

namespace RenderCore { class IThreadContext; class IResource; }

namespace RenderCore { namespace Techniques
{
	class RenderPassInstance;
	class ParsingContext;

	RenderPassInstance RenderPassToPresentationTarget(
		IThreadContext& context,
        ParsingContext& parserContext);

	RenderPassInstance RenderPassToPresentationTarget(
		IThreadContext& context,
		const std::shared_ptr<RenderCore::IResource>& presentationTarget,
        ParsingContext& parserContext);

	RenderPassInstance RenderPassToPresentationTargetWithDepthStencil(
		IThreadContext& context,
		const std::shared_ptr<RenderCore::IResource>& presentationTarget,
        ParsingContext& parserContext);
}}
