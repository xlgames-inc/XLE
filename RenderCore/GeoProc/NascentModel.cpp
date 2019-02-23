// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentModel.h"
#include "NascentAnimController.h"
#include "GeometryAlgorithm.h"
#include "NascentObjectsSerialize.h"
#include "NascentGeometryObjects.h"
#include "NascentCommandStream.h"
#include "MeshDatabase.h"
#include "../Assets/AssetUtils.h"
#include "../../Core/Exceptions.h"

namespace RenderCore { namespace Assets { namespace GeoProc
{
	const NativeVBSettings NativeSettings = { true };       // use 16 bit floats

	auto NascentModel::FindGeometryBlock(NascentObjectGuid id) -> GeometryBlock*
	{
		for (auto&g:_geoBlocks)
			if (g.first._srcObject == id)
				return &g.second;
		return nullptr;
	}

	auto NascentModel::FindSkinControllerBlock(NascentObjectGuid id) -> SkinControllerBlock*
	{
		for (auto&g:_skinBlocks)
			if (g.first._srcObject == id)
				return &g.second;
		return nullptr;
	}

	auto NascentModel::FindCommand(NascentObjectGuid id) -> Command* 
	{
		for (auto&g:_commands)
			if (g.first._srcObject == id)
				return &g.second;
		return nullptr;
	}

	void NascentModel::Add(NascentObjectGuid id, const std::string& name, GeometryBlock&& object)
	{
		if (FindGeometryBlock(id))
			Throw(std::runtime_error("Attempting to register a GeometryBlock for a id that is already in use"));
		_geoBlocks.push_back(std::make_pair(Indexor{id, name}, std::move(object)));
	}

	void NascentModel::Add(NascentObjectGuid id, const std::string& name, SkinControllerBlock&& object)
	{
		if (FindSkinControllerBlock(id))
			Throw(std::runtime_error("Attempting to register a SkinControllerBlock for a id that is already in use"));
		_skinBlocks.push_back(std::make_pair(Indexor{id, name}, std::move(object)));
	}

	void NascentModel::Add(NascentObjectGuid id, const std::string& name, Command&& object)
	{
		if (FindCommand(id))
			Throw(std::runtime_error("Attempting to register a Command for a id that is already in use"));
		_commands.push_back(std::make_pair(Indexor{id, name}, std::move(object)));
	}

	void NascentModel::ApplyTransform(const std::string& bindingPoint, const Float4x4& transform)
	{
		for (auto&cmd:_commands) {
			if (cmd.second._localToModel == bindingPoint) {
				auto* geo = FindGeometryBlock(cmd.second._geometryBlock);
				assert(geo);
				Transform(*geo->_mesh, transform);
				cmd.second._localToModel = "Unnamed0";
			}
		}
	}

	std::unordered_map<std::string, std::vector<std::string>> NascentModel::BuildSkeletonInterface()
	{
		std::vector<std::string> bindingsForRootStructure;
		for (const auto&cmd:_commands) {
			auto i = std::find(bindingsForRootStructure.begin(), bindingsForRootStructure.end(), cmd.second._localToModel);
			if (i == bindingsForRootStructure.end())
				bindingsForRootStructure.push_back(cmd.second._localToModel);
		}

		std::unordered_map<std::string, std::vector<std::string>> result;
		result[std::string{}] = std::move(bindingsForRootStructure);

		for (const auto&controller:_skinBlocks) {
			auto& bindings = result[controller.second._skeleton];
			for (const auto&joint:controller.second._controller->_jointNames) {
				auto i = std::find(bindings.begin(), bindings.end(), joint);
				if (i == bindings.end())
					bindings.push_back(joint);
			}
		}

		return result;
	}

