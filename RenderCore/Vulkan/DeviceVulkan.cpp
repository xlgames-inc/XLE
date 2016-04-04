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

    static const char* AsString(VkResult res)
    {
        // String names for standard vulkan error codes
        switch (res)
        {
                // success codes
            case VK_SUCCESS: return "Success";
            case VK_NOT_READY: return "Not Ready";
            case VK_TIMEOUT: return "Timeout";
            case VK_EVENT_SET: return "Event Set";
            case VK_EVENT_RESET: return "Event Reset";
            case VK_INCOMPLETE: return "Incomplete";

                // error codes
            case VK_ERROR_OUT_OF_HOST_MEMORY: return "Out of host memory";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "Out of device memory";
            case VK_ERROR_INITIALIZATION_FAILED: return "Initialization failed";
            case VK_ERROR_DEVICE_LOST: return "Device lost";
            case VK_ERROR_MEMORY_MAP_FAILED: return "Memory map failed";
            case VK_ERROR_LAYER_NOT_PRESENT: return "Layer not present";
            case VK_ERROR_EXTENSION_NOT_PRESENT: return "Extension not present";
            case VK_ERROR_FEATURE_NOT_PRESENT: return "Feature not present";
            case VK_ERROR_INCOMPATIBLE_DRIVER: return "Incompatible driver";
            case VK_ERROR_TOO_MANY_OBJECTS: return "Too many objects";
            case VK_ERROR_FORMAT_NOT_SUPPORTED: return "Format not supported";

                // kronos extensions
            case VK_ERROR_SURFACE_LOST_KHR: return "[KHR] Surface lost";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "[KHR] Native window in use";
            case VK_SUBOPTIMAL_KHR: return "[KHR] Suboptimal";
            case VK_ERROR_OUT_OF_DATE_KHR: return "[KHR] Out of date";
            case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "[KHR] Incompatible display";
            case VK_ERROR_VALIDATION_FAILED_EXT: return "[KHR] Validation failed";

                // NV extensions
            case VK_ERROR_INVALID_SHADER_NV: return "[NV] Invalid shader";

            default: return "<<unknown>>";
        }
    }

    class VulkanAPIFailure : public Exceptions::BasicLabel
    {
    public:
        VulkanAPIFailure(VkResult res, const char message[]) : Exceptions::BasicLabel("%s [%s, %i]", message, AsString(res), res) {}
    };

	static std::vector<VkLayerProperties> EnumerateLayers()
	{
		for (;;) {
			uint32_t layerCount = 0;
			auto res = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failure in during enumeration of Vulkan layer capabilities. You must have an up-to-date Vulkan driver installed."));

			if (layerCount == 0)
				return std::vector<VkLayerProperties>();

			std::vector<VkLayerProperties> layerProps;
			layerProps.resize(layerCount);
			res = vkEnumerateInstanceLayerProperties(&layerCount, AsPointer(layerProps.begin()));
			if (res == VK_INCOMPLETE) continue;	// doc's arent clear as to whether layerCount is updated in this case
            if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failure in during enumeration of Vulkan layer capabilities. You must have an up-to-date Vulkan driver installed."));

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
			Throw(VulkanAPIFailure(res, "Failure in Vulkan instance construction. You must have an up-to-date Vulkan driver installed."));

		return VulkanSharedPtr<VkInstance>(
			rawResult,
			[](VkInstance inst) { vkDestroyInstance(inst, s_allocationCallbacks); });
	}

	static std::vector<VkPhysicalDevice> EnumeratePhysicalDevices(VkInstance vulkan)
	{
		for (;;) {
			uint32_t count = 0;
			auto res = vkEnumeratePhysicalDevices(vulkan, &count, nullptr);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failure in during enumeration of physical devices. You must have an up-to-date Vulkan driver installed."));

			if (count == 0)
				return std::vector<VkPhysicalDevice>();

			std::vector<VkPhysicalDevice> props;
			props.resize(count);
			res = vkEnumeratePhysicalDevices(vulkan, &count, AsPointer(props.begin()));
			if (res == VK_INCOMPLETE) continue;	// doc's arent clear as to whether layerCount is updated in this case
            if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failure in during enumeration of physical devices. You must have an up-to-date Vulkan driver installed."));

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
				Throw(VulkanAPIFailure(res, "Failure in Vulkan surface construction. You must have an up-to-date Vulkan driver installed."));

			// note --	capturing "vulkan" with an unprotected pointer here. We could use a protected
			//			pointer easily enough... But I guess this approach is in-line with Vulkan design ideas.
			return VulkanSharedPtr<VkSurfaceKHR>(
				rawResult,
				[vulkan](VkSurfaceKHR inst) { vkDestroySurfaceKHR(vulkan, inst, s_allocationCallbacks); });
		#else
			#pragma Windowing platform not supported
		#endif
	}

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
			Throw(VulkanAPIFailure(res, "Failure while creating Vulkan logical device. You must have an up-to-date Vulkan driver installed."));

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
        _physDev = { nullptr, ~0u };

			// We can't create the underlying device immediately... Because we need a pointer to
			// the "platformValue" (window handle) in order to check for physical device capabilities.
			// So, we must do a lazy initialization of _underlying.
    }

    Device::~Device()
    {
		_underlying.reset();
		_instance.reset();
    }

    static std::vector<VkSurfaceFormatKHR> GetSurfaceFormats(VkPhysicalDevice physDev, VkSurfaceKHR surface)
    {
        for (;;)
        {
            uint32_t count;
            auto res = vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &count, nullptr);
            if (res != VK_SUCCESS)
                Throw(VulkanAPIFailure(res, "Failure while querying physical device surface formats"));
            if (count == 0) return std::vector<VkSurfaceFormatKHR>();

            std::vector<VkSurfaceFormatKHR> result;
            result.resize(count);
            res = vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &count, AsPointer(result.begin()));
            if (res == VK_INCOMPLETE) continue;
            if (res != VK_SUCCESS)
                Throw(VulkanAPIFailure(res, "Failure while querying physical device surface formats"));

            return result;
        }
    }

    static std::vector<VkPresentModeKHR> GetPresentModes(VkPhysicalDevice physDev, VkSurfaceKHR surface)
    {
        for (;;)
        {
            uint32_t count;
            auto res = vkGetPhysicalDeviceSurfacePresentModesKHR(physDev, surface, &count, nullptr);
            if (res != VK_SUCCESS)
                Throw(VulkanAPIFailure(res, "Failure while querying surface present modes"));
            if (count == 0) return std::vector<VkPresentModeKHR>();

            std::vector<VkPresentModeKHR> result;
            result.resize(count);
            res = vkGetPhysicalDeviceSurfacePresentModesKHR(physDev, surface, &count, AsPointer(result.begin()));
            if (res == VK_INCOMPLETE) continue;
            if (res != VK_SUCCESS)
                Throw(VulkanAPIFailure(res, "Failure while querying surface present modes"));

            return result;
        }
    }

    static VkPresentModeKHR SelectPresentMode(IteratorRange<VkPresentModeKHR*> availableModes)
    {
        // If mailbox mode is available, use it, as is the lowest-latency non-
        // tearing mode.  If not, try IMMEDIATE which will usually be available,
        // and is fastest (though it tears).  If not, fall back to FIFO which is
        // always available.
        VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (auto pm:availableModes) {
            if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
                swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
            if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) &&
                (pm == VK_PRESENT_MODE_IMMEDIATE_KHR)) {
                swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
        return swapchainPresentMode;
    }

    static VkQueue GetQueue(VkDevice dev, unsigned queueFamilyIndex, unsigned queueIndex=0)
    {
        VkQueue queue = nullptr;
        vkGetDeviceQueue(dev, queueFamilyIndex, queueIndex, &queue);
        return queue;
    }

    std::unique_ptr<IPresentationChain> Device::CreatePresentationChain(
		const void* platformValue, unsigned width, unsigned height)
    {
		auto surface = CreateSurface(_instance.get(), platformValue);
		if (!_underlying) {
			_physDev = SelectPhysicalDeviceForRendering(_instance.get(), surface.get());
			_underlying = CreateUnderlyingDevice(_physDev);
		}

        // The following is based on the "initswapchain" sample from the vulkan SDK
        auto fmts = GetSurfaceFormats(_physDev._dev, surface.get());
        assert(!fmts.empty());  // expecting at least one

        // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
        // the surface has no preferred format.  Otherwise, at least one
        // supported format will be returned.
        auto chainFmt = 
            (fmts.empty() || (fmts.size() == 1 && fmts[0].format == VK_FORMAT_UNDEFINED)) 
            ? VK_FORMAT_B8G8R8A8_UNORM : fmts[0].format;

        VkSurfaceCapabilitiesKHR surfCapabilities;
        auto res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            _physDev._dev, surface.get(), &surfCapabilities);
        assert(res == VK_SUCCESS);

        VkExtent2D swapChainExtent;
        // width and height are either both -1, or both not -1.
        if (surfCapabilities.currentExtent.width == (uint32_t)-1) {
            // If the surface size is undefined, the size is set to
            // the size of the images requested.
            swapChainExtent.width = width;
            swapChainExtent.height = height;
        } else {
            // If the surface size is defined, the swap chain size must match
            swapChainExtent = surfCapabilities.currentExtent;
        }

        auto presentModes = GetPresentModes(_physDev._dev, surface.get());
        auto swapchainPresentMode = SelectPresentMode(MakeIteratorRange(presentModes));
        
        // Determine the number of VkImage's to use in the swap chain (we desire to
        // own only 1 image at a time, besides the images being displayed and
        // queued for display):
        auto desiredNumberOfSwapChainImages = surfCapabilities.minImageCount + 1;
        if (surfCapabilities.maxImageCount > 0)
            desiredNumberOfSwapChainImages = std::min(desiredNumberOfSwapChainImages, surfCapabilities.maxImageCount);

        // setting "preTransform" to current transform... but clearing out other bits if the identity bit is set
        auto preTransform = 
            (surfCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfCapabilities.currentTransform;

        // finally, fill in our SwapchainCreate structure
        VkSwapchainCreateInfoKHR swapChainInfo = {};
        swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapChainInfo.pNext = nullptr;
        swapChainInfo.surface = surface.get();
        swapChainInfo.minImageCount = desiredNumberOfSwapChainImages;
        swapChainInfo.imageFormat = chainFmt;
        swapChainInfo.imageExtent.width = swapChainExtent.width;
        swapChainInfo.imageExtent.height = swapChainExtent.height;
        swapChainInfo.preTransform = preTransform;
        swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapChainInfo.imageArrayLayers = 1;
        swapChainInfo.presentMode = swapchainPresentMode;
        swapChainInfo.oldSwapchain = nullptr;
        swapChainInfo.clipped = true;
        swapChainInfo.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapChainInfo.queueFamilyIndexCount = 0;
        swapChainInfo.pQueueFamilyIndices = nullptr;

        auto underlyingDev = _underlying.get();
        VkSwapchainKHR swapChainRaw = nullptr;
        res = vkCreateSwapchainKHR(underlyingDev, &swapChainInfo, s_allocationCallbacks, &swapChainRaw);
        VulkanSharedPtr<VkSwapchainKHR> result(
            swapChainRaw,
            [underlyingDev](VkSwapchainKHR chain) { vkDestroySwapchainKHR(underlyingDev, chain, s_allocationCallbacks); } );
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure while creating swap chain"));

		return std::make_unique<PresentationChain>(
            std::move(result), 
            _underlying,
            GetQueue(_underlying.get(), _physDev._renderingQueueFamily), 
            platformValue);
    }

    void    Device::BeginFrame(IPresentationChain* presentationChain)
    {
        PresentationChain* swapChain = checked_cast<PresentationChain*>(presentationChain);
        swapChain->AcquireNextImage();
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

    static void SubmitSemaphoreSignal(VkQueue queue, VkSemaphore semaphore)
    {
        VkSubmitInfo submitInfo;
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.pWaitSemaphores = nullptr;
        submitInfo.pWaitDstStageMask = nullptr;
        submitInfo.commandBufferCount = 0;
        submitInfo.pCommandBuffers = nullptr;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores= &semaphore;

        auto res = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure while queuing semaphore signal"));
    }

    void            PresentationChain::Present()
    {
        if (_activeImageIndex > unsigned(_images.size())) return;

        // Queue a single operation to trigger the semaphore representing the end of rendering
        // We could do this a little more efficiently by combining this with the last submit.
        SubmitSemaphoreSignal(_queue, _images[_activeImageIndex]._presentSemaphore.get());

        const VkSwapchainKHR swapChains[] = { _swapChain.get() };
        uint32_t imageIndices[] = { _activeImageIndex };
        const VkSemaphore semaphores[] = { _images[_activeImageIndex]._presentSemaphore.get() };

        VkPresentInfoKHR present;
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.pNext = NULL;
        present.swapchainCount = dimof(swapChains);
        present.pSwapchains = swapChains;
        present.pImageIndices = imageIndices;
        present.pWaitSemaphores = semaphores;
        present.waitSemaphoreCount = dimof(semaphores);
        present.pResults = NULL;

        auto res = vkQueuePresentKHR(_queue, &present);
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure while queuing present"));

        _activeImageIndex = ~0x0u;
    }

    void            PresentationChain::Resize(unsigned newWidth, unsigned newHeight)
    {
        // todo -- we'll need to destroy and recreate the swapchain here
    }

    std::shared_ptr<ViewportContext> PresentationChain::GetViewportContext() const
    {
        return nullptr;
    }

    void PresentationChain::AcquireNextImage()
    {
        // note --  Due to the timeout here, we get a synchronise here.
        //          This will prevent issues when either the GPU or CPU is
        //          running ahead of the other... But we could do better by
        //          using the semaphores
        uint32_t nextImageIndex = ~0x0u;
        const auto timeout = UINT64_MAX;
        auto res = vkAcquireNextImageKHR(
            _device.get(), _swapChain.get(), 
            timeout,
            VK_NULL_HANDLE, VK_NULL_HANDLE,
            &nextImageIndex);
        _activeImageIndex = nextImageIndex;

        // TODO: Deal with the VK_SUBOPTIMAL_KHR and VK_ERROR_OUT_OF_DATE_KHR
        // return codes
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure during acquire next image"));
    }

    static std::vector<VkImage> GetImages(VkDevice dev, VkSwapchainKHR swapChain)
    {
        for (;;)
        {
            uint32_t count;
            auto res = vkGetSwapchainImagesKHR(dev, swapChain, &count, nullptr);
            if (res != VK_SUCCESS)
                Throw(VulkanAPIFailure(res, "Failure while querying physical device surface formats"));
            if (count == 0) return std::vector<VkImage>();

            std::vector<VkImage> rawPtrs;
            rawPtrs.resize(count);
            res = vkGetSwapchainImagesKHR(dev, swapChain, &count, AsPointer(rawPtrs.begin()));
            if (res == VK_INCOMPLETE) continue;
            if (res != VK_SUCCESS)
                Throw(VulkanAPIFailure(res, "Failure while querying physical device surface formats"));

            // we don't have to destroy the images with VkDestroyImage -- they will be destroyed when the
            // swapchain is destroyed.
            return rawPtrs;
        }
    }

    static VulkanSharedPtr<VkSemaphore> CreateBasicSemaphore(VkDevice dev)
    {
        VkSemaphoreCreateInfo presentCompleteSemaphoreCreateInfo;
        presentCompleteSemaphoreCreateInfo.sType =
            VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        presentCompleteSemaphoreCreateInfo.pNext = NULL;
        presentCompleteSemaphoreCreateInfo.flags = 0;

        VkSemaphore rawPtr = nullptr;
        auto res = vkCreateSemaphore(
            dev, &presentCompleteSemaphoreCreateInfo,
            s_allocationCallbacks, &rawPtr);
        VulkanSharedPtr<VkSemaphore> result(
            rawPtr,
            [dev](VkSemaphore sem) { vkDestroySemaphore(dev, sem, s_allocationCallbacks); });
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure while creating Vulkan semaphore"));
        return std::move(result);
    }
    
    PresentationChain::PresentationChain(
        VulkanSharedPtr<VkSwapchainKHR> swapChain, 
        VulkanSharedPtr<VkDevice> device,
        VkQueue queue, 
        const void* platformValue)
    : _swapChain(std::move(swapChain))
    , _device(std::move(device))
    , _queue(queue)
    , _platformValue(platformValue)
    {
        _activeImageIndex = ~0x0u;

        // We need to get pointers to each image and build the synchronization semaphores
        auto images = GetImages(_device.get(), _swapChain.get());
        _images.reserve(images.size());
        for (auto i:images)
            _images.emplace_back(
                Image { i, CreateBasicSemaphore(_device.get()) });
    }

    PresentationChain::~PresentationChain()
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

