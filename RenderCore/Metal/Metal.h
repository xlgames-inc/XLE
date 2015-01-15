// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Core/SelectConfiguration.h"

#define GFXAPI_DX11         1
#define GFXAPI_DX9          2
#define GFXAPI_OPENGLES     3

#if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS

    #if defined(SELECT_OPENGL)
        #define GFXAPI_ACTIVE   GFXAPI_OPENGLES
    #else
        #define GFXAPI_ACTIVE   GFXAPI_DX11
    #endif

#elif PLATFORMOS_ACTIVE == PLATFORMOS_ANDROID
    #define GFXAPI_ACTIVE   GFXAPI_OPENGLES
#endif

#if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
    
    #if defined(SELECT_OPENGL)
        #define GFXAPI_TARGET   GFXAPI_OPENGLES
    #else
        #define GFXAPI_TARGET   GFXAPI_DX11
    #endif

#elif PLATFORMOS_TARGET == PLATFORMOS_ANDROID
    #define GFXAPI_TARGET   GFXAPI_OPENGLES
#endif

// #define _PSTE(X,Y) X##Y
#define __STRIZE(X) #X
#define _STRIZE(X) __STRIZE(X)

#if GFXAPI_ACTIVE == GFXAPI_DX11
    #define METAL_HEADER(X) _STRIZE(../DX11/Metal/X)

    namespace RenderCore {
        namespace Metal_DX11 {}
        namespace Metal = Metal_DX11;
    }
#else
    #define METAL_HEADER(X) _STRIZE(../OpenGLES/Metal/X)

    namespace RenderCore {
        namespace Metal_OpenGLES {}
        namespace Metal = Metal_OpenGLES;
    }
#endif

