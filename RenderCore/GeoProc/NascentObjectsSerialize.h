// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/ICompileOperation.h"
#include "../../Utility/IteratorUtils.h"

namespace RenderCore { namespace Assets { class RawAnimationCurve; }}

namespace RenderCore { namespace Assets { namespace GeoProc
{
	class NascentGeometryObjects;
	class NascentModelCommandStream;
	class NascentSkeleton;
	class NascentAnimationSet;

	std::vector<::Assets::ICompileOperation::OperationResult> SerializeSkinToChunks(
		const char name[], 
		NascentGeometryObjects& geoObjects, 
		NascentModelCommandStream& cmdStream, 
		NascentSkeleton& skeleton);

	std::vector<::Assets::ICompileOperation::OperationResult> SerializeSkeletonToChunks(
		const char name[], 
		const NascentSkeleton& skeleton);

	std::vector<::Assets::ICompileOperation::OperationResult> SerializeAnimationsToChunks(
		const char name[],
		const NascentAnimationSet& animationSet,
		IteratorRange<const RawAnimationCurve*> curves);
}}}
