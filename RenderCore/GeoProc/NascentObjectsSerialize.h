// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/NascentChunkArray.h"

namespace RenderCore { namespace Assets { namespace GeoProc
{
	class NascentGeometryObjects;
	class NascentModelCommandStream;
	class NascentSkeleton;

	::Assets::NascentChunkArray SerializeSkinToChunks(
		const char name[], 
		NascentGeometryObjects& geoObjects, 
		NascentModelCommandStream& cmdStream, 
		NascentSkeleton& skeleton);

	::Assets::NascentChunkArray SerializeSkeletonToChunks(
		const char name[], 
		NascentSkeleton& skeleton);
}}}
