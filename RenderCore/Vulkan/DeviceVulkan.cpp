// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Device.h"
#include "Metal/VulkanCore.h"
#include "Metal/ObjectFactory.h"
#include "Metal/Format.h"
#include "Metal/Pools.h"
#include "Metal/Resource.h"
#include "../../BufferUploads/IBufferUploads.h"
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

	static const char* s_instanceExtensions[] = 
	{
		VK_KHR_SURFACE_EXTENSION_NAME
		#if PLATFORMOS_TARGET  == PLATFORMOS_WINDOWS
			, VK_KHR_WIN32_SURFACE_EXTENSION_NAME
		#endif
        , VK_EXT_DEBUG_REPORT_EXTENSION_NAME
	};

	static const char* s_deviceExtensions[] =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	static const char* s_instanceLayers[] =
	{
		"VK_LAYER_GOOGLE_threading",
		"VK_LAYER_LUNARG_device_limits",
		"VK_LAYER_LUNARG_draw_state",
		"VK_LAYER_LUNARG_image",
		"VK_LAYER_LUNARG_mem_tracker",
		"VK_LAYER_LUNARG_object_tracker",
		"VK_LAYER_LUNARG_param_checker",
		"VK_LAYER_LUNARG_swapchain",
		"VK_LAYER_GOOGLE_unique_objects"/*,
        "VK_LAYER_RENDERDOC_Capture"*/
	};

	static const char* s_deviceLayers[] =
	{
		"VK_LAYER_GOOGLE_threading",
		"VK_LAYER_LUNARG_device_limits",
		"VK_LAYER_LUNARG_draw_state",
		"VK_LAYER_LUNARG_image",
		"VK_LAYER_LUNARG_mem_tracker",
		"VK_LAYER_LUNARG_object_tracker",
		"VK_LAYER_LUNARG_param_checker",
		"VK_LAYER_LUNARG_swapchain",
		"VK_LAYER_GOOGLE_unique_objects"/*,
        "VK_LAYER_RENDERDOC_Capture"*/
	};

    using VulkanAPIFailure = Metal_Vulkan::VulkanAPIFailure;

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

    static VkDebugReportCallbackEXT msg_callback;
    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback( VkFlags msgFlags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location, int32_t msgCode, const char *pLayerPrefix, const char *pMsg, void *pUserData )
    {
	    (void)msgFlags; (void)objType; (void)srcObject; (void)location; (void)pUserData; (void)msgCode;
        LogInfo << pLayerPrefix << ": " << pMsg;
	    return false;
    }
    
    static void debug_init(VkInstance instance)
    {
        VkDebugReportCallbackCreateInfoEXT debug_callback_info = {};
        debug_callback_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        debug_callback_info.pfnCallback = debug_callback;
        // debug_callback_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        debug_callback_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	
	    auto proc = ((PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr( instance, "vkCreateDebugReportCallbackEXT" ));
        proc( instance, &debug_callback_info, Metal_Vulkan::g_allocationCallbacks, &msg_callback );
    }
    
    static void debug_destroy(VkInstance instance)
    {
	    ((PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr( instance, "vkDestroyDebugReportCallbackEXT" ))( instance, msg_callback, 0 );
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

		auto availableLayers = EnumerateLayers();

        std::vector<const char*> filteredLayers;
        for (unsigned c=0; c<dimof(s_instanceLayers); ++c) {
            auto i = std::find_if(
                availableLayers.begin(), availableLayers.end(),
                [c](VkLayerProperties layer) { return XlEqString(layer.layerName, s_instanceLayers[c]); });
            if (i != availableLayers.end())
                filteredLayers.push_back(s_instanceLayers[c]);
        }

		VkInstanceCreateInfo inst_info = {};
		inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		inst_info.pNext = NULL;
		inst_info.flags = 0;
		inst_info.pApplicationInfo = &app_info;
		inst_info.enabledLayerCount = (uint32_t)filteredLayers.size();
		inst_info.ppEnabledLayerNames = AsPointer(filteredLayers.begin());
		inst_info.enabledExtensionCount = (uint32_t)dimof(s_instanceExtensions);
		inst_info.ppEnabledExtensionNames = s_instanceExtensions;

		VkInstance rawResult = nullptr;
		VkResult res = vkCreateInstance(&inst_info, Metal_Vulkan::g_allocationCallbacks, &rawResult);
		auto instance = VulkanSharedPtr<VkInstance>(
			rawResult,
			[](VkInstance inst) { debug_destroy(inst); vkDestroyInstance(inst, Metal_Vulkan::g_allocationCallbacks); });
        if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure in Vulkan instance construction. You must have an up-to-date Vulkan driver installed."));

        debug_init(instance.get());
        return std::move(instance);
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
			auto res = vkCreateWin32SurfaceKHR(vulkan, &createInfo, Metal_Vulkan::g_allocationCallbacks, &rawResult);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failure in Vulkan surface construction. You must have an up-to-date Vulkan driver installed."));

			// note --	capturing "vulkan" with an unprotected pointer here. We could use a protected
			//			pointer easily enough... But I guess this approach is in-line with Vulkan design ideas.
			return VulkanSharedPtr<VkSurfaceKHR>(
				rawResult,
				[vulkan](VkSurfaceKHR inst) { vkDestroySurfaceKHR(vulkan, inst, Metal_Vulkan::g_allocationCallbacks); });
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

        auto availableLayers = EnumerateLayers();
        std::vector<const char*> filteredLayers;
        for (unsigned c=0; c<dimof(s_deviceLayers); ++c) {
            auto i = std::find_if(
                availableLayers.begin(), availableLayers.end(),
                [c](VkLayerProperties layer) { return XlEqString(layer.layerName, s_deviceLayers[c]); });
            if (i != availableLayers.end())
                filteredLayers.push_back(s_deviceLayers[c]);
        }

		VkDeviceCreateInfo device_info = {};
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.pNext = nullptr;
		device_info.queueCreateInfoCount = 1;
		device_info.pQueueCreateInfos = &queue_info;
		device_info.enabledLayerCount = (uint32)filteredLayers.size();
		device_info.ppEnabledLayerNames = AsPointer(filteredLayers.begin());
		device_info.enabledExtensionCount = (uint32_t)dimof(s_deviceExtensions);
		device_info.ppEnabledExtensionNames = s_deviceExtensions;
		device_info.pEnabledFeatures = nullptr;

		VkDevice rawResult = nullptr;
		auto res = vkCreateDevice(physDev._dev, &device_info, Metal_Vulkan::g_allocationCallbacks, &rawResult);
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while creating Vulkan logical device. You must have an up-to-date Vulkan driver installed."));
		return VulkanSharedPtr<VkDevice>(
			rawResult,
			[](VkDevice dev) { vkDestroyDevice(dev, Metal_Vulkan::g_allocationCallbacks); });
	}

    Device::Device()
    {
            // todo -- we need to do this in a bind to DLL step
        Metal_Vulkan::InitFormatConversionTables();

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
        Metal_Vulkan::SetDefaultObjectFactory(nullptr);
		if (_underlying.get())
			vkDeviceWaitIdle(_underlying.get());
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
			_objectFactory = Metal_Vulkan::ObjectFactory(_physDev._dev, _underlying);
			_pools._renderingCommandPool = Metal_Vulkan::CommandPool(_objectFactory, _physDev._renderingQueueFamily);
            _pools._mainDescriptorPool = Metal_Vulkan::DescriptorPool(_objectFactory);
            _pools._mainPipelineCache = _objectFactory.CreatePipelineCache();

			_foregroundPrimaryContext = std::make_shared<ThreadContextVulkan>(
				shared_from_this(), 
                _objectFactory,
				_pools._renderingCommandPool.Allocate(Metal_Vulkan::CommandPool::BufferType::Primary));

            Metal_Vulkan::SetDefaultObjectFactory(&_objectFactory);
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
        res = vkCreateSwapchainKHR(underlyingDev, &swapChainInfo, Metal_Vulkan::g_allocationCallbacks, &swapChainRaw);
        VulkanSharedPtr<VkSwapchainKHR> result(
            swapChainRaw,
            [underlyingDev](VkSwapchainKHR chain) { vkDestroySwapchainKHR(underlyingDev, chain, Metal_Vulkan::g_allocationCallbacks); } );
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure while creating swap chain"));

		return std::make_unique<PresentationChain>(
			std::move(surface), std::move(result), _objectFactory,
            GetQueue(_underlying.get(), _physDev._renderingQueueFamily), 
            _pools,
            BufferUploads::TextureDesc::Plain2D(swapChainExtent.width, swapChainExtent.height, chainFmt),
            platformValue);
    }

    static void BeginOneTimeSubmit(VkCommandBuffer cmd)
    {
        VkCommandBufferBeginInfo cmd_buf_info = {};
		cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmd_buf_info.pNext = nullptr;
		cmd_buf_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		cmd_buf_info.pInheritanceInfo = nullptr;
		auto res = vkBeginCommandBuffer(cmd, &cmd_buf_info);
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while beginning command buffer"));
    }

    void    Device::BeginFrame(IPresentationChain* presentationChain)
    {
        PresentationChain* swapChain = checked_cast<PresentationChain*>(presentationChain);
        swapChain->AcquireNextImage();

		// reset and begin the primary foreground command buffer
		auto cmdBuffer = _foregroundPrimaryContext->GetPrimaryCommandBuffer();
		auto res = vkResetCommandBuffer(cmdBuffer, 0);
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while resetting command buffer"));

		BeginOneTimeSubmit(cmdBuffer);
		auto rp = swapChain->BindDefaultRenderPass(cmdBuffer);

        if (rp)
            _foregroundPrimaryContext->GetMetalContext()->Bind(rp->ShareUnderlying());
    }

    std::shared_ptr<IThreadContext> Device::GetImmediateContext()
    {
		return _foregroundPrimaryContext;
    }

    std::unique_ptr<IThreadContext> Device::CreateDeferredContext()
    {
		return nullptr;
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

	VkInstance DeviceVulkan::GetVulkanInstance() { return _instance.get(); }
	VkDevice DeviceVulkan::GetUnderlyingDevice() { return _underlying.get(); }
    VkQueue DeviceVulkan::GetRenderingQueue()
    {
        return GetQueue(_underlying.get(), _physDev._renderingQueueFamily, 0);
    }

    Metal_Vulkan::GlobalPools&      DeviceVulkan::GetGlobalPools()
    {
        return Device::GetGlobalPools();
    }

	DeviceVulkan::DeviceVulkan() { }
	DeviceVulkan::~DeviceVulkan() { }

    //////////////////////////////////////////////////////////////////////////////////////////////////

	#if 0
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
	#endif

    void            PresentationChain::Present()
    {
        if (_activeImageIndex > unsigned(_images.size())) return;

		//////////////////////////////////////////////////////////////////

        auto& sync = _presentSyncs[_activePresentSync];

		VkSubmitInfo submitInfo;
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;

        VkSemaphore waitSema[] = { sync._onAcquireComplete.get() };
        VkSemaphore signalSema[] = { sync._onCommandBufferComplete.get() };
        VkPipelineStageFlags stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		submitInfo.waitSemaphoreCount = dimof(waitSema);
		submitInfo.pWaitSemaphores = waitSema;
        submitInfo.signalSemaphoreCount = dimof(signalSema);
		submitInfo.pSignalSemaphores = signalSema;
		submitInfo.pWaitDstStageMask = &stage;

		submitInfo.commandBufferCount = 0;
		submitInfo.pCommandBuffers = nullptr;
		if (_cmdBufferPendingCommit) {
			vkCmdEndRenderPass(_cmdBufferPendingCommit);
			auto res = vkEndCommandBuffer(_cmdBufferPendingCommit);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failure while ending command buffer"));
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &_cmdBufferPendingCommit;
		}

		auto res = vkQueueSubmit(_queue, 1, &submitInfo, VK_NULL_HANDLE);
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while queuing semaphore signal"));

        // hack -- need to implement CPU/GPU synchronisation
        vkDeviceWaitIdle(_device.get());
        _factory->FlushDestructionQueue();
        _globalPools->_mainDescriptorPool.FlushDestroys();

		_cmdBufferPendingCommit = nullptr;

		//////////////////////////////////////////////////////////////////

        const VkSwapchainKHR swapChains[] = { _swapChain.get() };
        uint32_t imageIndices[] = { _activeImageIndex };
        const VkSemaphore waitSema_2[] = { sync._onCommandBufferComplete.get() };

        VkPresentInfoKHR present;
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.pNext = NULL;
        present.swapchainCount = dimof(swapChains);
        present.pSwapchains = swapChains;
        present.pImageIndices = imageIndices;
        present.pWaitSemaphores = waitSema_2;
        present.waitSemaphoreCount = dimof(waitSema_2);
        present.pResults = NULL;

        res = vkQueuePresentKHR(_queue, &present);
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure while queuing present"));

        // queue a fence -- used to avoid acquiring an image before it has completed it's present.
        res = vkQueueSubmit(_queue, 0, nullptr, sync._presentFence.get());
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure while queuing post present fence"));

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
        _activePresentSync = (_activePresentSync+1) % dimof(_presentSyncs);
        auto& sync = _presentSyncs[_activePresentSync];
        auto fence = sync._presentFence.get();
        auto res = vkWaitForFences(_device.get(), 1, &fence, true, UINT64_MAX);
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure while waiting for presentation fence"));
        res = vkResetFences(_device.get(), 1, &fence);
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure while resetting presentation fence"));

        // note --  Due to the timeout here, we get a synchronise here.
        //          This will prevent issues when either the GPU or CPU is
        //          running ahead of the other... But we could do better by
        //          using the semaphores
        //
        // Note that we must handle the VK_NOT_READY result... Some implementations
        // may not block, even when timeout is some large value.
        // As stated in the documentation, we shouldn't rely on this function for
        // synchronisation -- instead, we should write an algorithm that will insert 
        // stalls as necessary
        uint32_t nextImageIndex = ~0x0u;
        const auto timeout = UINT64_MAX;
        res = vkAcquireNextImageKHR(
            _device.get(), _swapChain.get(), 
            timeout,
            sync._onAcquireComplete.get(), VK_NULL_HANDLE,
            &nextImageIndex);
        _activeImageIndex = nextImageIndex;

        // TODO: Deal with the VK_SUBOPTIMAL_KHR and VK_ERROR_OUT_OF_DATE_KHR
        // return codes
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure during acquire next image"));
    }

	static VkClearValue ClearDepthStencil(float depth, uint32_t stencil) { VkClearValue result; result.depthStencil = VkClearDepthStencilValue { depth, stencil }; return result; }
	static VkClearValue ClearColor(float r, float g, float b, float a) { VkClearValue result; result.color.float32[0] = r; result.color.float32[1] = g; result.color.float32[2] = b; result.color.float32[3] = a; return result; }

    RenderPass* PresentationChain::BindDefaultRenderPass(VkCommandBuffer cmd)
    {
        if (_activeImageIndex >= unsigned(_images.size())) return nullptr;
		assert(!_cmdBufferPendingCommit);

		// bind the default render pass for rendering directly to the swapchain
		VkRenderPassBeginInfo rp_begin;
		rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rp_begin.pNext = nullptr;
		rp_begin.renderPass = _defaultRenderPass.GetUnderlying();
		rp_begin.framebuffer = _images[_activeImageIndex]._defaultFrameBuffer.GetUnderlying();
		rp_begin.renderArea.offset.x = 0;
		rp_begin.renderArea.offset.y = 0;
		rp_begin.renderArea.extent.width = _bufferDesc._width;
		rp_begin.renderArea.extent.height = _bufferDesc._height;
		
		VkClearValue clearValues[] = { ClearColor(0.5f, 0.25f, 1.f, 1.f), ClearDepthStencil(1.f, 0) };
		rp_begin.pClearValues = clearValues;
		rp_begin.clearValueCount = dimof(clearValues);

		vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
		_cmdBufferPendingCommit = cmd;

        return &_defaultRenderPass;
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

            // We don't have to destroy the images with VkDestroyImage -- they will be destroyed when the
            // swapchain is destroyed.
            return rawPtrs;
        }
    }

    RenderPass::RenderPass(
        const Metal_Vulkan::ObjectFactory& factory,
        IteratorRange<TargetInfo*> rtvAttachments,
        TargetInfo dsvAttachment)
    {
        // The render targets and depth buffers slots are called "attachments"
        // In this case, we will create a render pass with a single subpass.
        // That subpass will reference all buffers.
        // This sets up the slots for rendertargets and depth buffers -- but it
        // doesn't assign the specific images.
      
        bool hasDepthBuffer = dsvAttachment._format != VK_FORMAT_UNDEFINED;

        VkAttachmentDescription attachmentsStatic[8];
        std::vector<VkAttachmentDescription> attachmentsOverflow;
        VkAttachmentReference colorReferencesStatic[8];
        std::vector<VkAttachmentReference> colorReferencesOverflow;

        VkAttachmentDescription* attachments = attachmentsStatic;
        auto attachmentCount = rtvAttachments.size() + unsigned(hasDepthBuffer);
        if (attachmentCount > dimof(attachmentsStatic)) {
            attachmentsOverflow.resize(attachmentCount);
            attachments = AsPointer(attachmentsOverflow.begin());
        }
        XlClearMemory(attachments, sizeof(VkAttachmentDescription) * attachmentCount);

        VkAttachmentReference* colorReferences = colorReferencesStatic;
        if (rtvAttachments.size() > dimof(colorReferencesStatic)) {
            colorReferencesOverflow.resize(rtvAttachments.size());
            colorReferences = AsPointer(colorReferencesOverflow.begin());
        }

        // note -- 
        //      Is it safe to set the initialLayout in the RenderPass to _UNDEFINED
        //      every time? If we're rendering to an image that we've touched before,
        //      does it mean we can start using the same value for initial and final layouts?

        auto* i = attachments;
        for (auto& rtv:rtvAttachments) {
            i->format = rtv._format;
            i->samples = rtv._samples;
            i->loadOp = (rtv._previousState ==  PreviousState::DontCare) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            i->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            i->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            i->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            i->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            i->finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            i->flags = 0;

            colorReferences[i-attachments] = {};
            colorReferences[i-attachments].attachment = uint32_t(i-attachments);
            colorReferences[i-attachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            ++i;
        }

        VkAttachmentReference depth_reference = {};

        if (hasDepthBuffer) {
            i->format = dsvAttachment._format;
            i->samples = dsvAttachment._samples;
            i->loadOp = (dsvAttachment._previousState ==  PreviousState::DontCare) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            i->storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            // note -- retaining stencil values frame to frame
            i->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            i->stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

            i->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            i->finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            i->flags = 0;

            depth_reference.attachment = uint32_t(i - attachments);
            depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            ++i;
        }

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.flags = 0;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = nullptr;
        subpass.colorAttachmentCount = uint32_t(rtvAttachments.size());
        subpass.pColorAttachments = colorReferences;
        subpass.pResolveAttachments = nullptr;
        subpass.pDepthStencilAttachment = hasDepthBuffer ? &depth_reference : nullptr;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = nullptr;

        VkRenderPassCreateInfo rp_info = {};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.pNext = nullptr;
        rp_info.attachmentCount = (uint32_t)attachmentCount;
        rp_info.pAttachments = attachments;
        rp_info.subpassCount = 1;
        rp_info.pSubpasses = &subpass;
        rp_info.dependencyCount = 0;
        rp_info.pDependencies = nullptr;

        _underlying = factory.CreateRenderPass(rp_info);
    }

	RenderPass::RenderPass() {}

    RenderPass::~RenderPass() {}

    

    

    static VkImageTiling SelectDepthStencilTiling(VkPhysicalDevice physDev, VkFormat fmt)
    {
        // note --  flipped around the priority here from the samples (so that we usually won't
        //          select linear tiling)
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physDev, fmt, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return VK_IMAGE_TILING_OPTIMAL;
        } else if (props.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return VK_IMAGE_TILING_LINEAR;
        } else {
            Throw(Exceptions::BasicLabel("Format (%i) can't be used for a depth stencil", unsigned(fmt)));
        }
    }

    static VulkanSharedPtr<VkDeviceMemory> AllocateDeviceMemory(
        const Metal_Vulkan::ObjectFactory& factory, VkMemoryRequirements memReqs)
    {
        auto type = factory.FindMemoryType(memReqs.memoryTypeBits);
        if (type >= 32)
            Throw(Exceptions::BasicLabel("Could not find compatible memory type for image"));
        return factory.AllocateMemory(memReqs.size, type);
    }

    Resource::Resource(const Metal_Vulkan::ObjectFactory& factory, const BufferUploads::BufferDesc& desc)
    : _desc(desc)
    {
        if (desc._type == BufferUploads::BufferDesc::Type::Texture) {
            const auto& tex = desc._textureDesc;

                //
                //  Create the "image" first....
                //
            VkImageCreateInfo image_info = {};
            image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            image_info.pNext = nullptr;
            image_info.imageType = VK_IMAGE_TYPE_2D;
            image_info.format = (VkFormat)tex._nativePixelFormat;
            image_info.extent.width = tex._width;
            image_info.extent.height = tex._height;
            image_info.extent.depth = tex._depth;
            image_info.mipLevels = tex._mipCount;
            image_info.arrayLayers = tex._arrayCount;
            image_info.samples = Metal_Vulkan::AsSampleCountFlagBits(tex._samples);
            image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image_info.queueFamilyIndexCount = 0;
            image_info.pQueueFamilyIndices = nullptr;
            image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            image_info.flags = 0;
            image_info.tiling = SelectDepthStencilTiling(factory.GetPhysicalDevice(), image_info.format);

            _image = factory.CreateImage(image_info);

                //
                //  We must decide on the right type of memory, and then allocate 
                //  the memory buffer.
                //
            auto dev = factory.GetDevice().get();
            VkMemoryRequirements mem_reqs = {};
            vkGetImageMemoryRequirements(dev, _image.get(), &mem_reqs);

            _mem = AllocateDeviceMemory(factory, mem_reqs);

                //
                //  Bind the memory to the image
                //
            auto res = vkBindImageMemory(dev, _image.get(), _mem.get(), 0);
            if (res != VK_SUCCESS)
                Throw(VulkanAPIFailure(res, "Failed while binding device memory to image"));

            // Samples set the "image layout" here. But we may not need to do that for resources
            // that will be used with a RenderPass -- since we can just setup the layout when we
            // create the renderpass.
        }
    }

	Resource::Resource() {}

    Resource::~Resource() {}

    ImageView::~ImageView() {}
    
	DepthStencilView::DepthStencilView() {}

    DepthStencilView::DepthStencilView(const Metal_Vulkan::ObjectFactory& factory, Resource& res)
    {
        if (res.GetDesc()._type != BufferUploads::BufferDesc::Type::Texture)
            Throw(Exceptions::BasicLabel("Attempting to build a DepthStencilView for a resource that is not a texture"));

        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.pNext = nullptr;
        view_info.image = res.GetImage();
        view_info.format = (VkFormat)res.GetDesc()._textureDesc._nativePixelFormat;
        view_info.components.r = VK_COMPONENT_SWIZZLE_R;
        view_info.components.g = VK_COMPONENT_SWIZZLE_G;
        view_info.components.b = VK_COMPONENT_SWIZZLE_B;
        view_info.components.a = VK_COMPONENT_SWIZZLE_A;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.flags = 0;
        assert(view_info.image != VK_NULL_HANDLE);

        if (view_info.format == VK_FORMAT_D16_UNORM_S8_UINT ||
            view_info.format == VK_FORMAT_D24_UNORM_S8_UINT ||
            view_info.format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
            view_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        _underlying = factory.CreateImageView(view_info);
    }

    RenderTargetView::RenderTargetView(const Metal_Vulkan::ObjectFactory& factory, Resource& res)
    : RenderTargetView(factory, res.GetImage(), (VkFormat)res.GetDesc()._textureDesc._nativePixelFormat)
    {}

	RenderTargetView::RenderTargetView() {}

    RenderTargetView::RenderTargetView(const Metal_Vulkan::ObjectFactory& factory, VkImage image, VkFormat fmt)
    {
        VkImageViewCreateInfo color_image_view = {};
        color_image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        color_image_view.pNext = nullptr;
        color_image_view.format = fmt;
        color_image_view.components.r = VK_COMPONENT_SWIZZLE_R;
        color_image_view.components.g = VK_COMPONENT_SWIZZLE_G;
        color_image_view.components.b = VK_COMPONENT_SWIZZLE_B;
        color_image_view.components.a = VK_COMPONENT_SWIZZLE_A;
        color_image_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        color_image_view.subresourceRange.baseMipLevel = 0;
        color_image_view.subresourceRange.levelCount = 1;
        color_image_view.subresourceRange.baseArrayLayer = 0;
        color_image_view.subresourceRange.layerCount = 1;
        color_image_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        color_image_view.flags = 0;
        color_image_view.image = image;

        // samples set the image layout here... Maybe it's not necessary for us, because we can set it up
        // in the render pass...?

        _underlying = factory.CreateImageView(color_image_view);
    }

    FrameBuffer::FrameBuffer(
        const Metal_Vulkan::ObjectFactory& factory,
        IteratorRange<VkImageView*> views,
        RenderPass& renderPass,
        unsigned width, unsigned height)
    {
        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.pNext = nullptr;
        fb_info.renderPass = renderPass.GetUnderlying();
        fb_info.attachmentCount = (uint32_t)views.size();
        fb_info.pAttachments = views.begin();
        fb_info.width = width;
        fb_info.height = height;
        fb_info.layers = 1;
        _underlying = factory.CreateFramebuffer(fb_info);
    }

	FrameBuffer::FrameBuffer() {}

    FrameBuffer::~FrameBuffer() {}

    PresentationChain::PresentationChain(
		VulkanSharedPtr<VkSurfaceKHR> surface,
        VulkanSharedPtr<VkSwapchainKHR> swapChain,
        const Metal_Vulkan::ObjectFactory& factory,
        VkQueue queue, 
        Metal_Vulkan::GlobalPools& globalPools,
        const BufferUploads::TextureDesc& bufferDesc,
        const void* platformValue)
    : _surface(std::move(surface))
	, _swapChain(std::move(swapChain))
    , _queue(queue)
    , _device(factory.GetDevice())
    , _platformValue(platformValue)
	, _bufferDesc(bufferDesc)
	, _cmdBufferPendingCommit(nullptr)
    , _factory(&factory)
    , _globalPools(&globalPools)
    {
        _activeImageIndex = ~0x0u;

        // We need to get pointers to each image and build the synchronization semaphores
        auto images = GetImages(_device.get(), _swapChain.get());
        _images.reserve(images.size());
        for (auto& i:images) _images.emplace_back(Image { i });

		_depthStencilResource = Resource(
            factory, 
            BufferUploads::CreateDesc(
                BufferUploads::BindFlag::DepthStencil,
                0, BufferUploads::GPUAccess::Read | BufferUploads::GPUAccess::Write,
                BufferUploads::TextureDesc::Plain2D(bufferDesc._width, bufferDesc._height, VK_FORMAT_D24_UNORM_S8_UINT, 1, 1, bufferDesc._samples),
                "DefaultDepth"));
        _dsv = DepthStencilView(factory, _depthStencilResource);

        // We must create a default render pass for rendering to the swap-chain images. 
        // In the most basic rendering operations, we just render directly into these buffers.
        // More complex applications may render into separate buffers, and then resolve onto 
        // the swap chain buffers. In those cases, the use of PresentationChain may be radically
        // different (eg, we don't even need to call AcquireNextImage() until we're ready to
        // do the resolve).
        // Also, more complex applications may want to combine the resolve operation into the
        // render pass for rendering to offscreen buffers.

        auto vkSamples = Metal_Vulkan::AsSampleCountFlagBits(bufferDesc._samples);
        RenderPass::TargetInfo rtvAttachments[] = { RenderPass::TargetInfo((VkFormat)bufferDesc._nativePixelFormat, vkSamples, RenderPass::PreviousState::Clear) };

        const VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
        const bool createDepthBuffer = depthFormat != VK_FORMAT_UNDEFINED;
        RenderPass::TargetInfo depthTargetInfo;
        if (constant_expression<createDepthBuffer>::result())
            depthTargetInfo = RenderPass::TargetInfo(depthFormat, vkSamples, RenderPass::PreviousState::Clear);
        
        _defaultRenderPass = RenderPass(factory, MakeIteratorRange(rtvAttachments), depthTargetInfo);

        // Now create the frame buffers to match the render pass
        VkImageView imageViews[2];
        imageViews[1] = _dsv.GetUnderlying();

        for (auto& i:_images) {
			i._rtv = RenderTargetView(factory, i._underlying, (VkFormat)bufferDesc._nativePixelFormat);
            imageViews[0] = i._rtv.GetUnderlying();
            i._defaultFrameBuffer = FrameBuffer(factory, MakeIteratorRange(imageViews), _defaultRenderPass, bufferDesc._width, bufferDesc._height);
        }

        // Create the synchronisation primitives
        // This pattern is similar to the "Hologram" sample in the Vulkan SDK
        for (unsigned c=0; c<dimof(_presentSyncs); ++c) {
            _presentSyncs[c]._onCommandBufferComplete = factory.CreateSemaphore();
            _presentSyncs[c]._onAcquireComplete = factory.CreateSemaphore();
            _presentSyncs[c]._presentFence = factory.CreateFence(VK_FENCE_CREATE_SIGNALED_BIT);
        }
        _activePresentSync = 0;

        // We need to set the image layout for these images we created
        // this is a little frustrating. I wonder if the GPU is just rearranging the pixel contents?
        if (constant_expression<false>::result()) {
            auto cmd = globalPools._renderingCommandPool.Allocate(Metal_Vulkan::CommandPool::BufferType::Primary);
            BeginOneTimeSubmit(cmd.get());

            for (auto& i:_images)
				Metal_Vulkan::SetImageLayout(
                    cmd.get(), i._underlying, 
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			Metal_Vulkan::SetImageLayout(
                cmd.get(), _depthStencilResource.GetImage(),
                VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

            auto res = vkEndCommandBuffer(cmd.get());
            if (res != VK_SUCCESS)
			    Throw(VulkanAPIFailure(res, "Failure while ending command buffer"));

            auto cmdRaw = cmd.get();
            VkSubmitInfo submitInfo;
		    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		    submitInfo.pNext = nullptr;
		    submitInfo.waitSemaphoreCount = 0;
		    submitInfo.pWaitSemaphores = nullptr;
		    submitInfo.pWaitDstStageMask = nullptr;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmdRaw;
		    submitInfo.signalSemaphoreCount = 0;
		    submitInfo.pSignalSemaphores = nullptr;

            auto fence = factory.CreateFence();
            res = vkQueueSubmit(_queue, 1, &submitInfo, fence.get());
		    if (res != VK_SUCCESS)
			    Throw(VulkanAPIFailure(res, "Failure while queuing semaphore signal"));
            VkFence fences[] = { fence.get() };
            res = vkWaitForFences(factory.GetDevice().get(), dimof(fences), fences, true, UINT64_MAX);
            if (res != VK_SUCCESS)
			    Throw(VulkanAPIFailure(res, "Failure while waiting for fence"));
        }

    }

    PresentationChain::~PresentationChain()
    {
		_defaultRenderPass = RenderPass();
		_images.clear();
		_dsv = DepthStencilView();
		_depthStencilResource = Resource();
		_swapChain.reset();
		_device.reset();
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
        return true;
    }

    auto ThreadContext::GetStateDesc() const -> ThreadContextStateDesc
    {
		return {};
    }

	void ThreadContext::InvalidateCachedState() const {}

    ThreadContext::ThreadContext(
        std::shared_ptr<Device> device, 
        const Metal_Vulkan::ObjectFactory& factory, 
        VulkanSharedPtr<VkCommandBuffer> primaryCommandBuffer)
    : _device(device)
	, _frameId(0)
	, _metalContext(std::make_shared<Metal_Vulkan::DeviceContext>(factory, std::move(primaryCommandBuffer), device->GetGlobalPools()))
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

    VkCommandBuffer ThreadContextVulkan::GetPrimaryCommandBuffer()
    {
        return _metalContext->GetPrimaryCommandList().get();
    }

    const std::shared_ptr<Metal_Vulkan::DeviceContext>& ThreadContextVulkan::GetMetalContext()
    {
        return _metalContext;
    }

    ThreadContextVulkan::ThreadContextVulkan(
        std::shared_ptr<Device> device, 
        const Metal_Vulkan::ObjectFactory& factory, 
        VulkanSharedPtr<VkCommandBuffer> primaryCommandBuffer)
    : ThreadContext(std::move(device), factory, std::move(primaryCommandBuffer))
    {}

	ThreadContextVulkan::~ThreadContextVulkan() {}

}

