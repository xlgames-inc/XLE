// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <cstdint>

namespace OSServices
{
    // These exist for legacy reasons; just wrappers over the old win32 api event objects
    // Generally using these constructions doesn't lead to the most reliable patterns, however
    typedef void* XlHandle;

    static const uint32_t XL_WAIT_OBJECT_0 = 0;
    static const uint32_t XL_WAIT_ABANDONED = 1000;
    static const uint32_t XL_WAIT_TIMEOUT = 10000;
    static const uint32_t XL_INFINITE = 0xFFFFFFFF;

    static const uint32_t XL_MAX_WAIT_OBJECTS = 64;
    static const uint32_t XL_CRITICALSECTION_SPIN_COUNT = 1000;

    XlHandle XlCreateEvent(bool manualReset);
    bool XlResetEvent(XlHandle event);
    bool XlSetEvent(XlHandle event);
    bool XlCloseSyncObject(XlHandle object);
    uint32_t XlWaitForSyncObject(XlHandle object, uint32_t waitTime);
    uint32_t XlWaitForMultipleSyncObjects(uint32_t waitCount, XlHandle waitObjects[], bool waitAll, uint32_t waitTime, bool alterable);

    static const XlHandle XlHandle_Invalid = XlHandle(~size_t(0x0));
}
