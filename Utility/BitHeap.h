// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Threading/Mutex.h"
#include "../Core/Types.h"
#include <vector>

namespace Utility
{
    class BitHeap
    {
    public:
        uint32  Allocate();
        uint32  AllocateNoExpand();
        void    Deallocate(uint32 value);
        bool    IsAllocated(uint32 value) const;
        void    Reserve(uint32 count);

        BitHeap(unsigned slotCount = 8 * 64);
        BitHeap(BitHeap&& moveFrom);
        BitHeap& operator=(BitHeap&& moveFrom);
        ~BitHeap();
    private:
        std::vector<uint64>         _heap;
        mutable Threading::Mutex    _lock;

        BitHeap(const BitHeap& cloneFrom);
        BitHeap& operator=(const BitHeap& cloneFrom);
    };
}

using namespace Utility;
