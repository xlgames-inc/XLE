// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Core/SelectConfiguration.h"

#if PLATFORMOS_TARGET == PLATFORMOS_OSX
	// Emulate GLES using desktop GL
	#include <OpenGL/gl.h>
    #include <OpenGL/glext.h>
#elif PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
	// Project Angle emulation layer
	#include <GLES2/gl2.h>
	#include <GLES2/gl2ext.h>
	#include <GLES3/gl3.h>
    #define glLabelObjectEXT(...)
#elif PLATFORMOS_TARGET == PLATFORMOS_ANDROID
    #include <GLES/gl.h>
    #include <OpenGLES/ES3/gl.h>
    #include <OpenGLES/ES2/glext.h>
    #include <OpenGLES/ES3/glext.h>
    #define glClearDepth glClearDepthf
#else
	// Real GLES
	#include <OpenGLES/ES3/gl.h>
    #include <OpenGLES/ES2/glext.h>
	#include <OpenGLES/ES3/glext.h>
#endif

#include "FakeGLES.h"
