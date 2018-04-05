
#include "Device_CGL.h"
#include "Metal/DeviceContext.h"
#include "Metal/ObjectFactory.h"
#include "Metal/Resource.h"
#include "../IAnnotator.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Core/Exceptions.h"
#include <type_traits>
#include <iostream>
#include <assert.h>

#include <OpenGL/OpenGL.h>
#include <AppKit/NSOpenGL.h>
#include <AppKit/NSOpenGLView.h>

namespace RenderCore { namespace ImplOpenGLES
{
    static Metal_OpenGLES::FeatureSet::BitField GetFeatureSet()
    {
        return Metal_OpenGLES::FeatureSet::GLES200;
    }

    IResourcePtr    ThreadContext::BeginFrame(IPresentationChain& presentationChain)
    {
        assert(!_activeFrameContext);
        if (_activeFrameContext) {
            CGLReleaseContext(_activeFrameContext);
            _activeFrameContext = nullptr;
        }

        auto& presChain = *checked_cast<PresentationChain*>(&presentationChain);
        _activeFrameContext = presChain.GetUnderlying().get().CGLContextObj;
        CGLRetainContext(_activeFrameContext);
        CGLSetCurrentContext(_activeFrameContext);

        // auto rbDesc = Metal_OpenGLES::ExtractDesc((OpenGL::RenderBuffer*)0);
        // (void)rbDesc;

        /*glBindFramebuffer(GL_FRAMEBUFFER, 0);
        GLint objectType = ~0, objectName = ~0u;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &objectType);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &objectName);*/

        /*GLint swapRectangle[4] = {0,0,0,0};
        GLint backingSize[2] = {0,0};
        auto err = CGLGetParameter(_activeFrameContext, kCGLCPSwapRectangle, swapRectangle);
        err = CGLGetParameter(_activeFrameContext, kCGLCPSurfaceBackingSize, backingSize);
        (void)err;*/

        if (presChain._fakeBackBuffer)
            return presChain._fakeBackBuffer;

        return presChain._backBufferResource;
    }

    void        ThreadContext::Present(IPresentationChain& presentationChain)
    {
        auto& presChain = *checked_cast<PresentationChain*>(&presentationChain);
        assert(presChain.GetUnderlying().get().CGLContextObj == _activeFrameContext);
        if (_activeFrameContext) {
            // If using "fake back buffer" mode, blt to the true back buffer
            if (presChain._fakeBackBuffer) {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, presChain._fakeBackBufferFrameBuffer->AsRawGLHandle());
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glBlitFramebuffer(
                    0, 0, presChain._backBufferDesc._textureDesc._width, presChain._backBufferDesc._textureDesc._height,
                    0, 0, presChain._backBufferDesc._textureDesc._width, presChain._backBufferDesc._textureDesc._height,
                    GL_COLOR_BUFFER_BIT, GL_NEAREST);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            }

            CGLFlushDrawable(_activeFrameContext);
            CGLReleaseContext(_activeFrameContext);
            _activeFrameContext = nullptr;
        }
        CGLSetCurrentContext(_sharedContext);
    }

    bool                        ThreadContext::IsImmediate() const { return false; }
    ThreadContextStateDesc      ThreadContext::GetStateDesc() const { return {}; }
    std::shared_ptr<IDevice>    ThreadContext::GetDevice() const
    {
        return _device.lock();
    }
    void                        ThreadContext::IncrFrameId() {}
    void                        ThreadContext::InvalidateCachedState() const {}

    IAnnotator&                 ThreadContext::GetAnnotator()
    {
        if (!_annotator) {
            auto d = _device.lock();
            assert(d);
            _annotator = RenderCore::CreateAnnotator(*d);
        }
        return *_annotator;
    }

    ThreadContext::ThreadContext(CGLContextObj sharedContext, const std::shared_ptr<Device>& device)
    : _device(device)
    , _sharedContext(sharedContext)
    , _activeFrameContext(nullptr)
    {
        CGLRetainContext(_sharedContext);
    }

