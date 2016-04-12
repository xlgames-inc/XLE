// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#define FLEX_CONTEXT_Device				FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_DeviceDX11			FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_PresentationChain	FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_ThreadContext		FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_ThreadContextDX11	FLEX_CONTEXT_CONCRETE

#include "../IDevice.h"
#include "../IThreadContext.h"
#include "IDeviceDX11.h"
#include "Metal/DX11.h"
#include "../../Utility/IntrusivePtr.h"

namespace RenderCore
{
////////////////////////////////////////////////////////////////////////////////

    namespace Metal_DX11 { class DeviceContext; class ObjectFactory; }

    class Device;

    class PresentationChain : public Base_PresentationChain
    {
    public:
        void                Resize(unsigned newWidth, unsigned newHeight) /*override*/;
        IDXGI::SwapChain*   GetUnderlying() const { return _underlying.get(); }

        void                AttachToContext(ID3D::DeviceContext* context, ID3D::Device* device);

        std::shared_ptr<ViewportContext> GetViewportContext() const;

        PresentationChain(intrusive_ptr<IDXGI::SwapChain> underlying, const void* attachedWindow);
        ~PresentationChain();
    private:
        intrusive_ptr<IDXGI::SwapChain>     _underlying;
        const void*                         _attachedWindow;
        intrusive_ptr<ID3D::Texture2D>      _defaultDepthTarget;
        std::shared_ptr<ViewportContext>    _viewportContext;
    };

////////////////////////////////////////////////////////////////////////////////

    class ThreadContext : public Base_ThreadContext
    {
    public:
        void    BeginFrame(IPresentationChain& presentationChain);
        void    Present(IPresentationChain& presentationChain) /*override*/;

        bool                        IsImmediate() const;
        ThreadContextStateDesc      GetStateDesc() const;
        std::shared_ptr<IDevice>    GetDevice() const;
        void                        ClearAllBoundTargets() const;
        void                        IncrFrameId();
		void						InvalidateCachedState() const;

        ThreadContext(intrusive_ptr<ID3D::DeviceContext> devContext, std::shared_ptr<Device> device);
        ~ThreadContext();
    protected:
        std::shared_ptr<Metal_DX11::DeviceContext> _underlying;
        std::weak_ptr<Device>   _device;  // (must be weak, because Device holds a shared_ptr to the immediate context)
        unsigned                _frameId;
    };

    class ThreadContextDX11 : public ThreadContext, public Base_ThreadContextDX11
    {
    public:
        virtual void*       QueryInterface(const GUID& guid);
        std::shared_ptr<Metal_DX11::DeviceContext>&  GetUnderlying();

        ThreadContextDX11(intrusive_ptr<ID3D::DeviceContext> devContext, std::shared_ptr<Device> device);
        ~ThreadContextDX11();
    };

////////////////////////////////////////////////////////////////////////////////

    class Device : public Base_Device, public std::enable_shared_from_this<Device>
    {
    public:
        std::unique_ptr<IPresentationChain>     CreatePresentationChain(const void* platformValue, unsigned width, unsigned height) /*override*/;

        std::pair<const char*, const char*>     GetVersionInformation();

        std::shared_ptr<IThreadContext>         GetImmediateContext();
        std::unique_ptr<IThreadContext>         CreateDeferredContext();

		ResourcePtr CreateResource(
			const ResourceDesc& desc,
			const std::function<SubResourceInitData(unsigned, unsigned)>&);

        ID3D::Device*           GetUnderlyingDevice() { return _underlying.get(); }

        Device();
        ~Device();

    protected:
        intrusive_ptr<ID3D::Device>         _underlying;
        intrusive_ptr<ID3D::DeviceContext>  _immediateContext;
        D3D_FEATURE_LEVEL                   _featureLevel;

        std::shared_ptr<ThreadContextDX11>  _immediateThreadContext;

        intrusive_ptr<IDXGI::Factory>       GetDXGIFactory();
        std::unique_ptr<Metal_DX11::ObjectFactory> _mainFactory;
    };

    class DeviceDX11 : public Device, public Base_DeviceDX11
    {
    public:
        virtual void*           QueryInterface(const GUID& guid);
        ID3D::Device*           GetUnderlyingDevice();
        ID3D::DeviceContext*    GetImmediateDeviceContext();
        
        DeviceDX11();
        ~DeviceDX11();
    };

////////////////////////////////////////////////////////////////////////////////
}
