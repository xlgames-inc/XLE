
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
        return nullptr;     // the target is always render buffer 0
    }

    void        ThreadContext::Present(IPresentationChain& presentationChain)
    {
        auto& presChain = *checked_cast<PresentationChain*>(&presentationChain);
        assert(presChain.GetUnderlying().get().CGLContextObj == _activeFrameContext);
        if (_activeFrameContext) {
            CGLFlushDrawable(_activeFrameContext);
            CGLReleaseContext(_activeFrameContext);
            _activeFrameContext = nullptr;
        }
        CGLSetCurrentContext(_sharedContext);
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
        _deviceContext = std::make_shared<Metal_OpenGLES::DeviceContext>();
    }

    ThreadContextOpenGLES::~ThreadContextOpenGLES() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    Device::Device()
    {
        _objectFactory = std::make_shared<Metal_OpenGLES::ObjectFactory>();

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
        id objCObj = (id)platformValue;
        if ([objCObj isKindOfClass:NSOpenGLView.class]) {
            _nsContext = ((NSOpenGLView*)objCObj).openGLContext;
        } else {
            /*CGLPixelFormatAttribute*/
            unsigned pixelAttrs[] = {
                kCGLPFADoubleBuffer,
                // kCGLPFAOpenGLProfile, (int) kCGLOGLPVersion_GL4_Core,
                kCGLPFAColorSize, 24,
                kCGLPFAAlphaSize, 8,
                0,
            };

            int virtualScreenCount;
            CGLPixelFormatObj pixelFormat;
            auto error = CGLChoosePixelFormat((const CGLPixelFormatAttribute*)pixelAttrs, &pixelFormat, &virtualScreenCount);
            assert(!error);
            assert(pixelFormat);
            (void)virtualScreenCount;

            CGLContextObj context;
            error = CGLCreateContext(pixelFormat, sharedContext, &context);
            assert(!error);
            assert(context);
            CGLReleasePixelFormat(pixelFormat);

            _nsContext = TBC::moveptr([[NSOpenGLContext alloc] initWithCGLContextObj:context]);
            CGLReleaseContext(context);

            _nsContext.get().view = (NSView*)platformValue;
        }
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

namespace RenderCore
{
    IDeviceOpenGLES::~IDeviceOpenGLES() {}
    IThreadContextOpenGLES::~IThreadContextOpenGLES() {}
}

