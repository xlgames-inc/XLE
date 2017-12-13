
#include "../SystemUtils.h"
#include "../../Core/SelectConfiguration.h"
#include "../../Core/Types.h"

namespace Utility
{
    uint32 XlGetCurrentThreadId()
    {
        return 0;
    }

    uint64 GetPerformanceCounter()
    {
        return 0;
    }

    uint64 GetPerformanceCounterFrequency()
    {
        return 0;
    }

    XlHandle XlCreateEvent(bool manualReset) { return 0; }
    bool XlResetEvent(XlHandle event) { return false; }
    bool XlSetEvent(XlHandle event) { return false; }
    bool XlCloseSyncObject(XlHandle object) { return false; }
    uint32 XlWaitForSyncObject(XlHandle object, uint32 waitTime) { return 0; }
    uint32 XlWaitForMultipleSyncObjects(uint32 waitCount, XlHandle waitObjects[], bool waitAll, uint32 waitTime, bool alterable) { return 0; }

    bool XlGetCurrentDirectory(uint32 dim, char dst[])
    {
        if (dim > 0) dst[0] = '\0';
        return false;
    }
    bool XlGetCurrentDirectory(uint32 dim, ucs2 dst[])
    {
        if (dim > 0) dst[0] = '\0';
        return false;
    }
    uint64 XlGetCurrentFileTime() { return 0; }

    void XlGetProcessPath(utf8 dst[], size_t bufferCount)
    {
        if (bufferCount > 0) dst[0] = '\0';
    }

    void XlGetProcessPath(ucs2 dst[], size_t bufferCount)
    {
        if (bufferCount > 0) dst[0] = '\0';
    }

    void XlChDir(const utf8 path[]) {}
    void XlChDir(const ucs2 path[]) {}

    const char* XlGetCommandLine() { return ""; }

    ModuleId GetCurrentModuleId() { return 0; }

}
