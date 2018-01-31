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

#endif

#define GFXAPI_ACTIVE GFXAPI_TARGET

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

