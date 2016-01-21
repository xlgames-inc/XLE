// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////

#define CLIBRARIES_MSVC     1
#define CLIBRARIES_GCC      2

#define STL_MSVC            1
#define STL_GCC             2

#define COMPILER_TYPE_MSVC       1
#define COMPILER_TYPE_GCC        2

#if defined(__GNUC__)

    #define CLIBRARIES_ACTIVE   CLIBRARIES_GCC
    #define STL_ACTIVE          STL_GCC
    #define COMPILER_ACTIVE     COMPILER_TYPE_GCC

#elif defined(_MSC_VER)

    #define CLIBRARIES_ACTIVE   CLIBRARIES_MSVC
    #define STL_ACTIVE          STL_MSVC
    #define COMPILER_ACTIVE     COMPILER_TYPE_MSVC

#else

    #pragma error("Cannot determine C libraries and STL type. Platform unsupported!")

#endif

////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    #define TARGET_64BIT 1
#else
    #define TARGET_64BIT 0
#endif

#if TARGET_64BIT
    #define SIZEOF_PTR 8
#else
    #define SIZEOF_PTR 4
#endif

#if COMPILER_ACTIVE == COMPILER_TYPE_GCC

    #if defined(__GXX_RTTI)
        #define FEATURE_RTTI    __GXX_RTTI
    #else
        #define FEATURE_RTTI    0
    #endif

    #if defined(__EXCEPTIONS)
        #define FEATURE_EXCEPTIONS    __EXCEPTIONS
    #else
        #define FEATURE_EXCEPTIONS    0
    #endif

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

#if defined(__ANDROID__)

    #define PLATFORMOS_ACTIVE   PLATFORMOS_ANDROID
    #define PLATFORMOS_TARGET   PLATFORMOS_ANDROID

#elif defined(_WIN32) || defined(_WIN64)

    #define PLATFORMOS_ACTIVE   PLATFORMOS_WINDOWS
    #define PLATFORMOS_TARGET   PLATFORMOS_WINDOWS

#else

    #pragma error("Cannot determine platform OS. Platform unsupported!")

#endif

#define ENDIANNESS_LITTLE       1
#define ENDIANNESS_BIG          2

       //   All our targets are little endian (currently)
#define ENDIANNESS_ACTIVE       ENDIANNESS_LITTLE
#define ENDIANNESS_TARGET       ENDIANNESS_LITTLE

