// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Device_EGL.h"
#include "Metal/DeviceContext.h"
#include "../IAnnotator.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Core/Exceptions.h"
#include <type_traits>
#include <iostream>
#include <assert.h>
#include "IncludeGLES.h"

namespace RenderCore { namespace ImplOpenGLES
{
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

            //
            //      Initialisation is complete. We can commit to our member pointers now.
            //

        _objectFactory = std::make_shared<Metal_OpenGLES::ObjectFactory>();
    }

    Device::~Device()
    {
        if (_display != EGL_NO_DISPLAY) {
            EGLBoolean result = eglTerminate(_display);
            (void)result; assert(result);
        }
    }

    std::unique_ptr<IPresentationChain>   Device::CreatePresentationChain(const void* platformValue, unsigned width, unsigned height)
    {
        std::unique_ptr<IPresentationChain> result = std::make_unique<PresentationChain>(*_objectFactory, _sharedContext, _display, _config, platformValue, width, height);

            //      Angle project problem! We need to all eglMakeCurrent before
            //      we can create an results (vertex buffers, index buffers, textures, etc)
        /*auto presChain = checked_cast<PresentationChain*>(result.get());
        EGLSurface underlyingSurface = *(presChain->GetSurface());
        if (!eglMakeCurrent(    *_display,   
                                underlyingSurface, 
                                underlyingSurface, 
                                _sharedContext.get())) {
            Throw(::Exceptions::BasicLabel("Failure in eglMakeCurrent"));
        }*/

        return std::move(result);
    }

    IResourcePtr    ThreadContext::BeginFrame(IPresentationChain &presentationChain)
    {
        auto deviceStrong = _device.lock();
        if (!deviceStrong) {
            Throw(::Exceptions::BasicLabel("Weak ref to device, device was freed!"));
        }

        assert(!_activeFrameContext);        
        _activeFrameContext = deviceStrong->GetSharedContext();

            //
            //      Make the immediate context the current context
            //      (with the presentation chain surface
            //
        auto &presChain = *checked_cast<PresentationChain*>(&presentationChain);
        if (!eglMakeCurrent(    deviceStrong->GetDisplay(),
                                presChain.GetSurface(),
                                presChain.GetSurface(),
                                _activeFrameContext)) {
            Throw(::Exceptions::BasicLabel("Failure in eglMakeCurrent"));
        }

        EGLint width, height;
        eglQuerySurface(deviceStrong->GetDisplay(), presChain.GetSurface(), EGL_WIDTH, &width);
        eglQuerySurface(deviceStrong->GetDisplay(), presChain.GetSurface(), EGL_HEIGHT, &height);
        
        // auto tex = Metal_OpenGLES::GetObjectFactory().CreateTexture();
        // _activeFrameRenderbuffer = std::make_shared<Metal_OpenGLES::Resource>(
        //    tex,
        //    CreateDesc(
        //        BindFlag::RenderTarget,
        //        0, GPUAccess::Write,
        //        TextureDesc::Plain2D(width, height, Format::R8G8B8A8_UNORM),
        //        "MainRenderbuffer"));
        // glBindTexture(GL_TEXTURE_2D, tex->AsRawGLHandle());
        // eglBindTexImage(deviceStrong->GetDisplay(), presChain.GetSurface(), tex->AsRawGLHandle());

        auto backBufferDesc = CreateDesc(
            BindFlag::RenderTarget, 0, GPUAccess::Write,
            TextureDesc::Plain2D(width, height, Format::R8G8B8A8_UNORM),        // SRGB?
            "backbuffer");
        _activeFrameRenderbuffer = std::make_shared<Metal_OpenGLES::Resource>(
            Metal_OpenGLES::Resource::CreateBackBuffer(backBufferDesc));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);

        // glClearColor(1.f, 1.0f, 1.0f, 1.f);
        // glClearDepthf(1.0f);
        // glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

        /*glBindFramebuffer(GL_FRAMEBUFFER, _activeFrameBuffer->AsRawGLHandle());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,  GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _activeFrameRenderbuffer->GetRenderBuffer()->AsRawGLHandle());*/

