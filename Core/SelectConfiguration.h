// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <ciso646>      // (for standard library detection)

////////////////////////////////////////////////////////////////////////////////////////////////

#define CLIBRARIES_MSVC     1
#define CLIBRARIES_GCC      2
#define CLIBRARIES_LIBCPP   3

#define STL_MSVC            1
#define STL_GCC             2
#define STL_LIBCPP          3

#define COMPILER_TYPE_MSVC       1
#define COMPILER_TYPE_GCC        2
#define COMPILER_TYPE_CLANG      3

#if defined(__clang__)
    #define COMPILER_ACTIVE     COMPILER_TYPE_CLANG
#elif defined(__GNUC__)
    #define COMPILER_ACTIVE     COMPILER_TYPE_GCC
#elif defined(_MSC_VER)
    #define COMPILER_ACTIVE     COMPILER_TYPE_MSVC
#else
    #error "Cannot determine current compiler type. Platform unsupported!"
#endif

#if (_LIBCPP_VERSION)
    #define CLIBRARIES_ACTIVE   CLIBRARIES_LIBCPP
    #define STL_ACTIVE          STL_LIBCPP
#elif defined(__GLIBCXX__)
    #define CLIBRARIES_ACTIVE   CLIBRARIES_GCC
    #define STL_ACTIVE          STL_GCC
#elif defined(_CPPLIB_VER)
    #define CLIBRARIES_ACTIVE   CLIBRARIES_MSVC
    #define STL_ACTIVE          STL_MSVC
#else
    #error "Cannot determine C libraries and STL type. Platform unsupported!"
#endif

////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__) || defined(__LP64__)
    #define TARGET_64BIT 1
#else
    #define TARGET_64BIT 0
#endif

#if TARGET_64BIT
    #define SIZEOF_PTR 8
#else
    #define SIZEOF_PTR 4
#endif

#if (COMPILER_ACTIVE == COMPILER_TYPE_GCC) || (COMPILER_ACTIVE == COMPILER_TYPE_CLANG)

    #if defined(__GXX_RTTI)
        #define FEATURE_RTTI    __GXX_RTTI
    #elif defined(_CPPRTTI)   // (set when ms-compatibility mode is enabled in clang)
        #define FEATURE_RTTI    1
    #else
        #define FEATURE_RTTI    0
    #endif

    #if defined(__EXCEPTIONS)
        #define FEATURE_EXCEPTIONS    __EXCEPTIONS
    #elif defined(_CPPUNWIND)   // (set when ms-compatibility mode is enabled in clang)
        #define FEATURE_EXCEPTIONS  1
    #else
        #define FEATURE_EXCEPTIONS    0
    #endif

    #define COMPILER_DEFAULT_IMPLICIT_OPERATORS 1

#elif COMPILER_ACTIVE == COMPILER_TYPE_MSVC

    #if defined(_CPPRTTI)
        #define FEATURE_RTTI    1
    #else
        #define FEATURE_RTTI    0
    #endif

    #if defined(_CPPUNWIND)
        #define FEATURE_EXCEPTIONS  1
    #else
        #define FEATURE_EXCEPTIONS  0
    #endif

	#if _MSC_VER >= 1900
		#define COMPILER_DEFAULT_IMPLICIT_OPERATORS 1
	#endif

#else

    #define FEATURE_RTTI        1      // assume it's on
    #define FEATURE_EXCEPTIONS  1

#endif

#define PLATFORMOS_WINDOWS      1
#define PLATFORMOS_ANDROID      2
#define PLATFORMOS_OSX          3
#define PLATFORMOS_IOS          4
#define PLATFORMOS_LINUX        5

#if defined(__ANDROID__)

    #define PLATFORMOS_ACTIVE   PLATFORMOS_ANDROID
    #define PLATFORMOS_TARGET   PLATFORMOS_ANDROID

#elif defined(_WIN32) || defined(_WIN64)

    #define PLATFORMOS_ACTIVE   PLATFORMOS_WINDOWS
    #define PLATFORMOS_TARGET   PLATFORMOS_WINDOWS

#elif __APPLE__

    #include "TargetConditionals.h"
    #if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
        #define PLATFORMOS_ACTIVE   PLATFORMOS_IOS
        #define PLATFORMOS_TARGET   PLATFORMOS_IOS
    #elif TARGET_OS_MAC
        #define PLATFORMOS_ACTIVE   PLATFORMOS_OSX
        #define PLATFORMOS_TARGET   PLATFORMOS_OSX
    #else
        #error "Unknown Apple platform"
    #endif

#elif __linux__

    #define PLATFORMOS_ACTIVE   PLATFORMOS_LINUX
    #define PLATFORMOS_TARGET   PLATFORMOS_LINUX

#else

    #error "Cannot determine platform OS. Platform unsupported!"

#endif

#define ENDIANNESS_LITTLE       1
#define ENDIANNESS_BIG          2

       //   All our targets are little endian (currently)
#define ENDIANNESS_ACTIVE       ENDIANNESS_LITTLE
#define ENDIANNESS_TARGET       ENDIANNESS_LITTLE

