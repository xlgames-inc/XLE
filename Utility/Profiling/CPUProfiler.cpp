// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CPUProfiler.h"
#include "../MemoryUtils.h"
#include "../PtrUtils.h"
#include <algorithm>
#include <stack>

namespace Utility
{

    void HierarchicalCPUProfiler::EndFrame()
    {
        assert(XlGetCurrentThreadId() == _threadId);
        assert(_aeStackI==0);
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

    static HierarchicalCPUProfiler::ResolvedEvent BlankEvent(const char label[])
    {
        HierarchicalCPUProfiler::ResolvedEvent evnt;
        evnt._label = label;
        evnt._inclusiveTime = evnt._exclusiveTime =0 ;
        evnt._eventCount = 0;
        evnt._firstChild = HierarchicalCPUProfiler::ResolvedEvent::s_id_Invalid;
        evnt._sibling = HierarchicalCPUProfiler::ResolvedEvent::s_id_Invalid;
        return evnt;
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
                if (_workingStackIndex > 0) {
                    link._parent = parentsAndChildren[workingStack[_workingStackIndex-1]]._child;
                }
                link._child = AsPointer(i);
                link._label = (const char*)*(i+1);
                link._id = workingId++;
                link._resolvedChildrenTime = link._resolvedInclusiveTime = 0;
                ++i;

                workingStack[_workingStackIndex++] = parentsAndChildren.size();
                parentsAndChildren.push_back(link);

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

        class PreResolveEvent
        {
        public:
            ResolvedEvent::Id _parentOutput;
            const ParentAndChildLink* _childrenStart;
            const ParentAndChildLink* _childrenEnd;
        };

        std::stack<PreResolveEvent> finalResolveQueue;
        // finalResolveQueue.reserve(s_maxStackDepth);

        auto inputI = parentsAndChildren.cbegin();
        auto rootEventsStart = inputI++;
        while (inputI->_parent == nullptr) { ++inputI; }
        auto rootEventsEnd = inputI;

        auto lastRootEventOutputId = ResolvedEvent::s_id_Invalid;

            //  For each root event, we must start the tree, and queue the children
            //  Note that we're not merging root events here! Merging occurs for every
            //  other events, just not the root events
        for (auto root = rootEventsStart; root < rootEventsEnd; ++root) {
            auto outputId = result.size();
            if (lastRootEventOutputId != ResolvedEvent::s_id_Invalid) {
                    // attach the new root event as a sibling of the last one added
                result[lastRootEventOutputId]._sibling = outputId;
            }

            ResolvedEvent rootEvent;
            rootEvent._label = root->_label;
            rootEvent._inclusiveTime = root->_resolvedInclusiveTime;
            rootEvent._exclusiveTime = root->_resolvedInclusiveTime - root->_resolvedChildrenTime;
            rootEvent._eventCount = 1;
            rootEvent._firstChild = ResolvedEvent::s_id_Invalid;
            rootEvent._sibling = ResolvedEvent::s_id_Invalid;
            result.push_back(rootEvent);

            auto parentLinkSearch = root->_child;

            PreResolveEvent queuedEvent;
            queuedEvent._parentOutput = outputId;
            queuedEvent._childrenStart = AsPointer(inputI);
            
            while (inputI < parentsAndChildren.cend() && inputI->_parent == parentLinkSearch) { ++inputI; }
            queuedEvent._childrenEnd = AsPointer(inputI);

            finalResolveQueue.push(queuedEvent);
            lastRootEventOutputId = outputId;
        }

            //  While we have children in our "finalResolveQueue", we need to go 
            //  through and turn them into ResolveEvents.
            //
            //  During this phase, we also need to do mergers. When we encounter 
            //  multiple siblings with the same label, they must be merged into 
            //  a single ResolvedEvent.
            //
            //  We must also merge children of events in the same way. The entire
            //  hierarchy of the merge labels will be collapsed together, and where
            //  identical labels occur at the same tree depth, they get merged to 
            //  become a single event.
        while (!finalResolveQueue.empty()) {
            const auto w = finalResolveQueue.top();
            finalResolveQueue.pop();
            auto& parentOutput = result[w._parentOutput];

            auto childIterator = w._childrenStart;
            while (childIterator < w._childrenEnd) {
                auto mergedChildStart = childIterator;
                auto mergedChildEnd = mergedChildStart+1;
                while (mergedChildEnd->_label == mergedChildStart->_label) { ++mergedChildEnd; }

                    //  All of these children will be collapsed into a single
                    //  resolved event. But first we need to check if there
                    //  is already a ResolvedEvent attached to the same parent,
                    //  with the same label;
                auto existingChildIterator = parentOutput._firstChild;
                auto lastSibling = ResolvedEvent::s_id_Invalid;
                while (existingChildIterator != ResolvedEvent::s_id_Invalid) {
                    lastSibling = existingChildIterator;
                    if (result[existingChildIterator]._label == mergedChildStart->_label) {
                        break;
                    }
                    existingChildIterator = result[existingChildIterator]._sibling;
                }

                if (existingChildIterator == ResolvedEvent::s_id_Invalid) {
                        //  It doesn't exist. Create a new event, and attach it to the last sibling 
                        //  of the parent
                    ResolvedEvent newEvent = BlankEvent(mergedChildStart->_label);
                    existingChildIterator = result.size();
                    result.push_back(newEvent);
                    if (lastSibling != ResolvedEvent::s_id_Invalid) {
                        result[lastSibling]._sibling = existingChildIterator;
                    } else {
                        parentOutput._firstChild = existingChildIterator;
                    }
                }

                    //  Now either we've created a new event, or we're merging into an existing one.
                auto& dstEvent = result[existingChildIterator];
                dstEvent._eventCount += mergedChildEnd - mergedChildStart;
                for (auto c = mergedChildStart; c<mergedChildEnd; ++c) {
                    dstEvent._inclusiveTime += c->_resolvedInclusiveTime;
                    dstEvent._exclusiveTime += c->_resolvedInclusiveTime - c->_resolvedChildrenTime;

                        //  This item becomes a new parent. We need to push in the
                        //  resolve operations for all of the children
                    PreResolveEvent queuedEvent;
                    queuedEvent._parentOutput = existingChildIterator;
                    queuedEvent._childrenStart = AsPointer(inputI);
            
                    auto parentLinkSearch = c->_child;
                    while (inputI < parentsAndChildren.cend() && inputI->_parent == parentLinkSearch) { ++inputI; }
                    queuedEvent._childrenEnd = AsPointer(inputI);

                    if (queuedEvent._childrenEnd != queuedEvent._childrenStart) {
                        finalResolveQueue.push(queuedEvent);
                    }
                }

                childIterator = mergedChildEnd;
            }
        }

            //  We should always get to the end of this array. If we don't get all the way through, it means that
            //  the ordering must off. It means we're going to loose the results of some children
        assert(inputI == parentsAndChildren.cend());
        
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

