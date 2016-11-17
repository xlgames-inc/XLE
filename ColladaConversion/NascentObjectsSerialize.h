// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/ChunkFile.h"
#include <vector>
#include <memory>

namespace Serialization { class NascentBlockSerializer; }

namespace RenderCore { namespace ColladaConversion
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

	class NascentGeometryObjects;
	class NascentModelCommandStream;
	class NascentSkeleton;

	NascentChunkArray SerializeSkinToChunks(
		const char name[], 
		NascentGeometryObjects& geoObjects, 
		NascentModelCommandStream& cmdStream, 
		NascentSkeleton& skeleton);

	NascentChunkArray SerializeSkeletonToChunks(
		const char name[], 
		NascentSkeleton& skeleton);

	NascentChunkArray MakeNascentChunkArray(
		const std::initializer_list<NascentChunk>& inits);

	std::vector<uint8> AsVector(const Serialization::NascentBlockSerializer& serializer);
}}
