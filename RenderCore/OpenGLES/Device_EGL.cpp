// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Device.h"
#include "Metal/DeviceContext.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Core/Exceptions.h"
#include <type_traits>
#include <iostream>
#include <assert.h>
#include "IncludeGLES.h"

namespace RenderCore
{
    //////////////////////////////////////////////////////////////////////////////////////////////////

    Device::Device()
    {
        _display = EGL_NO_DISPLAY;
        _config = nullptr;

            //
            //      Create a EGL "display" object
            //
            //
        EGL::Display displayTemp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (displayTemp == EGL_NO_DISPLAY) {
            Throw(::Exceptions::BasicLabel("Failure while creating display"));
        }

        DestructorPointer<EGL::Display, decltype(&eglTerminate)> display(displayTemp, &eglTerminate);

        EGLint majorVersion = 0, minorVersion = 0;
        EGLint configCount = 0;
        if (!eglInitialize(display, &majorVersion, &minorVersion)) {
            Throw(::Exceptions::BasicLabel("Failure while creating display"));
        }

        if (!eglGetConfigs(display, NULL, 0, &configCount)) {
            Throw(::Exceptions::BasicLabel("Failure in eglGetConfigs"));
        }

        std::vector<EGL::Config> configs(configCount);
        if (!eglGetConfigs(display, AsPointer(configs.begin()), configs.size(), &configCount)) {
            Throw(::Exceptions::BasicLabel("Failure in eglGetConfigs"));
        }

        for (auto i = configs.begin(); i!=configs.end(); ++i) {
            EGLint bufferSize, redSize, greenSize, blueSize, luminanceSize, alphaSize, alphaMaskSize;
            EGLint bindToTextureRGB, bindToTextureRGBA, colorBufferType, configCaveat, configId, conformant;
            EGLint depthSize, level, matchNativePixmap, maxSwapInterval, minSwapInterval, nativeRenderable;
            EGLint nativeVisualType, renderableType, sampleBuffers, samples, stencilSize, surfaceType;
            EGLint transparentType, transparentRedValue, transparentGreenValue, transparentBlueValue;

            eglGetConfigAttrib(display, *i, EGL_BUFFER_SIZE, &bufferSize);
            eglGetConfigAttrib(display, *i, EGL_RED_SIZE, &redSize);
            eglGetConfigAttrib(display, *i, EGL_GREEN_SIZE, &greenSize);
            eglGetConfigAttrib(display, *i, EGL_BLUE_SIZE, &blueSize);
            eglGetConfigAttrib(display, *i, EGL_LUMINANCE_SIZE, &luminanceSize);
            eglGetConfigAttrib(display, *i, EGL_ALPHA_SIZE, &alphaSize);
            eglGetConfigAttrib(display, *i, EGL_ALPHA_MASK_SIZE, &alphaMaskSize);
            eglGetConfigAttrib(display, *i, EGL_BIND_TO_TEXTURE_RGB, &bindToTextureRGB);
            eglGetConfigAttrib(display, *i, EGL_BIND_TO_TEXTURE_RGBA, &bindToTextureRGBA);
            eglGetConfigAttrib(display, *i, EGL_COLOR_BUFFER_TYPE, &colorBufferType);
            eglGetConfigAttrib(display, *i, EGL_CONFIG_CAVEAT, &configCaveat);
            eglGetConfigAttrib(display, *i, EGL_CONFIG_ID, &configId);
            eglGetConfigAttrib(display, *i, EGL_CONFORMANT, &conformant);
            eglGetConfigAttrib(display, *i, EGL_DEPTH_SIZE, &depthSize);
            eglGetConfigAttrib(display, *i, EGL_LEVEL, &level);
            matchNativePixmap = false; // eglGetConfigAttrib(display, *i, EGL_MATCH_NATIVE_PIXMAP, &matchNativePixmap);
            eglGetConfigAttrib(display, *i, EGL_MAX_SWAP_INTERVAL, &maxSwapInterval);
            eglGetConfigAttrib(display, *i, EGL_MIN_SWAP_INTERVAL, &minSwapInterval);
            eglGetConfigAttrib(display, *i, EGL_NATIVE_RENDERABLE, &nativeRenderable);
            eglGetConfigAttrib(display, *i, EGL_NATIVE_VISUAL_TYPE, &nativeVisualType);
            eglGetConfigAttrib(display, *i, EGL_RENDERABLE_TYPE, &renderableType);
            eglGetConfigAttrib(display, *i, EGL_SAMPLE_BUFFERS, &sampleBuffers);
            eglGetConfigAttrib(display, *i, EGL_SAMPLES, &samples);
            eglGetConfigAttrib(display, *i, EGL_STENCIL_SIZE, &stencilSize);
            eglGetConfigAttrib(display, *i, EGL_SURFACE_TYPE, &surfaceType);
            eglGetConfigAttrib(display, *i, EGL_TRANSPARENT_TYPE, &transparentType);
            eglGetConfigAttrib(display, *i, EGL_TRANSPARENT_RED_VALUE, &transparentRedValue);
            eglGetConfigAttrib(display, *i, EGL_TRANSPARENT_GREEN_VALUE, &transparentGreenValue);
            eglGetConfigAttrib(display, *i, EGL_TRANSPARENT_BLUE_VALUE, &transparentBlueValue);

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

        EGL::Config config = nullptr;
        if (!eglChooseConfig(display, configAttribList, &config, 1, &configCount)) {
            Throw(::Exceptions::BasicLabel("Failure in eglChooseConfig"));
        }

            //
            //      Collect version information
            //          -- useful for reporting to the log file
            //

        const char* versionMain          = eglQueryString(display, EGL_VERSION);
        const char* versionVendor        = eglQueryString(display, EGL_VENDOR);
        const char* versionClientAPIs    = eglQueryString(display, EGL_CLIENT_APIS);
        const char* versionExtensions    = eglQueryString(display, EGL_EXTENSIONS);
        (void)versionMain; (void)versionVendor; (void)versionClientAPIs; (void)versionExtensions;

            //
            //      Build the "immediate" context
            //          One context to rule them all.
            //

        EGLint contextAttribs[]  = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };
        EGL::Display contextTemp = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
        if (contextTemp == EGL_NO_CONTEXT) {
            Throw(::Exceptions::BasicLabel("Failure while creating the immediate context"));
        }

            // (here, contextDestroyer just binds the first parameter to eglDestroyContext)
        auto contextDestroyer = [=](EGL::Context context) {eglDestroyContext(displayTemp, context);};
        DestructorPointer<EGL::Context, decltype(contextDestroyer)> context(contextTemp, contextDestroyer);

            //
            //      Initialisation is complete. We can commit to our member pointers now.
            //

        intrusive_ptr<Metal_OpenGLES::DeviceContext> finalContext = moveptr(new Metal_OpenGLES::DeviceContext(display, context.release()));

        _display = display.release();
        _immediateContext = std::move(finalContext);
        _config = config;
    }

