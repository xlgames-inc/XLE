// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NascentObjectGuid.h"
#include "../Format.h"
#include "../Types.h"
#include "../StateDesc.h"
#include "../Assets/TransformationCommands.h"		// for ITransformationMachineOptimizer
#include "../../Assets/ICompileOperation.h"
#include "../../Math/Matrix.h"
#include "../../Utility/UTFUtils.h"
#include "../../Utility/IteratorUtils.h"
#include <memory>
#include <string>
#include <vector>

namespace RenderCore { namespace Assets { class DrawCallDesc; }}

namespace RenderCore { namespace Assets { namespace GeoProc
{
	class MeshDatabase;
	class UnboundSkinController;
	class NascentSkeletonMachine;
	class NascentSkeleton;
	struct NativeVBSettings;

	class NascentModel
	{
	public:
		class Indexor
		{
		public:
			NascentObjectGuid		_srcObject;
			std::string				_name;
		};

		struct DrawCallDesc
		{
			unsigned    _firstIndex = ~0u, _indexCount = ~0u;
			Topology	_topology = Topology(0);
		};
		
		class GeometryBlock
		{
		public:
			std::shared_ptr<MeshDatabase>	_mesh;
			std::vector<DrawCallDesc>		_drawCalls;
			std::vector<uint32_t>			_meshVertexIndexToSrcIndex;		// srcIndex = _meshVertexIndexToSrcIndex[meshDatabaseUnifiedVertexIndex]
			Float4x4						_geoSpaceToNodeSpace;

			std::vector<uint8_t>			_indices;
			Format							_indexFormat = Format(0);
		};

		class SkinControllerBlock
		{
		public:
			std::shared_ptr<UnboundSkinController>	_controller;
			std::string								_skeleton;
		};

		class Command
		{
		public:
			NascentObjectGuid				_geometryBlock;
			std::vector<NascentObjectGuid>	_skinControllerBlocks;
			std::string						_localToModel;
			std::vector<std::string>		_materialBindingSymbols;
			unsigned						_levelOfDetail = ~0u;
		};

		const GeometryBlock* FindGeometryBlock(NascentObjectGuid id) const;
		const SkinControllerBlock* FindSkinControllerBlock(NascentObjectGuid id) const;
		const Command* FindCommand(NascentObjectGuid id) const;

		void Add(NascentObjectGuid id, const std::string& name, GeometryBlock&& object);
		void Add(NascentObjectGuid id, const std::string& name, SkinControllerBlock&& object);
		void Add(NascentObjectGuid id, const std::string& name, Command&& object);

		IteratorRange<const std::pair<Indexor,GeometryBlock>*> GetGeometryBlocks() const { return MakeIteratorRange(_geoBlocks); }
		IteratorRange<const std::pair<Indexor,SkinControllerBlock>*> GetSkinControllerBlocks() const { return MakeIteratorRange(_skinBlocks); }
		IteratorRange<const std::pair<Indexor,Command>*> GetCommands() const { return MakeIteratorRange(_commands); }

		void ApplyTransform(const std::string& bindingPoint, const Float4x4& transform);

		std::vector<std::pair<std::string, std::string>> BuildSkeletonInterface() const;

		std::vector<::Assets::ICompileOperation::SerializedArtifact> SerializeToChunks(
			const std::string& name,
			const NascentSkeleton& embeddedSkeleton,
			const NativeVBSettings& nativeSettings) const;

	private:
		std::vector<std::pair<Indexor,GeometryBlock>>		_geoBlocks;
		std::vector<std::pair<Indexor,SkinControllerBlock>>	_skinBlocks;
		std::vector<std::pair<Indexor,Command>>				_commands;
	};

	class ModelTransMachineOptimizer : public ITransformationMachineOptimizer
    {
    public:
        bool CanMergeIntoOutputMatrix(unsigned outputMatrixIndex) const;
        void MergeIntoOutputMatrix(unsigned outputMatrixIndex, const Float4x4& transform);
		IteratorRange<const Float4x4*> GetMergedOutputMatrices() const { return MakeIteratorRange(_mergedTransforms); }

        ModelTransMachineOptimizer(
			const NascentModel& model,
			IteratorRange<const std::pair<std::string, std::string>*> bindingNameInterface);
        ModelTransMachineOptimizer();
        ~ModelTransMachineOptimizer();
    protected:
        std::vector<bool>			_canMergeIntoTransform;
        std::vector<Float4x4>		_mergedTransforms;
		std::vector<std::pair<std::string, std::string>>	_bindingNameInterface;
    };

	void OptimizeSkeleton(NascentSkeleton& embeddedSkeleton, NascentModel& model);
	

}}}
