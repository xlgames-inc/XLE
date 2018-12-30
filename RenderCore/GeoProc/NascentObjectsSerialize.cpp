// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentObjectsSerialize.h"
#include "NascentRawGeometry.h"
#include "NascentCommandStream.h"
#include "NascentGeometryObjects.h"
#include "SkeletonRegistry.h"
// #include "SCommandStream.h"
#include "../Assets/AssetUtils.h"
#include "../Assets/AssetsCore.h"
#include "../Assets/NascentChunk.h"
#include "../Assets/ModelImmutableData.h"      // just for RenderCore::Assets::SkeletonBinding
#include "../Assets/RawAnimationCurve.h"
#include "../../Assets/BlockSerializer.h"

namespace RenderCore { namespace Assets { namespace GeoProc
{
	static const unsigned ModelScaffoldVersion = 1;
	static const unsigned ModelScaffoldLargeBlocksVersion = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////

	static ::Assets::Blob SerializeSkin(
		Serialization::NascentBlockSerializer& serializer, 
		NascentGeometryObjects& objs)
	{
		auto result = std::make_shared<std::vector<uint8>>();
		{
			Serialization::NascentBlockSerializer tempBlock;
			for (auto i = objs._rawGeos.begin(); i!=objs._rawGeos.end(); ++i) {
				i->second.Serialize(tempBlock, *result);
			}
			serializer.SerializeSubBlock(tempBlock);
			::Serialize(serializer, objs._rawGeos.size());
		}

		{
			Serialization::NascentBlockSerializer tempBlock;
			for (auto i = objs._skinnedGeos.begin(); i!=objs._skinnedGeos.end(); ++i) {
				i->second.Serialize(tempBlock, *result);
			}
			serializer.SerializeSubBlock(tempBlock);
			::Serialize(serializer, objs._skinnedGeos.size());
		}
		return result;
	}

    class DefaultPoseData
    {
    public:
        std::vector<Float4x4>       _defaultTransforms;
        std::pair<Float3, Float3>   _boundingBox;
    };

    static DefaultPoseData CalculateDefaultPoseData(
        const NascentSkeletonMachine& skeleton,
		const NascentSkeletonInterface& skeletonInterf,
		const TransformationParameterSet& parameters,
        const NascentModelCommandStream& cmdStream,
        const NascentGeometryObjects& geoObjects)
    {
        DefaultPoseData result;

        auto skeletonOutput = skeleton.GenerateOutputTransforms(parameters);

        auto skelOutputInterface = skeletonInterf.GetOutputInterface();
        auto streamInputInterface = cmdStream.GetInputInterface();
        RenderCore::Assets::SkeletonBinding skelBinding(
            RenderCore::Assets::SkeletonMachine::OutputInterface
                {AsPointer(skelOutputInterface.begin()), skelOutputInterface.size()},
            RenderCore::Assets::ModelCommandStream::InputInterface
                {AsPointer(streamInputInterface.begin()), streamInputInterface.size()});

        auto finalMatrixCount = (unsigned)streamInputInterface.size(); // immData->_visualScene.GetInputInterface()._jointCount;
        result._defaultTransforms.resize(finalMatrixCount);
        for (unsigned c=0; c<finalMatrixCount; ++c) {
            auto machineOutputIndex = skelBinding.ModelJointToMachineOutput(c);
            if (machineOutputIndex == ~unsigned(0x0)) {
                result._defaultTransforms[c] = Identity<Float4x4>();
            } else {
                result._defaultTransforms[c] = skeletonOutput[machineOutputIndex];
            }
        }

            // if we have any non-identity internal transforms, then we should 
            // write a default set of transformations. But many models don't have any
            // internal transforms -- in this case all of the generated transforms
            // will be identity. If we find this case, they we should write zero
            // default transforms.
        bool hasNonIdentity = false;
        const float tolerance = 1e-6f;
        for (unsigned c=0; c<finalMatrixCount; ++c)
            hasNonIdentity |= !Equivalent(result._defaultTransforms[c], Identity<Float4x4>(), tolerance);
        if (!hasNonIdentity) {
            finalMatrixCount = 0u;
            result._defaultTransforms.clear();
        }

        result._boundingBox = geoObjects.CalculateBoundingBox(
            cmdStream, MakeIteratorRange(result._defaultTransforms));

        return result;
    }

