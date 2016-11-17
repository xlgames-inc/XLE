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
#include "SCommandStream.h"
#include "../RenderCore/Assets/AssetUtils.h"
#include "../RenderCore/Assets/ModelImmutableData.h"      // just for RenderCore::Assets::SkeletonBinding
#include "../Assets/BlockSerializer.h"

namespace RenderCore { namespace ColladaConversion
{
	static const unsigned ModelScaffoldVersion = 1;
	static const unsigned ModelScaffoldLargeBlocksVersion = 0;

	static void DestroyChunkArray(const void* chunkArray) { delete (std::vector<NascentChunk>*)chunkArray; }

	NascentChunkArray MakeNascentChunkArray(
        const std::initializer_list<NascentChunk>& inits)
    {
        return NascentChunkArray(
            new std::vector<NascentChunk>(inits),
            &DestroyChunkArray);
    }

    std::vector<uint8> AsVector(const Serialization::NascentBlockSerializer& serializer)
    {
        auto block = serializer.AsMemoryBlock();
        size_t size = Serialization::Block_GetSize(block.get());
        return std::vector<uint8>(block.get(), PtrAdd(block.get(), size));
    }

    template<typename Char>
        static std::vector<uint8> AsVector(std::basic_stringstream<Char>& stream)
    {
        auto str = stream.str();
        return std::vector<uint8>((const uint8*)AsPointer(str.begin()), (const uint8*)AsPointer(str.end()));
    }

