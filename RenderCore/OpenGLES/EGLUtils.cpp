// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EGLUtils.h"
#include "Metal/ExtensionFunctions.h"
#include "../../Utility/BitUtils.h"
#include "../../Utility/StringFormat.h"
#include <sstream>

namespace RenderCore { namespace ImplOpenGLES
{

    const char* ConfigAttributeToName(EGLint attribute)
    {
        switch (attribute) {
        case EGL_ALPHA_SIZE: return "EGL_ALPHA_SIZE";
        case EGL_ALPHA_MASK_SIZE: return "EGL_ALPHA_MASK_SIZE";
        case EGL_BIND_TO_TEXTURE_RGB: return "EGL_BIND_TO_TEXTURE_RGB";
        case EGL_BIND_TO_TEXTURE_RGBA: return "EGL_BIND_TO_TEXTURE_RGBA";
        case EGL_BLUE_SIZE: return "EGL_BLUE_SIZE";
        case EGL_BUFFER_SIZE: return "EGL_BUFFER_SIZE";
        case EGL_COLOR_BUFFER_TYPE: return "EGL_COLOR_BUFFER_TYPE";
        case EGL_CONFIG_CAVEAT: return "EGL_CONFIG_CAVEAT";
        case EGL_CONFIG_ID: return "EGL_CONFIG_ID";
        case EGL_CONFORMANT: return "EGL_CONFORMANT";
        case EGL_DEPTH_SIZE: return "EGL_DEPTH_SIZE";
        case EGL_GREEN_SIZE: return "EGL_GREEN_SIZE";
        case EGL_LEVEL: return "EGL_LEVEL";
        case EGL_LUMINANCE_SIZE: return "EGL_LUMINANCE_SIZE";
        case EGL_MAX_PBUFFER_WIDTH: return "EGL_MAX_PBUFFER_WIDTH";
        case EGL_MAX_PBUFFER_HEIGHT: return "EGL_MAX_PBUFFER_HEIGHT";
        case EGL_MAX_PBUFFER_PIXELS: return "EGL_MAX_PBUFFER_PIXELS";
        case EGL_MAX_SWAP_INTERVAL: return "EGL_MAX_SWAP_INTERVAL";
        case EGL_MIN_SWAP_INTERVAL: return "EGL_MIN_SWAP_INTERVAL";
        case EGL_NATIVE_RENDERABLE: return "EGL_NATIVE_RENDERABLE";
        case EGL_NATIVE_VISUAL_ID: return "EGL_NATIVE_VISUAL_ID";
        case EGL_NATIVE_VISUAL_TYPE: return "EGL_NATIVE_VISUAL_TYPE";
        case EGL_RED_SIZE: return "EGL_RED_SIZE";
        case EGL_RENDERABLE_TYPE: return "EGL_RENDERABLE_TYPE";
        case EGL_SAMPLE_BUFFERS: return "EGL_SAMPLE_BUFFERS";
        case EGL_SAMPLES: return "EGL_SAMPLES";
        case EGL_STENCIL_SIZE: return "EGL_STENCIL_SIZE";
        case EGL_SURFACE_TYPE: return "EGL_SURFACE_TYPE";
        case EGL_TRANSPARENT_TYPE: return "EGL_TRANSPARENT_TYPE";
        case EGL_TRANSPARENT_RED_VALUE: return "EGL_TRANSPARENT_RED_VALUE";
        case EGL_TRANSPARENT_GREEN_VALUE: return "EGL_TRANSPARENT_GREEN_VALUE";
        case EGL_TRANSPARENT_BLUE_VALUE: return "EGL_TRANSPARENT_BLUE_VALUE";
        default: return "<<unknown>>";
        }
    }

    const char* ContextAttributeToName(EGLint attribute)
    {
        switch (attribute) {
        case EGL_CONFIG_ID: return "EGL_CONFIG_ID";
        case EGL_CONTEXT_CLIENT_TYPE: return "EGL_CONTEXT_CLIENT_TYPE";
        case EGL_CONTEXT_CLIENT_VERSION: return "EGL_CONTEXT_CLIENT_VERSION";
        case EGL_RENDER_BUFFER: return "EGL_RENDER_BUFFER";
        default: return "<<unknown>>";
        }
    }

