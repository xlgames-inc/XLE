// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "ChunkFile.h"
#include "BlockSerializer.h"
#include "../Utility/IteratorUtils.h"
#include "../Core/Prefix.h"
#include <vector>
#include <memory>

namespace Serialization { class NascentBlockSerializer; }

namespace Assets
{
	class NascentChunk
	{
	public:
		Serialization::ChunkFile::ChunkHeader _hdr;
		::Assets::Blob _data;
	};

	using NascentChunkArray = std::shared_ptr<std::vector<NascentChunk>>;
	NascentChunkArray MakeNascentChunkArray(const std::initializer_list<NascentChunk>& inits);

	::Assets::Blob AsBlob(const Serialization::NascentBlockSerializer& serializer);
	::Assets::Blob AsBlob(IteratorRange<const void*>);

	template<typename Char>
		static ::Assets::Blob AsBlob(std::basic_stringstream<Char>& stream)
	{
		auto str = stream.str();
		return AsBlob(MakeIteratorRange(AsPointer(str.begin()), AsPointer(str.end())));
	}

	template<typename Type>
		static ::Assets::Blob SerializeToBlob(const Type& obj)
	{
		Serialization::NascentBlockSerializer serializer;
		::Serialize(serializer, obj);
		return AsBlob(serializer);
	}
}
