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
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#define STRICT              // (note; if you get a compile error here, it means windows.h is being included from somewhere else (eg, TBB or DirectX)
#include <windows.h>
#undef max
#undef min
#undef DrawText
#undef GetObject
#undef ERROR        // causes problems with easylogging++
#endif


