// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Prefix.h"
#include <limits>
#include <cstdint>

    //
    //      Common types; uniform names syntax  
    //
    //          [u]int<bit count> (eg, uint32)
    //
    //          see also "uint32_t" style names 
    //          commonly used with GCC.
    //

#ifndef __PLATFORM_INDEPENDENT_TYPE_DEFINED__
#define __PLATFORM_INDEPENDENT_TYPE_DEFINED__

typedef signed char          int8;
typedef unsigned char       uint8;
typedef short                int16;
typedef unsigned short      uint16;
typedef int                  int32;
typedef unsigned int        uint32;
typedef int64_t              int64;     // __int64
typedef uint64_t            uint64;     // unsigned __int64

#endif

typedef unsigned             uint;
typedef uint8                byte;

#if CLIBRARIES_ACTIVE == CLIBRARIES_MSVC
    #include <crtdefs.h>
    typedef uintptr_t           uintptr;
#else
    #if TARGET_64BIT
        typedef uint64          uintptr;
    #else
        typedef uint32          uintptr;
    #endif
#endif

typedef float               f32;
typedef double              f64;

#undef MIN_INT8    
#undef MAX_INT8    
#undef MIX_UINT8   
#undef MAX_UINT8   

#undef MIN_INT16   
#undef MAX_INT16   
#undef MIX_UINT16  
#undef MAX_UINT16  

#undef MIN_INT32   
#undef MAX_INT32   
#undef MIX_UINT32  
#undef MAX_UINT32  

#undef MIN_INT64   
#undef MAX_INT64   
#undef MIX_UINT64  
#undef MAX_UINT64  

#undef MIN_FLOAT32 
#undef MAX_FLOAT32 

#undef MIN_FLOAT64 
#undef MAX_FLOAT64 

#undef MIN_INT     
#undef MAX_INT     


#define MIN_INT8        std::numeric_limits<int8>::min()
#define MAX_INT8        std::numeric_limits<int8>::max()
#define MIX_UINT8       std::numeric_limits<uint8>::min()
#define MAX_UINT8       std::numeric_limits<uint8>::max()

#define MIN_INT16       std::numeric_limits<int16>::min()
#define MAX_INT16       std::numeric_limits<int16>::max()
#define MIX_UINT16      std::numeric_limits<uint16>::min()
#define MAX_UINT16      std::numeric_limits<uint16>::max()

#define MIN_INT32       std::numeric_limits<int32>::min()
#define MAX_INT32       std::numeric_limits<int32>::max()
#define MIX_UINT32      std::numeric_limits<uint32>::min()
#define MAX_UINT32      std::numeric_limits<uint32>::max()

#define MIN_INT64       std::numeric_limits<int64>::min()
#define MAX_INT64       std::numeric_limits<int64>::max()
#define MIX_UINT64      std::numeric_limits<uint64>::min()
#define MAX_UINT64      std::numeric_limits<uint64>::max()

#define MIN_FLOAT32     std::numeric_limits<float>::min()
#define MAX_FLOAT32     std::numeric_limits<float>::max()

#define MIN_FLOAT64     std::numeric_limits<double>::min()
#define MAX_FLOAT64     std::numeric_limits<double>::max()

#define MIN_INT         MIN_INT32
#define MAX_INT         MAX_INT32

