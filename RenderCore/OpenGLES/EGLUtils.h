// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Format.h"
#include <string>
#include <iostream>
#include <vector>
#include <EGL/egl.h>

namespace RenderCore { namespace ImplOpenGLES
{

    const char* ConfigAttributeToName(EGLint attribute);
    const char* ContextAttributeToName(EGLint attribute);
    const char* SurfaceAttributeToName(EGLint attribute);
    const char* ErrorToName(EGLint errorCode);

    unsigned RenderableTypeAsGLESVersion(unsigned renderableType);
    unsigned GetGLESVersionFromConfig(EGLDisplay display, EGLConfig config);
    unsigned GetGLESVersionFromContext(EGLDisplay display, EGLContext context);

    namespace Conv
    {
        std::string IntToString(EGLint i);
        std::string HexIntToString(EGLint i);
        std::string BoolToString(EGLint i);
        std::string RenderableTypeBitsToString(EGLint renderableTypeBits);
        std::string SurfaceTypeBitsToString(EGLint surfaceTypeBits);
        Format GetTargetFormat(EGLDisplay display, EGLConfig cfg);
        Format GetDepthStencilFormat(EGLDisplay display, EGLConfig cfg);
    }

    template<typename Stream, typename Type>
        Stream& StreamConfigAttrib(Stream& str, EGLDisplay display, EGLConfig cfg, EGLint attribute, Type toStringFn)
    {
        EGLint value = 0;
        auto successful = eglGetConfigAttrib(display, cfg, attribute, &value);
        if (successful) {
            str << "    [" << ConfigAttributeToName(attribute) << "]: " << (toStringFn)(value) << std::endl;
        } else {
            str << "    Failed querying cfg attribute: (" << ConfigAttributeToName(attribute) << ") due to error (" << ErrorToName(eglGetError()) << ")" << std::endl;
        }
        return str;
    }

    template<typename Stream, typename Type>
        Stream& StreamContextAttrib(Stream& str, EGLDisplay display, EGLContext context, EGLint attribute, Type toStringFn)
    {
        #if !defined(PGDROID)
            EGLint value = 0;
            auto successful = eglQueryContext(display, context, attribute, &value);
            if (successful) {
                str << "    [" << ContextAttributeToName(attribute) << "]: " << (toStringFn)(value) << std::endl;
            } else {
                str << "    Failed querying context attribute: (" << ContextAttributeToName(attribute) << ") due to error (" << ErrorToName(eglGetError()) << ")" << std::endl;
            }
        #endif
        return str;
    }

    template<typename Stream, typename Type>
        Stream& StreamSurfaceAttrib(Stream& str, EGLDisplay display, EGLSurface surface, EGLint attribute, Type toStringFn)
    {
        EGLint value = 0;
        auto successful = eglQuerySurface(display, surface, attribute, &value);
        if (successful) {
            str << "    [" << SurfaceAttributeToName(attribute) << "]: " << (toStringFn)(value) << std::endl;
        } else {
            str << "    Failed querying surface attribute: (" << SurfaceAttributeToName(attribute) << ") due to error (" << ErrorToName(eglGetError()) << ")" << std::endl;
        }
        return str;
    }