    Device::~Device()
    {
        if (_display != EGL_NO_DISPLAY) {
            EGLBoolean result = eglTerminate(_display);
            (void)result; assert(result);
            _display = nullptr;
        }
    }

    std::unique_ptr<IPresentationChain>   Device::CreatePresentationChain(const void* platformValue)
    {
        std::unique_ptr<IPresentationChain> result = std::make_unique<PresentationChain>(_display, _config, platformValue);

            //      Angle project problem! We need to all eglMakeCurrent before
            //      we can create an results (vertex buffers, index buffers, textures, etc)
        auto presChain = checked_cast<PresentationChain*>(result.get());
        EGL::Surface underlyingSurface = presChain->GetUnderlyingSurface();
        if (!eglMakeCurrent(    _display,   
                                underlyingSurface, 
                                underlyingSurface, 
                                _immediateContext->GetUnderlying())) {
            Throw(::Exceptions::BasicLabel("Failure in eglMakeCurrent"));
        }

        return std::move(result);
    }

    void    Device::BeginFrame(IPresentationChain* presentationChain)
    {
        auto presChain = checked_cast<PresentationChain*>(presentationChain);

            //
            //      Make the immediate context the current context
            //      (with the presentation chain surface
            //
        EGL::Surface underlyingSurface = presChain->GetUnderlyingSurface();
        if (!eglMakeCurrent(    _display,   
                                underlyingSurface, 
                                underlyingSurface, 
                                _immediateContext->GetUnderlying())) {
            Throw(::Exceptions::BasicLabel("Failure in eglMakeCurrent"));
        }

        EGLint width, height;
        eglQuerySurface(_display, underlyingSurface, EGL_WIDTH, &width);
        eglQuerySurface(_display, underlyingSurface, EGL_HEIGHT, &height);
        
        glViewport(0, 0, width, height);
        glClearColor(1.f, 0.5f, 0.5f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
    }

    void* Device::QueryInterface(const GUID& guid)
    {
        return nullptr;
    }

    #if !FLEX_USE_VTABLE_Device
        namespace Detail
        {
            void* Ignore_Device::QueryInterface(const GUID& guid)
            {
                return nullptr;
            }
        }
    #endif
    
    void* DeviceOpenGLES::QueryInterface(const GUID& guid)
    {
        // if (guid == __uuidof(IDeviceOpenGLES)) {
        //     return (IDeviceOpenGLES*)this;
        // }
        // return NULL;
        return (IDeviceOpenGLES*)this;
    }

    DeviceOpenGLES::DeviceOpenGLES() {}
    DeviceOpenGLES::~DeviceOpenGLES() {}

    intrusive_ptr<Metal_OpenGLES::DeviceContext>   DeviceOpenGLES::CreateDeferredContext()
    {
        EGLint contextAttribs[]  = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };
        EGL::Context contextTemp = eglCreateContext(_display, _config, _immediateContext->GetUnderlying(), contextAttribs);
        if (contextTemp == EGL_NO_CONTEXT) {
            Throw(::Exceptions::BasicLabel("Failure while creating the immediate context"));
        }

        return moveptr(new Metal_OpenGLES::DeviceContext(_display, contextTemp));
    }

