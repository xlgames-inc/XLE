// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Device_EGL.h"
#include "Metal/DeviceContext.h"
#include "../IAnnotator.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/BitUtils.h"
#include "../../Core/Exceptions.h"
#include <type_traits>
#include <iostream>
#include <sstream>
#include <assert.h>
#include <experimental/optional>
#include "Metal/IncludeGLES.h"

namespace RenderCore { namespace ImplOpenGLES
{
    static Metal_OpenGLES::FeatureSet::BitField AsGLESFeatureSet(unsigned glesVersion)
    {
        Metal_OpenGLES::FeatureSet::BitField featureSet = 0;
        if (glesVersion >= 200u) {
            featureSet |= Metal_OpenGLES::FeatureSet::GLES200 | Metal_OpenGLES::FeatureSet::ETC1TC;
        }
        
        if (glesVersion >= 300u) {
            featureSet |= Metal_OpenGLES::FeatureSet::GLES300 | Metal_OpenGLES::FeatureSet::ETC2TC;
        }

        // We must have an EGL context bound in order to call glGetString() on some platforms.
        assert(eglGetCurrentContext() != EGL_NO_CONTEXT);

        const char* extensionsString = (const char*)glGetString(GL_EXTENSIONS);
        if (extensionsString) {
            if (strstr(extensionsString, "AMD_compressed_ATC_texture") || strstr(extensionsString, "ATI_texture_compression_atitc")) {
                featureSet |= Metal_OpenGLES::FeatureSet::ATITC;
            }
        }

        assert(featureSet != 0);
        return featureSet;
    }

//////////////////////////////////////////////////////////////////////////////////////////////////

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
        EGLint value = 0;
        auto successful = eglQueryContext(display, context, attribute, &value);
        if (successful) {
            str << "    [" << ContextAttributeToName(attribute) << "]: " << (toStringFn)(value) << std::endl;
        } else {
            str << "    Failed querying context attribute: (" << ContextAttributeToName(attribute) << ") due to error (" << ErrorToName(eglGetError()) << ")" << std::endl;
        }
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

    namespace Conv
    {
        static std::string IntToString(EGLint i) { return std::to_string(i); }
        static std::string HexIntToString(EGLint i) { char buffer[32]; XlI32toA(i, buffer, dimof(buffer), 16); return "0x" + std::string(buffer); }
        static std::string BoolToString(EGLint i) { return i ? "true" : "false"; }

        static std::string RenderableTypeBitsToString(EGLint renderableTypeBits)
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

        static std::string SurfaceTypeBitsToString(EGLint surfaceTypeBits)
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

        static Format GetTargetFormat(EGLDisplay display, EGLConfig cfg)
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

        static Format GetDepthStencilFormat(EGLDisplay display, EGLConfig cfg)
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

////////////////////////////////////////////////////////////////////////////////////////////////////

    static std::experimental::optional<EGLConfig> TryGetEGLSharedConfig(EGLDisplay display)
    {
        EGLint renderableTypesToCheck[] = {
            EGL_OPENGL_ES3_BIT, EGL_OPENGL_ES2_BIT
        };

        // Select a configuration to use for the "shared" starter context

        for (unsigned c=0; c<dimof(renderableTypesToCheck); ++c) {
            std::vector<EGLint> configAttribs;
            // Note that by requesting a conformant configuration, and setting EGL_CONFIG_CAVEAT to none,
            // we're specific requesting a conformant API
            configAttribs.push_back(EGL_RENDERABLE_TYPE); configAttribs.push_back(renderableTypesToCheck[c]);
            configAttribs.push_back(EGL_CONFORMANT); configAttribs.push_back(renderableTypesToCheck[c]);
            configAttribs.push_back(EGL_SURFACE_TYPE); configAttribs.push_back(EGL_PBUFFER_BIT);
            configAttribs.push_back(EGL_CONFIG_CAVEAT); configAttribs.push_back(EGL_NONE);
            configAttribs.push_back(EGL_NONE); configAttribs.push_back(EGL_NONE);

            EGLConfig config;
            EGLint numConfigs = 0;
            auto chooseConfigResult = eglChooseConfig(display, configAttribs.data(), &config, 1, &numConfigs);
            if (chooseConfigResult && numConfigs == 1) {
                return config;
            }
        }

        return {};
    }

