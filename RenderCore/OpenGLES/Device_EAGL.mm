
#include "Device_EAGL.h"
#include "Metal/DeviceContext.h"
#include "Metal/IndexedGLType.h"
#include "Metal/Resource.h"
#include "../IAnnotator.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Core/Exceptions.h"
#include <type_traits>
#include <iostream>
#include <assert.h>
#include "IncludeGLES.h"

#include <OpenGLES/EAGL.h>
#include <UIKit/UIView.h>

namespace RenderCore { namespace ImplOpenGLES
{

    IResourcePtr    ThreadContext::BeginFrame(IPresentationChain& presentationChain)
    {
        assert(!_activeFrameContext);
        _activeFrameContext = nullptr;

        auto& presChain = *checked_cast<PresentationChain*>(&presentationChain);
        _activeFrameContext = presChain.GetEAGLContext();
        _activeFrameRenderbuffer = presChain.GetFrameRenderbuffer();
        if (!_activeFrameContext)
            _activeFrameContext = _sharedContext;

        [EAGLContext setCurrentContext:_activeFrameContext.get()];

        _activeFrameBuffer = Metal_OpenGLES::GetObjectFactory().CreateFrameBuffer();
        glBindFramebuffer(GL_FRAMEBUFFER, _activeFrameBuffer->AsRawGLHandle());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _activeFrameRenderbuffer->AsRawGLHandle());
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            Throw(std::runtime_error("Framebuffer not complete in PresentationChain::PresentationChain"));

        GLint backingWidth, backingHeight;
        glBindRenderbuffer(GL_RENDERBUFFER, _activeFrameRenderbuffer->AsRawGLHandle());
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);
        glViewport(0,0,backingWidth, backingHeight);

        return nullptr;
    }

    void        ThreadContext::Present(IPresentationChain& presentationChain)
    {
        auto& presChain = *checked_cast<PresentationChain*>(&presentationChain);
        assert(!presChain.GetEAGLContext() || presChain.GetEAGLContext() == _activeFrameContext);
        if (_activeFrameContext) {
            glBindRenderbuffer(GL_RENDERBUFFER, _activeFrameRenderbuffer->AsRawGLHandle());
            [_activeFrameContext.get() presentRenderbuffer:GL_RENDERBUFFER];
        }
        _activeFrameRenderbuffer.reset();
        _activeFrameBuffer.reset();
        _activeFrameContext = nullptr;
        [EAGLContext setCurrentContext:_sharedContext.get()];
    }

    bool                        ThreadContext::IsImmediate() const { return false; }
    ThreadContextStateDesc      ThreadContext::GetStateDesc() const { return {}; }
    std::shared_ptr<IDevice>    ThreadContext::GetDevice() const { return nullptr; }
    void                        ThreadContext::IncrFrameId() {}
    void                        ThreadContext::InvalidateCachedState() const {}

    IAnnotator&                 ThreadContext::GetAnnotator()
    {
        if (!_annotator) {
            auto d = _device.lock();
            assert(d);
            _annotator = CreateAnnotator(*d);
        }
        return *_annotator;
    }

    ThreadContext::ThreadContext(EAGLContext* sharedContext, const std::shared_ptr<Device>& device)
    : _device(device)
    , _sharedContext(sharedContext)
    {}

    ThreadContext::~ThreadContext() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    std::shared_ptr<Metal_OpenGLES::DeviceContext>&  ThreadContextOpenGLES::GetUnderlying()
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

    ThreadContextOpenGLES::ThreadContextOpenGLES(EAGLContext* sharedContext, const std::shared_ptr<Device>& device)
    : ThreadContext(sharedContext, device)
    {
        _deviceContext = std::make_shared<Metal_OpenGLES::DeviceContext>();
    }

    ThreadContextOpenGLES::~ThreadContextOpenGLES() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    Device::Device()
    {
        auto* t = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
        _sharedContext = TBC::moveptr(t);
        [EAGLContext setCurrentContext:_sharedContext.get()];
        _objectFactory = std::make_shared<Metal_OpenGLES::ObjectFactory>();
    }

    Device::~Device()
    {
    }

    std::unique_ptr<IPresentationChain>   Device::CreatePresentationChain(const void* platformValue, unsigned width, unsigned height)
    {
        return std::make_unique<PresentationChain>(
            *_objectFactory,
            _sharedContext.get(),
            platformValue, width, height);
    }

    void* Device::QueryInterface(size_t guid)
    {
        return nullptr;
    }

    IResourcePtr Device::CreateResource(const ResourceDesc& desc, const ResourceInitializer& init)
    {
        // hack -- only getting a single subresource here!
        return std::make_shared<Metal_OpenGLES::Resource>(*_objectFactory, desc, init);
    }

    DeviceDesc Device::GetDesc()
    {
        return DeviceDesc { "OpenGLES-EAGL", "", "" };
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

    #if !FLEX_USE_VTABLE_Device
        namespace Detail
        {
            void* Ignore_Device::QueryInterface(size_t guid)
            {
                return nullptr;
            }
        }
    #endif

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
        EAGLContext* eaglContext,
        const void* platformValue, unsigned width, unsigned height)
    {
        if (![((NSObject*)platformValue) isKindOfClass:UIView.class])
            Throw(std::runtime_error("Platform value in PresentationChain::PresentationChain is not a UIView"));

        auto* view = (UIView*)platformValue;
        if (![view.layer conformsToProtocol:@protocol(EAGLDrawable)])
            Throw(std::runtime_error("Layer in UIView passed to PresentationChain::PresentationChain does not conform to EAGLDrawable protocol"));

        auto eaglDrawable = (id<EAGLDrawable>)view.layer;
        eaglDrawable.drawableProperties =
            [NSDictionary dictionaryWithObjectsAndKeys:
                [NSNumber numberWithBool:FALSE], kEAGLDrawablePropertyRetainedBacking,
                kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
                nil];

        _frameRenderbuffer = objFactory.CreateRenderBuffer();
        glBindRenderbuffer(GL_RENDERBUFFER, _frameRenderbuffer->AsRawGLHandle());

        auto res = [eaglContext renderbufferStorage: GL_RENDERBUFFER fromDrawable: eaglDrawable];
        if (!res)
            Throw(std::runtime_error("Failed to allocate renderbuffer storage for EAGL drawable in PresentationChain::PresentationChain"));

        // Get the drawable buffer's width and height so we can create a depth buffer for the FBO
        GLint backingWidth, backingHeight;
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);
    }

    PresentationChain::~PresentationChain()
    {
    }

    void PresentationChain::Resize(unsigned newWidth, unsigned newHeight)
    {
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