    intrusive_ptr<Metal_OpenGLES::DeviceContext>   DeviceOpenGLES::GetImmediateContext()
    {
        return _immediateContext;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////

    PresentationChain::PresentationChain(EGL::Display display, EGL::Config config, const void* platformValue)
    : _display(display)
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
        EGL::Surface surfaceTemp = eglCreateWindowSurface(
            display, config, 
            EGLNativeWindowType(platformValue), surfaceAttribList);
        if (surfaceTemp == EGL_NO_SURFACE) {
            Throw(::Exceptions::BasicLabel("Failure constructing EGL window surface"));
        }

        _surface = surfaceTemp;
    }

    PresentationChain::~PresentationChain()
    {
        if (_surface != EGL_NO_SURFACE) {
            EGLBoolean result = eglDestroySurface(_display, _surface);
            (void)result; assert(result);
            _surface = nullptr;
        }
    }

    void            PresentationChain::Present()
    {
        EGLBoolean result = eglSwapBuffers(_display, _surface);
        (void)result; assert(result);
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////

    render_dll_export std::unique_ptr<IDevice>    CreateDevice()
    {
        return std::make_unique<DeviceOpenGLES>();
    }

    render_dll_export Metal_OpenGLES::GlobalResources&        GetGlobalResources()
    {
        static Metal_OpenGLES::GlobalResources gGlobalResources;
        return gGlobalResources;
    }
}