    static std::experimental::optional<EGLConfig> TryGetEGLSurfaceConfig(
        EGLDisplay display, EGLint renderableType, const PresentationChainDesc& desc)
    {
        assert(desc._format == Format::R8G8B8_UNORM);

        std::vector<EGLint> configAttribs;
        configAttribs.push_back(EGL_RED_SIZE); configAttribs.push_back(8);
        configAttribs.push_back(EGL_GREEN_SIZE); configAttribs.push_back(8);
        configAttribs.push_back(EGL_BLUE_SIZE); configAttribs.push_back(8);
        configAttribs.push_back(EGL_ALPHA_SIZE); configAttribs.push_back(EGL_DONT_CARE);
        configAttribs.push_back(EGL_DEPTH_SIZE); configAttribs.push_back(24);
        configAttribs.push_back(EGL_STENCIL_SIZE); configAttribs.push_back(8);
        configAttribs.push_back(EGL_SAMPLE_BUFFERS); configAttribs.push_back(desc._samples._sampleCount > 1 ? 1 : 0);
        configAttribs.push_back(EGL_SAMPLES); configAttribs.push_back(msaaSamples._sampleCount > 1 ? msaaSamples._sampleCount : 0);
        configAttribs.push_back(EGL_RENDERABLE_TYPE); configAttribs.push_back(renderableType);
        configAttribs.push_back(EGL_SURFACE_TYPE); configAttribs.push_back(EGL_WINDOW_BIT|EGL_PBUFFER_BIT);
        configAttribs.push_back(EGL_NONE); configAttribs.push_back(EGL_NONE);

        EGLConfig config;
        EGLint numConfigs = 0;
        auto chooseConfigResult = eglChooseConfig(display, configAttribs.data(), &config, 1, &numConfigs);
        if (chooseConfigResult && numConfigs == 1)
            return config;

        if (!chooseConfigResult) {
            Log(Error) << "eglChooseConfig returned error code with error: (" << ErrorToName(eglGetError()) << ")" << std::endl;
        }

        return {};
    }

//////////////////////////////////////////////////////////////////////////////////////////////////

    static unsigned RenderableTypeAsGLESVersion(unsigned renderableType)
    {
        if (renderableType & EGL_OPENGL_ES3_BIT) return 300;
        if (renderableType & EGL_OPENGL_ES2_BIT) return 200;
        if (renderableType & EGL_OPENGL_ES_BIT)  return 100;
        return 0;
    }

    static unsigned GetGLESVersionFromConfig(EGLDisplay display, EGLConfig config)
    {
        EGLint renderableType = 0;
        bool res = eglGetConfigAttrib(display, config, EGL_RENDERABLE_TYPE, &renderableType);
        assert(res);
        return RenderableTypeAsGLESVersion(renderableType);
    }

    static unsigned GetGLESVersionFromContext(EGLDisplay display, EGLContext context)
    {
        EGLint clientVersion = 0;
        bool res = eglQueryContext(display, context, EGL_CONTEXT_CLIENT_VERSION, &clientVersion);
        assert(res);
        return clientVersion * 100;
    }

    Device::Device()
    {
        // Create EGL display object
        _display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (_display == EGL_NO_DISPLAY) {
            Throw(::Exceptions::BasicLabel("Failure while get EGL display (%s)", ErrorToName(eglGetError())));
        }

        EGLint majorVersion = 0, minorVersion = 0;
        if (!eglInitialize(_display, &majorVersion, &minorVersion)) {
            Throw(::Exceptions::BasicLabel("Failure while initializing EGL display (%s)", ErrorToName(eglGetError())));
        }
        (void)majorVersion; (void)minorVersion;

        #if defined(_DEBUG)
            Log(Verbose) << "Initializing EGL Device: " << std::endl << _display;
        #endif
    }

