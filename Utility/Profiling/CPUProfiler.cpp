// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CPUProfiler.h"
#include "../MemoryUtils.h"
#include "../PtrUtils.h"
#include <algorithm>

namespace Utility
{

    void HierarchicalCPUProfiler::BeginFrame()
    {
        static_assert(s_bufferCount > 1, "Expecting at least 2 buffers");
        std::swap(_events[0], _events[1]);  // (actually only the first 2 would be used)
        std::swap(_idAtEventsStart[0], _idAtEventsStart[1]);

        _idAtEventsStart[0] = _workingId;

            // erase without deleting memory
        _events[0].erase(_events[0].begin(), _events[0].end());
        assert(_aeStackI == 0);
    }

    struct ParentAndChildLink
    {
        const uint64* _parent;
        const uint64* _child;
        const char* _label;
        unsigned _id;

        uint64 _resolvedInclusiveTime;
        uint64 _resolvedChildrenTime;
    };

    static bool CompareParentAndChildLink(const ParentAndChildLink& lhs, const ParentAndChildLink& rhs)
    {
            //  sort by parent first, and then label, and finally child
            //  This will resolve in a breadth-first ordering, and all
            //  children with the same label will be sequential.
        if (lhs._parent == rhs._parent) {
            if (lhs._label == rhs._label) {
                return lhs._child < rhs._child;
            }
            return lhs._label < rhs._label;
        }
        return lhs._parent < rhs._parent;
    }
    
    auto HierarchicalCPUProfiler::CalculateResolvedEvents() const -> std::vector<ResolvedEvent>
    {
            //  First, we need to rearrange the call stack in a
            //  breath-first hierarchy order (sortable by label)
            //  This requires iterating through the entire list of events. 
            //  Once it's in breath-first order, it should become
            //  much easier to do the next few operations.
        std::vector<ParentAndChildLink> parentsAndChildren;
        parentsAndChildren.reserve(_events[1].size()/2);    // Approximation of events count

        unsigned workingStack[s_maxStackDepth];
        unsigned _workingStackIndex = 0;
        auto workingId = _idAtEventsStart[1];

        auto i=_events[1].cbegin();
        for (; i!=_events[1].cend(); ++i) {
            uint64 time = *i;
            if (time & (1ull << 63ull)) {

                    //  This is an "end" event. We can resolve the last
                    //  event in the stack. If there's nothing on the stack,
                    //  then we've popped too many times!

                time &= ~(1ull << 63ull);
                if (!_workingStackIndex) {
                    assert(0);
                } else {
                    auto entryIndex = workingStack[_workingStackIndex-1];
                    assert(entryIndex < parentsAndChildren.size());
                    auto& entry = parentsAndChildren[entryIndex];

                    uint64 startTime = *entry._child;
                    assert(time >= startTime);
                    uint64 inclusiveTime = time - startTime;
                    entry._resolvedInclusiveTime = inclusiveTime;
                    --_workingStackIndex;

                    if (_workingStackIndex > 0) {
                        auto parentIndex = workingStack[_workingStackIndex-1];
                        assert(parentIndex < parentsAndChildren.size());
                        auto& parentEntry = parentsAndChildren[parentIndex];
                        parentEntry._resolvedChildrenTime += entry._resolvedInclusiveTime;
                    }
                }

            } else {

                assert((_workingStackIndex+1) <= dimof(workingStack));

                    // create a new parent and child link, and add to our list
                ParentAndChildLink link;
                link._parent = nullptr;
                link._child = AsPointer(i);
                link._label = (const char*)*(i+1);
                link._id = workingId++;
                link._resolvedChildrenTime = link._resolvedInclusiveTime = 0;
                ++i;

            }
        }

            //  Sort into breadth first order. Note that it really is breadth first order.
            //  the first sorting term is a pointer into the event list, and this will
            //  always work out to put children immediately after their children.
        std::sort(parentsAndChildren.begin(), parentsAndChildren.end(), CompareParentAndChildLink);

            //  Now we have to go through and generate breadth-first output
            //  While doing this, we'll also combine multiple calls to the same label
            //  into one. It's difficult to do this before the sorting step. In theory, it
            //  might be possible, but would probably require extra restrictions and bookkeeping.
        std::vector<ResolvedEvent> result;
        result.reserve(parentsAndChildren.size());

        std::queue<std::pair<unsigned, unsigned>> finalResolveQueue;

        {
            auto i = parentsAndChildren.begin();
            auto firstChildI = i+1;
            ResolvedEvent rootEvent;
            rootEvent._name = i->_label;
            rootEvent._inclusiveTime = i->_resolvedInclusiveTime;
            rootEvent._exclusiveTime = i->_resolvedInclusiveTime - i->_resolvedChildrenTime;
            rootEvent._eventCount = 1;

            auto childEnd = firstChildI;
            while (childEnd < parentsAndChildren.cend() && childEnd->_parent == i->_child) { ++childEnd; }
            rootEvent._childCount = childEnd = firstChildI;

            finalResolveQueue.push_back(std::make_pair(0,1));
        }

        while (!finalResolveQueue.empty()) {
            unsigned parentIndex = finalResolveQueue.front()->first;
            unsigned firstChildIndex = finalResolveQueue.front()->second;
            finalResolveQueue.pop();


        }

        for (auto i=parentsAndChildren.cbegin(); i!=parentsAndChildren.cend();) {

            auto childStart = i+1;
            auto childEnd = childStart;
            while (childEnd < parentsAndChildren.cend() && childEnd->_parent == i->_child) { ++childEnd; }

            ResolvedEvent evnt;
            evnt._childCount = childEnd - childStart;
            evnt._exclusiveTime = i->_resolvedInclusiveTime - i->_resolvedChildrenTime;
            evnt._inclusiveTime = i->_resolvedInclusiveTime;
            evnt._name = i->_label;
            evnt._eventCount = 1;

        }
        
        return result;
    }

    HierarchicalCPUProfiler::HierarchicalCPUProfiler()
    {
        _workingId = 0;
        for (unsigned c=0; c<s_bufferCount; ++c) {
            _events[c].reserve(16 * 1024);
            _idAtEventsStart[c] = _workingId;
        }

        #if defined(_DEBUG)
            _threadId = XlGetCurrentThreadId();
            XlZeroMemory(_aeStack);
            _aeStackI = 0;
        #endif
    }

    HierarchicalCPUProfiler::~HierarchicalCPUProfiler()
    {
    }
}