    const char* SurfaceAttributeToName(EGLint attribute)
    {
        switch (attribute) {
        case EGL_CONFIG_ID: return "EGL_CONFIG_ID";
        case EGL_HEIGHT: return "EGL_HEIGHT";
        case EGL_HORIZONTAL_RESOLUTION: return "EGL_HORIZONTAL_RESOLUTION";
        case EGL_LARGEST_PBUFFER: return "EGL_LARGEST_PBUFFER";
        case EGL_MIPMAP_LEVEL: return "EGL_MIPMAP_LEVEL";
        case EGL_MIPMAP_TEXTURE: return "EGL_MIPMAP_TEXTURE";
        case EGL_MULTISAMPLE_RESOLVE: return "EGL_MULTISAMPLE_RESOLVE";
        case EGL_PIXEL_ASPECT_RATIO: return "EGL_PIXEL_ASPECT_RATIO";
        case EGL_RENDER_BUFFER: return "EGL_RENDER_BUFFER";
        case EGL_SWAP_BEHAVIOR: return "EGL_SWAP_BEHAVIOR";
        case EGL_TEXTURE_FORMAT: return "EGL_TEXTURE_FORMAT";
        case EGL_TEXTURE_TARGET: return "EGL_TEXTURE_TARGET";
        case EGL_VERTICAL_RESOLUTION: return "EGL_VERTICAL_RESOLUTION";
        case EGL_WIDTH: return "EGL_WIDTH";
        default: return "<<unknown>>";
        }
    }

    const char* ErrorToName(EGLint errorCode)
    {
        switch (errorCode) {
        case EGL_SUCCESS: return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED: return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS: return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC: return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE: return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT: return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG: return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY: return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE: return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH: return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER: return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST: return "EGL_CONTEXT_LOST";
        default: return "<<unknown>>";
        }
    }


    namespace Conv
    {
        std::string IntToString(EGLint i) { return std::to_string(i); }
        std::string HexIntToString(EGLint i) { char buffer[32]; XlI32toA(i, buffer, dimof(buffer), 16); return "0x" + std::string(buffer); }
        std::string BoolToString(EGLint i) { return i ? "true" : "false"; }

        std::string RenderableTypeBitsToString(EGLint renderableTypeBits)
        {
            std::pair<unsigned, const char*> bitNames[] =  {
                { IntegerLog2(uint32_t(EGL_OPENGL_ES_BIT)), "EGL_OPENGL_ES_BIT" },
                { IntegerLog2(uint32_t(EGL_OPENVG_BIT)), "EGL_OPENVG_BIT" },
                { IntegerLog2(uint32_t(EGL_OPENGL_ES2_BIT)), "EGL_OPENGL_ES2_BIT" },
                { IntegerLog2(uint32_t(EGL_OPENGL_ES3_BIT)), "EGL_OPENGL_ES3_BIT" },
                { IntegerLog2(uint32_t(EGL_OPENGL_BIT)), "EGL_OPENGL_BIT" }
            };
            std::stringstream str;
            bool started = false;
            for (unsigned c=0; c<32; ++c) {
                if (!(renderableTypeBits & (1<<c))) continue;
                if (started)
                    str << " | ";
                bool gotBitName = false;
                for (auto& n:bitNames) {
                    if (n.first == c) {
                        str << n.second;
                        started = gotBitName = true;
                        break;
                    }
                }

                if (!gotBitName)
                    str << HexIntToString(1<<c);
            }
            return str.str();
        }

        std::string SurfaceTypeBitsToString(EGLint surfaceTypeBits)
        {
            std::pair<unsigned, const char*> bitNames[] =  {
                { IntegerLog2(uint32_t(EGL_PBUFFER_BIT)), "EGL_PBUFFER_BIT" },
                { IntegerLog2(uint32_t(EGL_PIXMAP_BIT)), "EGL_PIXMAP_BIT" },
                { IntegerLog2(uint32_t(EGL_WINDOW_BIT)), "EGL_WINDOW_BIT" },
                { IntegerLog2(uint32_t(EGL_VG_COLORSPACE_LINEAR_BIT)), "EGL_VG_COLORSPACE_LINEAR_BIT" },
                { IntegerLog2(uint32_t(EGL_VG_ALPHA_FORMAT_PRE_BIT)), "EGL_VG_ALPHA_FORMAT_PRE_BIT" },
                { IntegerLog2(uint32_t(EGL_MULTISAMPLE_RESOLVE_BOX_BIT)), "EGL_MULTISAMPLE_RESOLVE_BOX_BIT" },
                { IntegerLog2(uint32_t(EGL_SWAP_BEHAVIOR_PRESERVED_BIT)), "EGL_SWAP_BEHAVIOR_PRESERVED_BIT" }
            };
            std::stringstream str;
            bool started = false;
            for (unsigned c=0; c<32; ++c) {
                if (!(surfaceTypeBits & (1<<c))) continue;
                if (started)
                    str << " | ";
                bool gotBitName = false;
                for (auto& n:bitNames) {
                    if (n.first == c) {
                        str << n.second;
                        started = gotBitName = true;
                        break;
                    }
                }

                if (!gotBitName)
                    str << HexIntToString(1<<c);
            }
            return str.str();
        }

