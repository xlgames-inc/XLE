// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MemoryUtils.h"
#include "PtrUtils.h"
#include "StringUtils.h"
#include "../Core/SelectConfiguration.h"
#include "../Foreign/Hash/MurmurHash2.h"
#include "../Foreign/Hash/MurmurHash3.h"
#include <assert.h>

namespace Utility
{
    uint64 Hash64(const void* begin, const void* end, uint64 seed)
    {
                //
                //      Use MurmurHash2 to generate a 64 bit value.
                //      (MurmurHash3 only generates 32 bit and 128 bit values)
                //      
                //      Note that there may be some compatibility problems, because
                //      murmur hash doesn't generate the same value for big and little
                //      endian processors. It's not clear if the quality of the 
                //      hashing is as good on big endian processors.
                //
                //      "MurmurHash64A" is optimised for 64 bit processors; 
                //      "MurmurHash64B" is for 32 bit processors.
                //
        const bool crossPlatformHash = true;
        auto sizeInBytes = size_t(end)-size_t(begin);

        #if PLATFORMOS_TARGET == PLATFORMOS_ANDROID
            // We must ensure that we're only performing aligned reads
            static uint64_t fixedBuffer[256/sizeof(uint64_t)] __attribute__((aligned(16)));
            std::unique_ptr<uint64_t[], PODAlignedDeletor> variableBuffer;
            if (size_t(begin) & 0x7) {
                if (sizeInBytes <= sizeof(fixedBuffer)) {
                    std::memcpy(fixedBuffer, begin, sizeInBytes);
                    begin = fixedBuffer;
                } else {
                    variableBuffer.reset((uint64_t*)XlMemAlign(sizeInBytes, sizeof(uint64_t)));
                    std::memcpy(variableBuffer.get(), begin, sizeInBytes);
                    begin = variableBuffer.get();
                }
            }
            assert((size_t(begin) & 0x7) == 0);
        #endif

        if (TARGET_64BIT || crossPlatformHash) {
            return MurmurHash64A(begin, int(sizeInBytes), seed);
        } else {
            return MurmurHash64B(begin, int(sizeInBytes), seed);
        }
    }

    uint64 Hash64(const char str[], uint64 seed)
    {
        return Hash64(str, XlStringEnd(str), seed);
    }

    uint64 Hash64(const std::string& str, uint64 seed)
    {
        return Hash64(AsPointer(str.begin()), AsPointer(str.end()), seed);
    }

	uint64 Hash64(StringSection<char> str, uint64 seed)
	{
		return Hash64(str.begin(), str.end(), seed);
	}

    uint32 Hash32(const void* begin, const void* end, uint32 seed)
    {
        uint32 temp;
        MurmurHash3_x86_32(begin, int(size_t(end)-size_t(begin)), seed, &temp);
        return temp;
    }

    uint32 IntegerHash32(uint32 key)
    {
            // taken from https://gist.github.com/badboy/6267743
            // See also http://burtleburtle.net/bob/hash/integer.html
        key = (key+0x7ed55d16) + (key<<12);
        key = (key^0xc761c23c) ^ (key>>19);
        key = (key+0x165667b1) + (key<<5);
        key = (key+0xd3a2646c) ^ (key<<9);
        key = (key+0xfd7046c5) + (key<<3);
        key = (key^0xb55a4f09) ^ (key>>16);
        return key;
    }
    
    uint64 IntegerHash64(uint64 key)
    {
            // taken from https://gist.github.com/badboy/6267743
        key = (~key) + (key << 21); // key = (key << 21) - key - 1;
        key = key ^ (key >> 24);
        key = (key + (key << 3)) + (key << 8); // key * 265
        key = key ^ (key >> 14);
        key = (key + (key << 2)) + (key << 4); // key * 21
        key = key ^ (key >> 28);
        key = key + (key << 31);
        return key;
    }
}

