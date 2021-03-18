// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MemoryManagement.h"
#include "../Core/Prefix.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/PtrUtils.h"
#include <algorithm>
#include <assert.h>

#pragma warning(disable:4245)

namespace BufferUploads
{

        /////////////////////////////////////////////////////////////////////////////////
            //////   R E F E R E N C E   C O U N T I N G   L A Y E R   //////
        /////////////////////////////////////////////////////////////////////////////////

    std::pair<signed,signed> ReferenceCountingLayer::AddRef(unsigned start, unsigned size, const char name[])
    {
        ScopedLock(_lock);
        Marker internalStart = ToInternalSize(start);
        Marker internalSize = ToInternalSize(AlignSize(size));

        if (_entries.empty()) {
            Entry newBlock;
            newBlock._start = internalStart;
            newBlock._end = internalStart+internalSize;
            newBlock._refCount = 1;
            DEBUG_ONLY(if (name) newBlock._name = name);
            _entries.insert(_entries.end(), newBlock);
            return std::make_pair(newBlock._refCount, newBlock._refCount);
        }

        std::vector<Entry>::iterator i = std::lower_bound(_entries.begin(), _entries.end(), internalStart, CompareStart());
        if (i != _entries.begin() && ((i-1)->_end > internalStart)) {
            --i;
        }

        Marker currentStart = internalStart;
        Marker internalEnd = internalStart+internalSize;
        signed refMin = INT_MAX, refMax = INT_MIN;
        for (;;++i) {
            if (i >= _entries.end() || currentStart < i->_start) {
                    //      this this is past the end of any other blocks -- add new a block
                Entry newBlock;
                newBlock._start = currentStart;
                newBlock._end = std::min(internalEnd, Marker((i<_entries.end())?i->_start:INT_MAX));
                newBlock._refCount = 1;
                DEBUG_ONLY(if (name) newBlock._name = name);
                assert(newBlock._start < newBlock._end);
                assert(newBlock._end != 0xbaad);
                bool end = i >= _entries.end() || internalEnd <= i->_start;
                i = _entries.insert(i, newBlock)+1;
                refMin = std::min(refMin, 1); refMax = std::max(refMax,1);
                if (end) {
                    break;  // it's the end
                }
                currentStart = i->_start;
            }

            if (i->_start == currentStart) {
                if (internalEnd >= i->_end) {
                        // we've hit a block identical to the one we're looking for. Just increase the ref count
                    signed newRefCount = ++i->_refCount;
                    refMin = std::min(refMin, newRefCount); refMax = std::max(refMax, newRefCount);
                    assert(i->_start < i->_end);
                    assert(i->_end != 0xbaad);
                    currentStart = i->_end;
                    if (name && name[0]) {
                        DEBUG_ONLY(i->_name = name);     // we have to take on the new name here. Sometimes we'll get a number of sub blocks inside of a super block. The last sub block will allocate the entirely remaining part of the super block. When this happens, rename to the sub block name.
                    }
                    if (internalEnd == i->_end) {
                        break;  // it's the end
                    }
                } else {
                        // split the block and add a new one in front
                    Entry newBlock;
                    newBlock._start = i->_start;
                    newBlock._end = internalEnd;
                    DEBUG_ONLY(if (name) newBlock._name = name);
                    signed newRefCount = newBlock._refCount = i->_refCount+1;
                    i->_start = internalEnd;
                    assert(newBlock._start < newBlock._end && i->_start < i->_end);
                    assert(i->_end != 0xbaad && newBlock._end != 0xbaad);
                    _entries.insert(i, newBlock);
                    refMin = std::min(refMin, newRefCount); refMax = std::max(refMax, newRefCount);
                    break;  // it's the end
                }
            } else {
                if (internalEnd < i->_end) {
                        //  This is a block that falls entirely within the old block. We need to create a new block, splitting the old one if necessary
                        // we need do split the end part of the old block off too, and then insert 2 blocks
                    Entry newBlock[2];
                    newBlock[0]._start = i->_start;
                    newBlock[0]._end = currentStart;
                    newBlock[0]._refCount = i->_refCount;
                    DEBUG_ONLY(newBlock[0]._name = i->_name);
                    newBlock[1]._start = currentStart;
                    newBlock[1]._end = internalEnd;
                    DEBUG_ONLY(if (name) newBlock[1]._name = name);
                    signed newRefCount = newBlock[1]._refCount = i->_refCount+1;
                    i->_start = internalEnd;
                    assert(newBlock[0]._start < newBlock[0]._end && newBlock[1]._start < newBlock[1]._end&& i->_start < i->_end);
                    assert(i->_end != 0xbaad && newBlock[0]._end != 0xbaad && newBlock[1]._end != 0xbaad);
                    _entries.insert(i, newBlock, &newBlock[2]);
                    refMin = std::min(refMin, newRefCount); refMax = std::max(refMax, newRefCount);
                    break;
                } else {
                    Marker iEnd = i->_end;
                    Entry newBlock;
                    newBlock._start = i->_start;
                    newBlock._end = currentStart;
                    newBlock._refCount = i->_refCount;
                    DEBUG_ONLY(newBlock._name.swap(i->_name));
                    i->_start = currentStart;
                    DEBUG_ONLY(if (name) i->_name = name);
                    signed newRefCount = ++i->_refCount;
                    assert(newBlock._start < newBlock._end && i->_start < i->_end);
                    assert(i->_end != 0xbaad && newBlock._end != 0xbaad);
                    i = _entries.insert(i, newBlock)+1;
                    refMin = std::min(refMin, newRefCount); refMax = std::max(refMax, newRefCount);

                    if (internalEnd == iEnd) {
                        break;
                    }

                    currentStart = iEnd;
                        // this block extends into the next area -- so continue around the loop again
                }
            }
        }

        return std::make_pair(refMin, refMax);
    }

