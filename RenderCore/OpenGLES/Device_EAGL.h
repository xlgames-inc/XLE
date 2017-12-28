#pragma once

#define FLEX_CONTEXT_Device             FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_DeviceOpenGLES     FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_PresentationChain  FLEX_CONTEXT_CONCRETE

#include "../IDevice.h"
#include "../IThreadContext.h"
#include "IDeviceOpenGLES.h"
#include "../../Utility/Mixins.h"
#include "../../Utility/IntrusivePtr.h"
#include "../../../Externals/Misc/OCPtr.h"

@class EAGLContext;

namespace RenderCore { namespace Metal_OpenGLES { class ObjectFactory; }}

namespace RenderCore { namespace ImplOpenGLES
{
////////////////////////////////////////////////////////////////////////////////

    class PresentationChain : public Base_PresentationChain
    {
    public:
        void                Resize(unsigned newWidth, unsigned newHeight) /*override*/;
        const std::shared_ptr<PresentationChainDesc>& GetDesc() const;
        EAGLContext*        GetUnderlying() { return _eaglContext.get(); }

        PresentationChain(const void* platformValue, unsigned width, unsigned height);
        ~PresentationChain();

    private:
        TBC::OCPtr<EAGLContext> _eaglContext;
    };

////////////////////////////////////////////////////////////////////////////////

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

        ThreadContext();
        ~ThreadContext();
    };

    class ThreadContextOpenGLES : public ThreadContext, public Base_ThreadContextOpenGLES
    {
    public:
        std::shared_ptr<Metal_OpenGLES::DeviceContext>&  GetUnderlying();
        virtual void*       QueryInterface(size_t guid);
        ThreadContextOpenGLES();
        ~ThreadContextOpenGLES();
    private:
        std::shared_ptr<Metal_OpenGLES::DeviceContext> _deviceContext;
    };

////////////////////////////////////////////////////////////////////////////////

    class Device :  public Base_Device, noncopyable
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