	static NascentRawGeometry CompleteInstantiation(const NascentModel::GeometryBlock& geoBlock)
	{
		const bool generateMissingTangentsAndNormals = true;
        if (constant_expression<generateMissingTangentsAndNormals>::result()) {
			auto indexCount = geoBlock._indices.size() * 8 / BitsPerPixel(geoBlock._indexFormat);
            GenerateNormalsAndTangents(
                *geoBlock._mesh, 0,
				1e-3f,
                geoBlock._indices.data(), indexCount, geoBlock._indexFormat);
        }

            // If we have normals, tangents & bitangents... then we can remove one of them
            // (it will be implied by the remaining two). We can choose to remove the 
            // normal or the bitangent... Lets' remove the binormal, because it makes it 
            // easier to do low quality rendering with normal maps turned off.
        const bool removeRedundantBitangents = true;
        if (constant_expression<removeRedundantBitangents>::result())
            RemoveRedundantBitangents(*geoBlock._mesh);

        NativeVBLayout vbLayout = BuildDefaultLayout(*geoBlock._mesh, NativeSettings);
        auto nativeVB = geoBlock._mesh->BuildNativeVertexBuffer(vbLayout);

		std::vector<uint64_t> matBindingSymbols;
		std::vector<DrawCallDesc> drawCalls;
		for (const auto&d:geoBlock._drawCalls) {
			drawCalls.push_back(DrawCallDesc{d._firstIndex, d._indexCount, 0, (unsigned)matBindingSymbols.size(), d._topology});
			matBindingSymbols.push_back(matBindingSymbols.size());
		}

        return NascentRawGeometry {
            nativeVB, geoBlock._indices,
            RenderCore::Assets::CreateGeoInputAssembly(vbLayout._elements, (unsigned)vbLayout._vertexStride),
            geoBlock._indexFormat,
			drawCalls, matBindingSymbols };
	}

	std::vector<::Assets::ICompileOperation::OperationResult> NascentModel::SerializeToChunks(const std::string& name, const NascentSkeleton& embeddedSkeleton)
	{
		NascentGeometryObjects geoObjects;
		NascentModelCommandStream cmdStream;

		for (const auto&cmd:_commands) {
			auto* geoBlock = FindGeometryBlock(cmd.second._geometryBlock);
			if (!geoBlock) continue;

			std::vector<uint64_t> materialGuid;
			materialGuid.reserve(cmd.second._materialBindingSymbols.size());
			for (const auto&mat:cmd.second._materialBindingSymbols)
				materialGuid.push_back(Hash64(mat));

			auto* skinController = FindSkinControllerBlock(cmd.second._skinControllerBlock);
			if (!skinController) {
				auto i = std::find_if(geoObjects._rawGeos.begin(), geoObjects._rawGeos.end(),
					[&cmd](const std::pair<NascentObjectGuid, NascentRawGeometry>& p) { return p.first == cmd.second._geometryBlock; });
				if (i == geoObjects._rawGeos.end()) {
					auto rawGeo = CompleteInstantiation(*geoBlock);
					geoObjects._rawGeos.emplace_back(
						std::make_pair(cmd.second._geometryBlock, std::move(rawGeo)));
					i = geoObjects._rawGeos.end()-1;
				}

				cmdStream.Add(
					NascentModelCommandStream::GeometryInstance {
						(unsigned)std::distance(geoObjects._rawGeos.begin(), i),
						cmdStream.RegisterInputInterfaceMarker(cmd.second._localToModel),
						std::move(materialGuid),
						cmd.second._levelOfDetail
					});
			} else {
				auto i = std::find_if(geoObjects._skinnedGeos.begin(), geoObjects._skinnedGeos.end(),
					[&cmd](const std::pair<NascentObjectGuid, NascentBoundSkinnedGeometry>& p) { return p.first == cmd.second._skinControllerBlock; });
				if (i == geoObjects._skinnedGeos.end()) {
					auto rawGeo = CompleteInstantiation(*geoBlock);
					auto boundController = BindController(
						rawGeo,
						*skinController->_controller,
						{}, "");
					geoObjects._skinnedGeos.emplace_back(
						std::make_pair(cmd.second._skinControllerBlock, std::move(boundController)));
					i = geoObjects._skinnedGeos.end()-1;
				}

				cmdStream.Add(
					NascentModelCommandStream::SkinControllerInstance {
						(unsigned)std::distance(geoObjects._skinnedGeos.begin(), i),
						cmdStream.RegisterInputInterfaceMarker(cmd.second._localToModel),
						std::move(materialGuid),
						cmd.second._levelOfDetail
					});
			}
		}

		return SerializeSkinToChunks(name, geoObjects, cmdStream, embeddedSkeleton);
	}

}}}
