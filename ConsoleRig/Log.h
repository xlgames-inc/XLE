// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/SelectConfiguration.h"

#pragma push_macro("ScopedLock")
#undef ScopedLock

    // using "easylogging++" library for simple logging output
    //  Hacks to prevent inclusion of windows.h via "easylogging++.h"
    //  (it's ugly, but in this case, it might worth it. Log.h should
    //  be included into many files, possibiliy even the vast majority
    //  of files. It would be better if we didn't have to include windows.h
    //  into every file.)
    //  Another solution is to use a logging library that will move
    //  this os-level code into an out-of-line file (and perhaps provide
    //  better functionality for redirecting log messages to different
    //  places!)
#if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS

    #if TARGET_64BIT
            // 64 bit version of easylogging++ requires windows.h!
        #include "../Core/WinAPI/IncludeWindows.h"
    #endif

    #pragma push_macro("WINAPI")
    #pragma push_macro("WINBASEAPI")
    #pragma push_macro("DECLSPEC_IMPORT")
    #pragma push_macro("INVALID_FILE_ATTRIBUTES")
    #pragma push_macro("FILE_ATTRIBUTE_DIRECTORY")
    #pragma push_macro("LOCALE_USER_DEFAULT")

    #if !defined(WINAPI)
        #define WINAPI      __stdcall
    #endif
    #if !defined(WINBASEAPI)
        #define DECLSPEC_IMPORT __declspec(dllimport)
        #define WINBASEAPI DECLSPEC_IMPORT
    #endif
    #if !defined(INVALID_FILE_ATTRIBUTES)
        #define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
        #define FILE_ATTRIBUTE_DIRECTORY 0x00000010
        #define LOCALE_USER_DEFAULT (0x2 << 10)
    #endif
    typedef unsigned long       DWORD;
    typedef unsigned short      WORD;
    extern "C" WINBASEAPI void WINAPI Sleep(DWORD );
    extern "C" WINBASEAPI DWORD WINAPI GetCurrentThreadId(void);
    extern "C" WINBASEAPI DWORD WINAPI GetEnvironmentVariableA(const char*, char*, DWORD);
    extern "C" WINBASEAPI DWORD WINAPI GetFileAttributesA(const char*);
    extern "C" WINBASEAPI DWORD WINAPI GetTickCount(void);

    #include "../Foreign/easyloggingpp/easylogging++.h"

    #undef WINAPI
    #undef WINBASEAPI
    #undef DECLSPEC_IMPORT
    #undef INVALID_FILE_ATTRIBUTES
    #undef FILE_ATTRIBUTE_DIRECTORY
    #undef LOCALE_USER_DEFAULT
    
    #pragma pop_macro("LOCALE_USER_DEFAULT")
    #pragma pop_macro("FILE_ATTRIBUTE_DIRECTORY")
    #pragma pop_macro("INVALID_FILE_ATTRIBUTES")
    #pragma pop_macro("DECLSPEC_IMPORT")
    #pragma pop_macro("WINBASEAPI")
    #pragma pop_macro("WINAPI")
#else
    #include "easylogging++.h"
#endif

#pragma pop_macro("ScopedLock")

#if defined(_DEBUG)
    #define DEBUG_LOGGING_ENABLED
#endif

namespace ConsoleRig
{
        //
        //  Note that there are 2 types of logging macros
        //      * macros that are only enabled in debug builds
        //      * macros that are enabled in debug and release/profile builds
        //
        //  There are a few different levels of severity:
        //      * Verbose   (debug-only and always)
        //      * Info      (debug-only and always)
        //      * Warning   (debug-only and always)
        //      * Error     (always)
        //      * Fatal     (always)
        //
        //  This means that any severity level can be used in release/profile builds.
        //  But the Error & Fatal severity modes are always enabled. This makes sense
        //  because if there are important errors, they should always be reported, 
        //  regardless of the build mode.
        //

    #if defined(DEBUG_LOGGING_ENABLED)

        #define LogVerbose(L)   LVERBOSE(L)
        #define LogInfo         LINFO
        #define LogWarning      LWARNING
        
        #define LogVerboseEveryN(L)   LVERBOSE_EVERY_N(L)
        #define LogInfoEveryN         LINFO_EVERY_N
        #define LogWarningEveryN      LWARNING_EVERY_N

    #else

            // suppress logging statements in the log when disabled
        namespace Internal
        {
            class DummyStream {};
        }

        template <typename T> 
            inline Internal::DummyStream operator<<(Internal::DummyStream stream, T) 
                { return stream; }

        #define LogVerbose(L)   LVERBOSE(L)
        #define LogInfo         LINFO
        #define LogWarning      LWARNING

        #define LogVerboseEveryN(L)   LVERBOSE_EVERY_N(L)
        #define LogInfoEveryN         LINFO_EVERY_N
        #define LogWarningEveryN      LWARNING_EVERY_N

    #endif

    #define LogAlwaysVerbose(L)   LVERBOSE(L)
    #define LogAlwaysInfo         LINFO
    #define LogAlwaysWarning      LWARNING
    #define LogAlwaysError        LERROR
    #define LogAlwaysFatal        LFATAL

    #define LogAlwaysVerboseEveryN(L)   LVERBOSE_EVERY_N(L)
    #define LogAlwaysInfoEveryN         LINFO_EVERY_N
    #define LogAlwaysWarningEveryN      LWARNING_EVERY_N
    #define LogAlwaysErrorEveryN        LERROR_EVERY_N
    #define LogAlwaysFatalEveryN        LFATAL_EVERY_N

    /// <summary>Initialise the logging system</summary>
    /// No shutdown necessary. Provide a filename for an optional configuration file.
    /// The configuration file should be defined in the format defined by the 
    /// "easylogging++" library
    void Logging_Startup(const char configFile[] = nullptr, const char logFileName[] = nullptr);
    void Logging_Shutdown();
}

namespace LogUtilMethods
{
        // compatibility methods for an older interface
    void LogVerboseF(unsigned level, const char format[], ...);
    void LogInfoF(const char format[], ...);
    void LogWarningF(const char format[], ...);
    
    void LogAlwaysVerboseF(unsigned level, const char format[], ...);
    void LogAlwaysInfoF(const char format[], ...);
    void LogAlwaysWarningF(const char format[], ...);
    void LogAlwaysErrorF(const char format[], ...);
    void LogAlwaysFatalF(const char format[], ...);
}

using namespace LogUtilMethods;
