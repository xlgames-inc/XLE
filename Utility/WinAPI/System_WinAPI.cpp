// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../Core/Prefix.h"
#include "../../Core/Types.h"
#include "../Threading/LockFree.h"
#include "../StringUtils.h"
#include "../StringFormat.h"
#include "../TimeUtils.h"
#include "../../Core/WinAPI/IncludeWindows.h"
#include <process.h>
#include <share.h>
#include <time.h>

#include <Psapi.h>
#include <Shellapi.h>

namespace Utility
{

//////////////////////////////////////////////////////////////////////////

Millisecond Millisecond_Now()
{
    return GetTickCount();      //  Very inaccurate windows timer!
}

Microsecond Microsecond_Now()
{
        //  obviously this implementation is stupid!
        //  Pending more accurate implementation
        //  (this is only accurate to a few milliseconds)
        //  see also other sources for timers in windows (eg, the multi media system timer)
    return 1000ull * uint64(Millisecond_Now());
}

uint64 GetPerformanceCounter()
{
    LARGE_INTEGER i;
    QueryPerformanceCounter(&i);
    return i.QuadPart;
}

uint64 GetPerformanceCounterFrequency()
{
    LARGE_INTEGER i;
    QueryPerformanceFrequency(&i);
    return i.QuadPart;
}

//////////////////////////////////////////////////////////////////////////

void XlGetLocalTime(uint64 time, struct tm* local)
{
    __time64_t fileTime = time;
    _localtime64_s(local, &fileTime);
}

uint64 XlMakeFileTime(struct tm* local)
{
    return _mktime64(local);
}

uint64 XlGetCurrentFileTime()
{
    return _time64(0);
}

double XlDiffTime(uint64 endTime, uint64 beginTime)
{
    return _difftime64(endTime, beginTime);
}

//////////////////////////////////////////////////////////////////////////
uint32 XlGetCurrentThreadId()
{
    return GetCurrentThreadId();
}

bool XlIsCriticalSectionLocked(void* cs) 
{
    CRITICAL_SECTION* csPtr = reinterpret_cast<CRITICAL_SECTION*>(cs);
    return csPtr->RecursionCount > 0 && (DWORD)csPtr->OwningThread == XlGetCurrentThreadId();
}

static uint32 FromWinWaitResult(uint32 winResult)
{
    switch(winResult) {
    case WAIT_OBJECT_0:
        return XL_WAIT_OBJECT_0;

    case WAIT_ABANDONED:
        return XL_WAIT_ABANDONED;

    case WAIT_TIMEOUT:
        return XL_WAIT_TIMEOUT;

    default:
        if (winResult - WAIT_OBJECT_0 < XL_MAX_WAIT_OBJECTS) {
            return winResult;
        }
        return XL_WAIT_ABANDONED;
    }
}


void XlGetNumCPUs(int* physical, int* logical, int* avail)
{
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);

    if (physical) {
        //shouldbe_implemented();
        *physical = (int)sys_info.dwNumberOfProcessors / 2; 
    }

    if (logical) {
        *logical = (int)sys_info.dwNumberOfProcessors;
    }
    if (avail) {
        *avail = (int)sys_info.dwNumberOfProcessors;   
    }

}

uint32 XlGetCurrentProcessId()
{
    return GetCurrentProcessId();
}

bool XlGetCurrentDirectory(uint32 nBufferLength, char lpBuffer[])
{
	return GetCurrentDirectoryA((DWORD)nBufferLength, lpBuffer) != FALSE;
}

bool XlGetCurrentDirectory(uint32 nBufferLength, ucs2* lpBuffer[])
{
    return GetCurrentDirectoryW((DWORD)nBufferLength, (wchar_t*)lpBuffer) != FALSE;
}

bool XlCloseSyncObject(XlHandle h)
{
    BOOL closeResult = CloseHandle(h);
    return closeResult != FALSE;
}

uint32 XlWaitForSyncObject(XlHandle h, uint32 waitTime)
{
    return FromWinWaitResult(WaitForSingleObject(h, waitTime));
}

bool XlReleaseMutex(XlHandle h)
{
    return ReleaseMutex(h) != FALSE;
}

