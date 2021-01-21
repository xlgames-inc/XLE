// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "IncludeGLES.h"

namespace OpenGL
{
    extern PFNGLLABELOBJECTEXTPROC g_labelObject;       // pointer to glLabelObjectEXT
    extern PFNGLDRAWARRAYSINSTANCEDEXTPROC g_drawArraysInstanced;       // pointer to glDrawArraysInstancedEXT
    extern PFNGLDRAWELEMENTSINSTANCEDEXTPROC g_drawElementsInstanced;       // pointer to glDrawElementsInstancedEXT

    extern PFNGLPUSHGROUPMARKEREXTPROC g_pushGroupMarker;       // pointer to glPushGroupMarkerEXT
    extern PFNGLPOPGROUPMARKEREXTPROC g_popGroupMarker;       // pointer to glPopGroupMarkerEXT

    extern PFNGLBINDVERTEXARRAYOESPROC g_bindVertexArray; // glBindVertexArrayOES
    extern PFNGLDELETEVERTEXARRAYSOESPROC g_deleteVertexArrays; // glDeleteVertexArraysOES
    extern PFNGLGENVERTEXARRAYSOESPROC g_genVertexArrays; // glGenVertexArraysOES
    extern PFNGLISVERTEXARRAYOESPROC g_isVertexArray; // glIsVertexArrayOES
}
