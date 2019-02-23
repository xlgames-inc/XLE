// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "NascentObjectGuid.h"
#include "../Format.h"
#include "../Types.h"
#include "../../Assets/ICompileOperation.h"
#include "../../Math/Matrix.h"
#include "../../Utility/UTFUtils.h"
#include "../../Utility/IteratorUtils.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace RenderCore { namespace Assets { class DrawCallDesc; }}

namespace RenderCore { namespace Assets { namespace GeoProc
{
	class MeshDatabase;
	class UnboundSkinController;
	class NascentSkeletonMachine;
	class NascentSkeleton;

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
			unsigned    _firstIndex, _indexCount;
			Topology	_topology;
		};
		
		class GeometryBlock
		{
		public:
			std::shared_ptr<MeshDatabase>	_mesh;
			std::vector<DrawCallDesc>		_drawCalls;
			std::vector<uint32_t>			_meshVertexIndexToSrcIndex;		// srcIndex = _meshVertexIndexToSrcIndex[meshDatabaseUnifiedVertexIndex]

			std::vector<uint8_t>			_indices;
			Format							_indexFormat;
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
			NascentObjectGuid			_geometryBlock;
			NascentObjectGuid			_skinControllerBlock;
			std::string					_localToModel;
			std::vector<std::string>	_materialBindingSymbols;
			unsigned					_levelOfDetail;
		};

		GeometryBlock* FindGeometryBlock(NascentObjectGuid id);
		SkinControllerBlock* FindSkinControllerBlock(NascentObjectGuid id);
		Command* FindCommand(NascentObjectGuid id);

		void Add(NascentObjectGuid id, const std::string& name, GeometryBlock&& object);
		void Add(NascentObjectGuid id, const std::string& name, SkinControllerBlock&& object);
		void Add(NascentObjectGuid id, const std::string& name, Command&& object);

		IteratorRange<const std::pair<Indexor,Command>*> GetCommands() const { return MakeIteratorRange(_commands); }

		void ApplyTransform(const std::string& bindingPoint, const Float4x4& transform);

		std::unordered_map<std::string, std::vector<std::string>> BuildSkeletonInterface();

		std::vector<::Assets::ICompileOperation::OperationResult> SerializeToChunks(
			const std::string& name,
			const NascentSkeleton& embeddedSkeleton);

	private:
		std::vector<std::pair<Indexor,GeometryBlock>>		_geoBlocks;
		std::vector<std::pair<Indexor,SkinControllerBlock>>	_skinBlocks;
		std::vector<std::pair<Indexor,Command>>				_commands;
	};

	/*
	class NascentSkeletonFile
	{
	public:
		class Skeleton
		{
		public:
			NascentObjectGuid _srcObject;
			std::basic_string<utf8> _name;
			std::shared_ptr<NascentSkeletonMachine> _machine;

			struct InterfaceElement
			{
				NascentObjectGuid _srcObject;
				std::basic_string<utf8> _name;
			};
			std::vector<InterfaceElement> _inputs;
			std::vector<InterfaceElement> _outputs;
		};

		std::vector<Skeleton> _skeletons;
	};
	*/

}}}
