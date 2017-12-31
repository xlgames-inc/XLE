// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/NascentChunk.h"
#include "../../Utility/IteratorUtils.h"

namespace RenderCore { namespace Assets { class RawAnimationCurve; }}

namespace RenderCore { namespace Assets { namespace GeoProc
{
	class NascentGeometryObjects;
	class NascentModelCommandStream;
	class NascentSkeleton;
	class NascentAnimationSet;

	::Assets::NascentChunkArray SerializeSkinToChunks(
		const char name[], 
		NascentGeometryObjects& geoObjects, 
		NascentModelCommandStream& cmdStream, 
		NascentSkeleton& skeleton);

	::Assets::NascentChunkArray SerializeSkeletonToChunks(
		const char name[], 
		const NascentSkeleton& skeleton);

	::Assets::NascentChunkArray SerializeAnimationsToChunks(
		const char name[],
		const NascentAnimationSet& animationSet,
		IteratorRange<const RawAnimationCurve*> curves);
}}}
