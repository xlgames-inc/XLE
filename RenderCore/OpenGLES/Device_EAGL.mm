
#include "Device_EAGL.h"
#include "Metal/DeviceContext.h"
#include "Metal/IndexedGLType.h"
#include "Metal/Resource.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Core/Exceptions.h"
#include <type_traits>
#include <iostream>
#include <assert.h>
#include "IncludeGLES.h"

namespace RenderCore { namespace ImplOpenGLES
{

    ResourcePtr    ThreadContext::BeginFrame(IPresentationChain& presentationChain) { return nullptr; }

    void        ThreadContext::Present(IPresentationChain& presentationChain) {}

    bool                        ThreadContext::IsImmediate() const { return false; }
    ThreadContextStateDesc      ThreadContext::GetStateDesc() const { return {}; }
    std::shared_ptr<IDevice>    ThreadContext::GetDevice() const { return nullptr; }
    void                        ThreadContext::IncrFrameId() {}
    void                        ThreadContext::InvalidateCachedState() const {}

    IAnnotator&                 ThreadContext::GetAnnotator() { return *(IAnnotator*)nullptr; }

    ThreadContext::ThreadContext() {}
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

    ThreadContextOpenGLES::ThreadContextOpenGLES()
    {
        _deviceContext = std::make_shared<Metal_OpenGLES::DeviceContext>();
    }

    ThreadContextOpenGLES::~ThreadContextOpenGLES() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    Device::Device()
    {
        _immediateContext = std::make_shared<ThreadContextOpenGLES>();
        _objectFactory = std::make_shared<Metal_OpenGLES::ObjectFactory>(*this);
    }

    Device::~Device()
    {
    }

    std::unique_ptr<IPresentationChain>   Device::CreatePresentationChain(const void* platformValue, unsigned width, unsigned height)
    {
        return std::make_unique<PresentationChain>(platformValue, width, height);
    }

    void* Device::QueryInterface(size_t guid)
    {
        return nullptr;
    }

    ResourcePtr Device::CreateResource(const ResourceDesc& desc, const ResourceInitializer& init)
    {
        // hack -- only getting a single subresource here!
        return std::make_shared<Metal_OpenGLES::Resource>(*_objectFactory, desc, init({0,0}));
    }

    DeviceDesc Device::GetDesc()
    {
        return DeviceDesc { "OpenGLES-EAGL", "", "" };
    }

    std::unique_ptr<IThreadContext>   Device::CreateDeferredContext()
    {
        return std::make_unique<ThreadContextOpenGLES>();
    }

    std::shared_ptr<IThreadContext>   Device::GetImmediateContext()
    {
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

    PresentationChain::PresentationChain(const void* platformValue, unsigned width, unsigned height)
    {

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

    render_dll_export std::unique_ptr<IDevice> CreateDevice()
    {
        return std::make_unique<DeviceOpenGLES>();
    }

}}

