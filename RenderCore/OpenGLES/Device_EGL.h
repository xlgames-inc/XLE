
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
        virtual void Resize(unsigned newWidth, unsigned newHeight) override;
        virtual const std::shared_ptr<PresentationChainDesc>& GetDesc() const override { return _desc; }

        const std::shared_ptr<Metal_OpenGLES::Resource>& GetTargetRenderbuffer();

        unsigned GetGUID() const { return _guid; }
        EGLSurface GetSurface() const { return _surface; }
        EGLContext GetSurfaceBoundContext() const { return _surfaceBoundContext; }
        EGLDisplay GetDisplay() const { return _display; }

        PresentationChain(EGLDisplay display, EGLContext sharedContext, EGLConfig sharedContextCfg, const void *platformValue, const PresentationChainDesc& desc);
        ~PresentationChain();

    private:
        unsigned _guid;
        EGLSurface _surface;
        EGLContext _surfaceBoundContext;
        EGLDisplay _display;
        std::shared_ptr<Metal_OpenGLES::Resource> _targetRenderbuffer;
        std::shared_ptr<PresentationChainDesc> _desc;
    };

////////////////////////////////////////////////////////////////////////////////

    class Device;
    
    class ThreadContext : public Base_ThreadContext
    {
    public:
        virtual IResourcePtr BeginFrame(IPresentationChain& presentationChain) override;
        virtual void Present(IPresentationChain& presentationChain) override;

        virtual void* QueryInterface(size_t guid) override;
        virtual bool IsImmediate() const override { return false; }
        virtual std::shared_ptr<IDevice> GetDevice() const override;
        virtual void InvalidateCachedState() const override {}

        virtual IAnnotator& GetAnnotator() override;
        virtual ThreadContextStateDesc GetStateDesc() const override { return {}; }

        void MakeDeferredContext();

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
        EGLDisplay _display;

        std::shared_ptr<Metal_OpenGLES::Resource> _activeTargetRenderbuffer;
        intrusive_ptr<OpenGL::FrameBuffer> _temporaryFramebuffer;
    };

    class ThreadContextOpenGLES : public ThreadContext, public Base_ThreadContextOpenGLES
    {
    public:
        virtual const std::shared_ptr<Metal_OpenGLES::DeviceContext>& GetDeviceContext() override { return _deviceContext; }
        virtual bool IsBoundToCurrentThread() override;
        virtual bool BindToCurrentThread() override;
        virtual void UnbindFromCurrentThread() override;
        virtual void* QueryInterface(size_t guid) override;
        virtual std::shared_ptr<IThreadContext> Clone() override;

        ThreadContextOpenGLES(EGLContext sharedContext, const std::shared_ptr<Device>& device);
        ~ThreadContextOpenGLES();

    private:
        std::shared_ptr<Metal_OpenGLES::DeviceContext> _deviceContext;
    }; 

////////////////////////////////////////////////////////////////////////////////

    class Device : public Base_Device, public std::enable_shared_from_this<Device>
    {
    public:
        virtual std::unique_ptr<IPresentationChain> CreatePresentationChain(const void* platformWindowHandle, const PresentationChainDesc& desc) override;

        virtual void* QueryInterface(size_t guid) override;

        virtual std::shared_ptr<IThreadContext> GetImmediateContext() override;
        virtual std::unique_ptr<IThreadContext> CreateDeferredContext() override;

        using ResourceInitializer = std::function<SubResourceInitData(SubResourceId)>;
        virtual IResourcePtr CreateResource(const ResourceDesc& desc, const ResourceInitializer& init) override;
        virtual FormatCapability QueryFormatCapability(Format format, BindFlag::BitField bindingType) override;

        virtual DeviceDesc GetDesc() override { return DeviceDesc { "OpenGLES-EGL", "", "" }; }

        EGLContext GetSharedContext() const { return _sharedContext; }
        EGLDisplay GetDisplay() const { return _display; };
        EGLConfig GetConfig() const { return _config; }
        unsigned GetGLESVersion() const;

        Device();
        ~Device();

    protected:
        std::shared_ptr<ThreadContextOpenGLES> _immediateContext;
        std::shared_ptr<Metal_OpenGLES::ObjectFactory> _objectFactory;
        EGLContext _sharedContext;
        EGLDisplay _display;
        EGLConfig _config;

        std::shared_ptr<Metal_OpenGLES::ObjectFactory> GetObjectFactory();
    };

    class DeviceOpenGLES : public Device, public Base_DeviceOpenGLES
    {
    public:
        virtual Metal_OpenGLES::FeatureSet::BitField GetFeatureSet() override;
        virtual void* QueryInterface(size_t guid) override;

        DeviceOpenGLES();
        ~DeviceOpenGLES();
    };

////////////////////////////////////////////////////////////////////////////////
} }