    template<typename Stream>
        Stream& StreamConfig(Stream&& str, EGLDisplay display, EGLConfig cfg)
    {
        StreamConfigAttrib(str, display, cfg, EGL_ALPHA_SIZE, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_ALPHA_MASK_SIZE, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_BIND_TO_TEXTURE_RGB, Conv::BoolToString);
        StreamConfigAttrib(str, display, cfg, EGL_BIND_TO_TEXTURE_RGBA, Conv::BoolToString);
        StreamConfigAttrib(str, display, cfg, EGL_BLUE_SIZE, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_BUFFER_SIZE, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_COLOR_BUFFER_TYPE, [](EGLint i) -> std::string {
            switch (i) {
            case EGL_RGB_BUFFER: return "EGL_RGB_BUFFER";
            case EGL_LUMINANCE_BUFFER: return "EGL_LUMINANCE_BUFFER";
            default: return std::string("<<unknown>>: ") + Conv::IntToString(i);
            }});
        StreamConfigAttrib(str, display, cfg, EGL_CONFIG_CAVEAT, [](EGLint i) -> std::string {
            switch (i) {
            case EGL_NONE: return "EGL_NONE";
            case EGL_SLOW_CONFIG: return "EGL_SLOW_CONFIG";
            #if defined(EGL_NON_CONFORMANT_CONFIG)
                case EGL_NON_CONFORMANT_CONFIG:  return "EGL_NON_CONFORMANT_CONFIG";        // (from our headers)
            #endif
            #if defined(EGL_NON_CONFORMANT)
                case EGL_NON_CONFORMANT: return "EGL_NON_CONFORMANT";                    // (from the spec)
            #endif
            default: return std::string("<<unknown>>: ") + Conv::IntToString(i);
            }});
        StreamConfigAttrib(str, display, cfg, EGL_CONFIG_ID, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_CONFORMANT, Conv::RenderableTypeBitsToString);
        StreamConfigAttrib(str, display, cfg, EGL_DEPTH_SIZE, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_GREEN_SIZE, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_LEVEL, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_LUMINANCE_SIZE, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_MAX_PBUFFER_WIDTH, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_MAX_PBUFFER_HEIGHT, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_MAX_PBUFFER_PIXELS, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_MAX_SWAP_INTERVAL, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_MIN_SWAP_INTERVAL, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_NATIVE_RENDERABLE, Conv::BoolToString);
        StreamConfigAttrib(str, display, cfg, EGL_NATIVE_VISUAL_ID, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_NATIVE_VISUAL_TYPE, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_RED_SIZE, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_RENDERABLE_TYPE, Conv::RenderableTypeBitsToString);
        StreamConfigAttrib(str, display, cfg, EGL_SAMPLE_BUFFERS, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_SAMPLES, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_STENCIL_SIZE, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_SURFACE_TYPE, Conv::SurfaceTypeBitsToString);
        StreamConfigAttrib(str, display, cfg, EGL_TRANSPARENT_TYPE, [](EGLint i) -> std::string {
            switch (i) {
            case EGL_TRANSPARENT_RGB: return "EGL_TRANSPARENT_RGB";
            case EGL_NONE: return "EGL_NONE";
            default: return std::string("<<unknown>>: ") + Conv::IntToString(i);
            }});
        StreamConfigAttrib(str, display, cfg, EGL_TRANSPARENT_RED_VALUE, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_TRANSPARENT_GREEN_VALUE, Conv::IntToString);
        StreamConfigAttrib(str, display, cfg, EGL_TRANSPARENT_BLUE_VALUE, Conv::IntToString);

        return str;
    }

    template<typename Stream>
        Stream& StreamConfigShort(Stream&& str, EGLDisplay display, EGLConfig cfg)
    {
        EGLint renderable, width, height, surfaceType;
        EGLint samples, sampleBuffers;
        if (    eglGetConfigAttrib(display, cfg, EGL_RENDERABLE_TYPE, &renderable)
            &&  eglGetConfigAttrib(display, cfg, EGL_MAX_PBUFFER_WIDTH, &width)
            &&  eglGetConfigAttrib(display, cfg, EGL_MAX_PBUFFER_HEIGHT, &height)
            &&  eglGetConfigAttrib(display, cfg, EGL_SAMPLES, &samples)
            &&  eglGetConfigAttrib(display, cfg, EGL_SAMPLE_BUFFERS, &sampleBuffers)
            &&  eglGetConfigAttrib(display, cfg, EGL_SURFACE_TYPE, &surfaceType)) {
            str << ((renderable & EGL_OPENGL_ES3_BIT) ? "ES3 " : ((renderable & EGL_OPENGL_ES2_BIT) ? "ES2 " : ((renderable & EGL_OPENGL_ES_BIT) ? "ES1 " : "Unknown ")));
            str << AsString(Conv::GetTargetFormat(display, cfg)) << ", " << AsString(Conv::GetDepthStencilFormat(display, cfg));
            str << " " << width << "x" << height;
            if (sampleBuffers) str << "(msaa " << samples << ")";
            str << " (" << Conv::SurfaceTypeBitsToString(surfaceType) << ")";
            return str;
        }
        return str << "No short description";
    }

    template<typename Stream>
        Stream& operator<<(Stream&& str, EGLDisplay display)
    {
        str << "EGL_CLIENT_APIS: " << eglQueryString(display, EGL_CLIENT_APIS) << std::endl;
        str << "EGL_VENDOR: " << eglQueryString(display, EGL_VENDOR) << std::endl;
        str << "EGL_VERSION: " << eglQueryString(display, EGL_VERSION) << std::endl;
        str << "EGL_EXTENSIONS: " << eglQueryString(display, EGL_EXTENSIONS) << std::endl;

        // Write out some information about the available configurations

        EGLint configCount = 0;
        if (!eglGetConfigs(display, nullptr, 0, &configCount)) {
            str << "Failure getting config count for display (" << ErrorToName(eglGetError()) << ")" << std::endl;
            return str;
        }

        std::vector<EGLConfig> configs(configCount);
        if (!eglGetConfigs(display, configs.data(), configCount, &configCount)) {
            str << "Failure getting configs for display (" << ErrorToName(eglGetError()) << ")" << std::endl;
            return str;
        }

        str << "Got (" << configs.size() << ") EGL configs" << std::endl;
        for (auto c=configs.begin(); c!=configs.end(); ++c) {
            str << "[" << std::distance(configs.begin(), c) << "] ";
            StreamConfigShort(str, display, *c) << std::endl;
        }

        /*for (auto c=configs.begin(); c!=configs.end(); ++c) {
            str << "----[" << std::distance(configs.begin(), c) << "]----" << std::endl;
            StreamConfig(str, display, *c);
        }*/
        return str;
    }

