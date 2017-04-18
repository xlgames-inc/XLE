// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ThreadLibrary.h"
#include "../../Core/Prefix.h"
#include "../../Core/Types.h"
#include "../../Core/SelectConfiguration.h"

// Currently we have a problem including <thread> into C++/CLR files
// This makes it difficult to use the standard library functions for things like CurrentThreadId()
#if defined(USE_STANDARD_THREAD_UTILS)
	#include <thread>
#endif

/// \namespace Utility::Interlocked
///
/// This namespace provides low-level utility functions for atomic operations.
/// Mostly this namespace wraps some low-level api providing the functionality
/// required (this gives us flexibility to choose different low level api, and
/// perhaps provide different solutions for different platforms).
///
/// # Return values
///
/// Important note:
///     Functions in the Interlocked namespace always return the previous
///     value of the target! This differs from the Win32 API InterlockedAdd, etc!
///
///         Win32 InterlockedAdd(&target, addition) -> returns old "target" + addition
///         Interlocked::InterlockedAdd(&target, addition) -> returns old "target"
///
///     This simple rule seems is more intuitive and consistant. Interlocked::Exchange
///     and other functions also work in this same way -- return the "old" value.
///
///     But it means that code previously used Win32 InterlockedAdd() must be change
///     to suit this behaviour.
///
///     As an example, here is reference count Release() behaviour:
///     <example>
///         <code>\code
///             auto oldRefCount = Interlocked::Decrement(&refCount);
///             if (oldRefCount == 1) {
///                     // (ref count was previously 1, and so is now 0)
///                 delete *this;
///             }
///         \endcode</code>
///     </example>
///
/// # Release / Acquire semantics
///
/// When using atomic functions, there are issues related to the ordering of instruction
/// completion relative to other instructions. In certain cases, a thread can see an atomic
/// value change in way that appears out-of-order relative to other memory changes. 
///
/// This is particularly important for mutexes and threading control primitives. When a mutex
/// is unlocked, it's normal to assume that memory writes to the object that mutex protects
/// will be committed to memory. However, this isn't guaranteed unless there is a memory barrier
/// to properly synchronize memory. Normally mutex implementations will take care of this. However
/// it's worth noting when writing custom thread control (eg, a spin lock) that use these Interlocked
/// methods.
///
/// For more information on this topic, see documentation on std::memory_order (part of the new C++ standard)
///
/// When using the Win32 intrinsic implementations of InterlockedExchange, etc, these automatically
/// place a read/write barrier that effects the behaviour of the compiler, and possibly a memory barrier
/// as well. See the MSVC documentation for _ReadWriteBarrier. The documentation is a little unclear
/// about whether there is a runtime memory barrier (and on what platforms it takes effect).

