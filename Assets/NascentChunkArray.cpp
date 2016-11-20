// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentChunkArray.h"
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

	std::vector<uint8> AsVector(const Serialization::NascentBlockSerializer& serializer)
	{
		auto block = serializer.AsMemoryBlock();
		size_t size = Serialization::Block_GetSize(block.get());
		return std::vector<uint8>(block.get(), PtrAdd(block.get(), size));
	}
}

