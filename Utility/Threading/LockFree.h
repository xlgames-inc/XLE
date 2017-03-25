// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ThreadingUtils.h"
#include "../PtrUtils.h"
#include "Mutex.h"
#include <queue>
#include <assert.h>

namespace Utility
{

// XL utility functions
static const uint32 XL_WAIT_OBJECT_0 = 0;
static const uint32 XL_WAIT_ABANDONED = 1000;
static const uint32 XL_WAIT_TIMEOUT = 10000;
static const uint32 XL_INFINITE = 0xFFFFFFFF;

static const uint32 XL_MAX_WAIT_OBJECTS = 64;
static const uint32 XL_CRITICALSECTION_SPIN_COUNT = 1000;

XlHandle XlCreateEvent(bool manualReset);
bool XlResetEvent(XlHandle event);
bool XlSetEvent(XlHandle event);
bool XlCloseSyncObject(XlHandle object);
uint32 XlWaitForSyncObject(XlHandle object, uint32 waitTime);
uint32 XlWaitForMultipleSyncObjects(uint32 waitCount, XlHandle waitObjects[], bool waitAll, uint32 waitTime, bool alterable);
uint32 XlGetCurrentThreadId();

static const XlHandle XlHandle_Invalid = XlHandle(~size_t(0x0));

namespace LockFree
{
    template<typename Type, int Count>
        class FixedSizeQueue
    {
    public:

            //
            //      Fixed size queue that is a thread safe
            //      BUT -- there must be only one thread "pop"ing items!
            //      There can be multiple pushers, just a single popper.
            //
            //      Also, in this queue, items never change position. So
            //      Type::operator= might be called on push(), but it won't
            //      be called again after that.
            //
            //      This could be extended to support variable length queues,
            //      but it would require maintaining multiple pages, and might
            //      require locks for determining when pages are full or empty.
            //

        bool push(const Type&);
        void push_stall(const Type&);
        void push_overflow(const Type&);

        bool push(Type&&);
        void push_stall(Type&&);
        void push_overflow(Type&&);
        
        bool try_front(Type*&) const;
        void pop();
        size_t size() const;

        void compress_overflow();

        FixedSizeQueue();
        ~FixedSizeQueue();

    private:
        uint8 _buffer[sizeof(Type)*Count];

            //      if _pushPtr==_popPtr it means the queue is empty
            //      But that means we can't fill the queue completely.
            //      Our true capacity is Count-1, and the entry before the
            //      _popPtr is always empty.
        Type* _popPtr;
        std::atomic<Type*> _pushPtr;
        std::atomic<Type*> _pushAllocatePtr;

        FixedSizeQueue(const FixedSizeQueue<Type,Count>&);
        const FixedSizeQueue<Type,Count>& operator=(const FixedSizeQueue<Type,Count>&);

        bool _overflowQueue_isEmpty;
        bool _overflowQueue_needsCompression;
        mutable bool _popNextFromOverflow;
        mutable Threading::Mutex _overflowQueue_Lock;
        std::queue<Type> _overflowQueue;
    };

            
    template<typename Type, int Count>
        FixedSizeQueue<Type,Count>::FixedSizeQueue()
        {
            _popPtr = (Type*)_buffer;
            Interlocked::ExchangePointer(&_pushPtr, (Type*)_buffer);
            Interlocked::ExchangePointer(&_pushAllocatePtr, (Type*)_buffer);

            #if defined(_DEBUG)
                void* test0 = Interlocked::LoadPointer((std::atomic<void*> volatile const*)&_pushPtr);
                void* test1 = Interlocked::LoadPointer((std::atomic<void*> volatile const*)&_pushAllocatePtr);
                assert(test0 == _buffer && test1 == _buffer);
            #endif

            _overflowQueue_isEmpty = true;
            _popNextFromOverflow = false;
            _overflowQueue_needsCompression = false;
        }

    template<typename Type, int Count>
        FixedSizeQueue<Type,Count>::~FixedSizeQueue()
        {
            Type*t = 0;
            while (try_front(t)) {pop();}   // pop everything to make sure the destructors are called on all remaining things
        }

    #undef new 

