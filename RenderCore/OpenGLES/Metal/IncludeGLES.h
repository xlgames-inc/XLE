// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../../Utility/PlatformHacks.h"

#ifdef HACK_PLATFORM_IOS
    #import <OpenGLES/ES3/gl.h>
    #import <OpenGLES/ES3/glext.h>
#else
    #import <OpenGL/gl.h>
#endif
