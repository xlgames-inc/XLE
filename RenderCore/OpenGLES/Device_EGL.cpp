// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Device_EGL.h"
#include "Metal/DeviceContext.h"
#include "../IAnnotator.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Core/Exceptions.h"
#include <type_traits>
#include <iostream>
#include <assert.h>
#include "Metal/IncludeGLES.h"

namespace RenderCore { namespace ImplOpenGLES
{
    static unsigned s_glesVersion = 300u; // always assume this version of GLES

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

    static EGLConfig GetEGLSurfaceConfig(EGLDisplay display, Format colorFormat = Format::R8G8B8_UNORM, TextureSamples msaaSamples = TextureSamples::Create(0, 0))
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
        configAttribs.push_back(EGL_SAMPLES); configAttribs.push_back(msaaSamples._sampleCount);
        configAttribs.push_back(EGL_NONE); configAttribs.push_back(EGL_NONE);

        EGLConfig config;
        EGLint numConfigs;
        if (!eglChooseConfig(display, configAttribs.data(), &config, 1, &numConfigs)) {
            Throw(::Exceptions::BasicLabel("Failure in eglChooseConfig"));
        }

        return config;
    }

//////////////////////////////////////////////////////////////////////////////////////////////////

    Device::Device()
    {
        // Create EGL display object
        _display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (_display == EGL_NO_DISPLAY) {
            Throw(::Exceptions::BasicLabel("Failure while creating _display"));
        }

        EGLint majorVersion = 0, minorVersion = 0;
        if (!eglInitialize(_display, &majorVersion, &minorVersion)) {
            Throw(::Exceptions::BasicLabel("Failure while initalizing _display"));
        }

        // Build default EGL surface config
        _config = GetEGLSurfaceConfig(_display);

        // Collect version information, useful for reporting to the log file
        #if defined(_DEBUG)
            const char* versionMain = eglQueryString(_display, EGL_VERSION);
            const char* versionVendor = eglQueryString(_display, EGL_VENDOR);
            const char* versionClientAPIs = eglQueryString(_display, EGL_CLIENT_APIS);
            const char* versionExtensions = eglQueryString(_display, EGL_EXTENSIONS);
            (void)versionMain; (void)versionVendor; (void)versionClientAPIs; (void)versionExtensions;
        #endif

        // Build the immediate context
        EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE, EGL_NONE };
        _sharedContext = eglCreateContext(_display, _config, EGL_NO_CONTEXT, contextAttribs);
        if (_sharedContext == EGL_NO_CONTEXT) {
            Throw(::Exceptions::BasicLabel("Failure while creating the immediate context"));
        }

