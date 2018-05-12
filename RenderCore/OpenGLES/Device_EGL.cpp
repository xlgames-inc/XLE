// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Device_EGL.h"
#include "EGLUtils.h"
#include "Metal/DeviceContext.h"
#include "../IAnnotator.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/PtrUtils.h"
#include "../../Core/Exceptions.h"
#include <type_traits>
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
        Log(Verbose) << "In AsGLESFeatureSet, extensionsString: " << extensionsString << std::endl;

        assert(featureSet != 0);
        return featureSet;
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
            _rootContext = std::make_shared<ThreadContext>(_display, _rootContextConfig, EGL_NO_CONTEXT, 0, shared_from_this());

            #if defined(_DEBUG)
                Log(Verbose) << "Root context:" << std::endl;
                StreamContext(Log(Verbose), _display, _rootContext->GetUnderlying());
            #endif

            auto result = std::make_unique<PresentationChain>(_display, _rootContextConfig, platformWindowHandle, desc);

            // Finally, set this context & surface current, so we have at least something current to
            // start calling opengl commands.
            if (!eglMakeCurrent(_display, result->GetSurface(), result->GetSurface(), _rootContext->GetUnderlying()))
                Throw(::Exceptions::BasicLabel("Failure while setting EGL root context current (%s)", ErrorToName(eglGetError())));

            // We can only construct the object factory after the first presentation chain is created. This is because
            // we can't call eglMakeCurrent until at least one presentation chain has been constructed; and we can't
            // call any opengl functions until we call eglMakeCurrent. In particular, we can't call glGetString(), which is
            // required to calculate the feature set used to construct the object factory.
            auto featureSet = AsGLESFeatureSet(_glesVersion);
            assert(!_objectFactory);
            _objectFactory = std::make_shared<Metal_OpenGLES::ObjectFactory>(featureSet);
            _rootContext->SetFeatureSet(featureSet);
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
        return std::make_unique<ThreadContext>(_display, _rootContextConfig, _rootContext->GetUnderlying(), _rootContext->GetFeatureSet(), shared_from_this());
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

    unsigned DeviceOpenGLES::GetNativeFormatCode()
    {
        EGLint format = 0;
        if (!eglGetConfigAttrib(_display, _rootContextConfig, EGL_NATIVE_VISUAL_ID, &format)) {
            Log(Warning) << "Failure while getting native visual id from root context config" << std::endl;
        }

        return format;
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
        if (newWidth != _desc->_width || newHeight != _desc->_height) {
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
        glFlush();
        eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    std::shared_ptr<IThreadContext> ThreadContext::Clone()
    {
        // Clone is odd -- we need to return a new ThreadContext that uses the same
        // underlying EGLContext, but has a new DeviceContext
        return std::make_shared<ThreadContext>(_display, _context, _dummySurface, _deviceContext->GetFeatureSet(), _device.lock(), true);
    }

    void ThreadContext::SetFeatureSet(unsigned featureSet)
    {
        _deviceContext = std::make_shared<Metal_OpenGLES::DeviceContext>(featureSet);
    }

    unsigned ThreadContext::GetFeatureSet() const
    {
        return _deviceContext->GetFeatureSet();
    }

    ThreadContext::ThreadContext(EGLDisplay display, EGLContext context, EGLSurface dummySurface, unsigned featureSet, const std::shared_ptr<Device>& device, bool)
    : _display(display), _context(context), _dummySurface(dummySurface)
    , _device(device), _currentPresentationChainGUID(0)
    , _clonedContext(true)
    {
        _deviceContext = std::make_shared<Metal_OpenGLES::DeviceContext>(featureSet);
    }

    ThreadContext::ThreadContext(EGLDisplay display, EGLConfig cfgForNewContext, EGLContext rootContext, unsigned featureSet, const std::shared_ptr<Device>& device)
    : _device(device), _currentPresentationChainGUID(0)
    , _display(display)
    {
        auto glesVersion = GetGLESVersionFromConfig(_display, cfgForNewContext);

        // Build the root context
        EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, glesVersion / 100,
            EGL_NONE, EGL_NONE
        };
        _context = eglCreateContext(_display, cfgForNewContext, rootContext, contextAttribs);
        if (_context == EGL_NO_CONTEXT)
            Throw(::Exceptions::BasicLabel("Failure while creating EGL context (%s)", ErrorToName(eglGetError())));

        // create a "dummy PBuffer" so we can use this context for non-rendering operations (such as
        // loading & initialization)
        int pbufferAttribsList[] = {
            EGL_WIDTH, 1, EGL_HEIGHT, 1,
            EGL_NONE, EGL_NONE
        };
        _dummySurface = eglCreatePbufferSurface(_display, cfgForNewContext, pbufferAttribsList);
        if (_dummySurface == EGL_NO_SURFACE)
            Throw(::Exceptions::BasicLabel("Failure in eglCreatePbufferSurface (%s)", ErrorToName(eglGetError())));

        Log(Verbose) << "Created EGL context: " << std::endl;
        StreamContext(Log(Verbose), display, _context);

        if (featureSet)
            _deviceContext = std::make_shared<Metal_OpenGLES::DeviceContext>(featureSet);
    }

    ThreadContext::~ThreadContext()
    {
        if (!_clonedContext) {
            if (_dummySurface != EGL_NO_SURFACE) {
                if (_dummySurface == eglGetCurrentSurface(EGL_READ) ||
                    _dummySurface == eglGetCurrentSurface(EGL_DRAW))
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