#if (THREAD_LIBRARY == THREAD_LIBRARY_TINYTHREAD) || (THREAD_LIBRARY == THREAD_LIBRARY_STDCPP)

    #if (PLATFORMOS_TARGET == PLATFORMOS_IOS) || (PLATFORMOS_TARGET == PLATFORMOS_OSX) || (PLATFORMOS_TARGET == PLATFORMOS_ANDROID)

        #include <atomic>

        namespace Utility { namespace Interlocked
        {
            typedef std::atomic<int32> Value;
            typedef std::atomic<int64> Value64;

            force_inline int32 Exchange(Value volatile* target, int32 newValue)                                             { return std::atomic_exchange(target, newValue); }
            force_inline int32 CompareExchange(Value volatile* target, int32 newValue, int32 comparisonValue)               { auto prev = std::atomic_load(target); return std::atomic_compare_exchange_strong(target, &comparisonValue, newValue) ? comparisonValue : prev; }
            force_inline int32 Load(Value volatile* target)                   { return std::atomic_load(target); }
            
            force_inline int64 Exchange64(Value64 volatile* target, int64 newValue)                                       { return std::atomic_exchange(target, newValue); }
            force_inline int64 CompareExchange64(Value64 volatile* target, int64 newValue, int64 comparisonValue)         { auto prev = std::atomic_load(target); return std::atomic_compare_exchange_strong(target, &comparisonValue, newValue) ? comparisonValue : prev; }
            force_inline int64 Load64(Value64 volatile const* target)         { return std::atomic_load(target); }
            
            force_inline int32 Increment(Value volatile* target)              { return std::atomic_fetch_add(target, 1); }
            force_inline int32 Decrement(Value volatile* target)              { return std::atomic_fetch_add(target, -1); }
            force_inline int32 Add(Value volatile* target, int32 addition)    { return std::atomic_fetch_add(target, addition); }
            
            T1(Type) force_inline Type* ExchangePointer(std::atomic<Type*>* target, Type* newValue)                                 { return std::atomic_exchange(target, newValue); }
            T1(Type) force_inline Type* CompareExchangePointer(std::atomic<Type*>* target, Type* newValue, Type* comparisonValue)   { auto prev = std::atomic_load(target); return std::atomic_compare_exchange_strong(target, &comparisonValue, newValue) ? comparisonValue : prev; }
            T1(Type) force_inline Type* LoadPointer(std::atomic<Type*> volatile const* target)                                      { return std::atomic_load(target); }
        }}

    #else

        #include <intrin.h>

        #if defined(_M_IA64) || defined(_M_AMD64)
            #define EXCHANGE64_INTRINSIC
        #endif

        namespace Utility { namespace Interlocked 
        {
            typedef long Value;
            typedef int64 Value64;

            force_inline Value Exchange(Value volatile* target, Value newValue)           { return _InterlockedExchange(target, newValue); }

            inline Value CompareExchange(Value volatile* target, Value newValue, Value comparisonValue)                     { return _InterlockedCompareExchange(target, newValue, comparisonValue); }
            force_inline Value64 CompareExchange64(Value64 volatile* target, Value64 newValue, Value64 comparisonValue)     { return _InterlockedCompareExchange64(target, newValue, comparisonValue); }

            #if defined(EXCHANGE64_INTRINSIC)
                force_inline Value64 Exchange64(Value64 volatile* target, Value64 newValue)   { return _InterlockedExchange64(target, newValue); }
                force_inline void* ExchangePointer(void* volatile* target, void* newValue)    { return _InterlockedExchangePointer(target, newValue); }
                force_inline void* CompareExchangePointer(void* volatile* target, void* newValue, void* comparisonValue)      { return _InterlockedCompareExchangePointer(target, newValue, comparisonValue); }
            #else
                force_inline Value64 Exchange64(Value64 volatile* target, Value64 newValue)   
                {
                        //  Intrinsic for Exchange64 is not provided in <intrin.h> for 32 bit targets
                        //  however, the behaviour should be as follows -- 
                    Value64 old;
                    do {
                        old = *target;
                    } while (CompareExchange64(target, newValue, old) != old);
                    return old;
                }

                force_inline void* ExchangePointer(void* volatile* target, void* newValue)    
                { 
                    if (constant_expression<sizeof(void*) == 4>::result()) {
                        return (void*)Exchange((Value volatile*)target, (Value)newValue);
                    } else {
                        return (void*)Exchange64((Value64 volatile*)target, (Value64)newValue);
                    }
                }

                force_inline void* CompareExchangePointer(void* volatile* target, void* newValue, void* comparisonValue)      
                { 
                    if (constant_expression<sizeof(void*) == 4>::result()) {
                        return (void*)CompareExchange((Value volatile*)target, (Value)newValue, (Value)comparisonValue);
                    } else {
                        return (void*)CompareExchange64((Value64 volatile*)target, (Value64)newValue, (Value)comparisonValue);
                    }
                }
            #endif

                /* note -- "_InterlockedIncrement" will not return the correct result on Win95 and earlier! Expect crashes and leaks on that platform! */
            force_inline Value Increment(Value volatile* target)              { return _InterlockedIncrement(target)-1; }
            force_inline Value Decrement(Value volatile* target)              { return _InterlockedDecrement(target)+1; }
            force_inline Value Add(Value volatile* target, Value addition)    { return _InterlockedExchangeAdd(target, addition); }

            force_inline Value Load(Value volatile* target)                   { return *target; }
            force_inline Value64 Load64(Value64 volatile const* target)       { return *target; }
            force_inline void* LoadPointer(void* volatile const* target)      { return *target; }
        }}

    #endif

#endif

#if THREAD_LIBRARY == THREAD_LIBRARY_TBB

    #if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
        #include "../../Core/WinAPI/IncludeWindows.h"
    #endif
    #include <tbb/tbb_machine.h>        // (maybe includes <windows.h>?)

    namespace Utility { namespace Interlocked 
    {
        typedef int32 Value;
        typedef int64 Value64;

        inline Value Exchange(Value volatile* target, Value newValue)               { return __sync_lock_test_and_set((volatile int*)target, (int)newValue); }
        inline Value64 Exchange64(Value64 volatile* target, Value64 newValue)       { return __sync_lock_test_and_set((volatile long long*)target, (long long)newValue); }
        inline void* ExchangePointer(void* volatile* target, void* newValue)        { return (void*)__sync_lock_test_and_set((volatile void*)target, (ptrdiff_t)newValue); }

        inline Value CompareExchange(Value volatile* target, Value newValue, Value comparisonValue)             { return __sync_val_compare_and_swap((int*)target, (int)newValue, (int)comparisonValue); }
        inline Value64 CompareExchange64(Value64 volatile* target, Value64 newValue, Value64 comparisonValue)   { return __sync_val_compare_and_swap((long long*)target, (long long)newValue, (long long)comparisonValue); }
        inline void* CompareExchangePointer(void* volatile* target, void* newValue, void* comparisonValue)      { return (void*)__sync_val_compare_and_swap((volatile void*)target, (ptrdiff_t)newValue, (ptrdiff_t)comparisonValue); }
        
        inline Value Increment(Value volatile* target)              { return __sync_fetch_and_add((int*)target, 1); }
        inline Value Decrement(Value volatile* target)              { return __sync_fetch_and_add((int*)target, (int)-1); }
        inline Value Add(Value volatile* target, Value addition)    { return __sync_fetch_and_add((int*)target, addition); }

        inline Value Load(Value volatile* target) 
        {
            return *target;
            // return *tbb::internal::__TBB_load_full_fence(target);
        }

        inline Value64 Load64(Value64 volatile const* target) 
        {
            return *target;
            // return *tbb::internal::__TBB_load_full_fence(target);
        }

        inline void* LoadPointer(void* volatile const* target) 
        {
            return *target;
            // return *tbb::internal::__TBB_load_full_fence(target);
        }
    }}

