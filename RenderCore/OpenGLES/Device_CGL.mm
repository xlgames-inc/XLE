
#include "Device_CGL.h"
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

#include <OpenGL/OpenGL.h>
#include <AppKit/NSOpenGL.h>

namespace RenderCore { namespace ImplOpenGLES
{

    IResourcePtr    ThreadContext::BeginFrame(IPresentationChain& presentationChain)
    {
        assert(!_activeTargetContext);
        auto& presChain = *checked_cast<PresentationChain*>(&presentationChain);
        _activeTargetContext = presChain.GetUnderlying();
        [_activeTargetContext.get() makeCurrentContext];
        return nullptr;     // the target is always render buffer 0
    }

    void        ThreadContext::Present(IPresentationChain& presentationChain)
    {
        auto& presChain = *checked_cast<PresentationChain*>(&presentationChain);
        assert(presChain.GetUnderlying() == _activeTargetContext);
        [_activeTargetContext.get() flushBuffer];
        _activeTargetContext = nullptr;
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

    ThreadContext::ThreadContext(const std::shared_ptr<Device>& device)
    : _device(device)
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

    ThreadContextOpenGLES::ThreadContextOpenGLES(const std::shared_ptr<Device>& device)
    : ThreadContext(device)
    {
        _deviceContext = std::make_shared<Metal_OpenGLES::DeviceContext>();
    }

    ThreadContextOpenGLES::~ThreadContextOpenGLES() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    Device::Device()
    {
        _objectFactory = std::make_shared<Metal_OpenGLES::ObjectFactory>();
    }

    Device::~Device()
    {
    }

    std::unique_ptr<IPresentationChain>   Device::CreatePresentationChain(const void* platformValue, unsigned width, unsigned height)
    {
        return std::make_unique<PresentationChain>(
            *_objectFactory,
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
        return DeviceDesc { "OpenGLES-CGL", "", "" };
    }

    std::unique_ptr<IThreadContext>   Device::CreateDeferredContext()
    {
        return std::make_unique<ThreadContextOpenGLES>(shared_from_this());
    }

    std::shared_ptr<IThreadContext>   Device::GetImmediateContext()
    {
        if (!_immediateContext)
            _immediateContext = std::make_shared<ThreadContextOpenGLES>(shared_from_this());
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
        const void* platformValue, unsigned width, unsigned height)
    {
        /*CGLPixelFormatAttribute*/
        unsigned pixelAttrs[] = {
            kCGLPFADoubleBuffer,
            kCGLPFAOpenGLProfile, (int) kCGLOGLPVersion_GL4_Core,
            kCGLPFAColorSize, 24,
            kCGLPFAAlphaSize, 8,
            kCGLPFADepthSize, 24,
            kCGLPFAStencilSize, 8,
            kCGLPFASampleBuffers, 0,
            0,
        };

        int virtualScreenCount;
        CGLPixelFormatObj pixelFormat;
        auto error = CGLChoosePixelFormat((const CGLPixelFormatAttribute*)pixelAttrs, &pixelFormat, &virtualScreenCount);
        assert(!error);
        assert(pixelFormat);
        (void)virtualScreenCount;

        CGLContextObj context;
        error = CGLCreateContext(pixelFormat, NULL, &context);
        assert(!error);
        assert(context);

        _nsContext = TBC::moveptr([[NSOpenGLContext alloc] initWithCGLContextObj:context]);

        _nsContext.get().view = (NSView*)platformValue;
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

