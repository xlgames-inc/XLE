// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialScaffold.h"
#include "../../Assets/ChunkFileContainer.h"
#include "../../Assets/BlockSerializer.h"
#include "../../Assets/Assets.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Utility/Streams/PathUtils.h"

namespace RenderCore { namespace Assets
{
	class MaterialImmutableData
    {
    public:
        SerializableVector<std::pair<MaterialGuid, MaterialScaffold::Material>> _materials;
        SerializableVector<std::pair<MaterialGuid, SerializableVector<char>>> _materialNames;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

	const MaterialImmutableData& MaterialScaffold::ImmutableData() const
	{
		return *(const MaterialImmutableData*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
	}

	auto MaterialScaffold::GetMaterial(MaterialGuid guid) const -> const Material*
	{
		const auto& data = ImmutableData();
		auto i = std::lower_bound(data._materials.begin(), data._materials.end(), guid, CompareFirst<MaterialGuid, Material>());
		if (i != data._materials.end() && i->first == guid)
			return &i->second;
		return nullptr;
	}

	StringSection<> MaterialScaffold::GetMaterialName(MaterialGuid guid) const
	{
		const auto& data = ImmutableData();
		auto i = std::lower_bound(data._materialNames.begin(), data._materialNames.end(), guid, CompareFirst<MaterialGuid, SerializableVector<char>>());
		if (i != data._materialNames.end() && i->first == guid)
			return MakeStringSection(i->second.begin(), i->second.end());
		return {};
	}

	const ::Assets::AssetChunkRequest MaterialScaffold::ChunkRequests[]
	{
		::Assets::AssetChunkRequest{
			"Scaffold", ChunkType_ResolvedMat, ResolvedMat_ExpectedVersion,
			::Assets::AssetChunkRequest::DataType::BlockSerializer
		}
	};

	MaterialScaffold::MaterialScaffold(IteratorRange<::Assets::AssetChunkResult*> chunks, const ::Assets::DepValPtr& depVal)
	: _depVal(depVal)
	{
		assert(chunks.size() == 1);
		_rawMemoryBlock = std::move(chunks[0]._buffer);
	}

	MaterialScaffold::MaterialScaffold(MaterialScaffold&& moveFrom) never_throws
	: _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
	, _depVal(moveFrom._depVal)
	{}

	MaterialScaffold& MaterialScaffold::operator=(MaterialScaffold&& moveFrom) never_throws
	{
		ImmutableData().~MaterialImmutableData();
		_rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
		_depVal = moveFrom._depVal;
		return *this;
	}

	MaterialScaffold::~MaterialScaffold()
	{
		ImmutableData().~MaterialImmutableData();
	}


	MaterialScaffoldMaterial::MaterialScaffoldMaterial() { _techniqueConfig[0] = '\0'; }

	MaterialScaffoldMaterial::MaterialScaffoldMaterial(MaterialScaffoldMaterial&& moveFrom) never_throws
	: _bindings(std::move(moveFrom._bindings))
	, _matParams(std::move(moveFrom._matParams))
	, _stateSet(moveFrom._stateSet)
	, _constants(std::move(moveFrom._constants))
	{
		XlCopyString(_techniqueConfig, moveFrom._techniqueConfig);
	}

	MaterialScaffoldMaterial& MaterialScaffoldMaterial::operator=(MaterialScaffoldMaterial&& moveFrom) never_throws
	{
		_bindings = std::move(moveFrom._bindings);
		_matParams = std::move(moveFrom._matParams);
		_stateSet = moveFrom._stateSet;
		_constants = std::move(moveFrom._constants);
		XlCopyString(_techniqueConfig, moveFrom._techniqueConfig);
		return *this;
	}

	

}}

