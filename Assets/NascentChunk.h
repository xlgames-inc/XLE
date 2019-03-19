// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "AssetsCore.h"
#include "ChunkFile.h"
#include "BlockSerializer.h"
#include "ICompileOperation.h"
#include "../ConsoleRig/AttachableLibrary.h"	// (for LibVersionDesc)
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
		Blob _data;
	};

	using NascentChunkArray = std::shared_ptr<std::vector<NascentChunk>>;
	NascentChunkArray MakeNascentChunkArray(const std::initializer_list<NascentChunk>& inits);

	Blob AsBlob(const Serialization::NascentBlockSerializer& serializer);
	Blob AsBlob(IteratorRange<const void*>);

	template<typename Char>
		static Blob AsBlob(std::basic_stringstream<Char>& stream)
	{
		auto str = stream.str();
		return AsBlob(MakeIteratorRange(AsPointer(str.begin()), AsPointer(str.end())));
	}

	template<typename Type>
		static Blob SerializeToBlob(const Type& obj)
	{
		Serialization::NascentBlockSerializer serializer;
		::Serialize(serializer, obj);
		return AsBlob(serializer);
	}

	void BuildChunkFile(
		IFileInterface& file,
		IteratorRange<const NascentChunk*> chunks,
		const ConsoleRig::LibVersionDesc& versionInfo,
		std::function<bool(const NascentChunk&)> predicate = {});

	void BuildChunkFile(
		IFileInterface& file,
		IteratorRange<const ICompileOperation::OperationResult*> chunks,
		const ConsoleRig::LibVersionDesc& versionInfo,
		std::function<bool(const ICompileOperation::OperationResult&)> predicate = {});
}
