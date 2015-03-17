// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Core/SelectConfiguration.h"

#define THREAD_LIBRARY_TBB              1
#define THREAD_LIBRARY_TINYTHREAD       2
#define THREAD_LIBRARY_STDCPP           3

#if COMPILER_ACTIVE == COMPILER_TYPE_MSVC
    #if _MSC_VER >= 1700
        #define THREAD_LIBRARY THREAD_LIBRARY_STDCPP
    #else
            // older versions of Visual Studio standard library don't have threading support types
        #define THREAD_LIBRARY THREAD_LIBRARY_TINYTHREAD
    #endif
#else
    #define THREAD_LIBRARY THREAD_LIBRARY_STDCPP
#endif

