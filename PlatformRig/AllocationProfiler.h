// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

namespace PlatformRig
{

    class AccumulatedAllocations
    {
    public:
        AccumulatedAllocations();
        ~AccumulatedAllocations();

        class Snapshot
        {
        public:
            unsigned _allocationCount;
            unsigned _freeCount;
            unsigned _reallocCount;
            size_t _allocationsSize;
            size_t _freesSize;
            size_t _reallocsSize;
            Snapshot() : _allocationCount(0), _freeCount(0), _reallocCount(0), _allocationsSize(0), _freesSize(0), _reallocsSize(0) {}
        };

        Snapshot        GetAndClear()
        {
            auto result = _accumulating;
            _accumulating = Snapshot();
            return result;
        }

        static AccumulatedAllocations* GetInstance() { return _instance; }

        class CurrentHeapMetrics
        {
        public:
            size_t _usage;
            size_t _blockCount;
        };
        static CurrentHeapMetrics GetCurrentHeapMetrics();

        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
        Snapshot _accumulating;

    protected:
        static AccumulatedAllocations* _instance;
    };

}