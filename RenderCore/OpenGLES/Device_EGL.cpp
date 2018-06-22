// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Device_EGL.h"
#include "EGLUtils.h"
#include "Metal/DeviceContext.h"
#include "../IAnnotator.h"
#include "../Format.h"
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

            if (strstr(extensionsString, "GL_EXT_debug_label")) {
                featureSet |= Metal_OpenGLES::FeatureSet::LabelObject;
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

    static EGLConfig PickBestFromFilteredList(EGLDisplay display, IteratorRange<const EGLConfig*> filteredList, const Format requestFmt, TextureSamples samples)
    {
        // Log the configurations that the driver filtered out
        #if defined(_DEBUG)
            Log(Verbose) << "Filtered configs:" << std::endl;
            for (const auto&cfg:filteredList) {
                StreamConfigShort(Log(Verbose), display, cfg) << std::endl;
            }
        #endif

        // Look for something that matches exactly the request we asked for.
        // If we don't find, then just drop back to the first option
        for (const auto&cfg:filteredList) {
            EGLint sampleBuffers = 0;
            if (eglGetConfigAttrib(display, cfg, EGL_SAMPLE_BUFFERS, &sampleBuffers)) {
                auto fmt = Conv::GetTargetFormat(display, cfg);
                if (AsTypelessFormat(fmt) == AsTypelessFormat(requestFmt)
                    && sampleBuffers == (samples._sampleCount > 1)) {

                    #if defined(_DEBUG)
                        Log(Verbose) << "Got exact match:";
                        StreamConfigShort(Log(Verbose), display, cfg) << std::endl;
                    #endif
                    return cfg;
                }
            }
        }

        // Didn't find a close match, just return the first option that the driver
        // provided
        #if defined(_DEBUG)
            Log(Verbose) << "Falling back to first option:";
            StreamConfigShort(Log(Verbose), display, filteredList[0]) << std::endl;
        #endif
        return filteredList[0];
    }

    struct TargetRequest { EGLint r, g, b, a; };
    struct DepthStencilRequest { EGLint d, s; };
    static std::vector<EGLConfig> GetFilteredConfigs(
        EGLDisplay display,
        EGLint renderableType, EGLint surfaceType,
        const TargetRequest& target, const DepthStencilRequest& depthStencil,
        EGLint sampleCount)
    {
        std::vector<EGLint> configAttribs;
        configAttribs.push_back(EGL_RED_SIZE); configAttribs.push_back(target.r);
        configAttribs.push_back(EGL_GREEN_SIZE); configAttribs.push_back(target.g);
        configAttribs.push_back(EGL_BLUE_SIZE); configAttribs.push_back(target.b);
        if (target.a != EGL_DONT_CARE) {
            configAttribs.push_back(EGL_ALPHA_SIZE);
            configAttribs.push_back(target.a);
        }
        if (depthStencil.d != EGL_DONT_CARE) {
            configAttribs.push_back(EGL_DEPTH_SIZE);
            configAttribs.push_back(depthStencil.d);
        }
        if (depthStencil.s != EGL_DONT_CARE) {
            configAttribs.push_back(EGL_STENCIL_SIZE);
            configAttribs.push_back(depthStencil.s);
        }
        configAttribs.push_back(EGL_SAMPLE_BUFFERS); configAttribs.push_back((sampleCount > 1) ? 1 : 0);
        configAttribs.push_back(EGL_SAMPLES); configAttribs.push_back((sampleCount > 1) ? sampleCount : 0);
        configAttribs.push_back(EGL_RENDERABLE_TYPE); configAttribs.push_back(renderableType);
        configAttribs.push_back(EGL_CONFIG_CAVEAT); configAttribs.push_back(EGL_NONE);
        configAttribs.push_back(EGL_SURFACE_TYPE); configAttribs.push_back(surfaceType);
        configAttribs.push_back(EGL_NONE); configAttribs.push_back(EGL_NONE);

        EGLint numConfigs = 0;
        auto chooseConfigResult = eglChooseConfig(display, configAttribs.data(), NULL, 0, &numConfigs);
        if (chooseConfigResult && numConfigs > 1) {
            std::vector<EGLConfig> configs(numConfigs);
            auto chooseConfigResult = eglChooseConfig(display, configAttribs.data(), configs.data(), (EGLint)configs.size(), &numConfigs);
            if (chooseConfigResult)
                return configs;
        }

        return {};
    }

    static std::experimental::optional<std::pair<EGLConfig, EGLConfig>> TryGetEGLSurfaceConfig(
        EGLDisplay display, const PresentationChainDesc& desc)
    {
        // Try to select the right configuration to use, based on some basic configuration
        // guidelines passed in, and the preferred features of the device.
        //
        // The presentation chain interface doesn't allow the client to specify strict limitations
        // on the options selected. We only get to specify a desired format, and we have to try
        // to match it to what configurations the driver supports, in some sensible way.
        //
        // The way that eglChooseConfig works is a little non-intuitive. Some of the attributes (like
        // the color precision) are lower bounds, and the driver will prioritize higher precision
        // options. Other attributes are upper bounds.
        //
        // In other words, if we call eglChooseConfig and only consider the first option it returns,
        // we can end up with results that tend to be at either the maximum or minimum ranges that
        // the driver supports.
        //
        // So, we'll do this in two passes:
        //   1) we'll call eglChooseConfig with some rough starter values, and get back a set of
        //      configurations
        //   2) we'll search through the list of configurations and try to pick out the one that
        //      most closely matches the expected request
        //
        // The any of the list of configurations from 1) are expected to be reasonable, and the
        // first will probably be a good middle-of-the-road type selection. This helps us ensure
        // that we always get something reasonable, on every device (if we try to be too picky,
        // we risk matching nothing on some devices).
        //
        // The second pass is necessary for specifically picking out lower quality options (for
        // example when we want to return a 565 buffer, because the driver will almost always
        // prioritize that lower than a 888/8888 buffer).

        auto requestedPrecision = GetComponentPrecision(desc._format);
        auto components = GetComponents(desc._format);
        if (components != FormatComponents::RGB && components != FormatComponents::RGBAlpha) {
            Log(Warning) << "Presentation chain format was not a RGB or RGBA format. Treating as if a RGB format as requested." << std::endl;
        }
        auto requestHasAlpha = components == FormatComponents::RGBAlpha;

        // Select a priority order for target color format, based on desc._format
        // Note that in the fallback order, we don't need to specify all options. eglChooseConfig
        // is going to give us all configurations that match our request -- so generally we can just
        // check 2 formats: one that tries to isolate the specific format we want, and one that
        // is very permissive
        std::vector<TargetRequest> targetFallbackOrder;
        {
            TargetRequest A {  4,  4,  4, requestHasAlpha ? 4 : EGL_DONT_CARE };
            TargetRequest B {  5,  5,  5, requestHasAlpha ? 1 : EGL_DONT_CARE };
            TargetRequest C {  5,  6,  5, EGL_DONT_CARE };
            TargetRequest D {  8,  8,  8, requestHasAlpha ? 8 : EGL_DONT_CARE };
            TargetRequest E { 10, 10, 10, requestHasAlpha ? 2 : EGL_DONT_CARE };
            TargetRequest F { 11, 11, 10, EGL_DONT_CARE };

            if (requestedPrecision >= 10) { // HDR
                targetFallbackOrder = { E, A };
            } else if (requestedPrecision >= 8) { // 8-bit
                targetFallbackOrder = { D, A };
            } else if (requestedPrecision >= 5) { // low precision
                targetFallbackOrder = { B, A };
            } else {
                targetFallbackOrder = { A };
            }
            (void)C;(void)F;
        }

        std::vector<DepthStencilRequest> depthStencilFallbackOrder = {
            { 24, EGL_DONT_CARE },
            { 16, EGL_DONT_CARE }
        };

        // select a priority order for MSAA sample requests
        // Note that due to the ordering here, the system will prefer to select a less desired
        // target format with the right number of MSAA samples; even if there is a matching color
        // format with non-matching MSAA samples.
        std::vector<GLint> sampleCounts;
        if (desc._samples._sampleCount > 1) {
            sampleCounts = { desc._samples._sampleCount, 0 };
        } else {
            sampleCounts = { 0 };
        }

        // Always prefer ES3 if we can get it
        std::vector<GLint> renderableTypes = { EGL_OPENGL_ES3_BIT, EGL_OPENGL_ES2_BIT };

        for (auto renderableType:renderableTypes) {
            for (auto s:sampleCounts) {
                for (const auto&d:depthStencilFallbackOrder) {
                    for (const auto&t:targetFallbackOrder) {

                        auto filteredConfigs = GetFilteredConfigs(
                            display, renderableType, EGL_WINDOW_BIT,
                            t, d, s);
                        if (filteredConfigs.empty())
                            continue;

                        // Select our preferred option from the filtered list
                        auto windowSurfaceCfg = PickBestFromFilteredList(display, MakeIteratorRange(filteredConfigs), desc._format, desc._samples);

                        // Try to match it with a "pbuffer" configuration that is an similar as possible
                        auto deferredCfg = windowSurfaceCfg;
                        GLint surfaceTypes = 0;
                        if (eglGetConfigAttrib(display, deferredCfg, EGL_SURFACE_TYPE, &surfaceTypes)
                            && !(surfaceTypes & EGL_PBUFFER_BIT)) {

                            auto pbufferConfigs = GetFilteredConfigs(
                                display, renderableType, EGL_PBUFFER_BIT,
                                t, d, 0);

                            if (pbufferConfigs.empty())
                                continue;

                            deferredCfg = PickBestFromFilteredList(display, MakeIteratorRange(pbufferConfigs), desc._format, TextureSamples::Create(0,1));
                        }

                        return std::make_pair(windowSurfaceCfg, deferredCfg);
                    }
                }
            }
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
            auto config = TryGetEGLSurfaceConfig(_display, desc);
            if (!config)
                Throw(::Exceptions::BasicLabel("Cannot select root context configuration"));

            #if defined(_DEBUG)
                Log(Verbose) << "Root context selected configs:" << std::endl;
                StreamConfigShort(Log(Verbose), _display, config.value().first) << std::endl;
                StreamConfigShort(Log(Verbose), _display, config.value().second) << std::endl;
            #endif

            _rootContextConfig = config.value().first;
            _deferredContextConfig = config.value().second;
            _glesVersion = GetGLESVersionFromConfig(_display, _rootContextConfig);
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
        return std::make_unique<ThreadContext>(_display, _deferredContextConfig, _rootContext->GetUnderlying(), _rootContext->GetFeatureSet(), shared_from_this());
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

        if (!platformValue) {
            // Create a dummy pbuffer
            int pbufferAttribsList[] = {
                EGL_WIDTH, 1, EGL_HEIGHT, 1,
                EGL_NONE, EGL_NONE
            };
            _surface = eglCreatePbufferSurface(display, sharedContextCfg, pbufferAttribsList);
            if (_surface == EGL_NO_SURFACE)
                Throw(::Exceptions::BasicLabel("Failure in eglCreatePbufferSurface (%s)", ErrorToName(eglGetError())));
        } else {
            // Create the main output surface
            // This must be tied to the window in Win32 -- so we can't begin construction of this until we build the presentation chain.
            const EGLint surfaceAttribList[] = { EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE, EGL_NONE };
            _surface = eglCreateWindowSurface(display, sharedContextCfg, EGLNativeWindowType(platformValue), surfaceAttribList);
            if (_surface == EGL_NO_SURFACE)
                Throw(::Exceptions::BasicLabel("Failure constructing EGL window surface with error: (%s)", ErrorToName(eglGetError())));
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
        // Build the root context
        EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, GetGLESVersionFromConfig(_display, cfgForNewContext) / 100,
            EGL_NONE, EGL_NONE
        };
        _context = eglCreateContext(_display, cfgForNewContext, rootContext, contextAttribs);
        if (_context == EGL_NO_CONTEXT)
            Throw(::Exceptions::BasicLabel("Failure while creating EGL context (%s)", ErrorToName(eglGetError())));

        _dummySurface = EGL_NO_SURFACE;

        // create a "dummy PBuffer" so we can use this context for non-rendering operations (such as
        // loading & initialization)
        GLint surfaceTypes = 0;
        if (eglGetConfigAttrib(display, cfgForNewContext, EGL_SURFACE_TYPE, &surfaceTypes)
            && (surfaceTypes & EGL_PBUFFER_BIT)) {

            int pbufferAttribsList[] = {
                EGL_WIDTH, 1, EGL_HEIGHT, 1,
                EGL_NONE, EGL_NONE
            };
            _dummySurface = eglCreatePbufferSurface(_display, cfgForNewContext, pbufferAttribsList);
            if (_dummySurface == EGL_NO_SURFACE)
                Throw(::Exceptions::BasicLabel("Failure in eglCreatePbufferSurface (%s)", ErrorToName(eglGetError())));
        }

        #if defined(_DEBUG)
            Log(Verbose) << "Created EGL context: " << std::endl;
            StreamContext(Log(Verbose), display, _context);
        #endif

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
