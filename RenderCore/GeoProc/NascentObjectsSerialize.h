// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/ICompileOperation.h"

namespace RenderCore { namespace Assets { namespace GeoProc
{
	class NascentModel;
	class NascentSkeleton;
	class NascentAnimationSet;

	std::vector<::Assets::ICompileOperation::OperationResult> SerializeSkinToChunks(
		const std::string& name,
		const NascentModel& model,
		const NascentSkeleton& embeddedSkeleton);

	std::vector<::Assets::ICompileOperation::OperationResult> SerializeSkeletonToChunks(
		const std::string& name,
		const NascentSkeleton& skeleton);

	std::vector<::Assets::ICompileOperation::OperationResult> SerializeAnimationsToChunks(
		const std::string& name,
		const NascentAnimationSet& animationSet);
}}}
