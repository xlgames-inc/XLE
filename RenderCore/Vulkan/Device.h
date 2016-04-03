// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#define FLEX_CONTEXT_Device					FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_DeviceVulkan			FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_PresentationChain		FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_ThreadContext			FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_ThreadContextVulkan	FLEX_CONTEXT_CONCRETE

#include "../IDevice.h"
#include "../IThreadContext.h"
#include "IDeviceVulkan.h"
#include "../../Utility/IntrusivePtr.h"
#include "Metal/IncludeVulkan.h"

namespace RenderCore
{
////////////////////////////////////////////////////////////////////////////////

    namespace Metal_DX11 { class DeviceContext; class ObjectFactory; }

    class Device;

    class PresentationChain : public Base_PresentationChain
    {
    public:
        void                Present() /*override*/;
        void                Resize(unsigned newWidth, unsigned newHeight) /*override*/;

        PresentationChain();
        ~PresentationChain();
    private:
    };

////////////////////////////////////////////////////////////////////////////////

    class ThreadContext : public Base_ThreadContext
    {
    public:
        bool                        IsImmediate() const;
        ThreadContextStateDesc      GetStateDesc() const;
        std::shared_ptr<IDevice>    GetDevice() const;
        void                        ClearAllBoundTargets() const;
        void                        IncrFrameId();
		void						InvalidateCachedState() const;

        ThreadContext(std::shared_ptr<Device> device);
        ~ThreadContext();
    protected:
        std::weak_ptr<Device>   _device;  // (must be weak, because Device holds a shared_ptr to the immediate context)
		unsigned                _frameId;
    };

    class ThreadContextVulkan : public ThreadContext, public Base_ThreadContextVulkan
    {
    public:
        virtual void*       QueryInterface(const GUID& guid);

		ThreadContextVulkan(std::shared_ptr<Device> device);
        ~ThreadContextVulkan();
    };

////////////////////////////////////////////////////////////////////////////////

	template<typename Type>
		using VulkanSharedPtr = std::shared_ptr<typename std::remove_reference<decltype(*std::declval<Type>())>::type>;

	class Device : public Base_Device, public std::enable_shared_from_this<Device>
    {
    public:
        std::unique_ptr<IPresentationChain>     CreatePresentationChain(const void* platformValue, unsigned width, unsigned height) /*override*/;
        void    BeginFrame(IPresentationChain* presentationChain);

        std::pair<const char*, const char*>     GetVersionInformation();

        std::shared_ptr<IThreadContext>         GetImmediateContext();
        std::unique_ptr<IThreadContext>         CreateDeferredContext();

        Device();
        ~Device();
    protected:
		VulkanSharedPtr<VkInstance>			_instance;
		VulkanSharedPtr<VkDevice>			_underlying;
    };

    class DeviceVulkan : public Device, public Base_DeviceVulkan
    {
    public:
        virtual void*   QueryInterface(const GUID& guid);
		VkInstance		GetVulkanInstance();
		VkDevice		GetUnderlyingDevice();
        
        DeviceVulkan();
        ~DeviceVulkan();
    };

////////////////////////////////////////////////////////////////////////////////
}
