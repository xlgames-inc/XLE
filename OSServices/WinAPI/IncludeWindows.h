// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

    //      (minimize exposure to Win32/Win64 API by using the LEAN macros)
    //      Also use "STRICT", which requires explicit casting between windows
    //      handle types (like HINSTANCE, HBRUSH, etc).

#if !defined(_WINDOWS_)
#pragma push_macro("LOG")
#pragma push_macro("ERROR")
#if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN
#endif
#if !defined(WIN32_EXTRA_LEAN)
    #define WIN32_EXTRA_LEAN
#endif
#if !defined(NOMINMAX)
    #define NOMINMAX
#endif
#if !defined(STRICT)
    #define STRICT              // (note; if you get a compile error here, it means windows.h is being included from somewhere else (eg, TBB or DirectX)
#endif
#undef _MM_HINT_T0              // these cause a warning when compiling with clang intrinsics
#undef _MM_HINT_T2
#include <windows.h>
#undef max
#undef min
#undef DrawText
#undef GetObject
#undef CreateSemaphore
#undef CreateEvent
#undef ERROR
#undef GetCommandLine
#undef GetCurrentDirectory
#undef MoveFile
#undef DeleteFile
#undef GetProcessPath
#pragma pop_macro("ERROR")
#pragma pop_macro("LOG")
#endif


