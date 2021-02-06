// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../../RenderCore/Metal/Metal.h"

#if GFXAPI_TARGET == GFXAPI_APPLEMETAL
	#include "MetalTestShaders_MSL.h"
#elif GFXAPI_TARGET == GFXAPI_OPENGLES
	#include "MetalTestShaders_GLSL.h"
#elif GFXAPI_TARGET == GFXAPI_DX11
	#include "MetalTestShaders_HLSL.h"
#elif GFXAPI_TARGET == GFXAPI_VULKAN
    // Vulkan can work with multiple different high level shader languages
    // Currently we're using HLSL with Vulkan
    #include "MetalTestShaders_HLSL.h"
#else
	#error Unit test shaders not written for this graphics API
#endif