    template<typename Type, int Count>
        bool FixedSizeQueue<Type,Count>::push(const Type&newItem)
        {
                //  In a tight loop, use Interlocked::CompareExchangePointer to do a 
                //  interlocked add
            void* originalPushAllocatePtr = Interlocked::LoadPointer(&_pushAllocatePtr), *newPushAllocatePtr;
            for (;;) {
                void* comparisonValue = originalPushAllocatePtr;
                void* popPtr = _popPtr;
                newPushAllocatePtr = PtrAdd(comparisonValue, sizeof(Type));
                if (newPushAllocatePtr >= PtrAdd(_buffer, sizeof(Type)*Count)) {
                    newPushAllocatePtr = _buffer; // wrap around. _pushPtr should always point to a valid position
                }

                    //  This comparison is safe, because popPtr only moves in one direction (so we 
                    //  might get false positives, but not a false positive.) Also, if another thread
                    //  pushes, we'll fail in the CompareExchangePointer, and have to go around the loop again
                if (newPushAllocatePtr==popPtr) {
                    return false;
                }

                originalPushAllocatePtr = Interlocked::CompareExchangePointer(&_pushAllocatePtr, newPushAllocatePtr, comparisonValue);
                if (originalPushAllocatePtr == comparisonValue) {
                    break;
                }
                Threading::Pause();
            }

            new(originalPushAllocatePtr) Type(newItem);

                //  update the _pushPtr to match _pushAllocatePtr
                //  Note -- there is a slight stall here when we have multiple threads in ::push at the same time
                //          we have to increase _pushPtr one step at a time (in other words, we need to make sure
                //          we increase _pushPtr in the same order than we increased _pushAllocatePtr). But that
                //          means we may need to stall this thread waiting for another thread to increase _pushPtr
            for (;;) {
                void* originPushPtr = Interlocked::CompareExchangePointer(&_pushPtr, newPushAllocatePtr, originalPushAllocatePtr);
                if (originPushPtr == originalPushAllocatePtr) {
                    break;
                }
                Threading::Pause();
            }
            return true;
        }

    template<typename Type, int Count>
        bool FixedSizeQueue<Type,Count>::push(Type&& newItem)
        {
                //  In a tight loop, use Interlocked::CompareExchangePointer to do a 
                //  interlocked add
            Type* originalPushAllocatePtr = Interlocked::LoadPointer(&_pushAllocatePtr), *newPushAllocatePtr;
            for (;;) {
                Type* comparisonValue = originalPushAllocatePtr;
                Type* popPtr = _popPtr;
                newPushAllocatePtr = PtrAdd(comparisonValue, sizeof(Type));
                if (newPushAllocatePtr >= PtrAdd((Type*)_buffer, sizeof(Type)*Count)) {
                    newPushAllocatePtr = (Type*)_buffer; // wrap around. _pushPtr should always point to a valid position
                }

                    //  This comparison is safe, because popPtr only moves in one direction (so we 
                    //  might get false positives, but not a false positive.) Also, if another thread
                    //  pushes, we'll fail in the CompareExchangePointer, and have to go around the loop again
                if (newPushAllocatePtr==popPtr) {
                    return false;
                }

                originalPushAllocatePtr = Interlocked::CompareExchangePointer(&_pushAllocatePtr, newPushAllocatePtr, comparisonValue);
                if (originalPushAllocatePtr == comparisonValue) {
                    break;
                }
                Threading::Pause();
            }

            new(originalPushAllocatePtr) Type(std::forward<Type>(newItem));

                //  update the _pushPtr to match _pushAllocatePtr
                //  Note -- there is a slight stall here when we have multiple threads in ::push at the same time
                //          we have to increase _pushPtr one step at a time (in other words, we need to make sure
                //          we increase _pushPtr in the same order than we increased _pushAllocatePtr). But that
                //          means we may need to stall this thread waiting for another thread to increase _pushPtr
            for (;;) {
                Type* originPushPtr = Interlocked::CompareExchangePointer(&_pushPtr, newPushAllocatePtr, originalPushAllocatePtr);
                if (originPushPtr == originalPushAllocatePtr) {
                    break;
                }
                Threading::Pause();
            }
            return true;
        }

    #if defined(DEBUG_NEW)
        #define new DEBUG_NEW
    #endif

    template<typename Type, int Count>
        void FixedSizeQueue<Type,Count>::push_stall(const Type&newItem)
        {
            if (!push(newItem)) {
                //HPC startTime = GetHPC();
                while (!push(newItem)) {
                    //HPC currentTime = GetHPC();
                    //if (hpc2sec(currentTime - startTime) > 1) {
                    //    LogAlwaysWarning("Warning -- exceeded queue limit in fixed size queue. Stalling!");
                    //    startTime = currentTime;
                    //}
                    Threading::YieldTimeSlice();
                }
            }
        }

    template<typename Type, int Count>
        void FixedSizeQueue<Type,Count>::push_stall(Type&&newItem)
        {
            while (!push(std::forward<Type>(newItem))) {
                Threading::YieldTimeSlice();
            }
        }

    template<typename Type, int Count>
        void FixedSizeQueue<Type,Count>::push_overflow(const Type&newItem)
        {
            if (!push(newItem)) {
                ScopedLock(_overflowQueue_Lock);
                _overflowQueue_isEmpty = false;
                _overflowQueue_needsCompression = true;
                _overflowQueue.push(newItem);
            }
        }

