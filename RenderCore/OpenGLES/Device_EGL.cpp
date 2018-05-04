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
    static const bool s_useFakeBackBuffer = false;
    static unsigned s_glesVersion = 300;        // always assume this version of GLES

    static Metal_OpenGLES::FeatureSet::BitField AsGLESFeatureSet(unsigned glesVersion)
    {
        Metal_OpenGLES::FeatureSet::BitField featureSet = 0;
        if (glesVersion >= 200u) {
            featureSet |= Metal_OpenGLES::FeatureSet::GLES200 | Metal_OpenGLES::FeatureSet::ETC1TC;
        }
        
        if (glesVersion >= 300) {            
            featureSet |= Metal_OpenGLES::FeatureSet::GLES300 | Metal_OpenGLES::FeatureSet::ETC2TC;
        }

        const char* extensionsString = (const char*)glGetString(GL_EXTENSIONS);
        if (extensionsString) {
            if (    strstr(extensionsString, "AMD_compressed_ATC_texture")
                ||  strstr(extensionsString, "ATI_texture_compression_atitc")) {
                featureSet |= Metal_OpenGLES::FeatureSet::ATITC;
            }
        }

        assert(featureSet != 0);
        return featureSet;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////

    Device::Device()
    {
        _display = EGL_NO_DISPLAY;

            //
            //      Create a EGL "_display" object
            //
            //
        EGLDisplay _displayTemp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (_displayTemp == EGL_NO_DISPLAY) {
            Throw(::Exceptions::BasicLabel("Failure while creating _display"));
        }

        _display = _displayTemp;

        //DestructorPointer<EGLDisplay, decltype(&eglTerminate)> _display(_displayTemp, &eglTerminate);

        EGLint majorVersion = 0, minorVersion = 0;
        EGLint configCount = 0;
        if (!eglInitialize(_display, &majorVersion, &minorVersion)) {
            Throw(::Exceptions::BasicLabel("Failure while initalizing _display"));
        }

        if (!eglGetConfigs(_display, NULL, 0, &configCount)) {
            Throw(::Exceptions::BasicLabel("Failure in eglGetConfigs"));
        }

        std::vector<EGLConfig> configs(configCount);
        if (!eglGetConfigs(_display, AsPointer(configs.begin()), configs.size(), &configCount)) {
            Throw(::Exceptions::BasicLabel("Failure in eglGetConfigs"));
        }

#if 0
        for (auto i = configs.begin(); i!=configs.end(); ++i) {
            EGLint bufferSize, redSize, greenSize, blueSize, luminanceSize, alphaSize, alphaMaskSize;
            EGLint bindToTextureRGB, bindToTextureRGBA, colorBufferType, configCaveat, configId, conformant;
            EGLint depthSize, level, matchNativePixmap, maxSwapInterval, minSwapInterval, nativeRenderable;
            EGLint nativeVisualType, renderableType, sampleBuffers, samples, stencilSize, surfaceType;
            EGLint transparentType, transparentRedValue, transparentGreenValue, transparentBlueValue;

            eglGetConfigAttrib(_display, *i, EGL_BUFFER_SIZE, &bufferSize);
            eglGetConfigAttrib(_display, *i, EGL_RED_SIZE, &redSize);
            eglGetConfigAttrib(_display, *i, EGL_GREEN_SIZE, &greenSize);
            eglGetConfigAttrib(_display, *i, EGL_BLUE_SIZE, &blueSize);
            eglGetConfigAttrib(_display, *i, EGL_LUMINANCE_SIZE, &luminanceSize);
            eglGetConfigAttrib(_display, *i, EGL_ALPHA_SIZE, &alphaSize);
            eglGetConfigAttrib(_display, *i, EGL_ALPHA_MASK_SIZE, &alphaMaskSize);
            eglGetConfigAttrib(_display, *i, EGL_BIND_TO_TEXTURE_RGB, &bindToTextureRGB);
            eglGetConfigAttrib(_display, *i, EGL_BIND_TO_TEXTURE_RGBA, &bindToTextureRGBA);
            eglGetConfigAttrib(_display, *i, EGL_COLOR_BUFFER_TYPE, &colorBufferType);
            eglGetConfigAttrib(_display, *i, EGL_CONFIG_CAVEAT, &configCaveat);
            eglGetConfigAttrib(_display, *i, EGL_CONFIG_ID, &configId);
            eglGetConfigAttrib(_display, *i, EGL_CONFORMANT, &conformant);
            eglGetConfigAttrib(_display, *i, EGL_DEPTH_SIZE, &depthSize);
            eglGetConfigAttrib(_display, *i, EGL_LEVEL, &level);
            matchNativePixmap = false; // eglGetConfigAttrib(_display, *i, EGL_MATCH_NATIVE_PIXMAP, &matchNativePixmap);
            eglGetConfigAttrib(_display, *i, EGL_MAX_SWAP_INTERVAL, &maxSwapInterval);
            eglGetConfigAttrib(_display, *i, EGL_MIN_SWAP_INTERVAL, &minSwapInterval);
            eglGetConfigAttrib(_display, *i, EGL_NATIVE_RENDERABLE, &nativeRenderable);
            eglGetConfigAttrib(_display, *i, EGL_NATIVE_VISUAL_TYPE, &nativeVisualType);
            eglGetConfigAttrib(_display, *i, EGL_RENDERABLE_TYPE, &renderableType);
            eglGetConfigAttrib(_display, *i, EGL_SAMPLE_BUFFERS, &sampleBuffers);
            eglGetConfigAttrib(_display, *i, EGL_SAMPLES, &samples);
            eglGetConfigAttrib(_display, *i, EGL_STENCIL_SIZE, &stencilSize);
            eglGetConfigAttrib(_display, *i, EGL_SURFACE_TYPE, &surfaceType);
            eglGetConfigAttrib(_display, *i, EGL_TRANSPARENT_TYPE, &transparentType);
            eglGetConfigAttrib(_display, *i, EGL_TRANSPARENT_RED_VALUE, &transparentRedValue);
            eglGetConfigAttrib(_display, *i, EGL_TRANSPARENT_GREEN_VALUE, &transparentGreenValue);
            eglGetConfigAttrib(_display, *i, EGL_TRANSPARENT_BLUE_VALUE, &transparentBlueValue);

            std::string outputString = XlDynFormatString(
                "[%i] Config:\n"
                "  Sizes: %i[%i,%i,%i][%i,%i,%i]\n"
                "  Bind: [%i,%i] BufferType: [%i] Config: [%i, %i, %i]\n"
                "  Depth: [%i] Level: [%i] Match native pixmap: [%i]\n"
                "  Swap interval: [%i, %i] Native: [%i, %i]\n"
                "  Renderable: [%i] Samples: [%i, %i]\n"
                "  Stencil: [%i], Surface: [%i]\n"
                "  Tranparent: %i[%i, %i, %i]\n",
                std::distance(configs.begin(), i),
                bufferSize, redSize, greenSize, blueSize, luminanceSize, alphaSize, alphaMaskSize,
                bindToTextureRGB, bindToTextureRGBA, colorBufferType, configCaveat, configId, conformant, depthSize, level, matchNativePixmap,
                maxSwapInterval, minSwapInterval, nativeRenderable, nativeVisualType,
                renderableType, sampleBuffers, samples,
                stencilSize, surfaceType, transparentType, transparentRedValue, transparentGreenValue, transparentBlueValue);

            #if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS
                OutputDebugString(outputString.c_str());
            #endif
        }
#endif


            //
            //      Select EGL configuration (from available options)
            //

        //const bool      enableFrameBufferAlpha   = false;
        const unsigned  depthBufferSize          = 24;
        const bool      enableStencilBuffer      = true;
        //const bool      enableMultisampling      = false;
        const EGLint    configAttribList[] =
        {
            //EGL_BUFFER_SIZE,                EGL_DONT_CARE,
            EGL_RED_SIZE,                   8,
            EGL_GREEN_SIZE,                 8,
            EGL_BLUE_SIZE,                  8,
            //EGL_LUMINANCE_SIZE,             EGL_DONT_CARE,
            //EGL_ALPHA_SIZE,                 enableFrameBufferAlpha ? 8 : EGL_DONT_CARE,
            //EGL_ALPHA_MASK_SIZE,            EGL_DONT_CARE,
            //EGL_BIND_TO_TEXTURE_RGB,        EGL_DONT_CARE,
            //EGL_BIND_TO_TEXTURE_RGBA,       EGL_DONT_CARE,
            //EGL_COLOR_BUFFER_TYPE,          EGL_DONT_CARE,
            //EGL_CONFIG_CAVEAT,              EGL_DONT_CARE,
            //EGL_CONFIG_ID,                  EGL_DONT_CARE,
            //EGL_CONFORMANT,                 EGL_DONT_CARE,
            EGL_DEPTH_SIZE,                 depthBufferSize,
            //EGL_LEVEL,                      EGL_DONT_CARE,
            //EGL_MATCH_NATIVE_PIXMAP,        EGL_DONT_CARE,
            //EGL_MAX_SWAP_INTERVAL,          EGL_DONT_CARE,
            //EGL_MIN_SWAP_INTERVAL,          EGL_DONT_CARE,
            //EGL_NATIVE_RENDERABLE,          EGL_DONT_CARE,
            //EGL_NATIVE_VISUAL_TYPE,         EGL_DONT_CARE,
            //EGL_RENDERABLE_TYPE,            EGL_DONT_CARE,
            //EGL_SAMPLE_BUFFERS,             enableMultisampling ? 1 : 0,
            //EGL_SAMPLES,                    EGL_DONT_CARE,
            EGL_STENCIL_SIZE,               enableStencilBuffer ? 8 : EGL_DONT_CARE,
            //EGL_SURFACE_TYPE,               EGL_DONT_CARE,
            //EGL_TRANSPARENT_TYPE,           EGL_DONT_CARE,
            //EGL_TRANSPARENT_RED_VALUE,      EGL_DONT_CARE,
            //EGL_TRANSPARENT_GREEN_VALUE,    EGL_DONT_CARE,
            //EGL_TRANSPARENT_BLUE_VALUE,     EGL_DONT_CARE,
            EGL_NONE,                       EGL_NONE
        };

        if (!eglChooseConfig(_display, configAttribList, &_config, 1, &configCount)) {
            Throw(::Exceptions::BasicLabel("Failure in eglChooseConfig"));
        }

            //
            //      Collect version information
            //          -- useful for reporting to the log file
            //

        const char* versionMain          = eglQueryString(_display, EGL_VERSION);
        const char* versionVendor        = eglQueryString(_display, EGL_VENDOR);
        const char* versionClientAPIs    = eglQueryString(_display, EGL_CLIENT_APIS);
        const char* versionExtensions    = eglQueryString(_display, EGL_EXTENSIONS);
        (void)versionMain; (void)versionVendor; (void)versionClientAPIs; (void)versionExtensions;

            //
            //      Build the "immediate" context
            //          One context to rule them all.
            //

        EGLint contextAttribs[]  = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE, EGL_NONE };
        _sharedContext = eglCreateContext(_display, _config, EGL_NO_CONTEXT, contextAttribs);
        if (_sharedContext == EGL_NO_CONTEXT) {
            Throw(::Exceptions::BasicLabel("Failure while creating the immediate context"));
        }

            // (here, contextDestroyer just binds the first parameter to eglDestroyContext)
        //auto contextDestroyer = [=](EGLContext context) {eglDestroyContext(_displayTemp, context);};
        //DestructorPointer<EGLContext, decltype(contextDestroyer)> context(contextTemp, contextDestroyer);

        Metal_OpenGLES::CheckGLError("End of Device constructor");
    }

    Device::~Device()
    {
        if (_display != EGL_NO_DISPLAY) {
            EGLBoolean result = eglTerminate(_display);
            (void)result; assert(result);
        }
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

    std::unique_ptr<IPresentationChain>   Device::CreatePresentationChain(const void* platformValue, unsigned width, unsigned height)
    {
        auto result = std::make_unique<PresentationChain>(_sharedContext, _display, _config, platformValue, width, height);
        // We can only construct the object factory after the first presentation chain is called. This is because
        // we can't call eglMakeCurrent until at least one presentation chain has been constructed; and we can't
        // call any opengl functions until we call eglMakeCurrent. In particular, we can't call glGetString(), which is
        // required to calculate the feature set used to construct the object factory.
        if (!_objectFactory) {
            auto featureSet = AsGLESFeatureSet(s_glesVersion);
            _objectFactory = std::make_shared<Metal_OpenGLES::ObjectFactory>(featureSet);
        }
        return result;
    }

    IResourcePtr    ThreadContext::BeginFrame(IPresentationChain &presentationChain)
    {
        auto deviceStrong = _device.lock();
        if (!deviceStrong) {
            Throw(::Exceptions::BasicLabel("Weak ref to device, device was freed!"));
        }

        assert(!_activeFrameContext);   
        assert(!_activeTargetRenderbuffer);     
        _activeFrameContext = deviceStrong->GetSharedContext();

            //
            //      Make the immediate context the current context
            //      (with the presentation chain surface
            //
        auto &presChain = *checked_cast<PresentationChain*>(&presentationChain);
	    auto currentContext = eglGetCurrentContext();
	    if (    currentContext != _activeFrameContext
		    || _currentPresentationChainGUID != presChain.GetGUID()) {

	        if (!eglMakeCurrent(    deviceStrong->GetDisplay(),
	                                presChain.GetSurface(),
	                                presChain.GetSurface(),
	                                _activeFrameContext)) {
	            Throw(::Exceptions::BasicLabel("Failure in eglMakeCurrent"));
	        }

		    _currentPresentationChainGUID = presChain.GetGUID();
	    }

        #if defined(_DEBUG)
            const auto& presentationChainDesc = presentationChain.GetDesc();
            EGLint width, height;
            eglQuerySurface(deviceStrong->GetDisplay(), presChain.GetSurface(), EGL_WIDTH, &width);
            eglQuerySurface(deviceStrong->GetDisplay(), presChain.GetSurface(), EGL_HEIGHT, &height);

            if (    width != presentationChainDesc->_width
                ||  height != presentationChainDesc->_height) {
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

    void ThreadContext::Present(IPresentationChain& presentationChain) {
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
                glBlitFramebuffer(
                        0, 0, presChain.GetDesc()->_width, presChain.GetDesc()->_height,
                        0, 0, presChain.GetDesc()->_width, presChain.GetDesc()->_height,
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                _temporaryFramebuffer = nullptr;
            }
            eglSwapBuffers(device.GetDisplay(), presChain.GetSurface());
        }
        _activeTargetRenderbuffer = nullptr;
        _activeFrameContext = nullptr;
    }

    bool                        ThreadContext::IsImmediate() const { return false; }
    ThreadContextStateDesc      ThreadContext::GetStateDesc() const { return {}; }
    std::shared_ptr<IDevice>    ThreadContext::GetDevice() const { return _device.lock(); }
    void                        ThreadContext::IncrFrameId() {}
    void                        ThreadContext::InvalidateCachedState() const {}

    ThreadContext::ThreadContext(EGLContext sharedContext, const std::shared_ptr<Device>& device)
    : _sharedContext(sharedContext)
    , _device(device)
    , _activeFrameContext(nullptr)
    , _currentPresentationChainGUID(0)
    {}

    ThreadContext::~ThreadContext() {}

    IAnnotator&                 ThreadContext::GetAnnotator()
    {
        if (!_annotator) {
            auto d = _device.lock();
            assert(d);
            _annotator = CreateAnnotator(*d);
        }
        return *_annotator;
    }

    void* Device::QueryInterface(size_t guid)
    {
        if (guid == typeid(Device).hash_code()) {
            return (Device*)this;
        }
        return nullptr;
    }

    DeviceDesc Device::GetDesc() {
        return DeviceDesc { "OpenGLES-EGL", "", "" };
    }

    FormatCapability Device::QueryFormatCapability(Format format, BindFlag::BitField bindingType)
    {
        auto activeFeatureSet = GetObjectFactory()->GetFeatureSet();
        auto glFmt = Metal_OpenGLES::AsTexelFormatType(format);
        if (glFmt._internalFormat == GL_NONE)
            return FormatCapability::NotSupported;

        bool supported = true;
        if (bindingType & BindFlag::ShaderResource) {
            supported &= !!(activeFeatureSet & glFmt._textureFeatureSet);
        } else if ((bindingType & BindFlag::RenderTarget) || (bindingType & BindFlag::DepthStencil)) {
            supported &= !!(activeFeatureSet & glFmt._renderbufferFeatureSet);
        }

        return supported ? FormatCapability::Supported : FormatCapability::NotSupported;
    }
    
    void* DeviceOpenGLES::QueryInterface(size_t guid)
    {
        if (guid == typeid(IDeviceOpenGLES).hash_code()) {
            return (IDeviceOpenGLES*)this;
        }
        return Device::QueryInterface(guid);
    }

    Metal_OpenGLES::FeatureSet::BitField DeviceOpenGLES::GetFeatureSet()
    {
        return GetObjectFactory()->GetFeatureSet();
    }

    DeviceOpenGLES::DeviceOpenGLES() {}
    DeviceOpenGLES::~DeviceOpenGLES() {}

    std::unique_ptr<IThreadContext>   Device::CreateDeferredContext()
    {
        assert(_objectFactory);
        return std::make_unique<ThreadContextOpenGLES>(_sharedContext, shared_from_this());
    }

    std::shared_ptr<IThreadContext>   Device::GetImmediateContext()
    {
        assert(_objectFactory);
        if (!_immediateContext) {
            _immediateContext = std::make_shared<ThreadContextOpenGLES>(_sharedContext, shared_from_this());
        }
        return _immediateContext;
    }
    std::shared_ptr<IThreadContext>   DeviceOpenGLES::GetImmediateContext()
    {
        assert(_objectFactory);
        if (!_immediateContext) {
            _immediateContext = std::make_shared<ThreadContextOpenGLES>(_sharedContext, shared_from_this());
        }
        return _immediateContext;
    }

	IResourcePtr Device::CreateResource(const ResourceDesc& desc, const ResourceInitializer& init)
	{
        assert(_objectFactory);
		return Metal_OpenGLES::CreateResource(*_objectFactory, desc, init);
	} 

    //////////////////////////////////////////////////////////////////////////////////////////////////

	static unsigned s_nextPresentationChainGUID = 1;

    PresentationChain::PresentationChain(
        EGLContext eglContext,
        EGLDisplay display,
        EGLConfig config,
        const void* platformValue, unsigned width, unsigned height)
    {
        Metal_OpenGLES::CheckGLError("Start of PresentationChain constructor");

	    _guid = s_nextPresentationChainGUID++;

            //
            //      Create the main output surface
            //
            //          This must be tied to the window in Win32 -- so we can't being
            //          construction of this until we build the presentation chain.
            //
        // const bool      postSubBufferSupported = false;
        const EGLint    surfaceAttribList[] =
        {
            EGL_RENDER_BUFFER,                  EGL_BACK_BUFFER,
            // EGL_POST_SUB_BUFFER_SUPPORTED_NV,   postSubBufferSupported ? EGL_TRUE : EGL_FALSE,

                    // (OpenVG video playback settings) //
                    //  (not supported by angleproject) //
            // EGL_VG_COLORSPACE,                  EGL_VG_COLORSPACE_sRGB,
            // EGL_VG_ALPHA_FORMAT,                EGL_VG_ALPHA_FORMAT_NONPRE,

            EGL_NONE,                           EGL_NONE
        };

        EGLSurface surfaceTemp = eglCreateWindowSurface(
            display, config,
            EGLNativeWindowType(platformValue), surfaceAttribList);
        if (surfaceTemp == EGL_NO_SURFACE) {
            auto error = eglGetError();
            Throw(::Exceptions::BasicLabel("Failure constructing EGL window surface %x", error));
        }

        _surface = surfaceTemp;

        if (!eglMakeCurrent(display, _surface, _surface, eglContext)) {
            Throw(::Exceptions::BasicLabel("Failure making EGL window surface current"));
        }

        _backBufferDesc = CreateDesc(
            BindFlag::RenderTarget, 0, GPUAccess::Write,
            TextureDesc::Plain2D(width, height, Format::R8G8B8_UNORM),        // SRGB?
            "backbuffer");

        _desc = std::make_shared<PresentationChainDesc>();
        assert(_backBufferDesc._type == ResourceDesc::Type::Texture);
        _desc->_width = _backBufferDesc._textureDesc._width;
        _desc->_height = _backBufferDesc._textureDesc._height;
        _desc->_format = _backBufferDesc._textureDesc._format;
        _desc->_samples = _backBufferDesc._textureDesc._samples;

		Metal_OpenGLES::CheckGLError("End of PresentationChain constructor");
    }

    const std::shared_ptr<PresentationChainDesc>& PresentationChain::GetDesc() const
    {
        return _desc;
    }

    const std::shared_ptr<Metal_OpenGLES::Resource>& PresentationChain::GetTargetRenderbuffer()
    {
        if (!_targetRenderbuffer) {
            if (s_useFakeBackBuffer) {
                _targetRenderbuffer = std::make_shared<Metal_OpenGLES::Resource>(Metal_OpenGLES::GetObjectFactory(), _backBufferDesc);
            } else {
                _targetRenderbuffer = std::make_shared<Metal_OpenGLES::Resource>(Metal_OpenGLES::Resource::CreateBackBuffer(_backBufferDesc));
            }
        }
        return _targetRenderbuffer;
    }

    void PresentationChain::Resize(unsigned newWidth, unsigned newHeight) /*override*/
    {
        if (    newWidth == _backBufferDesc._textureDesc._width 
            &&  newHeight == _backBufferDesc._textureDesc._height)
            return;

        _backBufferDesc._textureDesc._width = newWidth;
        _backBufferDesc._textureDesc._height = newHeight;
        _desc->_width = _backBufferDesc._textureDesc._width;
        _desc->_height = _backBufferDesc._textureDesc._height;

        _targetRenderbuffer.reset(); // (it will be reconstructed on next call to GetTargetRenderbuffer()
    }

	EGLSurface PresentationChain::GetSurface() const
	{
		return _surface;
	}

    PresentationChain::~PresentationChain()
    {
        /*if (_surface && *_surface != EGL_NO_SURFACE) {
            EGLBoolean result = eglDestroySurface(*_display, *_surface);
            (void)result; assert(result);
            _surface = nullptr;
        }*/
    }

    ThreadContextOpenGLES::ThreadContextOpenGLES(EGLContext sharedContext, const std::shared_ptr<Device>& device)
    : ThreadContext(sharedContext, device)
    {
        auto featureSet = AsGLESFeatureSet(s_glesVersion);
        _deviceContext = std::make_shared<Metal_OpenGLES::DeviceContext>(featureSet);
    }

    ThreadContextOpenGLES::~ThreadContextOpenGLES() {}

    const std::shared_ptr<Metal_OpenGLES::DeviceContext>&  ThreadContextOpenGLES::GetDeviceContext()
    {
        return _deviceContext;
    }

    void*       ThreadContextOpenGLES::QueryInterface(size_t guid)
    {
        if (guid == typeid(IThreadContextOpenGLES).hash_code()) {
            return (IThreadContextOpenGLES*)this;
        }
        return ThreadContext::QueryInterface(guid);
    }

    bool        ThreadContextOpenGLES::IsBoundToCurrentThread()
    {
        EGLContext currentContext = eglGetCurrentContext();
        if (_activeFrameContext)
            return _activeFrameContext == currentContext;
        return _sharedContext == currentContext;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////
    render_dll_export std::shared_ptr<IDevice>    CreateDevice()
    {
        return std::make_shared<DeviceOpenGLES>();
    }

    void*                       ThreadContext::QueryInterface(size_t guid) {
        if (guid == typeid(EGLContext).hash_code()) {
            return _sharedContext;
        }
        return nullptr;
    }
} }

namespace RenderCore
{
    IDeviceOpenGLES::~IDeviceOpenGLES() {}
    IThreadContextOpenGLES::~IThreadContextOpenGLES() {}
    void *IThreadContext::QueryInterface(size_t guid) {return nullptr;}
}
