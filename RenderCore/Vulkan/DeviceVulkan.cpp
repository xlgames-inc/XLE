// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Device.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/PtrUtils.h"
#include "../../Core/SelectConfiguration.h"
#include <memory>

namespace RenderCore
{
	static std::string GetApplicationName()
	{
		return ConsoleRig::GlobalServices::GetCrossModule()._services.CallDefault<std::string>(
			ConstHash64<'appn', 'ame'>::Value, std::string("<<unnamed>>"));
	}

	static VkAllocationCallbacks* s_allocationCallbacks = nullptr;

	const char* s_instanceExtensions[] = 
	{
		VK_KHR_SURFACE_EXTENSION_NAME
		#if PLATFORMOS_TARGET  == PLATFORMOS_WINDOWS
			, VK_KHR_WIN32_SURFACE_EXTENSION_NAME
		#endif
	};

	const char* s_deviceExtensions[] =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

#if 0
	static std::vector<VkExtensionProperties> EnumerateExtensionProperties(
		VkPhysicalDevice physDev,
		const VkLayerProperties& layer)
	{
		for(;;) {
			uint32_t extCount = 0;
			auto res = vkEnumerateDeviceExtensionProperties(physDev, layer.layerName, &extCount, NULL);
			if (res)
				Throw(Exceptions::BasicLabel("Failure in during enumeration of Vulkan extension capabilities. You must have an up-to-date Vulkan driver installed."));

			if (extCount == 0)
				return std::vector<VkExtensionProperties>();

			std::vector<VkExtensionProperties> result;
			result.resize(extCount);
			res = vkEnumerateDeviceExtensionProperties(
				physDev, layer.layerName, &extCount, AsPointer(result.begin()));
			if (res == VK_INCOMPLETE) continue; // doc's arent clear as to whether layerCount is updated in this case

			return result;
		}
	}
#endif

	static std::vector<VkLayerProperties> EnumerateLayers()
	{
		for (;;) {
			uint32_t layerCount = 0;
			auto res = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
			if (res)
				Throw(Exceptions::BasicLabel("Failure in during enumeration of Vulkan layer capabilities. You must have an up-to-date Vulkan driver installed."));

			if (layerCount == 0)
				return std::vector<VkLayerProperties>();

			std::vector<VkLayerProperties> layerProps;
			layerProps.resize(layerCount);
			res = vkEnumerateInstanceLayerProperties(&layerCount, AsPointer(layerProps.begin()));
			if (res == VK_INCOMPLETE) continue;	// doc's arent clear as to whether layerCount is updated in this case

			return layerProps;
		}
	}

	static VulkanSharedPtr<VkInstance> CreateVulkanInstance()
	{
		auto appname = GetApplicationName();

		VkApplicationInfo app_info = {};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pNext = NULL;
		app_info.pApplicationName = appname.c_str();
		app_info.applicationVersion = 1;
		app_info.pEngineName = "XLE";
		app_info.engineVersion = 1;
		app_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);

		auto layers = EnumerateLayers();

		VkInstanceCreateInfo inst_info = {};
		inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		inst_info.pNext = NULL;
		inst_info.flags = 0;
		inst_info.pApplicationInfo = &app_info;
		inst_info.enabledLayerCount = 0;
		inst_info.ppEnabledLayerNames = nullptr;
		inst_info.enabledExtensionCount = (uint32_t)dimof(s_instanceExtensions);
		inst_info.ppEnabledExtensionNames = s_instanceExtensions;

		VkInstance rawResult = nullptr;
		VkResult res = vkCreateInstance(&inst_info, s_allocationCallbacks, &rawResult);
		if (res != VK_SUCCESS)
			Throw(Exceptions::BasicLabel("Failure in Vulkan instance construction. You must have an up-to-date Vulkan driver installed."));