    template<typename Type, int Count>
        void FixedSizeQueue<Type,Count>::push_overflow(Type&& newItem)
        {
            if (!push(std::forward<Type>(newItem))) {
                ScopedLock(_overflowQueue_Lock);
                _overflowQueue_isEmpty = false;
                _overflowQueue_needsCompression = true;
                _overflowQueue.push(std::forward<Type>(newItem));
            }
        }

    template<typename Type, int Count>
        bool FixedSizeQueue<Type,Count>::try_front(Type*&result) const
        {
                //  This is safe, so long as only this thread is doing "pop"
            Type* currentPushPtr = (Type*)Interlocked::LoadPointer(&_pushPtr);
            if (currentPushPtr == _popPtr) {
                if (!_overflowQueue_isEmpty) {
                    ScopedLock(_overflowQueue_Lock);
                    if (_overflowQueue.empty()) {
                        return false;
                    }
                    _popNextFromOverflow = true;
                    result = const_cast<Type*>(&_overflowQueue.front());
                    return true;
                }
                return false;
            }
            _popNextFromOverflow = false;
            result = _popPtr;
            return true;
        }

    template<typename Type, int Count>
        void FixedSizeQueue<Type,Count>::pop()
        {
                // Only one thread pops, so no special code (just make sure we only modify _popPtr once)
            if (!_popNextFromOverflow) {
                ((Type*)_popPtr)->~Type();
                Type* newPopPtr = _popPtr+1;
                if (newPopPtr >= (Type*)PtrAdd(_buffer, sizeof(Type)*Count)) {
                    newPopPtr = (Type*)_buffer;
                }
                _popPtr = newPopPtr;
            } else {
                ScopedLock(_overflowQueue_Lock);
                _overflowQueue.pop();
            }
        }

    template<typename Type, int Count>
        size_t FixedSizeQueue<Type,Count>::size() const
        {
                // because of threading, this can only be an approximate result
                //  we should load the pushptr first to avoid threading problems 
                //  when the push ptr passes an old pop ptr
            Type* pushPtr = (Type*)Interlocked::LoadPointer(&_pushPtr);
            Type* popPtr = _popPtr;
            if (pushPtr >= popPtr) {
                return pushPtr - popPtr;
            }

            return ((Type*)PtrAdd(_buffer, sizeof(Type)*Count) - popPtr) + (pushPtr - (Type*)_buffer);
        }

    template<typename Type, int Count>
        void FixedSizeQueue<Type,Count>::compress_overflow()
    {
        if (_overflowQueue_isEmpty && _overflowQueue_needsCompression) {
            ScopedLock(_overflowQueue_Lock);
            _overflowQueue = std::queue<Type>();    // destroy memory
            _overflowQueue_needsCompression = false;
        }
    }

    template<typename Type, int Count>
        class FixedSizeQueue_Waitable : public FixedSizeQueue<Type,Count>
    {
    public:
        bool push(const Type&);
        void push_stall(const Type&);
        XlHandle get_event();
        FixedSizeQueue_Waitable();
        ~FixedSizeQueue_Waitable();
    private:
        XlHandle _event;
    };

    template<typename Type, int Count>
        bool FixedSizeQueue_Waitable<Type,Count>::push(const Type&newItem)
        {
            bool result = FixedSizeQueue<Type,Count>::push(newItem);
            if (result) {
                XlSetEvent(_event);
            }
            return result;
        }

    template<typename Type, int Count>
        void FixedSizeQueue_Waitable<Type,Count>::push_stall(const Type&newItem)
        {
            while (!push(newItem)) {
                // HPC startTime = GetHPC();
                while (!push(newItem)) {
                    // HPC currentTime = GetHPC();
                    // if (hpc2sec(currentTime - startTime) > 1) {
                    //     LogAlwaysWarning("Warning -- exceeded queue limit in fixed size queue. Stalling!");
                    //     startTime = currentTime;
                    // }
                    XlSetEvent(_event); // raise the event, just to make sure the other thread is processing
                    Threading::YieldTimeSlice();
                }
            }
        }

    template<typename Type, int Count>
        FixedSizeQueue_Waitable<Type,Count>::FixedSizeQueue_Waitable()
        {
            _event = XlCreateEvent(false);
        }

    template<typename Type, int Count>
        FixedSizeQueue_Waitable<Type,Count>::~FixedSizeQueue_Waitable()
        {
            XlCloseSyncObject(_event);
        }

    template<typename Type, int Count>
        XlHandle FixedSizeQueue_Waitable<Type,Count>::get_event()
        {
            return _event;
        }
}

}

using namespace Utility;