    template<typename Stream>
        Stream& StreamContext(Stream&& str, EGLDisplay display, EGLContext context)
    {
        StreamContextAttrib(str, display, context, EGL_CONFIG_ID, Conv::IntToString);
        StreamContextAttrib(str, display, context, EGL_CONTEXT_CLIENT_TYPE, [](EGLint i) -> std::string {
            switch (i) {
            case EGL_OPENGL_API: return "EGL_OPENGL_API";
            case EGL_OPENGL_ES_API: return "EGL_OPENGL_ES_API";
            case EGL_OPENVG_API: return "EGL_OPENVG_API";
            default: return std::string("<<unknown>>: ") + Conv::IntToString(i);
            }});
        StreamContextAttrib(str, display, context, EGL_CONTEXT_CLIENT_VERSION, Conv::IntToString);
        StreamContextAttrib(str, display, context, EGL_RENDER_BUFFER, [](EGLint i) -> std::string {
            switch (i) {
            case EGL_SINGLE_BUFFER: return "EGL_SINGLE_BUFFER";
            case EGL_BACK_BUFFER: return "EGL_BACK_BUFFER";
            case EGL_NONE: return "EGL_NONE";
            default: return std::string("<<unknown>>: ") + Conv::IntToString(i);
            }});
        return str;
    }

    template<typename Stream>
        Stream& StreamSurface(Stream&& str, EGLDisplay display, EGLSurface surface)
    {
        StreamSurfaceAttrib(str, display, surface, EGL_CONFIG_ID, Conv::IntToString);
        StreamSurfaceAttrib(str, display, surface, EGL_HEIGHT, Conv::IntToString);
        StreamSurfaceAttrib(str, display, surface, EGL_HORIZONTAL_RESOLUTION, Conv::IntToString);
        StreamSurfaceAttrib(str, display, surface, EGL_LARGEST_PBUFFER, Conv::IntToString);
        StreamSurfaceAttrib(str, display, surface, EGL_MIPMAP_LEVEL, Conv::IntToString);
        StreamSurfaceAttrib(str, display, surface, EGL_MIPMAP_TEXTURE, Conv::BoolToString);
        StreamSurfaceAttrib(str, display, surface, EGL_MULTISAMPLE_RESOLVE, [](EGLint i) -> std::string {
            switch (i) {
            case EGL_MULTISAMPLE_RESOLVE_DEFAULT: return "EGL_MULTISAMPLE_RESOLVE_DEFAULT";
            case EGL_MULTISAMPLE_RESOLVE_BOX: return "EGL_MULTISAMPLE_RESOLVE_BOX";
            default: return std::string("<<unknown>>: ") + Conv::IntToString(i);
            }});
        StreamSurfaceAttrib(str, display, surface, EGL_PIXEL_ASPECT_RATIO, Conv::IntToString);
        StreamSurfaceAttrib(str, display, surface, EGL_RENDER_BUFFER, [](EGLint i) -> std::string {
            switch (i) {
            case EGL_BACK_BUFFER: return "EGL_BACK_BUFFER";
            case EGL_SINGLE_BUFFER: return "EGL_SINGLE_BUFFER";
            default: return std::string("<<unknown>>: ") + Conv::IntToString(i);
            }});
        StreamSurfaceAttrib(str, display, surface, EGL_SWAP_BEHAVIOR, [](EGLint i) -> std::string {
            switch (i) {
            case EGL_BUFFER_PRESERVED: return "EGL_BUFFER_PRESERVED";
            case EGL_BUFFER_DESTROYED: return "EGL_BUFFER_DESTROYED";
            default: return std::string("<<unknown>>: ") + Conv::IntToString(i);
            }});
        StreamSurfaceAttrib(str, display, surface, EGL_TEXTURE_FORMAT, [](EGLint i) -> std::string {
            switch (i) {
            case EGL_NO_TEXTURE: return "EGL_NO_TEXTURE";
            case EGL_TEXTURE_RGB: return "EGL_TEXTURE_RGB";
            case EGL_TEXTURE_RGBA: return "EGL_TEXTURE_RGBA";
            default: return std::string("<<unknown>>: ") + Conv::IntToString(i);
            }});
        StreamSurfaceAttrib(str, display, surface, EGL_TEXTURE_TARGET, [](EGLint i) -> std::string {
            switch (i) {
            case EGL_NO_TEXTURE: return "EGL_NO_TEXTURE";
            case EGL_TEXTURE_2D: return "EGL_TEXTURE_2D";
            default: return std::string("<<unknown>>: ") + Conv::IntToString(i);
            }});
        StreamSurfaceAttrib(str, display, surface, EGL_VERTICAL_RESOLUTION, Conv::IntToString);
        StreamSurfaceAttrib(str, display, surface, EGL_WIDTH, Conv::IntToString);

        return str;
    }

}}
