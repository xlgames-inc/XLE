// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NascentChunk.h"
#include "BlockSerializer.h"
#include "IFileSystem.h"
#include "../ConsoleRig/GlobalServices.h"

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

	void BuildChunkFile(
        ::Assets::IFileInterface& file,
        IteratorRange<::Assets::NascentChunk*> chunks,
        const ConsoleRig::LibVersionDesc& versionInfo,
        std::function<bool(const ::Assets::NascentChunk&)> predicate)
    {
        unsigned chunksForMainFile = 0;
		for (const auto& c:chunks)
            if (!predicate || predicate(c))
                ++chunksForMainFile;

        using namespace Serialization::ChunkFile;
        auto header = MakeChunkFileHeader(
            chunksForMainFile, 
            versionInfo._versionString, versionInfo._buildDateString);
        file.Write(&header, sizeof(header), 1);

        unsigned trackingOffset = unsigned(file.TellP() + sizeof(ChunkHeader) * chunksForMainFile);
        for (const auto& c:chunks)
            if (!predicate || predicate(c)) {
                auto hdr = c._hdr;
                hdr._fileOffset = trackingOffset;
				hdr._size = (Serialization::ChunkFile::SizeType)c._data->size();
                file.Write(&hdr, sizeof(hdr), 1);
                trackingOffset += hdr._size;
            }

        for (const auto& c:chunks)
            if (!predicate || predicate(c))
                file.Write(AsPointer(c._data->begin()), c._data->size(), 1);
    }

}

