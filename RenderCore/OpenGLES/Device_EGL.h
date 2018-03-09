
#pragma once

#include "../IDevice.h"
#include "IDeviceOpenGLES.h"
#include "ObjectFactory.h"
#include "Metal/Resource.h"
#include "../../Utility/Mixins.h"
#include "../../Utility/IntrusivePtr.h"
#include "../IThreadContext.h"

#include <EGL/egl.h>

namespace RenderCore { namespace ImplOpenGLES
{
////////////////////////////////////////////////////////////////////////////////

    class PresentationChain : public Base_PresentationChain
    {
    public:
        void Resize(unsigned newWidth, unsigned newHeight) /*override*/;
        const std::shared_ptr<PresentationChainDesc>& GetDesc() const;
        EGLContext GetEGLContext() const {return _eglContext; }
        const std::shared_ptr<Metal_OpenGLES::Resource>& GetFrameRenderbuffer() const { return _frameRenderbuffer; }
        EGLSurface GetSurface() const { return _surface; }

        PresentationChain(
                Metal_OpenGLES::ObjectFactory &objFactory,
                EGLContext sharedContext,
                EGLDisplay display,
                EGLConfig config,
                const void *platformValue, unsigned width, unsigned height);
        ~PresentationChain();
    private:
       EGLSurface _surface;
       EGLContext _eglContext;
       std::shared_ptr<Metal_OpenGLES::Resource> _frameRenderbuffer;
       std::shared_ptr<PresentationChainDesc> _desc;
    };

    class Device;

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
        virtual void *              QueryInterface(size_t guid);

        IAnnotator&                 GetAnnotator();

        ThreadContext(EGLContext sharedContext, const std::shared_ptr<Device> &device);
        ~ThreadContext();

    private:
        std::weak_ptr<Device> _device;
        std::unique_ptr<IAnnotator> _annotator;
        EGLContext _sharedContext;
        EGLContext _activeFrameContext;
        std::shared_ptr<Metal_OpenGLES::Resource> _activeFrameRenderbuffer;
        intrusive_ptr<OpenGL::FrameBuffer> _activeFrameBuffer;
    };

   
    class ThreadContextOpenGLES : public ThreadContext, public Base_ThreadContextOpenGLES
    {
    public:
        const std::shared_ptr<Metal_OpenGLES::DeviceContext>&  GetDeviceContext();
        virtual void*       QueryInterface(size_t guid);
        ThreadContextOpenGLES(EGLContext sharedContext, const std::shared_ptr<Device>& device);
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

        EGLDisplay GetDisplay() const {return _display;};

        Device();
        ~Device();

    protected:
        std::shared_ptr<ThreadContextOpenGLES>   _immediateContext;
		std::shared_ptr<Metal_OpenGLES::ObjectFactory> _objectFactory;
		EGLContext _sharedContext;
        EGLDisplay _display;
        EGLConfig  _config;
    };

    class DeviceOpenGLES : public Device, public Base_DeviceOpenGLES
    {
    public:
        std::shared_ptr<IThreadContext>    GetImmediateContext();
        virtual void* QueryInterface(size_t guid);
        virtual Metal_OpenGLES::DeviceContext *GetImmediateDeviceContext();

        DeviceOpenGLES();
        ~DeviceOpenGLES();
    };

////////////////////////////////////////////////////////////////////////////////
} }
