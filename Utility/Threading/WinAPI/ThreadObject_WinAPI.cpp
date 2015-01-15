// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../ThreadObject.h"
#include "../../../Core/Types.h"
#include "../../../Core/WinAPI/IncludeWindows.h"
#include <process.h>

namespace Utility { namespace Threading
{

        //  Basic thread implementation for Windows API (similar to std::thread, but a little more tolerant)

    Thread::Thread(ThreadFunction* threadFunction, void* argument)
    {
        const uint32 stackSize = 0;
        _threadHandle = _beginthreadex(NULL, stackSize, threadFunction, argument, 0, NULL);
    }

    Thread::~Thread()
    {
        CloseHandle(HANDLE(_threadHandle));
    }

    void Thread::join()
    {
        WaitForSingleObject(HANDLE(_threadHandle), INFINITE);
    }

}}

using namespace Utility;
