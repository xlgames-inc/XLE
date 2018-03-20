// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Core/Types.h"

namespace Utility { namespace Threading
{
    #if COMPILER_ACTIVE == COMPILER_TYPE_MSVC
        #define xl_thread_call __stdcall
    #else
        #define xl_thread_call __attribute((stdcall))
    #endif

    class Thread
    {
    public:
        typedef unsigned int (xl_thread_call ThreadFunction)(void*);

        Thread(ThreadFunction* threadFunction, void* argument);
        ~Thread();
        void join();        // (matching std::thread::join)

    protected:
        uintptr   _threadHandle;
    };

}}

using namespace Utility;

        