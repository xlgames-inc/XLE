// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MaterialScaffold.h"
#include "ModelImmutableData.h"     // for MaterialImmutableData
#include "../Techniques/TechniqueMaterial.h"
#include "../../Assets/ChunkFileContainer.h"
#include "../../Assets/BlockSerializer.h"

namespace RenderCore { namespace Assets
{

///////////////////////////////////////////////////////////////////////////////////////////////////

	const MaterialImmutableData& MaterialScaffold::ImmutableData() const
	{
		return *(const MaterialImmutableData*)Serialization::Block_GetFirstObject(_rawMemoryBlock.get());
	}

	const Techniques::Material* MaterialScaffold::GetMaterial(MaterialGuid guid) const
	{
		const auto& data = ImmutableData();
		auto i = std::lower_bound(data._materials.begin(), data._materials.end(), guid, CompareFirst<MaterialGuid, Techniques::Material>());
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
		return nullptr;
	}

	static const ::Assets::AssetChunkRequest MaterialScaffoldChunkRequests[]
	{
		::Assets::AssetChunkRequest{
			"Scaffold", ChunkType_ResolvedMat, ResolvedMat_ExpectedVersion,
			::Assets::AssetChunkRequest::DataType::BlockSerializer
		}
	};

	MaterialScaffold::MaterialScaffold(const ::Assets::ChunkFileContainer& chunkFile)
		: _depVal(chunkFile.GetDependencyValidation())
	{
		auto chunks = chunkFile.ResolveRequests(MakeIteratorRange(MaterialScaffoldChunkRequests));
		assert(chunks.size() == 1);
		_rawMemoryBlock = std::move(chunks[0]._buffer);
	}

	MaterialScaffold::MaterialScaffold(MaterialScaffold&& moveFrom) never_throws
		: _rawMemoryBlock(std::move(moveFrom._rawMemoryBlock))
		, _depVal(moveFrom._depVal)
	{}

	MaterialScaffold& MaterialScaffold::operator=(MaterialScaffold&& moveFrom) never_throws
	{
		assert(!_rawMemoryBlock);		// (not thread safe to use this operator after we've hit "ready" status
		_rawMemoryBlock = std::move(moveFrom._rawMemoryBlock);
		_depVal = moveFrom._depVal;
		return *this;
	}

	MaterialScaffold::~MaterialScaffold()
	{
		ImmutableData().~MaterialImmutableData();
	}


	MaterialGuid MakeMaterialGuid(StringSection<utf8> name)
	{
		//  If the material name is just a number, then we will use that
		//  as the guid. Otherwise we hash the name.
		const char* parseEnd = nullptr;
		uint64 hashId = XlAtoI64((const char*)name.begin(), &parseEnd, 16);
		if (!parseEnd || parseEnd != (const char*)name.end()) { hashId = Hash64(name.begin(), name.end()); }
		return hashId;
	}

}}