		return VulkanSharedPtr<VkInstance>(
			rawResult,
			[](VkInstance inst) { vkDestroyInstance(inst, s_allocationCallbacks); });
	}

	static std::vector<VkPhysicalDevice> EnumeratePhysicalDevices(VkInstance vulkan)
	{
		for (;;) {
			uint32_t count = 0;
			auto res = vkEnumeratePhysicalDevices(vulkan, &count, nullptr);
			if (res)
				Throw(Exceptions::BasicLabel("Failure in during enumeration of physical devices. You must have an up-to-date Vulkan driver installed."));

			if (count == 0)
				return std::vector<VkPhysicalDevice>();

			std::vector<VkPhysicalDevice> props;
			props.resize(count);
			res = vkEnumeratePhysicalDevices(vulkan, &count, AsPointer(props.begin()));
			if (res == VK_INCOMPLETE) continue;	// doc's arent clear as to whether layerCount is updated in this case

			return props;
		}
	}

	static std::vector<VkQueueFamilyProperties> EnumerateQueueFamilyProperties(VkPhysicalDevice dev)
	{
		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
		if (count == 0)
			return std::vector<VkQueueFamilyProperties>();

		std::vector<VkQueueFamilyProperties> props;
		props.resize(count);
		vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, AsPointer(props.begin()));
		return props;
	}

	static const char* AsString(VkPhysicalDeviceType type)
	{
		switch (type)
		{
		case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "Other";
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated";
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "Discrete";
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "Virtual";
		case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU";
		default: return "Unknown";
		}
	}

	static VulkanSharedPtr<VkSurfaceKHR> CreateSurface(VkInstance vulkan, const void* platformValue)
	{
		#if PLATFORMOS_TARGET  == PLATFORMOS_WINDOWS
			VkWin32SurfaceCreateInfoKHR createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			createInfo.pNext = NULL;
			createInfo.hinstance = GetModuleHandle(nullptr);
			createInfo.hwnd = (HWND)platformValue;

			VkSurfaceKHR rawResult = nullptr;
			auto res = vkCreateWin32SurfaceKHR(vulkan, &createInfo, s_allocationCallbacks, &rawResult);
			if (res != VK_SUCCESS)
				Throw(Exceptions::BasicLabel("Failure in Vulkan surface construction. You must have an up-to-date Vulkan driver installed."));

			// note --	capturing "vulkan" with an unprotected pointer here. We could use a protected
			//			pointer easily enough... But I guess this approach is in-line with Vulkan design ideas.
			return VulkanSharedPtr<VkSurfaceKHR>(
				rawResult,
				[vulkan](VkSurfaceKHR inst) { vkDestroySurfaceKHR(vulkan, inst, s_allocationCallbacks); });
		#else
			#pragma Windowing platform not supported
		#endif
	}

	class SelectedPhysicalDevice
	{
	public:
		VkPhysicalDevice _dev;
		unsigned _renderingQueueFamily;
	};

	static SelectedPhysicalDevice SelectPhysicalDeviceForRendering(VkInstance vulkan, VkSurfaceKHR surface)
	{
		auto devices = EnumeratePhysicalDevices(vulkan);
		if (devices.empty())
			Throw(Exceptions::BasicLabel("Could not find any Vulkan physical devices. You must have an up-to-date Vulkan driver installed."));

		// Iterate through the list of devices -- and if it matches our requirements, select that device.
		// We're expecting the Vulkan driver to return the devices in priority order.
		for (auto dev:devices) {
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(dev, &props);

			// We need a device with the QUEUE_GRAPHICS bit set, and that supports presenting.
			auto queueProps = EnumerateQueueFamilyProperties(dev);
			for (unsigned qi=0; qi<unsigned(queueProps.size()); ++qi) {
				if (!(queueProps[qi].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;

				// Awkwardly, we need to create the "VkSurfaceKHR" in order to check for
				// compatibility with the physical device. And creating the surface requires
				// a windows handle... So we can't select the physical device (or create the
				// logical device) until we have the windows handle.
				if (surface != nullptr) {
					VkBool32 supportsPresent = false;
					vkGetPhysicalDeviceSurfaceSupportKHR(
						dev, qi, surface, &supportsPresent);
					if (!supportsPresent) continue;
				}

				LogInfo 
					<< "Selecting physical device (" << props.deviceName 
					<< "). API Version: (" << props.apiVersion 
					<< "). Driver version: (" << props.driverVersion 
					<< "). Type: (" << AsString(props.deviceType) << ")";
				return SelectedPhysicalDevice { dev, qi };
			}
		}

		Throw(Exceptions::BasicLabel("There are physical Vulkan devices, but none of them support rendering. You must have an up-to-date Vulkan driver installed."));
	}

	static VulkanSharedPtr<VkDevice> CreateUnderlyingDevice(SelectedPhysicalDevice physDev)
	{
		VkDeviceQueueCreateInfo queue_info = {};
		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.pNext = nullptr;
		queue_info.queueCount = 1;
		// The queue priority value are specific to a single VkDevice -- so it shouldn't affect priorities
		// relative to another application.
		float queue_priorities[1] = { 0.5f };
		queue_info.pQueuePriorities = queue_priorities;
		queue_info.queueFamilyIndex = physDev._renderingQueueFamily;

		VkDeviceCreateInfo device_info = {};
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.pNext = nullptr;
		device_info.queueCreateInfoCount = 1;
		device_info.pQueueCreateInfos = &queue_info;
		device_info.enabledLayerCount = 0;
		device_info.ppEnabledLayerNames = nullptr;
		device_info.enabledExtensionCount = dimof(s_deviceExtensions);
		device_info.ppEnabledExtensionNames = s_deviceExtensions;
		device_info.pEnabledFeatures = nullptr;

		VkDevice rawResult = nullptr;
		auto res = vkCreateDevice(physDev._dev, &device_info, s_allocationCallbacks, &rawResult);
		if (res != VK_SUCCESS)
			Throw(Exceptions::BasicLabel("Failure while creating Vulkan logical device. You must have an up-to-date Vulkan driver installed."));

		return VulkanSharedPtr<VkDevice>(
			rawResult,
			[](VkDevice dev) { vkDestroyDevice(dev, s_allocationCallbacks); });
	}

    Device::Device()
    {
			//
			//	Create the instance. This will attach the Vulkan DLL. If there are no valid Vulkan drivers
			//	available, it will throw an exception here.
			//
		_instance = CreateVulkanInstance();

			// We can't create the underlying device immediately... Because we need a pointer to
			// the "platformValue" (window handle) in order to check for physical device capabilities.
			// So, we must do a lazy initialization of _underlying.
    }

    Device::~Device()
    {
		_underlying.reset();
		_instance.reset();
    }

    std::unique_ptr<IPresentationChain> Device::CreatePresentationChain(
		const void* platformValue, unsigned width, unsigned height)
    {
		auto surface = CreateSurface(_instance.get(), platformValue);
		if (!_underlying) {
			auto phyDev = SelectPhysicalDeviceForRendering(_instance.get(), surface.get());
			_underlying = CreateUnderlyingDevice(phyDev);
		}
		return nullptr;
    }

    void    Device::BeginFrame(IPresentationChain* presentationChain)
    {
        (void)presentationChain;
    }

    std::shared_ptr<IThreadContext> Device::GetImmediateContext()
    {
		if (!_underlying) {
			auto phyDev = SelectPhysicalDeviceForRendering(_instance.get(), nullptr);
			_underlying = CreateUnderlyingDevice(phyDev);
		}
		return std::make_shared<ThreadContextVulkan>(shared_from_this());
    }

    std::unique_ptr<IThreadContext> Device::CreateDeferredContext()
    {
		if (!_underlying) {
			auto phyDev = SelectPhysicalDeviceForRendering(_instance.get(), nullptr);
			_underlying = CreateUnderlyingDevice(phyDev);
		}
		return std::make_unique<ThreadContextVulkan>(shared_from_this());
    }

    extern char VersionString[];
    extern char BuildDateString[];
        
    std::pair<const char*, const char*> Device::GetVersionInformation()
    {
        return std::make_pair(VersionString, BuildDateString);
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////

    #if !FLEX_USE_VTABLE_Device && !DOXYGEN
        namespace Detail
        {
            void* Ignore_Device::QueryInterface(const GUID& guid)
            {
                return nullptr;
            }
        }
    #endif

    void*                   DeviceVulkan::QueryInterface(const GUID& guid)
    {
        if (guid == __uuidof(Base_DeviceVulkan)) {
            return (IDeviceVulkan*)this;
        }
        return nullptr;
    }

	VkInstance DeviceVulkan::GetVulkanInstance()
    {
        return _instance.get();
    }

	VkDevice DeviceVulkan::GetUnderlyingDevice()
    {
        return _underlying.get();
    }

	DeviceVulkan::DeviceVulkan()
    {
    }

	DeviceVulkan::~DeviceVulkan()
    {
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////

    PresentationChain::PresentationChain()
    {
    }

    PresentationChain::~PresentationChain()
    {
    }

    void            PresentationChain::Present()
    {
    }

    void            PresentationChain::Resize(unsigned newWidth, unsigned newHeight)
    {
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////

    render_dll_export std::shared_ptr<IDevice>    CreateDevice()
    {
        return std::make_shared<DeviceVulkan>();
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////

	#if !FLEX_USE_VTABLE_ThreadContext && !DOXYGEN
		namespace Detail
		{
			void* Ignore_ThreadContext::QueryInterface(const GUID& guid)
			{
				return nullptr;
			}
		}
	#endif

    bool    ThreadContext::IsImmediate() const
    {
        return false;
    }

    auto ThreadContext::GetStateDesc() const -> ThreadContextStateDesc
    {
		return {};
    }

	void ThreadContext::InvalidateCachedState() const {}

    ThreadContext::ThreadContext(std::shared_ptr<Device> device)
    : _device(std::move(device))
	, _frameId(0)
    {
    }

    ThreadContext::~ThreadContext() {}

    std::shared_ptr<IDevice> ThreadContext::GetDevice() const
    {
        // Find a pointer back to the IDevice object associated with this 
        // thread context...
        // There are two ways to do this:
        //  1) get the D3D::IDevice from the DeviceContext
        //  2) there is a pointer back to the RenderCore::IDevice() hidden within
        //      the D3D::IDevice
        // Or, we could just store a std::shared_ptr back to the device within
        // this object.
        return _device.lock();
    }

    void ThreadContext::ClearAllBoundTargets() const
    {
    }

    void ThreadContext::IncrFrameId()
    {
        ++_frameId;
    }

    void*   ThreadContextVulkan::QueryInterface(const GUID& guid)
    {
        if (guid == __uuidof(Base_ThreadContextVulkan)) { return (IThreadContextVulkan*)this; }
        return nullptr;
    }

    ThreadContextVulkan::ThreadContextVulkan(std::shared_ptr<Device> device)
    : ThreadContext(std::move(device))
    {}

	ThreadContextVulkan::~ThreadContextVulkan() {}

}

