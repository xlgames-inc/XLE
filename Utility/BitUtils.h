// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ArithmeticUtils.h"
#include "../Core/Types.h"

namespace Utility
{
    template <typename Type> 
        inline bool IsPowerOfTwo(Type x)
    {
            //  powers of two should have only 1 bit set... We can check using standard
            //  bit twiddling check...
        return (x & (x - 1)) == 0;
    }

    //      We can use the "bsr" instruction (if it's available) for 
    //      calculating the integer log 2. Let's use the abstractions for
    //      bsr in ArithmeticUtils.h.
    //
    //      Note that currently the Win32 implementation of bsr has a 
    //      number of conditions, which should optimise out if inlining
    //      works correctly.
    //
    //      See alternative methods for calculating this via well-known
    //      bit twiddling web site:
    //          https://graphics.stanford.edu/~seander/bithacks.html

    inline uint32 IntegerLog2(uint8 x)
    {
        return xl_bsr1(x);
    }

    inline uint32 IntegerLog2(uint16 x)
    {
        return xl_bsr2(x);
    }

    inline uint32 IntegerLog2(uint32 x)
    {
        return xl_bsr4(x);
    }

    inline uint32 IntegerLog2(uint64 x)
    {
        return xl_bsr8(x);
    }

    inline uint32 LeastSignificantBitSet(uint64 input)
    {
            // (same as count-trailing-zeroes)
        return xl_ctz8(input);
    }
}

using namespace Utility;

