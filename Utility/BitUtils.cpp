// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BitUtils.h"
#include "BitHeap.h"
#include <assert.h>

namespace Utility
{
    uint32  BitHeap::Allocate()
    {
        ScopedLock(_lock);
        for (auto i=_heap.begin(); i!=_heap.end(); ++i) {
            if (*i != 0) {
                auto bitIndex = LeastSignificantBitSet(*i);
                (*i) &= ~(1ull<<uint64(bitIndex));
                return ((uint32)std::distance(_heap.begin(), i))*64 + bitIndex;
            }
        }

        _heap.push_back(~uint64(1));
        return uint32(_heap.size()-1)*64;
    }

    uint32  BitHeap::AllocateNoExpand()
    {
        ScopedLock(_lock);
        for (auto i=_heap.begin(); i!=_heap.end(); ++i) {
            if (*i != 0) {
                auto bitIndex = LeastSignificantBitSet(*i);
                (*i) &= ~(1ull<<uint64(bitIndex));
                return ((uint32)std::distance(_heap.begin(), i))*64 + bitIndex;
            }
        }

        return ~uint32(0x0);
    }

    void    BitHeap::Deallocate(uint32 value)
    {
        uint32 bitIndex = value&(64-1);
        uint32 arrayIndex = value>>6;
        ScopedLock(_lock);
        if (arrayIndex < _heap.size()) {
            assert((_heap[arrayIndex] & (1ull<<uint64(bitIndex))) == 0);
            _heap[arrayIndex] |= 1ull<<uint64(bitIndex);
        }
    }

    bool    BitHeap::IsAllocated(uint32 value) const
    {
        uint32 bitIndex = value&(64-1);
        uint32 arrayIndex = value>>6;
        ScopedLock(_lock);
        if (arrayIndex < _heap.size()) {
            return (_heap[arrayIndex] & (1ull<<uint64(bitIndex))) == 0;
        }
        return false;
    }
    
    void    BitHeap::Reserve(uint32 count)
    {
        unsigned elementCount = (count + 64 - 1) / 64;
        if (_heap.size() < elementCount) {
            _heap.resize(elementCount, ~uint64(0x0));
        }
    }

    BitHeap::BitHeap(unsigned slotCount)
    {
        unsigned longLongCount = (slotCount + 64 - 1) / 64;
        _heap.resize(longLongCount, ~uint64(0x0));
        if ((slotCount % 64) != 0) {
                // prevent top bits from being allocated
            _heap[longLongCount-1] = ((slotCount % 64) - 1);
        }
    }

    BitHeap::BitHeap(BitHeap&& moveFrom)
    {
        _heap = std::move(moveFrom._heap);
    }

    BitHeap& BitHeap::operator=(BitHeap&& moveFrom)
    {
        _heap = std::move(moveFrom._heap);
        return *this;
    }

    BitHeap::~BitHeap()
    {}

}