        Format GetTargetFormat(EGLDisplay display, EGLConfig cfg)
        {
            EGLint red, green, blue, alpha;
            if (    eglGetConfigAttrib(display, cfg, EGL_RED_SIZE, &red)
                &&  eglGetConfigAttrib(display, cfg, EGL_GREEN_SIZE, &green)
                &&  eglGetConfigAttrib(display, cfg, EGL_BLUE_SIZE, &blue)
                &&  eglGetConfigAttrib(display, cfg, EGL_ALPHA_SIZE, &alpha)) {
                if (red == 5 && green == 6 && blue == 5 && alpha == 0) return Format::B5G6R5_UNORM;
                if (red == 5 && green == 5 && blue == 5 && alpha == 1) return Format::B5G5R5A1_UNORM;
                if (alpha == 0) {
                    if (red == green && blue == green)
                        return FindFormat(FormatCompressionType::None, FormatComponents::RGB, FormatComponentType::UNorm, red);
                } else {
                    if (red == green && blue == green && alpha == green)
                        return FindFormat(FormatCompressionType::None, FormatComponents::RGBAlpha, FormatComponentType::UNorm, red);
                }
            }
            return Format::Unknown;
        }

        Format GetDepthStencilFormat(EGLDisplay display, EGLConfig cfg)
        {
            EGLint depth, stencil;
            if (    eglGetConfigAttrib(display, cfg, EGL_DEPTH_SIZE, &depth)
                &&  eglGetConfigAttrib(display, cfg, EGL_STENCIL_SIZE, &stencil)) {
                if (depth == 24) return Format::D24_UNORM_S8_UINT;
                else if (depth == 32 && stencil == 0) return Format::D32_FLOAT;
                else if (depth == 32 && stencil == 8) return Format::D32_SFLOAT_S8_UINT;
                else if (depth == 16 && stencil == 0) return Format::D16_UNORM;
            }
            return Format::Unknown;
        }
    }

    unsigned RenderableTypeAsGLESVersion(unsigned renderableType)
    {
        if (renderableType & EGL_OPENGL_ES3_BIT) return 300;
        if (renderableType & EGL_OPENGL_ES2_BIT) return 200;
        if (renderableType & EGL_OPENGL_ES_BIT)  return 100;
        return 0;
    }

    unsigned GetGLESVersionFromConfig(EGLDisplay display, EGLConfig config)
    {
        EGLint renderableType = 0;
        bool res = eglGetConfigAttrib(display, config, EGL_RENDERABLE_TYPE, &renderableType);
        assert(res); (void)res;
        return RenderableTypeAsGLESVersion(renderableType);
    }

    unsigned GetGLESVersionFromContext(EGLDisplay display, EGLContext context)
    {
        EGLint clientVersion = 0;
        bool res = eglQueryContext(display, context, EGL_CONTEXT_CLIENT_VERSION, &clientVersion);
        assert(res); (void)res;
        return clientVersion * 100;
    }

    void BindExtensionFunctions()
    {
        OpenGL::g_labelObject = (PFNGLLABELOBJECTEXTPROC)eglGetProcAddress("glLabelObjectEXT");
        OpenGL::g_drawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDEXTPROC)eglGetProcAddress("glDrawArraysInstancedEXT");
        OpenGL::g_drawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDEXTPROC)eglGetProcAddress("glDrawElementsInstancedEXT");

        OpenGL::g_pushGroupMarker = (PFNGLPUSHGROUPMARKEREXTPROC)eglGetProcAddress("glPushGroupMarkerEXT");
        OpenGL::g_popGroupMarker = (PFNGLPOPGROUPMARKEREXTPROC)eglGetProcAddress("glPopGroupMarkerEXT");

        OpenGL::g_bindVertexArray = (PFNGLBINDVERTEXARRAYOESPROC)eglGetProcAddress("glBindVertexArrayOES");
        OpenGL::g_deleteVertexArrays = (PFNGLDELETEVERTEXARRAYSOESPROC)eglGetProcAddress("glDeleteVertexArraysOES");
        OpenGL::g_genVertexArrays = (PFNGLGENVERTEXARRAYSOESPROC)eglGetProcAddress("glGenVertexArraysOES");
        OpenGL::g_isVertexArray = (PFNGLISVERTEXARRAYOESPROC)eglGetProcAddress("glIsVertexArrayOES");
    }

}}
