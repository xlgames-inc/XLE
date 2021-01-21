// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Core/SelectConfiguration.h"

#if !defined(SELECT_APPLEMETAL) && !defined(SELECT_VULKAN) && !defined(SELECT_DX) && !defined(SELECT_OPENGL)
    // If you hit the following error, it probably means that the project being compiled is not configured
    // for use with the RenderCore::Metal namespace. This could either be intentional (ie, it's a project
    // that should be API-agnostic) or it might just be that the project hasn't been configured yet.
    // If you hit this error, check the project being compiled and check which configuration options
    // have been selected
    #error None of the "SELECT_..." macros are defined when including Metal.h. These macros determine which graphics API is used when accessing the RenderCore::Metal namespace.
#endif

#define GFXAPI_DX11         1
#define GFXAPI_DX9          2
#define GFXAPI_OPENGLES     3
#define GFXAPI_VULKAN		4
#define GFXAPI_APPLEMETAL   5

#if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
    
    #if defined(SELECT_OPENGL)
        #define GFXAPI_TARGET   GFXAPI_OPENGLES
	#elif defined(SELECT_VULKAN)
		#define GFXAPI_TARGET   GFXAPI_VULKAN
    #else
        #define GFXAPI_TARGET   GFXAPI_DX11
    #endif

#elif PLATFORMOS_TARGET == PLATFORMOS_IOS || PLATFORMOS_TARGET == PLATFORMOS_OSX

    #if defined(SELECT_APPLEMETAL)
        #define GFXAPI_TARGET   GFXAPI_APPLEMETAL
    #else
        #define GFXAPI_TARGET   GFXAPI_OPENGLES
    #endif

#elif PLATFORMOS_TARGET == PLATFORMOS_ANDROID

	#if defined(SELECT_VULKAN)
		#define GFXAPI_TARGET   GFXAPI_VULKAN
	#else
		#define GFXAPI_TARGET   GFXAPI_OPENGLES
	#endif

#elif PLATFORMOS_TARGET == PLATFORMOS_LINUX

    #if defined(SELECT_OPENGL)
		#define GFXAPI_TARGET   GFXAPI_OPENGLES
	#else
        #define GFXAPI_TARGET   GFXAPI_VULKAN
	#endif

#endif

// #define _PSTE(X,Y) X##Y
#define __STRIZE(X) #X
#define _STRIZE(X) __STRIZE(X)

#if GFXAPI_TARGET == GFXAPI_DX11
    #define METAL_HEADER(X) _STRIZE(../DX11/Metal/X)

    namespace RenderCore {
        namespace Metal_DX11 {}
        namespace Metal = Metal_DX11;
    }
#elif GFXAPI_TARGET == GFXAPI_VULKAN
	#define METAL_HEADER(X) _STRIZE(../Vulkan/Metal/X)

	namespace RenderCore {
		namespace Metal_Vulkan {}
		namespace Metal = Metal_Vulkan;
	}
#elif GFXAPI_TARGET == GFXAPI_APPLEMETAL
    #define METAL_HEADER(X) _STRIZE(../AppleMetal/Metal/X)

    namespace RenderCore {
        namespace Metal_AppleMetal {}
        namespace Metal = Metal_AppleMetal;
    }
#else
    #define METAL_HEADER(X) _STRIZE(../OpenGLES/Metal/X)

    namespace RenderCore {
        namespace Metal_OpenGLES {}
        namespace Metal = Metal_OpenGLES;
    }
#endif

