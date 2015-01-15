// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"

namespace Utility
{
    typedef uint32 Millisecond;     // 1/1000 of a second
    typedef uint64 Microsecond;     // 1/1000000 of a second

    Millisecond     Millisecond_Now();
    Microsecond     Microsecond_Now();

    uint64          GetPerformanceCounter();
    uint64          GetPerformanceCounterFrequency();
}

using namespace Utility;