    Device::~Device()
    {
        _rootContext.reset();
        if (_display != EGL_NO_DISPLAY) {
            EGLBoolean result = eglTerminate(_display);
            (void)result;
            assert(result);
        }
    }

    std::unique_ptr<IPresentationChain> Device::CreatePresentationChain(const void* platformWindowHandle, const PresentationChainDesc& desc)
    {
        if (!_rootContext) {
            auto config = TryGetEGLSurfaceConfig(_display, EGL_OPENGL_ES3_BIT, desc);
            if (!config)
                config = TryGetEGLSurfaceConfig(_display, EGL_OPENGL_ES2_BIT, desc);
            if (!config)
                Throw(::Exceptions::BasicLabel("Cannot select root context configuration"));

            #if defined(_DEBUG)
                Log(Verbose) << "Root context selected config:" << std::endl;
                StreamConfig(Log(Verbose), _display, config.value());
            #endif

            _rootContextConfig = config.value();
            _glesVersion = GetGLESVersionFromConfig(_display, config.value());
            _rootContext = std::make_shared<ThreadContext>(_display, _rootContextConfig, EGL_NO_CONTEXT, shared_from_this());

            #if defined(_DEBUG)
                Log(Verbose) << "Root context:" << std::endl;
                StreamContext(Log(Verbose), _display, _rootContext->GetUnderlying());
            #endif

            auto result = std::make_unique<PresentationChain>(_display, _rootContextConfig, platformWindowHandle, desc);

            // We can only construct the object factory after the first presentation chain is created. This is because
            // we can't call eglMakeCurrent until at least one presentation chain has been constructed; and we can't
            // call any opengl functions until we call eglMakeCurrent. In particular, we can't call glGetString(), which is
            // required to calculate the feature set used to construct the object factory.
            auto featureSet = _rootContext->GetDeviceContext()->GetFeatureSet();
            assert(!_objectFactory);
            _objectFactory = std::make_shared<Metal_OpenGLES::ObjectFactory>(featureSet);
            return result;
        } else {
            // Ensure that the requested parameters are compatible with the root config that has
            // already been created.
            // If they match, we can just return a new presentation chain
            return std::make_unique<PresentationChain>(_display, _rootContextConfig, platformWindowHandle, desc);
        }
    }

    std::shared_ptr<Metal_OpenGLES::ObjectFactory> Device::GetObjectFactory()
    {
        assert(_objectFactory);
        return _objectFactory;
    }

    std::shared_ptr<IThreadContext> Device::GetImmediateContext()
    {
        assert(_objectFactory && _rootContext);
        return _rootContext;
    }

    std::unique_ptr<IThreadContext> Device::CreateDeferredContext()
    {
        assert(_objectFactory);
        return std::make_unique<ThreadContext>(_display, _rootContextConfig, _rootContext->GetUnderlying(), shared_from_this());
    }

    FormatCapability Device::QueryFormatCapability(Format format, BindFlag::BitField bindingType)
    {
        auto activeFeatureSet = GetObjectFactory()->GetFeatureSet();
        auto glFmt = Metal_OpenGLES::AsTexelFormatType(format);
        if (glFmt._internalFormat == GL_NONE) {
            return FormatCapability::NotSupported;
        }

        bool supported = true;
        if (bindingType & BindFlag::ShaderResource) {
            supported &= !!(activeFeatureSet & glFmt._textureFeatureSet);
        } else if ((bindingType & BindFlag::RenderTarget) || (bindingType & BindFlag::DepthStencil)) {
            supported &= !!(activeFeatureSet & glFmt._renderbufferFeatureSet);
        }

        return supported ? FormatCapability::Supported : FormatCapability::NotSupported;
    }

