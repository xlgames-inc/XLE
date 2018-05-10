#pragma once

#include "../IDevice.h"
#include "../IThreadContext.h"
#include "IDeviceOpenGLES.h"
#include "ObjectFactory.h"
#include "../../Utility/Mixins.h"
#include "../../Utility/IntrusivePtr.h"
#include "../../../Externals/Misc/OCPtr.h"
#include <memory>

@class EAGLContext;

namespace RenderCore { namespace Metal_OpenGLES { class ObjectFactory; class Resource; }}

namespace RenderCore { namespace ImplOpenGLES
{
////////////////////////////////////////////////////////////////////////////////

    class PresentationChain : public Base_PresentationChain
    {
    public:
        void                Resize(unsigned newWidth, unsigned newHeight) /*override*/;
        const std::shared_ptr<PresentationChainDesc>& GetDesc() const;
        EAGLContext*        GetEAGLContext() { return _eaglContext.get(); }

        const std::shared_ptr<Metal_OpenGLES::Resource>& GetFrameRenderbuffer() const { return _frameRenderbuffer; }

        PresentationChain(
            Metal_OpenGLES::ObjectFactory& objFactory,
            EAGLContext* sharedContext,
            const void* platformValue, unsigned width, unsigned height);
        ~PresentationChain();

    private:
        TBC::OCPtr<EAGLContext> _eaglContext;
        std::shared_ptr<Metal_OpenGLES::Resource> _frameRenderbuffer;
        std::shared_ptr<PresentationChainDesc> _desc;
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
        virtual void*               QueryInterface(size_t guid);

        void        MakeDeferredContext();

        ThreadContext(EAGLContext* sharedContext, const std::shared_ptr<Device>& device);
        ~ThreadContext();

    protected:
        std::weak_ptr<Device>   _device;  // (must be weak, because Device holds a shared_ptr to the immediate context)
        std::unique_ptr<IAnnotator> _annotator;

        TBC::OCPtr<EAGLContext> _activeFrameContext;
        TBC::OCPtr<EAGLContext> _deferredContext;
        TBC::OCPtr<EAGLContext> _sharedContext;

        std::shared_ptr<Metal_OpenGLES::Resource> _activeFrameRenderbuffer;
        intrusive_ptr<OpenGL::FrameBuffer> _activeFrameBuffer;
    };

    class ThreadContextOpenGLES : public ThreadContext, public Base_ThreadContextOpenGLES
    {
    public:
        const std::shared_ptr<Metal_OpenGLES::DeviceContext>&  GetDeviceContext();
        virtual bool        IsBoundToCurrentThread();
        virtual bool        BindToCurrentThread();
        virtual void        UnbindFromCurrentThread();
        virtual void*       QueryInterface(size_t guid);
        virtual std::shared_ptr<IThreadContext> Clone();
        ThreadContextOpenGLES(EAGLContext* sharedContext, const std::shared_ptr<Device>& device);
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
        FormatCapability QueryFormatCapability(Format format, BindFlag::BitField bindingType);

        Device();
        ~Device();

    protected:
        std::shared_ptr<ThreadContextOpenGLES> _immediateContext;
        std::shared_ptr<Metal_OpenGLES::ObjectFactory> _objectFactory;
        TBC::OCPtr<EAGLContext> _sharedContext;
    };

    class DeviceOpenGLES : public Device, public Base_DeviceOpenGLES
    {
    public:
        virtual void* QueryInterface(size_t guid);
        Metal_OpenGLES::DeviceContext * GetImmediateDeviceContext();
        Metal_OpenGLES::FeatureSet::BitField GetFeatureSet();

        DeviceOpenGLES();
        ~DeviceOpenGLES();
    };

////////////////////////////////////////////////////////////////////////////////
}}

