// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Matrix.h"

namespace RenderCore { class IThreadContext; }
namespace RenderCore { namespace Techniques { class ParsingContext; } }
namespace RenderCore { namespace Assets { class SkeletonMachine; class TransformationParameterSet; } }

namespace RenderOverlays
{
	void    RenderSkeleton(
        RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext, 
		const RenderCore::Assets::SkeletonMachine& skeleton,
		const RenderCore::Assets::TransformationParameterSet& params,
		const Float4x4& localToWorld);

	void    RenderSkeleton(
        RenderCore::IThreadContext& context, 
        RenderCore::Techniques::ParsingContext& parserContext, 
		const RenderCore::Assets::SkeletonMachine& skeleton,
		const Float4x4& localToWorld);
}
