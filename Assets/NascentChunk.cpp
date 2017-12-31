// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentChunk.h"
#include "BlockSerializer.h"

namespace Assets
{
	static void DestroyChunkArray(const void* chunkArray) { delete (std::vector<NascentChunk>*)chunkArray; }

	NascentChunkArray MakeNascentChunkArray(
		const std::initializer_list<NascentChunk>& inits)
	{
		return NascentChunkArray(
			new std::vector<NascentChunk>(inits),
			&DestroyChunkArray);
	}

	::Assets::Blob AsBlob(IteratorRange<const void*> copyFrom)
	{
		return std::make_shared<std::vector<uint8_t>>((const uint8_t*)copyFrom.begin(), (const uint8_t*)copyFrom.end());
	}

	::Assets::Blob AsBlob(const Serialization::NascentBlockSerializer& serializer)
	{
		auto block = serializer.AsMemoryBlock();
		size_t size = Serialization::Block_GetSize(block.get());
		return AsBlob(MakeIteratorRange(block.get(), PtrAdd(block.get(), size)));
	}
}