XlHandle XlCreateSemaphore(int maxCount)
{
    HANDLE h = CreateSemaphore(NULL, 0, maxCount, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    return h;
}

uint32 XlSignalAndWait(XlHandle hSig, XlHandle hWait, uint32 waitTime)
{
    return FromWinWaitResult(SignalObjectAndWait(hSig, hWait, waitTime, FALSE));
}

bool XlReleaseSemaphore(XlHandle h, int releaseCount, int* previousCount)
{
    return ReleaseSemaphore(h, releaseCount, (LPLONG)previousCount) != FALSE;
}

XlHandle XlCreateEvent(bool manualReset)
{
    HANDLE h = CreateEvent(NULL, manualReset, FALSE, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    return h;
}

bool XlResetEvent(XlHandle h)
{
    return ResetEvent(h) != FALSE;
}

bool XlSetEvent(XlHandle h)
{
    return SetEvent(h) != FALSE;
}

bool XlPulseEvent(XlHandle h)
{
    return PulseEvent(h) != FALSE;
}

void XlOutputDebugString(const char* format)
{
    ::OutputDebugStringA(format);
}

void XlMessageBox(const char* content, const char* title)
{
    ::MessageBoxA(nullptr, content, title, MB_OK);
}

uint32 XlWaitForMultipleSyncObjects(uint32 waitCount, XlHandle waitObjects[], bool waitAll, uint32 waitTime, bool alterable)
{
    return FromWinWaitResult(WaitForMultipleObjectsEx(waitCount, waitObjects, waitAll ? TRUE : FALSE, waitTime, alterable));
}

void XlGetProcessPath(utf8 dst[], size_t bufferCount)    { GetModuleFileNameA(NULL, (char*)dst, (DWORD)bufferCount); }
void XlGetProcessPath(ucs2 dst[], size_t bufferCount)    { GetModuleFileNameW(NULL, (wchar_t*)dst, (DWORD)bufferCount); }
void XlChDir(const utf8 path[])                          { SetCurrentDirectoryA((const char*)path); }
void XlChDir(const ucs2 path[])                          { SetCurrentDirectoryW((const wchar_t*)path); }

#if 0

void XlStartSelfProcess(const char* commandLine, int delaySec, bool terminateSelf)
{
    if (delaySec < 0) {
        delaySec = 0;
    }

    if (!commandLine) {
        commandLine = GetCommandLineA();

        // skip first argument
        while (*commandLine && !XlIsSpace(*commandLine)) {
            ++commandLine;
        }

        // trim blank
        while (*commandLine && XlIsSpace(*commandLine)) {
            ++commandLine;
        }
    }

    char pathBuf[1024];
    GetTempPathA(dimof(pathBuf), pathBuf);

    char tempFileName[256];
    XlFormatString(tempFileName, dimof(tempFileName), "__resetart_%u", XlGetCurrentProcessId());
    
    char tempFilePathName[1024];
    XlMakePath(tempFilePathName, NULL, pathBuf, tempFileName, "bat");

    XLHFILE file = XlOpenFile(tempFilePathName, "w");
    if (IS_VALID_XLHFILE(file)) {
        char buf[2048];
        XlFormatString(buf, dimof(buf), "sleep %d\r\n", delaySec);
        XlWriteFile(file, buf, (uint32)XlStringSize(buf));

        char processImage[1024];
        GetModuleFileNameExA(XlGetCurrentProcess(), NULL, processImage, dimof(processImage));
        XlFormatString(buf, dimof(buf), "start %s %s\r\n", processImage, commandLine);
        XlWriteFile(file, buf, (uint32)XlStringSize(buf));

        XlFormatString(buf, dimof(buf), "sleep 10\r\n", delaySec);
        XlWriteFile(file, buf, (uint32)XlStringSize(buf));

        XlCloseFile(file);
        ShellExecute(NULL, NULL, tempFilePathName, NULL, NULL, SW_SHOW);

        if (terminateSelf) {
            TerminateProcess(XlGetCurrentProcess(), 0);
        }
    }
}

// win32 condition-variable work-around

void (WINAPI *InitializeConditionVariable_fn)(PCONDITION_VARIABLE) = NULL;
BOOL (WINAPI *SleepConditionVariableCS_fn)(PCONDITION_VARIABLE, PCRITICAL_SECTION, DWORD) = NULL;
void (WINAPI *WakeAllConditionVariable_fn)(PCONDITION_VARIABLE) = NULL;
void (WINAPI *WakeConditionVariable_fn)(PCONDITION_VARIABLE) = NULL;
void (WINAPI *DestroyConditionVariable_fn)(PCONDITION_VARIABLE) = NULL;

static void WINAPI dummy(PCONDITION_VARIABLE)
{
	// do nothing!
}

static void WINAPI intl_win32_cond_init(PCONDITION_VARIABLE p)
{
	intl_win32_cond* cond = reinterpret_cast<intl_win32_cond*>(p);

	if (InitializeCriticalSectionAndSpinCount(&cond->lock, 400) == 0) {
		return;
	}
	if ((cond->event = CreateEvent(NULL,TRUE,FALSE,NULL)) == NULL) {
		DeleteCriticalSection(&cond->lock);
		return;
	}

	cond->n_waiting = cond->n_to_wake = cond->generation = 0;
}


static void WINAPI intl_win32_cond_destroy(PCONDITION_VARIABLE p)
{
	intl_win32_cond* cond = reinterpret_cast<intl_win32_cond*>(p);
	DeleteCriticalSection(&cond->lock);
	CloseHandle(cond->event);
}

static BOOL WINAPI intl_win32_cond_wait(PCONDITION_VARIABLE p, PCRITICAL_SECTION m, DWORD ms)
{
	intl_win32_cond *cond = reinterpret_cast<intl_win32_cond*>(p);

	int generation_at_start;
	int waiting = 1;
	BOOL result = FALSE;

	DWORD start, end;
	DWORD org = ms;

	EnterCriticalSection(&cond->lock);
	++cond->n_waiting;
	generation_at_start = cond->generation;
	LeaveCriticalSection(&cond->lock);

	LeaveCriticalSection(m);

	start = GetTickCount();
	do {
		DWORD res;
		res = WaitForSingleObject(cond->event, ms);
		EnterCriticalSection(&cond->lock);
		if (cond->n_to_wake &&
		    cond->generation != generation_at_start) {
			--cond->n_to_wake;
			--cond->n_waiting;
			result = 2;
			waiting = 0;
			goto out;
		} else if (res != WAIT_OBJECT_0) {
			result = (res==WAIT_TIMEOUT) ? TRUE : FALSE;
			--cond->n_waiting;
			waiting = 0;
			goto out;
		} else if (ms != INFINITE) {
			end = GetTickCount();
			if (start + org <= end) {
				result = 1; /* Timeout */
				--cond->n_waiting;
				waiting = 0;
				goto out;
			} else {
				ms = start + org - end;
			}
		}
		/* If we make it here, we are still waiting. */
		if (cond->n_to_wake == 0) {
			/* There is nobody else who should wake up; reset
			 * the event. */
			ResetEvent(cond->event);
		}
	out:
		LeaveCriticalSection(&cond->lock);
	} while (waiting);

	EnterCriticalSection(m);

	EnterCriticalSection(&cond->lock);
	if (!cond->n_waiting)
		ResetEvent(cond->event);
	LeaveCriticalSection(&cond->lock);

	return result;
}


static void WINAPI intl_win32_cond_wake(PCONDITION_VARIABLE p) 
{
	intl_win32_cond* cond = reinterpret_cast<intl_win32_cond*>(p);
	EnterCriticalSection(&cond->lock);

	++cond->n_to_wake;
	cond->generation++;
	SetEvent(cond->event);
	LeaveCriticalSection(&cond->lock);
}

static void WINAPI intl_win32_cond_wake_all(PCONDITION_VARIABLE p) 
{
	intl_win32_cond* cond = reinterpret_cast<intl_win32_cond*>(p);
	EnterCriticalSection(&cond->lock);

	cond->n_to_wake = cond->n_waiting;

	cond->generation++;
	SetEvent(cond->event);
	LeaveCriticalSection(&cond->lock);
}

static struct win32_condvar_init
{

	win32_condvar_init() 
	{

		HMODULE h = GetModuleHandle(TEXT("kernel32.dll"));
		if (h == NULL) {
			return;
		}

	#define LOAD(name)				\
		(*((FARPROC*)&name##_fn)) = GetProcAddress(h, #name)

		LOAD(InitializeConditionVariable);
		LOAD(SleepConditionVariableCS);
		LOAD(WakeAllConditionVariable);
		LOAD(WakeConditionVariable);

		bool res =  InitializeConditionVariable_fn && 
					SleepConditionVariableCS_fn &&
					WakeConditionVariable_fn &&
					WakeAllConditionVariable_fn; 

		if (!res) {
			InitializeConditionVariable_fn = intl_win32_cond_init;
			DestroyConditionVariable_fn = intl_win32_cond_destroy;
			SleepConditionVariableCS_fn = intl_win32_cond_wait;
			WakeConditionVariable_fn = intl_win32_cond_wake;
			WakeAllConditionVariable_fn =intl_win32_cond_wake_all;
		} else {
			DestroyConditionVariable_fn = dummy;
		}
	}
} prepare_condvar;

#endif

}