    void* Device::QueryInterface(size_t guid)
    {
        if (guid == typeid(Device).hash_code()) {
            return (Device*)this;
        }
        return nullptr;
    }

    IResourcePtr Device::CreateResource(const ResourceDesc& desc, const ResourceInitializer& init)
    {
        assert(_objectFactory);
        return Metal_OpenGLES::CreateResource(*_objectFactory, desc, init);
    }

//////////////////////////////////////////////////////////////////////////////////////////////////

    Metal_OpenGLES::FeatureSet::BitField DeviceOpenGLES::GetFeatureSet()
    {
        return GetObjectFactory()->GetFeatureSet();
    }

    void* DeviceOpenGLES::QueryInterface(size_t guid)
    {
        if (guid == typeid(IDeviceOpenGLES).hash_code()) {
            return (IDeviceOpenGLES*)this;
        }
        return Device::QueryInterface(guid);
    }

    DeviceOpenGLES::DeviceOpenGLES() {}
    DeviceOpenGLES::~DeviceOpenGLES() {}

//////////////////////////////////////////////////////////////////////////////////////////////////

    static unsigned s_nextPresentationChainGUID = 1;

    PresentationChain::PresentationChain(EGLDisplay display, EGLConfig sharedContextCfg, const void* platformValue, const PresentationChainDesc& desc)
    {
        _guid = s_nextPresentationChainGUID++;
        _desc = std::make_shared<PresentationChainDesc>(desc);
        _display = display;

        // Create the main output surface
        // This must be tied to the window in Win32 -- so we can't begin construction of this until we build the presentation chain.
        const EGLint surfaceAttribList[] = { EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE, EGL_NONE };
        _surface = eglCreateWindowSurface(display, sharedContextCfg, EGLNativeWindowType(platformValue), surfaceAttribList);
        if (_surface == EGL_NO_SURFACE)
            Throw(::Exceptions::BasicLabel("Failure constructing EGL window surface with error: (%s)", ErrorToName(eglGetError())));
    }

    PresentationChain::~PresentationChain()
    {
        if (_surface != EGL_NO_SURFACE) {
            if (_surface == eglGetCurrentSurface(EGL_READ) || _surface == eglGetCurrentSurface(EGL_DRAW))
                eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            EGLBoolean result = eglDestroySurface(_display, _surface);
            (void)result; assert(result);
        }
    }

    void PresentationChain::Resize(unsigned newWidth, unsigned newHeight)
    {
        if (newWidth != _desc->_width && newHeight != _desc->_height) {
            _desc->_width = newWidth;
            _desc->_height = newHeight;

            _targetRenderbuffer.reset(); // it will be reconstructed on next call to GetTargetRenderbuffer()
        }
    }

    const std::shared_ptr<Metal_OpenGLES::Resource>& PresentationChain::GetTargetRenderbuffer()
    {
        if (!_targetRenderbuffer) {
            auto textureDesc = TextureDesc::Plain2D(_desc->_width, _desc->_height, _desc->_format, 1, 0, _desc->_samples);
            auto backBufferDesc = CreateDesc(BindFlag::RenderTarget, 0, GPUAccess::Write, textureDesc, "backbuffer");

            const bool useFakeBackBuffer = false;
            if (useFakeBackBuffer) {
                _targetRenderbuffer = std::make_shared<Metal_OpenGLES::Resource>(Metal_OpenGLES::GetObjectFactory(), backBufferDesc);
            } else {
                _targetRenderbuffer = std::make_shared<Metal_OpenGLES::Resource>(Metal_OpenGLES::Resource::CreateBackBuffer(backBufferDesc));
            }
        }
        return _targetRenderbuffer;
    }

//////////////////////////////////////////////////////////////////////////////////////////////////

