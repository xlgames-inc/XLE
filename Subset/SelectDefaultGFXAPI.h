// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/SelectConfiguration.h"

#if !defined(SELECT_APPLEMETAL) && !defined(SELECT_OPENGL)
    #if (PLATFORMOS_TARGET == PLATFORMOS_IOS)
        // Use Apple Metal on 64 IOS devices, and fall back to GL on 32 bit devices
        #if TARGET_64BIT
            #define SELECT_APPLEMETAL
        #else
            #define SELECT_OPENGL
        #endif
    #elif (PLATFORMOS_TARGET == PLATFORMOS_OSX)
        // Use Apple Metal by default on OSX
        #define SELECT_APPLEMETAL
    #else
        // Fall back to OGL on Android and Windows
        #define SELECT_OPENGL
    #endif
#endif