    size_t ReferenceCountingLayer::Validate()
    {
        size_t result = 0;
        for (std::vector<Entry>::iterator i=_entries.begin(); i<_entries.end(); ++i) {
            assert(i->_start < i->_end);
            if ((i+1)<_entries.end()) {
                assert(i->_end <= (i+1)->_start);
            }
            assert(i->_start <= 0x800 && i->_end <= 0x800);
            result += i->_refCount*size_t(i->_end-i->_start);
        }
        return result;
    }

    unsigned ReferenceCountingLayer::CalculatedReferencedSpace() const
    {
        ScopedLock(_lock);
        unsigned result = 0;
        for (std::vector<Entry>::const_iterator i=_entries.begin(); i<_entries.end(); ++i) {
            result += ToExternalSize(i->_end-i->_start);
        }
        return result;
    }

    void ReferenceCountingLayer::PerformDefrag(const std::vector<DefragStep>& defrag)
    {
        ScopedLock(_lock);
        std::vector<Entry>::iterator entryIterator = _entries.begin();
        for (   std::vector<DefragStep>::const_iterator s=defrag.begin(); 
                s!=defrag.end() && entryIterator!=_entries.end();) {
            unsigned entryStart  = ToExternalSize(entryIterator->_start);
            unsigned entryEnd    = ToExternalSize(entryIterator->_end);
            if (s->_sourceEnd <= entryStart) {
                ++s;
                continue;
            }

            if (s->_sourceStart >= entryEnd) {
                //      This deallocate iterator doesn't have an adjustment
                ++entryIterator;
                continue;
            }

                //
                //      We shouldn't have any blocks that are stretched between multiple 
                //      steps. If we've got a match it must match the entire deallocation block
                //
            assert(entryStart >= s->_sourceStart && entryStart < s->_sourceEnd);
            assert(entryEnd > s->_sourceStart && entryEnd <= s->_sourceEnd);

            signed offset = s->_destination - signed(s->_sourceStart);
            entryIterator->_start    = ToInternalSize(entryStart+offset);
            entryIterator->_end      = ToInternalSize(entryEnd+offset);
            ++entryIterator;
        }

            //  The defrag process may have modified the order of the entries (in the heap space)
            //  We need to resort by start
        std::sort(_entries.begin(), _entries.end(), CompareStart());
    }

