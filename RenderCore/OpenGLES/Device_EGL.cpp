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
            str << "[" << ConfigAttributeToName(attribute) << "]: " << (toStringFn)(value) << std::endl;
        } else {
            str << "Failed querying attribute: (" << ConfigAttributeToName(attribute) << ") due to error (" << ErrorToName(eglGetError()) << ")" << std::endl;
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
        Stream& StreamConfigs(Stream&& str, EGLDisplay display)
    {
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
            str << "----[" << std::distance(configs.begin(), c) << "]----" << std::endl;
            StreamConfig(str, display, *c);
        }
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
            // configAttribs.push_back(EGL_SURFACE_TYPE); configAttribs.push_back(EGL_PBUFFER_BIT);
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
        EGLDisplay display, EGLint renderableType, Format colorFormat = Format::R8G8B8_UNORM,
        TextureSamples msaaSamples = TextureSamples::Create(0, 0))
    {
        assert(colorFormat == Format::R8G8B8_UNORM);

        std::vector<EGLint> configAttribs;
        configAttribs.push_back(EGL_RED_SIZE); configAttribs.push_back(8);
        configAttribs.push_back(EGL_GREEN_SIZE); configAttribs.push_back(8);
        configAttribs.push_back(EGL_BLUE_SIZE); configAttribs.push_back(8);
        configAttribs.push_back(EGL_ALPHA_SIZE); configAttribs.push_back(EGL_DONT_CARE);
        configAttribs.push_back(EGL_DEPTH_SIZE); configAttribs.push_back(24);
        configAttribs.push_back(EGL_STENCIL_SIZE); configAttribs.push_back(8);
        configAttribs.push_back(EGL_SAMPLE_BUFFERS); configAttribs.push_back(msaaSamples._sampleCount > 1 ? 1 : 0);
        configAttribs.push_back(EGL_SAMPLES); configAttribs.push_back(msaaSamples._sampleCount > 1 ? msaaSamples._sampleCount : 0);
        configAttribs.push_back(EGL_RENDERABLE_TYPE); configAttribs.push_back(renderableType);
        configAttribs.push_back(EGL_SURFACE_TYPE); configAttribs.push_back(EGL_WINDOW_BIT);
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
        if (renderableType&EGL_OPENGL_ES3_BIT) return 300;
        if (renderableType&EGL_OPENGL_ES2_BIT) return 200;
        if (renderableType&EGL_OPENGL_ES_BIT) return 100;
        return 0;
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

        StreamConfigs(Log(Warning), _display);

        // Build default EGL surface config
        auto config = TryGetEGLSharedConfig(_display);
        if (!config) {
            Throw(::Exceptions::BasicLabel("Could not select a valid configuration while starting OpenGL"));
        }
        EGLint apiRes = 0;
        bool res = eglGetConfigAttrib(_display, config.value(), EGL_RENDERABLE_TYPE, &apiRes);
        assert(res);
        if (!(apiRes&EGL_OPENGL_ES3_BIT)) {
            Throw(::Exceptions::BasicLabel("Device does not support OpenGLES3.0"));
        }

        Log(Warning) << "Immediate context selected config:" << std::endl;
        StreamConfig(Log(Warning), _display, config.value());

        _config = config.value();

        // Collect version information, useful for reporting to the log file
        #if defined(_DEBUG)
            const char* versionMain = eglQueryString(_display, EGL_VERSION);
            const char* versionVendor = eglQueryString(_display, EGL_VENDOR);
            const char* versionClientAPIs = eglQueryString(_display, EGL_CLIENT_APIS);
            const char* versionExtensions = eglQueryString(_display, EGL_EXTENSIONS);
            (void)versionMain; (void)versionVendor; (void)versionClientAPIs; (void)versionExtensions;
        #endif

        // Build the immediate context
        EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, RenderableTypeAsGLESVersion(apiRes) / 100,
            EGL_NONE, EGL_NONE
        };
        _sharedContext = eglCreateContext(_display, _config, EGL_NO_CONTEXT, contextAttribs);
        if (_sharedContext == EGL_NO_CONTEXT) {
            Throw(::Exceptions::BasicLabel("Failure while creating the immediate context (%s)", ErrorToName(eglGetError())));
        }
    }

    Device::~Device()
    {
        if (_display != EGL_NO_DISPLAY) {
            EGLBoolean result = eglTerminate(_display);
            (void)result;
            assert(result);
        }
    }

    unsigned Device::GetGLESVersion() const
    {
        EGLint renderableType = 0;
        bool res = eglGetConfigAttrib(_display, _config, EGL_RENDERABLE_TYPE, &renderableType);
        assert(res);
        return RenderableTypeAsGLESVersion(renderableType);
    }

    std::unique_ptr<IPresentationChain> Device::CreatePresentationChain(const void* platformWindowHandle, const PresentationChainDesc& desc)
    {
        auto result = std::make_unique<PresentationChain>(_display, _sharedContext, _config, platformWindowHandle, desc);
        // We can only construct the object factory after the first presentation chain is created. This is because
        // we can't call eglMakeCurrent until at least one presentation chain has been constructed; and we can't
        // call any opengl functions until we call eglMakeCurrent. In particular, we can't call glGetString(), which is
        // required to calculate the feature set used to construct the object factory.
        if (!_objectFactory) {
            auto featureSet = AsGLESFeatureSet(GetGLESVersion());
            _objectFactory = std::make_shared<Metal_OpenGLES::ObjectFactory>(featureSet);
        }

        return result;
    }

    std::shared_ptr<Metal_OpenGLES::ObjectFactory> Device::GetObjectFactory()
    {
        // We can construct the object factory before creating the first presentation chain if we're not on Android.
        #if PLATFORMOS_TARGET != PLATFORMOS_ANDROID
            if (!_objectFactory) {
                auto featureSet = AsGLESFeatureSet(GetGLESVersion());
                _objectFactory = std::make_shared<Metal_OpenGLES::ObjectFactory>(featureSet);
            }
        #endif

        assert(_objectFactory);
        return _objectFactory;
    }

    std::shared_ptr<IThreadContext> Device::GetImmediateContext()
    {
        assert(_objectFactory);
        if (!_immediateContext) {
            _immediateContext = std::make_shared<ThreadContextOpenGLES>(_sharedContext, shared_from_this());
        }
        return _immediateContext;
    }

    std::unique_ptr<IThreadContext> Device::CreateDeferredContext()
    {
        assert(_objectFactory);
        auto result = std::make_unique<ThreadContextOpenGLES>(_sharedContext, shared_from_this());
        result->MakeDeferredContext();
        return result;
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

    PresentationChain::PresentationChain(EGLDisplay display, EGLContext sharedContext, EGLConfig sharedContextCfg, const void* platformValue, const PresentationChainDesc& desc)
    {
        Metal_OpenGLES::CheckGLError("Start of PresentationChain constructor");

        _guid = s_nextPresentationChainGUID++;
        _desc = std::make_shared<PresentationChainDesc>(desc);
        _display = display;

        EGLint renderableType = 0;
        bool res = eglGetConfigAttrib(display, sharedContextCfg, EGL_RENDERABLE_TYPE, &renderableType);
        assert(res);

        // Create a new EGLConfig based on format requested by presentation chain, then update the Device::_config
        auto config = TryGetEGLSurfaceConfig(display, renderableType, desc._format, desc._samples);
        if (!config) {
            Throw(::Exceptions::BasicLabel("Could not select valid configuration for presentation chain"));
        }

        Log(Warning) << "Presentation chain selected config:" << std::endl;
        StreamConfig(Log(Warning), display, config.value());

        // Create the main output surface
        // This must be tied to the window in Win32 -- so we can't begin construction of this until we build the presentation chain.
        const EGLint surfaceAttribList[] = { EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE, EGL_NONE };
        _surface = eglCreateWindowSurface(display, config.value(), EGLNativeWindowType(platformValue), surfaceAttribList);
        if (_surface == EGL_NO_SURFACE) {
            Throw(::Exceptions::BasicLabel("Failure constructing EGL window surface with error: (%s)", ErrorToName(eglGetError())));
        }

        EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, RenderableTypeAsGLESVersion(renderableType) / 100,
            EGL_NONE, EGL_NONE
        };
        _surfaceBoundContext = eglCreateContext(display, config.value(), sharedContext, contextAttribs);
        if (_surfaceBoundContext == EGL_NO_CONTEXT) {
            Throw(::Exceptions::BasicLabel("Failure while creating the surface bound context (%s)", ErrorToName(eglGetError())));
        }

        if (!eglMakeCurrent(display, _surface, _surface, _surfaceBoundContext)) {
            Throw(::Exceptions::BasicLabel("Failure making EGL window surface current with error (%s)", ErrorToName(eglGetError())));
        }
    }

    PresentationChain::~PresentationChain()
    {
        if (_surface != EGL_NO_SURFACE) {
            if (_surface == eglGetCurrentSurface(EGL_READ) || _surface == eglGetCurrentSurface(EGL_DRAW))
                eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            EGLBoolean result = eglDestroySurface(_display, _surface);
            (void)result; assert(result);
        }
        if (_surfaceBoundContext != EGL_NO_CONTEXT) {
            if (_surfaceBoundContext == eglGetCurrentContext())
                eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            EGLBoolean result = eglDestroyContext(_display, _surfaceBoundContext);
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

    ThreadContext::ThreadContext(EGLContext sharedContext, const std::shared_ptr<Device>& device)
        : _sharedContext(sharedContext), _device(device), _currentPresentationChainGUID(0)
        , _display(device->GetDisplay())
    {}

    ThreadContext::~ThreadContext()
    {
        if (_dummyPBufferSurface != EGL_NO_SURFACE) {
            if (_dummyPBufferSurface == eglGetCurrentSurface(EGL_READ) || _dummyPBufferSurface == eglGetCurrentSurface(EGL_DRAW))
                eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroySurface(_display, _dummyPBufferSurface);
            _dummyPBufferSurface = EGL_NO_SURFACE;
        }

        if (_deferredContext != EGL_NO_CONTEXT) {
            if (_deferredContext == eglGetCurrentContext())
                eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(_display, _deferredContext);
            _deferredContext = EGL_NO_CONTEXT;
        }
    }

    IResourcePtr ThreadContext::BeginFrame(IPresentationChain &presentationChain)
    {
        // Ensure that the IDevice still exists. If you run into this issue, it means that the device
        // that created this ThreadContext has already been destroyed -- which will be an issue, because
        // the device deletes _display when it is destroyed.
        assert(_device.lock());

        auto &presChain = *checked_cast<PresentationChain*>(&presentationChain);

        assert(!_activeFrameContext);   
        assert(!_activeTargetRenderbuffer);
        assert(presChain.GetDisplay() == _display);
        _activeFrameContext = presChain.GetSurfaceBoundContext();

        // Make the immediate context the current context (with the presentation chain surface)
        auto currentContext = eglGetCurrentContext();
        if (currentContext != _activeFrameContext || _currentPresentationChainGUID != presChain.GetGUID()) {
            if (!eglMakeCurrent(_display, presChain.GetSurface(), presChain.GetSurface(), _activeFrameContext)) {
                Throw(::Exceptions::BasicLabel("Failure in eglMakeCurrent with error: (%s)", ErrorToName(eglGetError())));
            }
            _currentPresentationChainGUID = presChain.GetGUID();
        }

        #if defined(_DEBUG)
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
        assert(presChain.GetSurfaceBoundContext() == _activeFrameContext);
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
        _activeFrameContext = EGL_NO_CONTEXT;
    }

    void ThreadContext::MakeDeferredContext()
    {
        if (_dummyPBufferSurface != EGL_NO_SURFACE) {
            eglDestroySurface(_display, _dummyPBufferSurface);
            _dummyPBufferSurface = EGL_NO_SURFACE;
        }

        if (_deferredContext != EGL_NO_CONTEXT) {
            if (_deferredContext == eglGetCurrentContext())
                eglMakeCurrent(_deferredContext, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(_display, _deferredContext);
            _deferredContext = EGL_NO_CONTEXT;
        }

        // Turn this into a deferred context by creating a surface with a dummy 1x1 pbuffer
        EGLint configAttribsList[] = {
            // EGL_CONFORMANT,         glesBit,
            EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE,       EGL_PBUFFER_BIT,
            EGL_CONFIG_CAVEAT,      EGL_NONE,
            EGL_NONE
        };

        EGLConfig config;

        EGLint numConfig = 0;
        if (eglChooseConfig(_display, configAttribsList, &config, 1, &numConfig) != EGL_TRUE || numConfig == 0)
            Throw(::Exceptions::BasicLabel("Failure in eglChooseConfig in MakeDeferredContext"));

        Log(Warning) << "Immediate context selected config:" << std::endl;
        StreamConfig(Log(Warning), _display, config);

        int pbufferAttribsList[] = {
            EGL_WIDTH, 1,
            EGL_HEIGHT, 1,
            EGL_NONE
        };

        _dummyPBufferSurface = eglCreatePbufferSurface(_display, config, pbufferAttribsList);
        if (_dummyPBufferSurface == EGL_NO_SURFACE)
            Throw(::Exceptions::BasicLabel("Failure in eglCreatePbufferSurface in MakeDeferredContext"));

        EGLint contextAttribsList[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE, EGL_NONE
        };

        _deferredContext = eglCreateContext(_display, config, _sharedContext, contextAttribsList);
        if (_deferredContext == EGL_NO_CONTEXT)
            Throw(::Exceptions::BasicLabel("Failure in eglCreateContext in MakeDeferredContext"));
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

//////////////////////////////////////////////////////////////////////////////////////////////////
    
    ThreadContextOpenGLES::ThreadContextOpenGLES(EGLContext sharedContext, const std::shared_ptr<Device>& device)
        : ThreadContext(sharedContext, device)
    {
        auto featureSet = AsGLESFeatureSet(device->GetGLESVersion());
        _deviceContext = std::make_shared<Metal_OpenGLES::DeviceContext>(featureSet);
    }

    ThreadContextOpenGLES::~ThreadContextOpenGLES() {}

    void* ThreadContext::QueryInterface(size_t guid)
    {
        if (guid == typeid(EGLContext).hash_code()) {
            return _sharedContext;
        }
        return nullptr;
    }

    void* ThreadContextOpenGLES::QueryInterface(size_t guid)
    {
        if (guid == typeid(IThreadContextOpenGLES).hash_code()) {
            return (IThreadContextOpenGLES*)this;
        }
        return ThreadContext::QueryInterface(guid);
    }

    bool ThreadContextOpenGLES::IsBoundToCurrentThread()
    {
        EGLContext currentContext = eglGetCurrentContext();
        if (_activeFrameContext != EGL_NO_CONTEXT) {
            return _activeFrameContext == currentContext;
        }
        if (_deferredContext != EGL_NO_CONTEXT) {
            return _deferredContext == currentContext;
        }
        return _sharedContext == currentContext;
    }

    bool ThreadContextOpenGLES::BindToCurrentThread()
    {
        if (_deferredContext != EGL_NO_CONTEXT) {
            // Ensure that the IDevice is still alive. If you hit this assert, it means that the device
            // may have been destroyed before this call was made. Since the device cleans up _display
            // in its destructor, it will turn the subsequent eglMakeCurrent into an invalid call
            assert(_device.lock());
            return eglMakeCurrent(_display, _dummyPBufferSurface, _dummyPBufferSurface, _deferredContext);
        } else {
            return false;
        }
    }

    void ThreadContextOpenGLES::UnbindFromCurrentThread()
    {
        assert(IsBoundToCurrentThread());
        eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    std::shared_ptr<IThreadContext> ThreadContextOpenGLES::Clone()
    {
        // Clone is odd -- we need to return a new ThreadContext that uses the same
        // underlying EGLContext, but has a new DeviceContext
        return std::make_shared<ThreadContextOpenGLES>(_sharedContext, _device.lock());
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
