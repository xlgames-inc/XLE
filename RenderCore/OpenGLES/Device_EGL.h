
#pragma once

#include "../IDevice.h"
#include "IDeviceOpenGLES.h"
#include "../../Utility/Mixins.h"
#include "../../Utility/IntrusivePtr.h"

namespace RenderCore
{
////////////////////////////////////////////////////////////////////////////////

    class PresentationChain : public Base_PresentationChain
    {
    public:
        void                Resize(unsigned newWidth, unsigned newHeight) /*override*/;
        const std::shared_ptr<PresentationChainDesc>& GetDesc() const;

        PresentationChain();
        ~PresentationChain();
    };

////////////////////////////////////////////////////////////////////////////////

    class ThreadContext : public Basic_ThreadContext
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

        ThreadContext();
        ~ThreadContext();
    };

////////////////////////////////////////////////////////////////////////////////

    class Device :  public Base_Device, noncopyable
    {
    public:
        std::unique_ptr<IPresentationChain> CreatePresentationChain(const void* platformValue);
        void* QueryInterface(size_t guid);

        std::shared_ptr<IThreadContext> GetImmediateContext();
        std::unique_ptr<IThreadContext> CreateDeferredContext();

        using ResourceInitializer = std::function<SubResourceInitData(SubResourceId)>;
        IResourcePtr CreateResource(const ResourceDesc& desc, const ResourceInitializer& init);
        DeviceDesc GetDesc() IPURE;

        Device();
        ~Device();

    protected:
        intrusive_ptr<Metal_OpenGLES::DeviceContext>   _immediateContext;
    };

    class DeviceOpenGLES : public Device, public Base_DeviceOpenGLES
    {
    public:
        intrusive_ptr<Metal_OpenGLES::DeviceContext>    GetImmediateContext();
        virtual void* QueryInterface(size_t guid);

        DeviceOpenGLES();
        ~DeviceOpenGLES();
    };

////////////////////////////////////////////////////////////////////////////////
}