#endif

namespace Utility
{
                /////////////////////////////////////////////////////////////////////////////

    class RefCountedObject
    {
    public:
            //
            //      The constructor for RefCountedObject initialises the reference count to 1.
            //      so:
            //          RefCountedObject* p = new Object();
            //          p->AddRef();
            //          p->Release();
            //          p->Release();
            //
            //      will not destroy the object until the second Release();
            //
            //      Also, when initialising a intrusive_ptr, remember to set the second parameter to false. 
            //      Otherwise the intrusive_ptr constructor will add another reference. 
            //      Eg:
            //          intrusive_ptr<Object> p(new Object, false);
            //
        signed AddRef() const;
        signed Release() const;
        signed GetRefCount() const;
        RefCountedObject();
        virtual ~RefCountedObject();
    private:
        mutable Interlocked::Value _refCount;
    };

                /////////////////////////////////////////////////////////////////////////////

    inline signed RefCountedObject::AddRef() const   { return Interlocked::Increment(&_refCount) + 1; }
    inline signed RefCountedObject::Release() const
    {
        auto oldRefCount = Interlocked::Decrement(&_refCount);
        if (oldRefCount==1) {
            delete this;        // destroy when we hit 0 ref counts
        }
        return oldRefCount-1;
    }
    inline signed RefCountedObject::GetRefCount() const   { return _refCount; }

    inline RefCountedObject::RefCountedObject() : _refCount(1) {}
    inline RefCountedObject::~RefCountedObject()    {}

}

using namespace Utility;



    /////////////////////////////////////////////////////////////////////////////
                //// Win32/64 API ////
#if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS

    #if !defined(WINAPI)
        #define WINAPI      __stdcall
    #endif
    #if !defined(WINBASEAPI)
        #define DECLSPEC_IMPORT __declspec(dllimport)
        #define WINBASEAPI DECLSPEC_IMPORT
    #endif
    extern "C" WINBASEAPI int WINAPI   SwitchToThread();
    extern "C" WINBASEAPI void WINAPI  Sleep(unsigned long milliseconds);
    extern "C" WINBASEAPI unsigned long WINAPI GetCurrentThreadId();
    extern "C" void         _mm_pause(void);
    #pragma intrinsic(_mm_pause)

    namespace Utility { namespace Threading {

		#if !defined(USE_STANDARD_THREAD_UTILS)
			inline void YieldTimeSlice()            { SwitchToThread(); }
			inline void Sleep(uint32 milliseconds)	{ ::Sleep(milliseconds); }
			using ThreadId = unsigned;
			inline unsigned CurrentThreadId()		{ return ::GetCurrentThreadId(); }
		#else
			inline void YieldTimeSlice()			{ std::this_thread::yield(); }
			inline void Sleep(uint32 milliseconds)	{ std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }
			using ThreadId = std::thread::id;
			inline ThreadId CurrentThreadId()		{ return std::this_thread::get_id(); }
		#endif
        
		inline void Pause() { _mm_pause(); }
    }}

#else

    #include <thread>

            //// Other / unsupported ////
    namespace Utility { namespace Threading {
        inline void YieldTimeSlice()			{ std::this_thread::yield(); }
        inline void Sleep(uint32 milliseconds)	{ std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }
        using ThreadId = std::thread::id;
        inline ThreadId CurrentThreadId()		{ return std::this_thread::get_id(); }

        #if (COMPILER_ACTIVE == COMPILER_TYPE_GCC) || (COMPILER_ACTIVE == COMPILER_TYPE_CLANG)
            inline void Pause() { __builtin_ia32_pause(); }
        #else
            inline void Pause() { assert(0); }
        #endif
    }}

#endif

