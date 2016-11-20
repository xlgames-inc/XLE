// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ChunkFile.h"
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
		std::vector<uint8> _data;

		NascentChunk(
			const Serialization::ChunkFile::ChunkHeader& hdr, 
			std::vector<uint8>&& data)
			: _hdr(hdr), _data(std::forward<std::vector<uint8>>(data)) {}
		NascentChunk(NascentChunk&& moveFrom) never_throws
			: _hdr(moveFrom._hdr)
			, _data(std::move(moveFrom._data))
		{}
		NascentChunk& operator=(NascentChunk&& moveFrom) never_throws
		{
			_hdr = moveFrom._hdr;
			_data = std::move(moveFrom._data);
			return *this;
		}
		NascentChunk(const NascentChunk& copyFrom) : _hdr(copyFrom._hdr), _data(copyFrom._data) {}
		NascentChunk& operator=(const NascentChunk& copyFrom)
		{
			_hdr = copyFrom._hdr;
			_data = copyFrom._data;
			return *this;
		}
		NascentChunk() {}
	};

	using NascentChunkArray = std::shared_ptr<std::vector<NascentChunk>>;
	NascentChunkArray MakeNascentChunkArray(const std::initializer_list<NascentChunk>& inits);
	std::vector<uint8> AsVector(const Serialization::NascentBlockSerializer& serializer);

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
}
