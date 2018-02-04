// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Core/SelectConfiguration.h"
#include "../../../Utility/PlatformHacks.h"

#if PLATFORMOS_TARGET == PLATFORMOS_OSX
	// Emulate GLES using desktop GL
	#include <OpenGL/gl.h>
#elif PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
	// Project Angle emulation layer
	#include <GLES2/gl2.h>
#else
	// Real GLES
	#include <OpenGLES/ES3/gl.h>
	#include <OpenGLES/ES3/glext.h>
#endif
