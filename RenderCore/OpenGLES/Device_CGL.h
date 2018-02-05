#pragma once

#include "IDeviceOpenGLES.h"
#include "../IDevice.h"
#include "../IThreadContext.h"
#include "../ResourceDesc.h"
#include "../../Utility/Mixins.h"
#include "../../Utility/IntrusivePtr.h"
#include "../../../Externals/Misc/OCPtr.h"

namespace RenderCore { namespace Metal_OpenGLES { class ObjectFactory; }}

@class NSOpenGLContext;
typedef struct _CGLContextObject       *CGLContextObj;

namespace RenderCore { namespace ImplOpenGLES
{
////////////////////////////////////////////////////////////////////////////////

    class PresentationChain : public Base_PresentationChain
    {
    public:
        void                Resize(unsigned newWidth, unsigned newHeight) /*override*/;
        const std::shared_ptr<PresentationChainDesc>& GetDesc() const;
        const TBC::OCPtr<NSOpenGLContext>& GetUnderlying() { return _nsContext; }

        PresentationChain(
            Metal_OpenGLES::ObjectFactory& objFactory,
            CGLContextObj sharedContext,
            const void* platformValue, unsigned width, unsigned height);
        ~PresentationChain();

        ResourceDesc _backBufferDesc;

    private:
        TBC::OCPtr<NSOpenGLContext> _nsContext;
    };

////////////////////////////////////////////////////////////////////////////////

    class Device;

    class ThreadContext : public Base_ThreadContext
    {
    public:
        IResourcePtr BeginFrame(IPresentationChain& presentationChain);
        void        Present(IPresentationChain& presentationChain) /*override*/;

        bool                        IsImmediate() const;
        ThreadContextStateDesc      GetStateDesc() const;
        std::shared_ptr<IDevice>    GetDevice() const;
        void                        IncrFrameId();
        void                        InvalidateCachedState() const;

        IAnnotator&                 GetAnnotator();

        CGLContextObj               GetSharedContext() { return _sharedContext; }
        CGLContextObj               GetActiveFrameContext() { return _activeFrameContext; }

        ThreadContext(CGLContextObj sharedContext, const std::shared_ptr<Device>& device);
        ~ThreadContext();

    private:
        std::weak_ptr<Device>   _device;  // (must be weak, because Device holds a shared_ptr to the immediate context)
        std::unique_ptr<IAnnotator> _annotator;

        CGLContextObj _activeFrameContext;
        CGLContextObj _sharedContext;
    };

    class ThreadContextOpenGLES : public ThreadContext, public Base_ThreadContextOpenGLES
    {
    public:
        const std::shared_ptr<Metal_OpenGLES::DeviceContext>&  GetDeviceContext();
        virtual void*       QueryInterface(size_t guid);
        ThreadContextOpenGLES(CGLContextObj sharedContext, const std::shared_ptr<Device>& device);
        ~ThreadContextOpenGLES();
    private:
        std::shared_ptr<Metal_OpenGLES::DeviceContext> _deviceContext;
    };

////////////////////////////////////////////////////////////////////////////////

    class Device :  public Base_Device, public std::enable_shared_from_this<Device>
    {
    public:
        std::unique_ptr<IPresentationChain> CreatePresentationChain(const void* platformValue, unsigned width, unsigned height);
        void* QueryInterface(size_t guid);

        std::shared_ptr<IThreadContext> GetImmediateContext();
        std::unique_ptr<IThreadContext> CreateDeferredContext();

        using ResourceInitializer = std::function<SubResourceInitData(SubResourceId)>;
        IResourcePtr CreateResource(const ResourceDesc& desc, const ResourceInitializer& init);
        DeviceDesc GetDesc();

        Device();
        ~Device();

    protected:
        std::shared_ptr<ThreadContextOpenGLES> _immediateContext;
        std::shared_ptr<Metal_OpenGLES::ObjectFactory> _objectFactory;
        CGLContextObj _sharedContext;
    };

    class DeviceOpenGLES : public Device, public Base_DeviceOpenGLES
    {
    public:
        virtual void* QueryInterface(size_t guid);
        Metal_OpenGLES::DeviceContext * GetImmediateDeviceContext();

        DeviceOpenGLES();
        ~DeviceOpenGLES();
    };

////////////////////////////////////////////////////////////////////////////////
}}