    IResourcePtr ThreadContext::BeginFrame(IPresentationChain &presentationChain)
    {
        // Ensure that the IDevice still exists. If you run into this issue, it means that the device
        // that created this ThreadContext has already been destroyed -- which will be an issue, because
        // the device deletes _display when it is destroyed.
        assert(_device.lock());

        auto &presChain = *checked_cast<PresentationChain*>(&presentationChain);
        assert(!_activeTargetRenderbuffer);
        assert(presChain.GetDisplay() == _display);

        // Make the immediate context the current context (with the presentation chain surface)
        auto currentContext = eglGetCurrentContext();
        if (currentContext != _context || _currentPresentationChainGUID != presChain.GetGUID()) {
            if (!eglMakeCurrent(_display, presChain.GetSurface(), presChain.GetSurface(), _context))
                Throw(::Exceptions::BasicLabel("Failure in eglMakeCurrent with error: (%s)", ErrorToName(eglGetError())));
            _currentPresentationChainGUID = presChain.GetGUID();
        }

        #if 0 // defined(_DEBUG)
            const auto& presentationChainDesc = presentationChain.GetDesc();
            EGLint width, height;
            eglQuerySurface(_display, presChain.GetSurface(), EGL_WIDTH, &width);
            eglQuerySurface(_display, presChain.GetSurface(), EGL_HEIGHT, &height);

            if (width != presentationChainDesc->_width || height != presentationChainDesc->_height) {
                Log(Warning) << "Presentation chain no longer matches size of the display -- do you need to call resize()?" << std::endl;
            }
        #endif

        _activeTargetRenderbuffer = presChain.GetTargetRenderbuffer();
        if (!_activeTargetRenderbuffer->IsBackBuffer()) {
            _temporaryFramebuffer = Metal_OpenGLES::GetObjectFactory().CreateFrameBuffer();
            glBindFramebuffer(GL_FRAMEBUFFER, _temporaryFramebuffer->AsRawGLHandle());
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _activeTargetRenderbuffer->GetRenderBuffer()->AsRawGLHandle());
        }

        return _activeTargetRenderbuffer;
    }

    void ThreadContext::Present(IPresentationChain& presentationChain)
    {
        auto &presChain = *checked_cast<PresentationChain*>(&presentationChain);
        if (_activeTargetRenderbuffer) {
            // if _activeTargetRenderbuffer is not marked as the back buffer, it means we're
            // in fake-back-buffer mode. In this case, we must copy the contents to the
            // framebuffer 0, so they will be available to present
            if (!_activeTargetRenderbuffer->IsBackBuffer()) {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, _temporaryFramebuffer->AsRawGLHandle());
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glBlitFramebuffer(0, 0, presChain.GetDesc()->_width, presChain.GetDesc()->_height, 0, 0, presChain.GetDesc()->_width, presChain.GetDesc()->_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                _temporaryFramebuffer = nullptr;
            }
            eglSwapBuffers(_display, presChain.GetSurface());
        }
        _activeTargetRenderbuffer = nullptr;
    }

    std::shared_ptr<IDevice> ThreadContext::GetDevice() const
    {
        return _device.lock();
    }

    IAnnotator& ThreadContext::GetAnnotator()
    {
        if (!_annotator) {
            auto d = _device.lock();
            assert(d);
            _annotator = CreateAnnotator(*d);
        }
        return *_annotator;
    }

    void* ThreadContext::QueryInterface(size_t guid)
    {
        if (guid == typeid(EGLContext).hash_code()) {
            return _context;
        }
        if (guid == typeid(IThreadContextOpenGLES).hash_code()) {
            return (IThreadContextOpenGLES*)this;
        }
        return nullptr;
    }

    bool ThreadContext::IsBoundToCurrentThread()
    {
        EGLContext currentContext = eglGetCurrentContext();
        return _context == currentContext;
    }

