// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MemoryUtils.h"

namespace Utility
{
    void PODAlignedDeletor::operator()(void* ptr) 
    { 
        XlMemAlignFree(ptr); 
    }

    uint64 ConstHash64FromString(const char* begin, const char* end)
    {
        // We want to match the results that we would
        // get from the compiler-time ConstHash64
        // Separate the string into 4 char types.
        // First char is in most significant bits,
        // but incomplete groups of 4 are aligned to
        // the least significant bits.

        uint64 result = ConstHash64<0>::Seed;
        const char* i = begin;
        while (*i) {
            const auto* s = i;
            unsigned newValue = 0;
            while (i != end && (i-s) < 4) {
                newValue = (newValue << 8) | (*i);
                ++i;
            }

            result = (newValue == 0) ? result : (((result << 21ull) | (result >> 43ull)) ^ uint64(newValue));
        }

        return result;
    }
}


