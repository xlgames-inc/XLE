// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Prefix.h"

        // // // //      Flexible interfaces configuration      // // // //
    #if defined(_DEBUG)
                #define FLEX_USE_VTABLE_ThreadContext   1
    #else
                #define FLEX_USE_VTABLE_ThreadContext    0
    #endif


namespace RenderCore
{
    #define FLEX_INTERFACE ThreadContext
#include "FlexForward.h"
    #undef FLEX_INTERFACE
}
