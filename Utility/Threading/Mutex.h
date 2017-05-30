// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Core/Prefix.h"
#include "ThreadLibrary.h"

///////////////////////////////////////////////////////////////////////////////
#if THREAD_LIBRARY == THREAD_LIBRARY_TINYTHREAD

        //  "TinyThread++" provides a nice placeholder for
        //  std::mutex, that can stand in until we can find a permanent
        //  portable threading solution

    #define XLE_WORKAROUND_WINDOWS_H
    #include <tinythread.h>
    #include <fast_mutex.h>

    namespace Utility { namespace Threading
    {
        typedef tthread::fast_mutex Mutex;
        typedef tthread::recursive_mutex RecursiveMutex;    // \todo -- haven't checked if this mutex is properly recursive
        typedef tthread::fast_mutex ReadWriteMutex;         // read/write mutex not provided by tinythread. Maybe implement with AcquireSRWLockShared? Possibly part of C++14?
    }}
    using namespace Utility;

    #if defined(ScopedLock)
        #undef ScopedLock
        #undef ScopedReadLock
        #undef ScopedModifyLock
    #endif

    #define ScopedLock(x)       tthread::lock_guard<::Utility::Threading::Mutex> _autoLockA(x)
    #define ScopedReadLock(x)   tthread::lock_guard<::Utility::Threading::ReadWriteMutex> _autoLockB(x)
    #define ScopedModifyLock(x) tthread::lock_guard<::Utility::Threading::ReadWriteMutex> _autoLockC(x)

#endif

///////////////////////////////////////////////////////////////////////////////
#if THREAD_LIBRARY == THREAD_LIBRARY_STDCPP

        //  C++ threading library isn't supported on Visual Studio 2010
        //  If we drop VS2010 support, this would be the best option

    #include <mutex>
    #include <condition_variable>
	#include <shared_mutex>

    namespace Utility { namespace Threading
    {
        using Mutex = std::mutex;
        using RecursiveMutex = std::recursive_mutex;
        // using ReadWriteMutex = std::shared_timed_mutex;
        using Conditional = std::condition_variable;
    }}
    using namespace Utility;

    #define ScopedLock(x)            std::unique_lock<decltype(x)> _autoLockA(x)
    #define ScopedReadLock(x)        std::unique_lock<decltype(x)> _autoLockB(x)
    #define ScopedModifyLock(x)      std::unique_lock<decltype(x)> _autoLockC(x)

#endif

///////////////////////////////////////////////////////////////////////////////
#if THREAD_LIBRARY == THREAD_LIBRARY_TBB

    #if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS
        #include "../../Core/WinAPI/IncludeWindows.h"      // (TBB brings in windows.h, so let's make sure our defines are set)
    #endif

        //  "Intel Threading Building Blocks" is being excluded because
        //  it's license is incompatible with XLE. But it's otherwise a
        //  good and very portable library!

    #undef new
    #include <tbb/critical_section.h>
    #include <tbb/queuing_rw_mutex.h>
    #include <tbb/recursive_mutex.h>
    #if defined(DEBUG_NEW)
        #define new DEBUG_NEW
    #endif

    namespace Utility { namespace Threading
    {
        typedef tbb::critical_section Mutex;
        typedef tbb::recursive_mutex RecursiveMutex;
        typedef tbb::queuing_rw_mutex ReadWriteMutex;
    }}
    using namespace Utility;

    #if defined(ScopedLock)
        #undef ScopedLock
        #undef ScopedReadLock
        #undef ScopedModifyLock
    #endif

    #define ScopedLock(x)            tbb::critical_section::scoped_lock _autoLockA(x)
    #define ScopedReadLock(x)        tbb::queuing_rw_mutex::scoped_lock _autoLockB(x, false)
    #define ScopedModifyLock(x)      tbb::queuing_rw_mutex::scoped_lock _autoLockC(x, true)

    #undef _STITCH

#endif
