// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

    // // // //      Flexible interfaces configuration      // // // //
    #if defined(_DEBUG)
                #define FLEX_USE_VTABLE_Manager     1
    #else
                #define FLEX_USE_VTABLE_Manager     0
    #endif

namespace BufferUploads
{
    #define FLEX_INTERFACE Manager
        #include "../RenderCore/FlexForward.h"
    #undef FLEX_INTERFACE
}

