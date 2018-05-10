
#pragma once

#include "../IDevice.h"
#include "IDeviceOpenGLES.h"
#include "Metal/ObjectFactory.h"
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
        const std::shared_ptr<Metal_OpenGLES::Resource>& GetTargetRenderbuffer();
        EGLSurface GetSurface() const;
	    unsigned GetGUID() const { return _guid; }

        PresentationChain(
                EGLContext sharedContext,
                EGLDisplay display,
                EGLConfig config,
                const void *platformValue, unsigned width, unsigned height);
        ~PresentationChain();
    private:
		EGLSurface _surface;
		std::shared_ptr<Metal_OpenGLES::Resource> _targetRenderbuffer;
		std::shared_ptr<PresentationChainDesc> _desc;
		ResourceDesc _backBufferDesc;
	    unsigned _guid;
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

        void        MakeDeferredContext();

        ThreadContext(EGLContext sharedContext, const std::shared_ptr<Device> &device);
        ~ThreadContext();

    protected:
        std::weak_ptr<Device> _device;
        std::unique_ptr<IAnnotator> _annotator;

        EGLContext _sharedContext = EGL_NO_CONTEXT;
        EGLContext _activeFrameContext = EGL_NO_CONTEXT;
        EGLContext _deferredContext = EGL_NO_CONTEXT;
        EGLSurface _dummyPBufferSurface = EGL_NO_SURFACE;
	    unsigned _currentPresentationChainGUID;

        std::shared_ptr<Metal_OpenGLES::Resource> _activeTargetRenderbuffer;
        intrusive_ptr<OpenGL::FrameBuffer> _temporaryFramebuffer;
    };

   
    class ThreadContextOpenGLES : public ThreadContext, public Base_ThreadContextOpenGLES
    {
    public:
        const std::shared_ptr<Metal_OpenGLES::DeviceContext>&  GetDeviceContext();
        virtual bool        IsBoundToCurrentThread();
		virtual bool 		BindToCurrentThread();
		virtual void 		UnbindFromCurrentThread();
        virtual void*       QueryInterface(size_t guid);
        virtual std::shared_ptr<IThreadContext> Clone();
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
        FormatCapability QueryFormatCapability(Format format, BindFlag::BitField bindingType);

        EGLDisplay GetDisplay() const { return _display; };
        EGLContext GetSharedContext() const { return _sharedContext; }
        EGLConfig GetConfig() const { return _config; }

        Device();
        ~Device();

    protected:
        std::shared_ptr<ThreadContextOpenGLES>   _immediateContext;
		std::shared_ptr<Metal_OpenGLES::ObjectFactory> _objectFactory;
		EGLContext _sharedContext;
        EGLDisplay _display;
        EGLConfig  _config;

        std::shared_ptr<Metal_OpenGLES::ObjectFactory> GetObjectFactory();
    };

    class DeviceOpenGLES : public Device, public Base_DeviceOpenGLES
    {
    public:
        Metal_OpenGLES::FeatureSet::BitField GetFeatureSet();
        virtual void* QueryInterface(size_t guid);

        DeviceOpenGLES();
        ~DeviceOpenGLES();
    };

////////////////////////////////////////////////////////////////////////////////
} }