    bool ThreadContext::BindToCurrentThread()
    {
        if (_context != EGL_NO_CONTEXT) {
            // Ensure that the IDevice is still alive. If you hit this assert, it means that the device
            // may have been destroyed before this call was made. Since the device cleans up _display
            // in its destructor, it will turn the subsequent eglMakeCurrent into an invalid call
            assert(_device.lock());
            return eglMakeCurrent(_display, _dummySurface, _dummySurface, _context);
        } else {
            return false;
        }
    }

    void ThreadContext::UnbindFromCurrentThread()
    {
        assert(IsBoundToCurrentThread());
        eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    std::shared_ptr<IThreadContext> ThreadContext::Clone()
    {
        // Clone is odd -- we need to return a new ThreadContext that uses the same
        // underlying EGLContext, but has a new DeviceContext
        return std::make_shared<ThreadContext>(_display, _context, _dummySurface, _device, _deviceContext->GetFeatureSet());
    }

    ThreadContext::ThreadContext(EGLDisplay display, EGLContext context, EGLSurface dummySurface, const std::weak_ptr<Device>& device, unsigned featureSet)
    : _display(display), _context(context), _dummySurface(dummySurface)
    , _device(device), _currentPresentationChainGUID(0)
    {
        _deviceContext = std::make_shared<Metal_OpenGLES::DeviceContext>(featureSet);
    }

    ThreadContext::ThreadContext(EGLDisplay display, EGLConfig cfgForNewContext, EGLContext rootContext, const std::shared_ptr<Device>& device)
    : _device(device), _currentPresentationChainGUID(0)
    , _display(display)
    {
        auto glesVersion = GetGLESVersionFromConfig(_display, cfgForNewContext);

        // Build the root context
        EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, glesVersion / 100,
            EGL_NONE, EGL_NONE
        };
        _context = eglCreateContext(_display, cfgForNewContext, EGL_NO_CONTEXT, contextAttribs);
        if (_context == EGL_NO_CONTEXT)
            Throw(::Exceptions::BasicLabel("Failure while creating EGL context (%s)", ErrorToName(eglGetError())));

        Log(Verbose) << "Created EGL context: " << std::endl;
        StreamContext(Log(Verbose), display, _context);

        int pbufferAttribsList[] = {
            EGL_WIDTH, 1,
            EGL_HEIGHT, 1,
            EGL_NONE
        };

        _dummySurface = eglCreatePbufferSurface(_display, cfgForNewContext, pbufferAttribsList);
        if (_dummySurface == EGL_NO_SURFACE)
            Throw(::Exceptions::BasicLabel("Failure in eglCreatePbufferSurface (%s)", ErrorToName(eglGetError())));

        if (!eglMakeCurrent(_display, _dummySurface, _dummySurface, _context))
            Throw(::Exceptions::BasicLabel("Failure making dummy EGL surface current with error (%s)", ErrorToName(eglGetError())));

        auto featureSet = AsGLESFeatureSet(glesVersion);
        _deviceContext = std::make_shared<Metal_OpenGLES::DeviceContext>(featureSet);
    }

    ThreadContext::~ThreadContext()
    {
        if (_dummySurface != EGL_NO_SURFACE) {
            if (_dummySurface == eglGetCurrentSurface(EGL_READ) || _dummySurface == eglGetCurrentSurface(EGL_DRAW))
                eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroySurface(_display, _dummySurface);
            _dummySurface = EGL_NO_SURFACE;
        }

        if (_context != EGL_NO_CONTEXT) {
            if (_context == eglGetCurrentContext())
                eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(_display, _context);
            _context = EGL_NO_CONTEXT;
        }
    }

//////////////////////////////////////////////////////////////////////////////////////////////////

    render_dll_export std::shared_ptr<IDevice> CreateDevice()
    {
        return std::make_shared<DeviceOpenGLES>();
    }

} }

namespace RenderCore
{
    IDeviceOpenGLES::~IDeviceOpenGLES() {}
    IThreadContextOpenGLES::~IThreadContextOpenGLES() {}
    void *IThreadContext::QueryInterface(size_t guid) { return nullptr; }
}
