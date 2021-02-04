// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../PollingThread.h"
#include <cstdint>
#include <string>
#include <any>

typedef struct _OVERLAPPED OVERLAPPED;

namespace OSServices
{
    using ConduitCompletionRoutine = void (__stdcall *)(unsigned long, unsigned long, OVERLAPPED*);
    typedef void* XlHandle;

    class IConduitProducer_CompletionRoutine : public IConduitProducer
	{
	public:
		virtual void BeginOperation(OVERLAPPED* overlapped, ConduitCompletionRoutine completionRoutine) = 0;
        virtual void CancelOperation(OVERLAPPED* overlapped) = 0;
        virtual std::any OnTrigger(unsigned numberOfBytesReturned) = 0;
	};

    class IConduitProducer_PlatformHandle : public IConduitProducer
	{
	public:
		virtual XlHandle GetPlatformHandle() const = 0;
	};

    // These exist for legacy reasons; just wrappers over the old win32 api event objects
    // Generally using these constructions doesn't lead to the most reliable patterns, however

    static const uint32_t XL_WAIT_OBJECT_0 = 0;
    static const uint32_t XL_WAIT_ABANDONED_0 = 1000;
    static const uint32_t XL_WAIT_TIMEOUT = 10000;
    static const uint32_t XL_WAIT_IO_COMPLETION = 10001;
    static const uint32_t XL_WAIT_FAILED = 0xFFFFFFFFu;
    static const uint32_t XL_INFINITE = 0xFFFFFFFFu;

    static const uint32_t XL_MAX_WAIT_OBJECTS = 64;
    static const uint32_t XL_CRITICALSECTION_SPIN_COUNT = 1000;

    XlHandle XlCreateEvent(bool manualReset);
    bool XlResetEvent(XlHandle event);
    bool XlSetEvent(XlHandle event);
    bool XlCloseSyncObject(XlHandle object);
    uint32_t XlWaitForSyncObject(XlHandle object, uint32_t waitTime);
    uint32_t XlWaitForMultipleSyncObjects(uint32_t waitCount, XlHandle waitObjects[], bool waitAll, uint32_t waitTime, bool alterable);

    static const XlHandle XlHandle_Invalid = XlHandle(~size_t(0x0));

    std::string SystemErrorCodeAsString(int errorCode);
}
