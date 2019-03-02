// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/ICompileOperation.h"
#include "../../Utility/IteratorUtils.h"

namespace RenderCore { namespace Assets { class RawAnimationCurve; }}

namespace Utility {
	template<typename Element>
		class SerializableVector;
}

namespace RenderCore { namespace Assets { namespace GeoProc
{
	class NascentGeometryObjects;
	class NascentModelCommandStream;
	class NascentSkeleton;
	class NascentAnimationSet;

	std::vector<::Assets::ICompileOperation::OperationResult> SerializeSkinToChunks(
		const std::string& name,
		const NascentGeometryObjects& geoObjects, 
		const NascentModelCommandStream& cmdStream, 
		const NascentSkeleton& skeleton);

	std::vector<::Assets::ICompileOperation::OperationResult> SerializeSkeletonToChunks(
		const std::string& name,
		const NascentSkeleton& skeleton);

	std::vector<::Assets::ICompileOperation::OperationResult> SerializeAnimationsToChunks(
		const std::string& name,
		const NascentAnimationSet& animationSet,
		const SerializableVector<RawAnimationCurve>& curves);
}}}