	static void TraceMetrics(std::ostream& stream, const NascentGeometryObjects& geoObjects, const NascentModelCommandStream& cmdStream, const NascentSkeleton& skeleton)
	{
		stream << "============== Geometry Objects ==============" << std::endl;
		stream << geoObjects;
		stream << std::endl;
		stream << "============== Command stream ==============" << std::endl;
		stream << cmdStream;
		stream << std::endl;
		stream << "============== Transformation Machine ==============" << std::endl;
		StreamOperator(stream, skeleton.GetSkeletonMachine(), skeleton.GetInterface(), skeleton.GetDefaultParameters());
	}

	std::vector<::Assets::ICompileOperation::OperationResult> SerializeSkinToChunks(const char name[], NascentGeometryObjects& geoObjects, NascentModelCommandStream& cmdStream, NascentSkeleton& skeleton)
	{
		Serialization::NascentBlockSerializer serializer;

		::Serialize(serializer, cmdStream);
		auto largeResourcesBlock = SerializeSkin(serializer, geoObjects);
		::Serialize(serializer, skeleton);

			// Generate the default transforms and serialize them out
			// unfortunately this requires we use the run-time types to
			// work out the transforms.
			// And that requires a bit of hack to get pointers to those 
			// run-time types
		{
			auto defaultPoseData = CalculateDefaultPoseData(skeleton.GetSkeletonMachine(), skeleton.GetInterface(), skeleton.GetDefaultParameters(), cmdStream, geoObjects);
			serializer.SerializeSubBlock(MakeIteratorRange(defaultPoseData._defaultTransforms));
			serializer.SerializeValue(size_t(defaultPoseData._defaultTransforms.size()));
			::Serialize(serializer, defaultPoseData._boundingBox.first);
			::Serialize(serializer, defaultPoseData._boundingBox.second);
		}

		// Find the max LOD value, and serialize that
		::Serialize(serializer, cmdStream.GetMaxLOD());

		// Serialize human-readable metrics information
		std::stringstream metricsStream;
		TraceMetrics(metricsStream, geoObjects, cmdStream, skeleton);

		auto scaffoldBlock = ::Assets::AsBlob(serializer);
		auto metricsBlock = ::Assets::AsBlob(metricsStream);

		return
			{
				::Assets::ICompileOperation::OperationResult{
					RenderCore::Assets::ChunkType_ModelScaffold, ModelScaffoldVersion, name,
					std::move(scaffoldBlock)},
				::Assets::ICompileOperation::OperationResult{
					RenderCore::Assets::ChunkType_ModelScaffoldLargeBlocks, ModelScaffoldLargeBlocksVersion, name,
					std::move(largeResourcesBlock)},
				::Assets::ICompileOperation::OperationResult{
					RenderCore::Assets::ChunkType_Metrics, 0, "metrics", 
					std::move(metricsBlock)}
			};
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	std::vector<::Assets::ICompileOperation::OperationResult> SerializeSkeletonToChunks(
		const char name[], 
		const NascentSkeleton& skeleton)
	{
		auto block = ::Assets::SerializeToBlob(skeleton);

		std::stringstream metricsStream;
		StreamOperator(metricsStream, skeleton.GetSkeletonMachine(), skeleton.GetInterface(), skeleton.GetDefaultParameters());
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
		const char name[],
		const NascentAnimationSet& animationSet,
		IteratorRange<const RenderCore::Assets::RawAnimationCurve*> curves)
	{
		Serialization::NascentBlockSerializer serializer;

		::Serialize(serializer, animationSet);
		serializer.SerializeSubBlock(curves);
		serializer.SerializeValue(curves.size());

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