        return _activeFrameRenderbuffer;
    }

    void ThreadContext::Present(IPresentationChain& presentationChain) {
        auto &presChain = *checked_cast<PresentationChain*>(&presentationChain);

        auto deviceStrong = _device.lock();
        if (!deviceStrong) {
            Throw(::Exceptions::BasicLabel("Weak ref to device, device was freed!"));
        }
        /*if (_activeFrameRenderbuffer) {
            eglReleaseTexImage(
                deviceStrong->GetDisplay(), presChain.GetSurface(), 
                _activeFrameRenderbuffer->GetTexture()->AsRawGLHandle());
                _activeFrameRenderbuffer.reset();
        }*/
		if (_activeFrameContext) {
            /*glBindFramebuffer(GL_READ_FRAMEBUFFER, _activeFrameBuffer->AsRawGLHandle());
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(
                    0, 0, presChain.GetDesc()->_width, presChain.GetDesc()->_height,
                    0, 0, presChain.GetDesc()->_width, presChain.GetDesc()->_height,
                    GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);*/
            eglSwapBuffers(deviceStrong->GetDisplay(), presChain.GetSurface());
            _activeFrameContext = nullptr;
        }
        /*eglMakeCurrent(
            deviceStrong->GetDisplay(),
            EGL_NO_SURFACE,
            EGL_NO_SURFACE,
            _sharedContext);*/
    }

    bool                        ThreadContext::IsImmediate() const { return false; }
    ThreadContextStateDesc      ThreadContext::GetStateDesc() const { return {}; }
    std::shared_ptr<IDevice>    ThreadContext::GetDevice() const { return _device.lock(); }
    void                        ThreadContext::IncrFrameId() {}
    void                        ThreadContext::InvalidateCachedState() const {}

    ThreadContext::ThreadContext(EGLContext sharedContext, const std::shared_ptr<Device>& device)
    : _sharedContext(sharedContext)
    , _device(device)
    {
        _activeFrameContext = nullptr;
    }

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
        return nullptr;
    }

    DeviceDesc Device::GetDesc() {
        return DeviceDesc { "OpenGLES-EGL", "", "" };
    }

    #if !FLEX_USE_VTABLE_Device && 0
        namespace Detail
        {
            void* Ignore_Device::QueryInterface(const GUID& guid)
            {
                return nullptr;
            }
        }
    #endif
    
    void* DeviceOpenGLES::QueryInterface(size_t guid)
    {
        // if (guid == __uuidof(IDeviceOpenGLES)) {
        //     return (IDeviceOpenGLES*)this;
        // }
        // return NULL;
        return (IDeviceOpenGLES*)this;
    }

    DeviceOpenGLES::DeviceOpenGLES() {}
    DeviceOpenGLES::~DeviceOpenGLES() {}

    std::unique_ptr<IThreadContext>   Device::CreateDeferredContext()
    {
        return std::make_unique<ThreadContextOpenGLES>(_sharedContext, shared_from_this());
        /*EGLint contextAttribs[]  = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };
        EGLContext contextTemp = eglCreateContext(_display, _config, _immediateContext->GetUnderlying(), contextAttribs);
        if (contextTemp == EGL_NO_CONTEXT) {
            Throw(::Exceptions::BasicLabel("Failure while creating the immediate context"));
        }

        return moveptr(new Metal_OpenGLES::DeviceContext(_display, contextTemp));*/
    }

    std::shared_ptr<IThreadContext>   Device::GetImmediateContext() {
        if (!_immediateContext) {
            _immediateContext = std::make_shared<ThreadContextOpenGLES>(_sharedContext, shared_from_this());
        }
        return _immediateContext;
    }
    std::shared_ptr<IThreadContext>   DeviceOpenGLES::GetImmediateContext()
    {
        if (!_immediateContext) {
            _immediateContext = std::make_shared<ThreadContextOpenGLES>(_sharedContext, shared_from_this());
        }
        return _immediateContext;
    }

	IResourcePtr Device::CreateResource(const ResourceDesc& desc, const ResourceInitializer& init)
	{
		return Metal_OpenGLES::CreateResource(*_objectFactory, desc, init);
	} 

    //////////////////////////////////////////////////////////////////////////////////////////////////

    PresentationChain::PresentationChain(
            Metal_OpenGLES::ObjectFactory &objFactory,
            EGLContext eglContext,
            EGLDisplay display,
            EGLConfig config,
            const void* platformValue, unsigned width, unsigned height)
    {
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
            Throw(::Exceptions::BasicLabel("Failure constructing EGL window surface"));
        }

        _surface = surfaceTemp;

        if (!eglMakeCurrent(display, _surface, _surface, eglContext)) {
            Throw(::Exceptions::BasicLabel("Failure making EGL window surface current"));
        }

        #if 0
            auto frameRenderbuffer = objFactory.CreateRenderBuffer();
            glBindRenderbuffer(GL_RENDERBUFFER, frameRenderbuffer->AsRawGLHandle());
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);

            _frameRenderbuffer = std::make_shared<Metal_OpenGLES::Resource>(frameRenderbuffer);

            _desc = std::make_shared<PresentationChainDesc>();
            auto resDesc = ExtractDesc(*_frameRenderbuffer);
            assert(resDesc._type == ResourceDesc::Type::Texture);
            _desc->_width = resDesc._textureDesc._width;
            _desc->_height = resDesc._textureDesc._height;
            _desc->_format = resDesc._textureDesc._format;
            _desc->_samples = resDesc._textureDesc._samples;
        #else
        #endif
    }

    const std::shared_ptr<PresentationChainDesc>& PresentationChain::GetDesc() const {
        return _desc;
    }


    void PresentationChain::Resize(unsigned newWidth, unsigned newHeight) /*override*/ {
        //assert(0); // not implemented
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
        _deviceContext = std::make_shared<Metal_OpenGLES::DeviceContext>();
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

    /*void            PresentationChain::Present()
    {
        EGLBoolean result = eglSwapBuffers(_display, _surface);
        (void)result; assert(result);
    }*/

    //////////////////////////////////////////////////////////////////////////////////////////////////
    Metal_OpenGLES::DeviceContext *DeviceOpenGLES::GetImmediateDeviceContext() {
        return nullptr;
    }
    //
    //

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

    /*render_dll_export Metal_OpenGLES::GlobalResources&        GetGlobalResources()
    {
        static Metal_OpenGLES::GlobalResources gGlobalResources;
        return gGlobalResources;
    }*/
} }

namespace RenderCore
{
    IDeviceOpenGLES::~IDeviceOpenGLES() {}
    IThreadContextOpenGLES::~IThreadContextOpenGLES() {}
    void *IThreadContext::QueryInterface(size_t guid) {return nullptr;}
}
