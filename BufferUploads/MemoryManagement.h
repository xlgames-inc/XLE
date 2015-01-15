// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IBufferUploads.h"
#include "../Utility/Threading/Mutex.h"
#include "../Utility/StringUtils.h"
#include "../Utility/HeapUtils.h"

namespace BufferUploads
{
            //////   R E F E R E N C E   C O U N T I N G   L A Y E R   //////

    class ReferenceCountingLayer : public MarkerHeap
    {
    public:
        std::pair<signed,signed> AddRef(unsigned start, unsigned size, const char name[] = NULL);
        std::pair<signed,signed> Release(unsigned start, unsigned size);

        size_t      Validate();
        unsigned    CalculatedReferencedSpace() const;
        unsigned    GetEntryCount() const                               { return (unsigned)_entries.size(); }
        std::pair<unsigned,unsigned> GetEntry(unsigned index) const     { const Entry& entry = _entries[index]; return std::make_pair(ToExternalSize(entry._start), ToExternalSize(entry._end-entry._start)); }
        #if defined(XL_DEBUG)
            std::string      GetEntryName(unsigned index) const         { const Entry& entry = _entries[index]; return entry._name; }
        #endif
        bool        ValidateBlock(unsigned start, unsigned size) const;

        void        PerformDefrag(const std::vector<DefragStep>& defrag);

        ReferenceCountingLayer(size_t size);
        ReferenceCountingLayer(const ReferenceCountingLayer& cloneFrom);
    protected:
        class Entry
        {
        public:
            Marker _start, _end;    // _end is stl style -- one past the end of the allocation
            signed _refCount;
            #if defined(XL_DEBUG)
                std::string _name;
            #endif
        };
        std::vector<Entry> _entries;
        mutable Threading::Mutex _lock;

        struct CompareStart
        {
            bool operator()(const Entry&lhs, Marker value)      { return lhs._start < value; }
            bool operator()(Marker value, const Entry&rhs)      { return value < rhs._start; }
            bool operator()(const Entry&lhs, const Entry&rhs)   { return lhs._start < rhs._start; }
        };
    };

}
