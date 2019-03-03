// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentObjectsSerialize.h"
#include "NascentModel.h"
#include "NascentCommandStream.h"
#include "../Assets/AssetUtils.h"
#include "../Assets/NascentChunk.h"

namespace RenderCore { namespace Assets { namespace GeoProc
{
	std::vector<::Assets::ICompileOperation::OperationResult> SerializeSkinToChunks(
		const std::string& name,
		const NascentModel& model,
		const NascentSkeleton& embeddedSkeleton)
	{
		return model.SerializeToChunks(name, embeddedSkeleton);
	}

	std::vector<::Assets::ICompileOperation::OperationResult> SerializeSkeletonToChunks(
		const std::string& name,
		const NascentSkeleton& skeleton)
	{
		auto block = ::Assets::SerializeToBlob(skeleton);

		std::stringstream metricsStream;
		StreamOperator(metricsStream, skeleton.GetSkeletonMachine(), skeleton.GetDefaultParameters());
		auto metricsBlock = ::Assets::AsBlob(metricsStream);

		return {
			::Assets::ICompileOperation::OperationResult{
				RenderCore::Assets::ChunkType_Skeleton, 0, name, 
				std::move(block)},
			::Assets::ICompileOperation::OperationResult{
				RenderCore::Assets::ChunkType_Metrics, 0, "metrics", 
				std::move(metricsBlock)}
		};
	}

	std::vector<::Assets::ICompileOperation::OperationResult> SerializeAnimationsToChunks(
		const std::string& name,
		const NascentAnimationSet& animationSet)
	{
		Serialization::NascentBlockSerializer serializer;
		::Serialize(serializer, animationSet);
		auto block = ::Assets::AsBlob(serializer);

		std::stringstream metricsStream;
		StreamOperator(metricsStream, animationSet);
		auto metricsBlock = ::Assets::AsBlob(metricsStream);

		return {
			::Assets::ICompileOperation::OperationResult{
				RenderCore::Assets::ChunkType_AnimationSet, 0, name, 
				std::move(block)},
			::Assets::ICompileOperation::OperationResult{
				RenderCore::Assets::ChunkType_Metrics, 0, "metrics", 
				std::move(metricsBlock)}
		};
	}

}}}
