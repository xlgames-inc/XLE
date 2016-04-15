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
#include "Metal/TextureView.h"	// for ShaderResourceView::Cleanup
#include "../Format.h"
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
		Metal_Vulkan::ShaderResourceView::Cleanup();
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
            _pools._mainDescriptorPool = Metal_Vulkan::DescriptorPool(_objectFactory);
            _pools._mainPipelineCache = _objectFactory.CreatePipelineCache();

            _foregroundPrimaryContext = std::make_shared<ThreadContextVulkan>(
				shared_from_this(), 
				GetQueue(_underlying.get(), _physDev._renderingQueueFamily),
                Metal_Vulkan::CommandPool(_objectFactory, _physDev._renderingQueueFamily),
				Metal_Vulkan::CommandPool::BufferType::Primary);
            _foregroundPrimaryContext->BeginCommandList();

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

		auto finalChain = std::make_unique<PresentationChain>(
			std::move(surface), std::move(result), _objectFactory,
            BufferUploads::TextureDesc::Plain2D(swapChainExtent.width, swapChainExtent.height, Metal_Vulkan::AsFormat(chainFmt)),
            platformValue);

        // (synchronously) set the initial layouts for the presentation chain images
        // It's a bit odd, but the Vulkan samples do this
        finalChain->SetInitialLayout(
            _objectFactory,
            _foregroundPrimaryContext->GetRenderingCommandPool(),
            _foregroundPrimaryContext->GetQueue());

        return std::move(finalChain);
    }

    std::shared_ptr<IThreadContext> Device::GetImmediateContext()
    {
		return _foregroundPrimaryContext;
    }

    std::unique_ptr<IThreadContext> Device::CreateDeferredContext()
    {
		return std::make_unique<ThreadContextVulkan>(
            shared_from_this(), 
            nullptr, 
            Metal_Vulkan::CommandPool(_objectFactory, _physDev._renderingQueueFamily),
            Metal_Vulkan::CommandPool::BufferType::Secondary);
    }

	namespace Internal
	{
		class ResourceAllocator : public std::allocator<Metal_Vulkan::Resource>
		{
		public:
			pointer allocate(size_type n, std::allocator<void>::const_pointer ptr)
			{
				Throw(::Exceptions::BasicLabel("Allocation attempted via ResourceAllocator"));
			}

			void deallocate(pointer p, size_type n)
			{
				delete (Metal_Vulkan::Resource*)p;
			}
		};
	}

	ResourcePtr AllocateResource(
		const Metal_Vulkan::ObjectFactory& factory,
		const ResourceDesc& desc,
		const std::function<SubResourceInitData(unsigned, unsigned)>& initData = std::function<SubResourceInitData(unsigned, unsigned)>())
	{
		const bool useAllocateShared = true;
		if (constant_expression<useAllocateShared>::result()) {
			auto res = std::allocate_shared<Metal_Vulkan::Resource>(
				Internal::ResourceAllocator(),
				std::ref(factory), std::ref(desc), std::ref(initData));
			return *reinterpret_cast<ResourcePtr*>(&res);
		}
		else {
			auto res = std::make_unique<Metal_Vulkan::Resource>(factory, desc, initData);
			return ResourcePtr(
				(RenderCore::Resource*)res.release(),
				[](RenderCore::Resource* res) { delete (Metal_Vulkan::Resource*)res; });
		}
	}

	ResourcePtr Device::CreateResource(
		const ResourceDesc& desc,
		const std::function<SubResourceInitData(unsigned, unsigned)>& initData)
	{
		return AllocateResource(_objectFactory, desc, initData);
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

    void            PresentationChain::Resize(unsigned newWidth, unsigned newHeight)
    {
        // todo -- we'll need to destroy and recreate the swapchain here
    }

    std::shared_ptr<ViewportContext> PresentationChain::GetViewportContext() const
    {
		return _viewportContext;
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

    Metal_Vulkan::FrameBufferLayout* PresentationChain::BindDefaultRenderPass(Metal_Vulkan::DeviceContext& context)
    {
        if (_activeImageIndex >= unsigned(_images.size())) return nullptr;

		// bind the default render pass for rendering directly to the swapchain
		context.BeginRenderPass(
            _defaultRenderPass, _images[_activeImageIndex]._defaultFrameBuffer,
            {0,0}, {_bufferDesc._width, _bufferDesc._height});

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

	void PresentationChain::PresentToQueue(VkQueue queue)
	{
		if (_activeImageIndex > unsigned(_images.size())) return;

		auto& sync = _presentSyncs[_activePresentSync];
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

		auto res = vkQueuePresentKHR(queue, &present);
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while queuing present"));

		// queue a fence -- used to avoid acquiring an image before it has completed it's present.
		res = vkQueueSubmit(queue, 0, nullptr, sync._presentFence.get());
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while queuing post present fence"));

		_activeImageIndex = ~0x0u;
	}

    PresentationChain::PresentationChain(
		VulkanSharedPtr<VkSurfaceKHR> surface,
        VulkanSharedPtr<VkSwapchainKHR> swapChain,
        const Metal_Vulkan::ObjectFactory& factory,
        const BufferUploads::TextureDesc& bufferDesc,
        const void* platformValue)
    : _surface(std::move(surface))
	, _swapChain(std::move(swapChain))
    , _device(factory.GetDevice())
    , _platformValue(platformValue)
	, _bufferDesc(bufferDesc)
    {
        _activeImageIndex = ~0x0u;
		_viewportContext = std::make_shared<ViewportContext>(VectorPattern<unsigned, 2>(bufferDesc._width, bufferDesc._height));

        // We need to get pointers to each image and build the synchronization semaphores
        auto images = GetImages(_device.get(), _swapChain.get());
        _images.reserve(images.size());
        for (auto& i:images) _images.emplace_back(Image { i });

        const auto depthFormat = Format::D24_UNORM_S8_UINT;
		_depthStencilResource = AllocateResource(
            factory, 
            CreateDesc(
                BufferUploads::BindFlag::DepthStencil,
                0, BufferUploads::GPUAccess::Read | BufferUploads::GPUAccess::Write,
                BufferUploads::TextureDesc::Plain2D(bufferDesc._width, bufferDesc._height, depthFormat, 1, 1, bufferDesc._samples),
                "DefaultDepth"));
        _dsv = Metal_Vulkan::DepthStencilView(factory, _depthStencilResource, Metal_Vulkan::TextureViewWindow());

        // We must create a default render pass for rendering to the swap-chain images. 
        // In the most basic rendering operations, we just render directly into these buffers.
        // More complex applications may render into separate buffers, and then resolve onto 
        // the swap chain buffers. In those cases, the use of PresentationChain may be radically
        // different (eg, we don't even need to call AcquireNextImage() until we're ready to
        // do the resolve).
        // Also, more complex applications may want to combine the resolve operation into the
        // render pass for rendering to offscreen buffers.

        using TargetInfo = Metal_Vulkan::FrameBufferLayout::TargetInfo;
        TargetInfo rtvAttachments[] = { TargetInfo(bufferDesc._format, bufferDesc._samples, Metal_Vulkan::FrameBufferLayout::PreviousState::Clear) };

        const bool createDepthBuffer = depthFormat != Format::Unknown;
        TargetInfo depthTargetInfo;
        if (constant_expression<createDepthBuffer>::result())
            depthTargetInfo = TargetInfo(depthFormat, bufferDesc._samples, Metal_Vulkan::FrameBufferLayout::PreviousState::Clear);
        
        _defaultRenderPass = Metal_Vulkan::FrameBufferLayout(factory, MakeIteratorRange(rtvAttachments), depthTargetInfo);

        // Now create the frame buffers to match the render pass
        VkImageView imageViews[2];
        imageViews[1] = _dsv.GetUnderlying();

        for (auto& i:_images) {
            Metal_Vulkan::TextureViewWindow window(
                bufferDesc._format, bufferDesc._dimensionality, 
                Metal_Vulkan::TextureViewWindow::SubResourceRange{0, bufferDesc._mipCount},
                Metal_Vulkan::TextureViewWindow::SubResourceRange{0, bufferDesc._arrayCount});
			i._rtv = Metal_Vulkan::RenderTargetView(factory, i._image, window);
            imageViews[0] = i._rtv.GetUnderlying();
            i._defaultFrameBuffer = Metal_Vulkan::FrameBuffer(factory, MakeIteratorRange(imageViews), _defaultRenderPass, bufferDesc._width, bufferDesc._height);
        }

        // Create the synchronisation primitives
        // This pattern is similar to the "Hologram" sample in the Vulkan SDK
        for (unsigned c=0; c<dimof(_presentSyncs); ++c) {
            _presentSyncs[c]._onCommandBufferComplete = factory.CreateSemaphore();
            _presentSyncs[c]._onAcquireComplete = factory.CreateSemaphore();
            _presentSyncs[c]._presentFence = factory.CreateFence(VK_FENCE_CREATE_SIGNALED_BIT);
        }
        _activePresentSync = 0;
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

    void PresentationChain::SetInitialLayout(
        const Metal_Vulkan::ObjectFactory& factory, 
        Metal_Vulkan::CommandPool& cmdPool, VkQueue queue)
    {
        // We need to set the image layout for these images we created
        // this is a little frustrating. I wonder if the GPU is just rearranging the pixel contents?
        auto cmd = cmdPool.Allocate(Metal_Vulkan::CommandPool::BufferType::Primary);
        BeginOneTimeSubmit(cmd.get());

        for (auto& i:_images)
			Metal_Vulkan::SetImageLayout(
                cmd.get(), i._image, 
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1, 1);
		Metal_Vulkan::SetImageLayout(
            cmd.get(), Metal_Vulkan::UnderlyingResourcePtr(_depthStencilResource).get()->GetImage(),
            VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, 1);

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
        res = vkQueueSubmit(queue, 1, &submitInfo, fence.get());
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while queuing semaphore signal"));
        VkFence fences[] = { fence.get() };
        res = vkWaitForFences(factory.GetDevice().get(), dimof(fences), fences, true, UINT64_MAX);
        if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while waiting for fence"));
    }

    PresentationChain::~PresentationChain()
    {
		_defaultRenderPass = Metal_Vulkan::FrameBufferLayout();
		_images.clear();
		_dsv = Metal_Vulkan::DepthStencilView();
		_depthStencilResource.reset();
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

	void    ThreadContext::BeginFrame(IPresentationChain& presentationChain)
	{
		PresentationChain* swapChain = checked_cast<PresentationChain*>(&presentationChain);
		swapChain->AcquireNextImage();

		auto rp = swapChain->BindDefaultRenderPass(*_metalContext);
		if (rp)
			_metalContext->Bind(rp->ShareUnderlying());
	}

	void            ThreadContext::Present(IPresentationChain& chain)
	{
		auto* swapChain = checked_cast<PresentationChain*>(&chain);
		auto& syncs = swapChain->GetSyncs();

		//////////////////////////////////////////////////////////////////

        _metalContext->EndRenderPass();
        auto cmdBuffer = _metalContext->ResolveCommandList();

		VkSubmitInfo submitInfo;
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;

		VkSemaphore waitSema[] = { syncs._onAcquireComplete.get() };
		VkSemaphore signalSema[] = { syncs._onCommandBufferComplete.get() };
        VkCommandBuffer rawCmdBuffers[] = { cmdBuffer.get() };
		VkPipelineStageFlags stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		submitInfo.waitSemaphoreCount = dimof(waitSema);
		submitInfo.pWaitSemaphores = waitSema;
		submitInfo.signalSemaphoreCount = dimof(signalSema);
		submitInfo.pSignalSemaphores = signalSema;
		submitInfo.pWaitDstStageMask = &stage;
		submitInfo.commandBufferCount = dimof(rawCmdBuffers);
		submitInfo.pCommandBuffers = rawCmdBuffers;
		
		auto res = vkQueueSubmit(_queue, 1, &submitInfo, VK_NULL_HANDLE);
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while queuing semaphore signal"));

		// hack -- need to implement CPU/GPU synchronisation
		vkDeviceWaitIdle(_underlyingDevice);
		_factory->FlushDestructionQueue();
		_globalPools->_mainDescriptorPool.FlushDestroys();
		_renderingCommandPool.FlushDestroys();

		//////////////////////////////////////////////////////////////////
		swapChain->PresentToQueue(_queue);

		// reset and begin the primary foreground command buffer immediately
		_metalContext->BeginCommandList();
	}

	

    bool    ThreadContext::IsImmediate() const
    {
        return _queue != nullptr;
    }

    auto ThreadContext::GetStateDesc() const -> ThreadContextStateDesc
    {
		return {};
    }

	void ThreadContext::InvalidateCachedState() const {}

    void ThreadContext::BeginCommandList()
    {
        _metalContext->BeginCommandList();
    }

    ThreadContext::ThreadContext(
		std::shared_ptr<Device> device,
		VkQueue queue,
        Metal_Vulkan::CommandPool&& cmdPool,
		Metal_Vulkan::CommandPool::BufferType cmdBufferType)
    : _device(device)
	, _frameId(0)
    , _renderingCommandPool(std::move(cmdPool))
	, _metalContext(
		std::make_shared<Metal_Vulkan::DeviceContext>(
			device->GetObjectFactory(), device->GetGlobalPools(), 
            _renderingCommandPool, cmdBufferType))
	, _factory(&device->GetObjectFactory())
	, _globalPools(&device->GetGlobalPools())
	, _queue(queue)
	, _underlyingDevice(device->GetUnderlyingDevice())
    {}

    ThreadContext::~ThreadContext() {}

    std::shared_ptr<IDevice> ThreadContext::GetDevice() const
    {
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

    const std::shared_ptr<Metal_Vulkan::DeviceContext>& ThreadContextVulkan::GetMetalContext()
    {
        return _metalContext;
    }

    ThreadContextVulkan::ThreadContextVulkan(
		std::shared_ptr<Device> device,
		VkQueue queue,
        Metal_Vulkan::CommandPool&& cmdPool,
		Metal_Vulkan::CommandPool::BufferType cmdBufferType)
    : ThreadContext(std::move(device), queue, std::move(cmdPool), cmdBufferType)
    {}

	ThreadContextVulkan::~ThreadContextVulkan() {}

}

