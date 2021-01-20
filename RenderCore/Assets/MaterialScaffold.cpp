// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialScaffold.h"
#include "ShaderPatchCollection.h"
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
		return *(const MaterialImmutableData*)::Assets::Block_GetFirstObject(_rawMemoryBlock.get());
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

	const ShaderPatchCollection*	MaterialScaffold::GetShaderPatchCollection(uint64_t hash) const
	{
		auto i = std::lower_bound(_patchCollections.begin(), _patchCollections.end(), hash);
		if (i != _patchCollections.end() && i->GetHash() == hash)
			return AsPointer(i);
		return nullptr;
	}

	const ::Assets::AssetChunkRequest MaterialScaffold::ChunkRequests[]
	{
		::Assets::AssetChunkRequest{
			"Scaffold", ChunkType_ResolvedMat, ResolvedMat_ExpectedVersion,
			::Assets::AssetChunkRequest::DataType::BlockSerializer
		},
		::Assets::AssetChunkRequest{
			"PatchCollections", ChunkType_PatchCollections, ResolvedMat_ExpectedVersion,
			::Assets::AssetChunkRequest::DataType::Raw
		}
	};

	MaterialScaffold::MaterialScaffold(IteratorRange<::Assets::AssetChunkResult*> chunks, const ::Assets::DepValPtr& depVal)
	: _depVal(depVal)
	{
		assert(chunks.size() == 2);
		_rawMemoryBlock = std::move(chunks[0]._buffer);

		InputStreamFormatter<utf8> formatter(
			StringSection<utf8>{(const utf8*)chunks[1]._buffer.get(), (const utf8*)PtrAdd(chunks[1]._buffer.get(), chunks[1]._bufferSize)});
		_patchCollections = DeserializeShaderPatchCollectionSet(formatter, {}, depVal);
	}

	MaterialScaffold::MaterialScaffold(MaterialScaffold&& moveFrom) never_throws
	: _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
	, _depVal(std::move(moveFrom._depVal))
	, _patchCollections(std::move(moveFrom._patchCollections))
	{}

	MaterialScaffold& MaterialScaffold::operator=(MaterialScaffold&& moveFrom) never_throws
	{
		ImmutableData().~MaterialImmutableData();
		_rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
		_depVal = std::move(moveFrom._depVal);
		_patchCollections = std::move(moveFrom._patchCollections);
		return *this;
	}

	MaterialScaffold::~MaterialScaffold()
	{
		ImmutableData().~MaterialImmutableData();
	}

	

}}