        Metal_OpenGLES::CheckGLError("End of Device constructor");
    }

    Device::~Device()
    {
        if (_display != EGL_NO_DISPLAY) {
            EGLBoolean result = eglTerminate(_display);
            (void)result;
            assert(result);
        }
    }

    std::unique_ptr<IPresentationChain> Device::CreatePresentationChain(const void* platformWindowHandle, const PresentationChainDesc& desc)
    {
        auto result = std::make_unique<PresentationChain>(_sharedContext, _display, &_config, platformWindowHandle, desc);
        // We can only construct the object factory after the first presentation chain is created. This is because
        // we can't call eglMakeCurrent until at least one presentation chain has been constructed; and we can't
        // call any opengl functions until we call eglMakeCurrent. In particular, we can't call glGetString(), which is
        // required to calculate the feature set used to construct the object factory.
        if (!_objectFactory) {
            auto featureSet = AsGLESFeatureSet(s_glesVersion);
            _objectFactory = std::make_shared<Metal_OpenGLES::ObjectFactory>(featureSet);
        }

        return result;
    }

    std::shared_ptr<Metal_OpenGLES::ObjectFactory> Device::GetObjectFactory()
    {
        // We can construct the object factory before creating the first presentation chain if we're not on Android.
        #if PLATFORMOS_TARGET != PLATFORMOS_ANDROID
            if (!_objectFactory) {
                auto featureSet = AsGLESFeatureSet(s_glesVersion);
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

    PresentationChain::PresentationChain(EGLContext eglContext, EGLDisplay display, EGLConfig *config, const void* platformValue, const PresentationChainDesc& desc)
    {
        Metal_OpenGLES::CheckGLError("Start of PresentationChain constructor");

        _guid = s_nextPresentationChainGUID++;
        _desc = std::make_shared<PresentationChainDesc>(desc);

        // Create a new EGLConfig based on format requested by presentation chain, then update the Device::_config
        *config = GetEGLSurfaceConfig(display, desc._format, desc._samples);

        // Create the main output surface
        // This must be tied to the window in Win32 -- so we can't begin construction of this until we build the presentation chain.
        const EGLint surfaceAttribList[] = { EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE, EGL_NONE};
        _surface = eglCreateWindowSurface(display, *config, EGLNativeWindowType(platformValue), surfaceAttribList);
        if (_surface == EGL_NO_SURFACE) {
            auto error = eglGetError();
            Throw(::Exceptions::BasicLabel("Failure constructing EGL window surface %x", error));
        }

        if (!eglMakeCurrent(display, _surface, _surface, eglContext)) {
            Throw(::Exceptions::BasicLabel("Failure making EGL window surface current"));
        }

        Metal_OpenGLES::CheckGLError("End of PresentationChain constructor");
    }

    PresentationChain::~PresentationChain()
    {
        /*if (_surface && *_surface != EGL_NO_SURFACE) {
         EGLBoolean result = eglDestroySurface(*_display, *_surface);
         (void)result; assert(result);
         _surface = nullptr;
         }*/
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
            eglDestroySurface(_display, _dummyPBufferSurface);
            _dummyPBufferSurface = EGL_NO_SURFACE;
        }

        if (_deferredContext != EGL_NO_CONTEXT) {
            if (_deferredContext == eglGetCurrentContext())
                eglMakeCurrent(_deferredContext, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(_display, _deferredContext);
            _deferredContext = EGL_NO_CONTEXT;
        }
    }

    IResourcePtr ThreadContext::BeginFrame(IPresentationChain &presentationChain)
    {
        auto deviceStrong = _device.lock();
        if (!deviceStrong) {
            Throw(::Exceptions::BasicLabel("Weak ref to device, device was freed!"));
        }

        assert(!_activeFrameContext);   
        assert(!_activeTargetRenderbuffer);     
        _activeFrameContext = deviceStrong->GetSharedContext();

        // Make the immediate context the current context (with the presentation chain surface)
        auto &presChain = *checked_cast<PresentationChain*>(&presentationChain);
        auto currentContext = eglGetCurrentContext();
        if (currentContext != _activeFrameContext || _currentPresentationChainGUID != presChain.GetGUID()) {
            if (!eglMakeCurrent(_display, presChain.GetSurface(), presChain.GetSurface(), _activeFrameContext)) {
                Throw(::Exceptions::BasicLabel("Failure in eglMakeCurrent"));
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

        auto deviceStrong = _device.lock();
        if (!deviceStrong) {
            Throw(::Exceptions::BasicLabel("Weak ref to device, device was freed!"));
        }
        auto &device = *deviceStrong;
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
            // EGL_RENDERABLE_TYPE,    glesBit,
            EGL_SURFACE_TYPE,       EGL_PBUFFER_BIT,
            EGL_CONFIG_CAVEAT,      EGL_NONE,
            EGL_NONE
        };

        EGLConfig config;

        EGLint numConfig = 0;
        if (eglChooseConfig(_display, configAttribsList, &config, 1, &numConfig) != EGL_TRUE || numConfig == 0)
            Throw(::Exceptions::BasicLabel("Failure in eglChooseConfig in MakeDeferredContext"));

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
            Throw(::Exceptions::BasicLabel("Failure in _activeFrameContext in MakeDeferredContext"));
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
        auto featureSet = AsGLESFeatureSet(s_glesVersion);
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
            // in it's destructor, it will turn the subsequent eglMakeCurrent into an invalid call
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