    std::pair<signed,signed> ReferenceCountingLayer::Release(unsigned start, unsigned size)
    {
        ScopedLock(_lock);

        Marker internalStart = ToInternalSize(start);
        Marker internalSize = ToInternalSize(AlignSize(size));

        if (_entries.empty()) {
            return std::make_pair(INT_MIN, INT_MIN);
        }

        std::vector<Entry>::iterator i = std::lower_bound(_entries.begin(), _entries.end(), internalStart, CompareStart());
        if (i != _entries.begin() && ((i-1)->_end > internalStart)) {
            --i;
        }

        Marker currentStart = internalStart;
        Marker internalEnd = internalStart+internalSize;
        signed refMin = INT_MAX, refMax = INT_MIN;
        for (;;) {
            if (i >= _entries.end() || currentStart < i->_start) {
                if (i >= _entries.end() || internalEnd <= i->_start)
                    break;
                currentStart = i->_start;
            }
            assert(i>=_entries.begin() && i<_entries.end());

            #if defined(_DEBUG)
                if (i->_start == currentStart) {
                    assert(internalEnd >= i->_end);
                } else {
                    assert(currentStart >= i->_end);
                }
            #endif

            if (i->_start == currentStart) {
                if (internalEnd >= i->_end) {
                    signed newRefCount = --i->_refCount;
                    Marker iEnd = i->_end;
                    if (!newRefCount) {
                        i = _entries.erase(i);
                    }
                    refMin = std::min(refMin, newRefCount); refMax = std::max(refMax, newRefCount);
                    if (internalEnd == iEnd) {
                        break;  // it's the end
                    }
                    currentStart = i->_end;
                    if (!newRefCount)
                        continue; // continue to skip the next ++i (because we've just done an erase)
                } else {
                        // split the block and add a new one in front
                    signed newRefCount = i->_refCount-1;
                    if (newRefCount == 0) {
                        i->_start = internalEnd;
                    } else {
                        Entry newBlock;
                        newBlock._start = currentStart;
                        newBlock._end = internalEnd;
                        newBlock._refCount = newRefCount;
                        DEBUG_ONLY(newBlock._name = i->_name);
                        i->_start = internalEnd;
                        assert(newBlock._start < newBlock._end && i->_start < i->_end);
                        assert(i->_end != 0xbaad && newBlock._end != 0xbaad);
                        i = _entries.insert(i, newBlock);
                    }
                    refMin = std::min(refMin, newRefCount); refMax = std::max(refMax, newRefCount);
                    break;  // it's the end
                }
            } else {
                if (internalEnd < i->_end) {
                    signed newRefCount = i->_refCount-1;
                    if (newRefCount==0) {
                        Entry newBlock;
                        newBlock._start = i->_start;
                        newBlock._end = currentStart;
                        newBlock._refCount = i->_refCount;
                        DEBUG_ONLY(newBlock._name = i->_name);
                        i->_start = internalEnd;
                        assert(newBlock._start < newBlock._end && i->_start < i->_end);
                        assert(i->_end != 0xbaad && newBlock._end != 0xbaad);
                        i = _entries.insert(i, newBlock);
                    } else {
                            //  This is a block that falls entirely within the old block. We need to create a new block, splitting the old one if necessary
                            // we need do split the end part of the old block off too, and then insert 2 blocks
                        Entry newBlock[2];
                        newBlock[0]._start = i->_start;
                        newBlock[0]._end = currentStart;
                        newBlock[0]._refCount = i->_refCount;
                        DEBUG_ONLY(newBlock[0]._name = i->_name);
                        newBlock[1]._start = currentStart;
                        newBlock[1]._end = internalEnd;
                        newBlock[1]._refCount = newRefCount;
                        DEBUG_ONLY(newBlock[1]._name = i->_name);
                        i->_start = internalEnd;
                        assert(newBlock[0]._start < newBlock[0]._end && newBlock[1]._start < newBlock[1]._end && i->_start < i->_end);
                        assert(i->_end != 0xbaad && newBlock[0]._end != 0xbaad && newBlock[1]._end != 0xbaad);
                        size_t offset = std::distance(_entries.begin(),i);
                        _entries.insert(i, newBlock, &newBlock[2]);
                        i = _entries.begin()+offset+2;
                    }
                    refMin = std::min(refMin, newRefCount); refMax = std::max(refMax, newRefCount);
                    break;
                } else {
                    Marker iEnd = i->_end;
                    signed newRefCount = i->_refCount-1;
                    if (newRefCount==0) {
                        i->_end = currentStart;
                    } else {
                        Entry newBlock;
                        newBlock._start = i->_start;
                        newBlock._end = currentStart;
                        newBlock._refCount = i->_refCount;
                        DEBUG_ONLY(newBlock._name = i->_name);
                        i->_start = currentStart;
                        i->_refCount = newRefCount;
                        assert(i>=_entries.begin() && i<_entries.end());
                        assert(newBlock._start < newBlock._end && i->_start < i->_end);
                        assert(i->_end != 0xbaad && newBlock._end != 0xbaad);
                        i = _entries.insert(i, newBlock)+1;
                    }
                    refMin = std::min(refMin, newRefCount); refMax = std::max(refMax, newRefCount);

                    if (internalEnd == iEnd) {
                        break;
                    }

                    currentStart = iEnd;
                        // this block extends into the next area -- so continue around the loop again
                }
            }

            ++i;
        }

        return std::make_pair(refMin, refMax);
    }

    bool        ReferenceCountingLayer::ValidateBlock(unsigned start, unsigned size) const
    {
        ScopedLock(_lock);
        Marker internalStart = ToInternalSize(start);
        Marker internalEnd = internalStart+ToInternalSize(AlignSize(size));
        std::vector<Entry>::const_iterator i = std::lower_bound(_entries.begin(), _entries.end(), internalStart, CompareStart());
        return (i != _entries.end() && i->_start == internalStart && i->_end == internalEnd);
    }

    ReferenceCountingLayer::ReferenceCountingLayer(size_t size)
    {
        _entries.reserve(64);
    }

    ReferenceCountingLayer::ReferenceCountingLayer(const ReferenceCountingLayer& cloneFrom)
    : _entries(cloneFrom._entries)
    {}
}

