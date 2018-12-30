// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Matrix.h"

namespace RenderCore { class IThreadContext; }
namespace RenderCore { namespace Techniques { class ParsingContext; } }

namespace RenderCore { namespace Assets
{
	class SkeletonMachine;

	void    RenderSkeleton(
        IThreadContext& context, 
        Techniques::ParsingContext& parserContext, 
		const SkeletonMachine& skeleton,
		const Float4x4& localToWorld);
}}

