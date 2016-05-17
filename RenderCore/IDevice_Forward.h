// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Prefix.h"

        // // // //      Flexible interfaces configuration      // // // //
#define FLEX_USE_VTABLE_PresentationChain    1
#define FLEX_USE_VTABLE_Device               1

namespace RenderCore
{
    #define FLEX_INTERFACE PresentationChain
#include "FlexForward.h"
    #undef FLEX_INTERFACE
    #define FLEX_INTERFACE Device
#include "FlexForward.h"
    #undef FLEX_INTERFACE
}