    ThreadContext::~ThreadContext()
    {
        if (_activeFrameContext) {
            CGLReleaseContext(_activeFrameContext);
            _activeFrameContext = nullptr;
        }
        if (_sharedContext) {
            CGLReleaseContext(_sharedContext);
            _sharedContext = nullptr;
        }
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    const std::shared_ptr<Metal_OpenGLES::DeviceContext>&  ThreadContextOpenGLES::GetDeviceContext()
    {
        return _deviceContext;
    }

    void*       ThreadContextOpenGLES::QueryInterface(size_t guid)
    {
        if (guid == typeid(IThreadContextOpenGLES).hash_code()) {
            return (IThreadContextOpenGLES*)this;
        }
        return nullptr;
    }

    ThreadContextOpenGLES::ThreadContextOpenGLES(CGLContextObj sharedContext, const std::shared_ptr<Device>& device)
    : ThreadContext(sharedContext, device)
    {
        _deviceContext = std::make_shared<Metal_OpenGLES::DeviceContext>(GetFeatureSet());
    }

    ThreadContextOpenGLES::~ThreadContextOpenGLES() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    Device::Device()
    {
        _objectFactory = std::make_shared<Metal_OpenGLES::ObjectFactory>(GetFeatureSet());

        /*CGLPixelFormatAttribute*/
        unsigned pixelAttrs[] = {
            // kCGLPFAOpenGLProfile, (int) kCGLOGLPVersion_GL4_Core,
            0,
        };

        int virtualScreenCount;
        CGLPixelFormatObj pixelFormat;
        auto error = CGLChoosePixelFormat((const CGLPixelFormatAttribute*)pixelAttrs, &pixelFormat, &virtualScreenCount);
        assert(!error);
        assert(pixelFormat);
        (void)virtualScreenCount;

        error = CGLCreateContext(pixelFormat, nullptr, &_sharedContext);
        CGLReleasePixelFormat(pixelFormat);

        assert(!error);
        assert(_sharedContext);

        CGLSetCurrentContext(_sharedContext);
    }

    Device::~Device()
    {
        CGLSetCurrentContext(nullptr);
        CGLReleaseContext(_sharedContext);
    }

    std::unique_ptr<IPresentationChain>   Device::CreatePresentationChain(const void* platformValue, unsigned width, unsigned height)
    {
        return std::make_unique<PresentationChain>(
            *_objectFactory,
            _sharedContext,
            platformValue, width, height);
    }

    void* Device::QueryInterface(size_t guid)
    {
        return nullptr;
    }

    IResourcePtr Device::CreateResource(const ResourceDesc& desc, const ResourceInitializer& init)
    {
        return Metal_OpenGLES::CreateResource(*_objectFactory, desc, init);
    }

    FormatCapability Device::QueryFormatCapability(Format format, BindFlag::BitField bindingType)
    {
        auto activeFeatureSet = _objectFactory->GetFeatureSet();
        auto glFmt = Metal_OpenGLES::AsTexelFormatType(format);
        if (glFmt._internalFormat == GL_NONE)
            return FormatCapability::NotSupported;

        bool supported = true;
        if (bindingType & BindFlag::ShaderResource) {
            supported = (activeFeatureSet & glFmt._textureFeatureSet);
        } else if ((bindingType & BindFlag::RenderTarget) || (bindingType & BindFlag::DepthStencil)) {
            supported = (activeFeatureSet & glFmt._renderbufferFeatureSet);
        }

        return supported ? FormatCapability::Supported : FormatCapability::NotSupported;
    }

    DeviceDesc Device::GetDesc()
    {
        return DeviceDesc { "OpenGLES-CGL", "", "" };
    }

    std::unique_ptr<IThreadContext>   Device::CreateDeferredContext()
    {
        return std::make_unique<ThreadContextOpenGLES>(_sharedContext, shared_from_this());
    }

    std::shared_ptr<IThreadContext>   Device::GetImmediateContext()
    {
        if (!_immediateContext)
            _immediateContext = std::make_shared<ThreadContextOpenGLES>(_sharedContext, shared_from_this());
        return _immediateContext;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    void* DeviceOpenGLES::QueryInterface(size_t guid)
    {
        if (guid == typeid(IDeviceOpenGLES).hash_code()) {
            return (IDeviceOpenGLES*)this;
        }
        return nullptr;
    }

    Metal_OpenGLES::DeviceContext * DeviceOpenGLES::GetImmediateDeviceContext()
    {
        return nullptr;
    }

    DeviceOpenGLES::DeviceOpenGLES() {}
    DeviceOpenGLES::~DeviceOpenGLES() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    PresentationChain::PresentationChain(
        Metal_OpenGLES::ObjectFactory& objFactory,
        CGLContextObj sharedContext,
        const void* platformValue, unsigned width, unsigned height)
    {
        _backBufferDesc = CreateDesc(
            BindFlag::RenderTarget, 0, GPUAccess::Write,
            TextureDesc::Plain2D(width, height, Format::R8G8B8A8_UNORM),        // SRGB?
            "backbuffer");

        _sharedContext = sharedContext;
        _platformValue = platformValue;
        CreateUnderlyingContext(objFactory);
    }

    PresentationChain::~PresentationChain()
    {
    }

    void PresentationChain::CreateUnderlyingContext(Metal_OpenGLES::ObjectFactory& objFactory)
    {
        _nsContext = nullptr;

        id objCObj = (id)_platformValue;
        if ([objCObj isKindOfClass:NSOpenGLView.class]) {
            _nsContext = ((NSOpenGLView*)objCObj).openGLContext;
        } else {
            /*CGLPixelFormatAttribute*/
            unsigned pixelAttrs[] = {
                kCGLPFADoubleBuffer,
                // kCGLPFAOpenGLProfile, (int) kCGLOGLPVersion_GL4_Core,
                kCGLPFAColorSize, 24,
                kCGLPFAAlphaSize, 8,
                kCGLPFABackingStore, 0,
                kCGLPFAAccelerated, 1,
                0,
            };

            int virtualScreenCount;
            CGLPixelFormatObj pixelFormat;
            auto error = CGLChoosePixelFormat((const CGLPixelFormatAttribute*)pixelAttrs, &pixelFormat, &virtualScreenCount);
            assert(!error);
            assert(pixelFormat);
            (void)virtualScreenCount;

            CGLContextObj context;
            error = CGLCreateContext(pixelFormat, _sharedContext, &context);
            assert(!error);
            assert(context);
            CGLReleasePixelFormat(pixelFormat);

            _nsContext = TBC::moveptr([[NSOpenGLContext alloc] initWithCGLContextObj:context]);
            CGLReleaseContext(context);

            _nsContext.get().view = (NSView*)_platformValue;
        }

        const bool useFakeBackbuffer = false;
        if (useFakeBackbuffer) {
            _fakeBackBuffer = std::make_shared<Metal_OpenGLES::Resource>(objFactory, _backBufferDesc);

            _fakeBackBufferFrameBuffer = objFactory.CreateFrameBuffer();
            glBindFramebuffer(GL_FRAMEBUFFER, _fakeBackBufferFrameBuffer->AsRawGLHandle());
            glBindRenderbuffer(GL_RENDERBUFFER, _fakeBackBuffer->GetRenderBuffer()->AsRawGLHandle());
            glFramebufferRenderbuffer(
                GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                _fakeBackBuffer->GetRenderBuffer()->AsRawGLHandle());
            GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, drawBuffers);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        _backBufferResource = std::make_shared<Metal_OpenGLES::Resource>(Metal_OpenGLES::Resource::CreateBackBuffer(_backBufferDesc));
    }

    void PresentationChain::Resize(unsigned newWidth, unsigned newHeight)
    {
        if (    newWidth == _backBufferDesc._textureDesc._width
            &&  newHeight == _backBufferDesc._textureDesc._height)
            return;
            
        _backBufferDesc._textureDesc._width = newWidth;
        _backBufferDesc._textureDesc._height = newHeight;

        auto& objFactory = Metal_OpenGLES::GetObjectFactory(*_fakeBackBuffer);
        CreateUnderlyingContext(objFactory);
    }

    const std::shared_ptr<PresentationChainDesc>& PresentationChain::GetDesc() const
    {
        static std::shared_ptr<PresentationChainDesc> dummy;
        return dummy;
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    render_dll_export std::shared_ptr<IDevice> CreateDevice()
    {
        return std::make_shared<DeviceOpenGLES>();
    }

}}

namespace RenderCore
{
    IDeviceOpenGLES::~IDeviceOpenGLES() {}
    IThreadContextOpenGLES::~IThreadContextOpenGLES() {}
}