    template<typename Type>
        static std::vector<uint8> SerializeToVector(const Type& obj)
    {
        Serialization::NascentBlockSerializer serializer;
        ::Serialize(serializer, obj);
        return AsVector(serializer);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	static void SerializeSkin(
		Serialization::NascentBlockSerializer& serializer, 
		std::vector<uint8>& largeResourcesBlock,
		NascentGeometryObjects& objs)
	{
		{
			Serialization::NascentBlockSerializer tempBlock;
			for (auto i = objs._rawGeos.begin(); i!=objs._rawGeos.end(); ++i) {
				i->second.Serialize(tempBlock, largeResourcesBlock);
			}
			serializer.SerializeSubBlock(tempBlock);
			::Serialize(serializer, objs._rawGeos.size());
		}

		{
			Serialization::NascentBlockSerializer tempBlock;
			for (auto i = objs._skinnedGeos.begin(); i!=objs._skinnedGeos.end(); ++i) {
				i->second.Serialize(tempBlock, largeResourcesBlock);
			}
			serializer.SerializeSubBlock(tempBlock);
			::Serialize(serializer, objs._skinnedGeos.size());
		}
	}

    class DefaultPoseData
    {
    public:
        std::vector<Float4x4>       _defaultTransforms;
        std::pair<Float3, Float3>   _boundingBox;
    };

    static DefaultPoseData CalculateDefaultPoseData(
        const NascentSkeleton::NascentTransformationMachine& transMachine,
        const NascentModelCommandStream& cmdStream,
        const NascentGeometryObjects& geoObjects)
    {
        DefaultPoseData result;

        auto skeletonOutput = transMachine.GenerateOutputTransforms(
            transMachine.GetDefaultParameters());

        auto skelOutputInterface = transMachine.GetOutputInterface();
        auto streamInputInterface = cmdStream.GetInputInterface();
        RenderCore::Assets::SkeletonBinding skelBinding(
            RenderCore::Assets::TransformationMachine::OutputInterface
                {AsPointer(skelOutputInterface.first.begin()), AsPointer(skelOutputInterface.second.begin()), skelOutputInterface.first.size()},
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

	static void TraceMetrics(std::ostream& stream, NascentGeometryObjects& geoObjects, NascentModelCommandStream& cmdStream, NascentSkeleton& skeleton)
	{
		stream << "============== Geometry Objects ==============" << std::endl;
		stream << geoObjects;
		stream << std::endl;
		stream << "============== Command stream ==============" << std::endl;
		stream << cmdStream;
		stream << std::endl;
		stream << "============== Transformation Machine ==============" << std::endl;
		StreamOperator(stream, skeleton.GetTransformationMachine());
	}

	NascentChunkArray SerializeSkinToChunks(const char name[], NascentGeometryObjects& geoObjects, NascentModelCommandStream& cmdStream, NascentSkeleton& skeleton)
	{
		Serialization::NascentBlockSerializer serializer;
		std::vector<uint8> largeResourcesBlock;

		::Serialize(serializer, cmdStream);
		SerializeSkin(serializer, largeResourcesBlock, geoObjects);
		::Serialize(serializer, skeleton);

			// Generate the default transforms and serialize them out
			// unfortunately this requires we use the run-time types to
			// work out the transforms.
			// And that requires a bit of hack to get pointers to those 
			// run-time types
		{
			const auto& transMachine = skeleton.GetTransformationMachine();

			auto defaultPoseData = CalculateDefaultPoseData(transMachine, cmdStream, geoObjects);
			serializer.SerializeSubBlock(
				AsPointer(defaultPoseData._defaultTransforms.cbegin()), 
				AsPointer(defaultPoseData._defaultTransforms.cend()));
			serializer.SerializeValue(size_t(defaultPoseData._defaultTransforms.size()));
			::Serialize(serializer, defaultPoseData._boundingBox.first);
			::Serialize(serializer, defaultPoseData._boundingBox.second);
		}

		// Find the max LOD value, and serialize that
		::Serialize(serializer, cmdStream.GetMaxLOD());

		// Serialize human-readable metrics information
		std::stringstream metricsStream;
		TraceMetrics(metricsStream, geoObjects, cmdStream, skeleton);

		auto scaffoldBlock = AsVector(serializer);
		auto metricsBlock = AsVector(metricsStream);

		Serialization::ChunkFile::ChunkHeader scaffoldChunk(
			RenderCore::Assets::ChunkType_ModelScaffold, ModelScaffoldVersion, name, unsigned(scaffoldBlock.size()));
		Serialization::ChunkFile::ChunkHeader largeBlockChunk(
			RenderCore::Assets::ChunkType_ModelScaffoldLargeBlocks, ModelScaffoldLargeBlocksVersion, name, (unsigned)largeResourcesBlock.size());
		Serialization::ChunkFile::ChunkHeader metricsChunk(
			RenderCore::Assets::ChunkType_Metrics, 0, "metrics", (unsigned)metricsBlock.size());

		return MakeNascentChunkArray(
			{
				NascentChunk(scaffoldChunk, std::move(scaffoldBlock)),
				NascentChunk(largeBlockChunk, std::move(largeResourcesBlock)),
				NascentChunk(metricsChunk, std::move(metricsBlock))
			});
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	static void TraceMetrics(std::ostream& stream, NascentSkeleton& skeleton)
	{
		StreamOperator(stream, skeleton.GetTransformationMachine());
	}

	NascentChunkArray SerializeSkeletonToChunks(
		const char name[], 
		NascentSkeleton& skeleton)
	{
		auto block = SerializeToVector(skeleton);

		std::stringstream metricsStream;
		TraceMetrics(metricsStream, skeleton);
		auto metricsBlock = AsVector(metricsStream);

		Serialization::ChunkFile::ChunkHeader scaffoldChunk(
			RenderCore::Assets::ChunkType_Skeleton, 0, name, unsigned(block.size()));
		Serialization::ChunkFile::ChunkHeader metricsChunk(
			RenderCore::Assets::ChunkType_Metrics, 0, "metrics", (unsigned)metricsBlock.size());

		return MakeNascentChunkArray({
			NascentChunk(scaffoldChunk, std::move(block)),
			NascentChunk(metricsChunk, std::move(metricsBlock))
		});
	}

}}
