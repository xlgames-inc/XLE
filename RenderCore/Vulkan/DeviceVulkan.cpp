// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Device.h"
#include "../IAnnotator.h"
#include "../Format.h"
#include "../Init.h"
#include "Metal/VulkanCore.h"
#include "Metal/ObjectFactory.h"
#include "Metal/Format.h"
#include "Metal/Pools.h"
#include "Metal/Resource.h"
#include "Metal/PipelineLayout.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/Threading/ThreadingUtils.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/PtrUtils.h"
#include "../../Core/SelectConfiguration.h"
#include <memory>

#if defined(_DEBUG)
    #define ENABLE_DEBUG_EXTENSIONS
#endif

namespace RenderCore { 
    extern char VersionString[];
    extern char BuildDateString[];
}

namespace RenderCore { namespace ImplVulkan
{
    using VulkanAPIFailure = Metal_Vulkan::VulkanAPIFailure;

	std::unique_ptr<IAnnotator> CreateAnnotator(IDevice& device);

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
        #if defined(ENABLE_DEBUG_EXTENSIONS)
            , VK_EXT_DEBUG_REPORT_EXTENSION_NAME
        #endif
	};

	static const char* s_deviceExtensions[] =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

    #if defined(ENABLE_DEBUG_EXTENSIONS)
	    static const char* s_instanceLayers[] =
	    {
			// "VK_LAYER_LUNARG_api_dump",
			"VK_LAYER_LUNARG_assistant_layer",
			"VK_LAYER_LUNARG_core_validation",
			// "VK_LAYER_LUNARG_device_simulation",
			// "VK_LAYER_LUNARG_monitor",
			"VK_LAYER_LUNARG_object_tracker",
			"VK_LAYER_LUNARG_parameter_validation",
			// "VK_LAYER_LUNARG_screenshot",
			"VK_LAYER_LUNARG_standard_validation"

		    "VK_LAYER_LUNARG_device_limits",
		    "VK_LAYER_LUNARG_draw_state",
		    "VK_LAYER_LUNARG_image",
		    "VK_LAYER_LUNARG_mem_tracker",
		    "VK_LAYER_LUNARG_object_tracker",
		    "VK_LAYER_LUNARG_param_checker",
		    "VK_LAYER_LUNARG_swapchain",

			// "VK_LAYER_LUNARG_vktrace"

		    "VK_LAYER_GOOGLE_threading",
			"VK_LAYER_GOOGLE_unique_objects",
            // "VK_LAYER_RENDERDOC_Capture",

			// "VK_LAYER_NV_optimus",
	    };

	    static const char* s_deviceLayers[] =
	    {
			// "VK_LAYER_LUNARG_api_dump",
			"VK_LAYER_LUNARG_assistant_layer",
			"VK_LAYER_LUNARG_core_validation",
			// "VK_LAYER_LUNARG_device_simulation",
			// "VK_LAYER_LUNARG_monitor",
			"VK_LAYER_LUNARG_object_tracker",
			"VK_LAYER_LUNARG_parameter_validation",
			// "VK_LAYER_LUNARG_screenshot",
			"VK_LAYER_LUNARG_standard_validation"

		    "VK_LAYER_LUNARG_device_limits",
		    "VK_LAYER_LUNARG_draw_state",
		    "VK_LAYER_LUNARG_image",
		    "VK_LAYER_LUNARG_mem_tracker",
		    "VK_LAYER_LUNARG_object_tracker",
		    "VK_LAYER_LUNARG_param_checker",
		    "VK_LAYER_LUNARG_swapchain",

			// "VK_LAYER_LUNARG_vktrace"

		    "VK_LAYER_GOOGLE_threading",
			"VK_LAYER_GOOGLE_unique_objects",
            // "VK_LAYER_RENDERDOC_Capture",

			// "VK_LAYER_NV_optimus",
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

        static VkDebugReportCallbackEXT msg_callback;
        static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback( VkFlags msgFlags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject, size_t location, int32_t msgCode, const char *pLayerPrefix, const char *pMsg, void *pUserData )
        {
	        (void)msgFlags; (void)objType; (void)srcObject; (void)location; (void)pUserData; (void)msgCode;
            Log(Warning) << pLayerPrefix << ": " << pMsg << std::endl;
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
    #else
        static void debug_init(VkInstance instance) {}
        static void debug_destroy(VkInstance instance) {}
    #endif

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

		VkInstanceCreateInfo inst_info = {};
		inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		inst_info.pNext = NULL;
		inst_info.flags = 0;
		inst_info.pApplicationInfo = &app_info;
		inst_info.enabledExtensionCount = (uint32_t)dimof(s_instanceExtensions);
		inst_info.ppEnabledExtensionNames = s_instanceExtensions;

        #if defined(ENABLE_DEBUG_EXTENSIONS)
            auto availableLayers = EnumerateLayers();

            std::vector<const char*> filteredLayers;
            for (unsigned c=0; c<dimof(s_instanceLayers); ++c) {
                auto i = std::find_if(
                    availableLayers.begin(), availableLayers.end(),
                    [c](VkLayerProperties layer) { return XlEqString(layer.layerName, s_instanceLayers[c]); });
                if (i != availableLayers.end())
                    filteredLayers.push_back(s_instanceLayers[c]);
            }

            inst_info.enabledLayerCount = (uint32_t)filteredLayers.size();
		    inst_info.ppEnabledLayerNames = AsPointer(filteredLayers.begin());
        #else
            inst_info.enabledLayerCount = 0;
            inst_info.ppEnabledLayerNames = nullptr;
        #endif

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

				Log(Verbose)
					<< "Selecting physical device (" << props.deviceName 
					<< "). API Version: (" << props.apiVersion 
					<< "). Driver version: (" << props.driverVersion 
					<< "). Type: (" << AsString(props.deviceType) << ")"
					<< std::endl;
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
		device_info.enabledExtensionCount = (uint32_t)dimof(s_deviceExtensions);
		device_info.ppEnabledExtensionNames = s_deviceExtensions;
		device_info.pEnabledFeatures = nullptr;

        #if defined(ENABLE_DEBUG_EXTENSIONS)
            auto availableLayers = EnumerateLayers();
            std::vector<const char*> filteredLayers;
            for (unsigned c=0; c<dimof(s_deviceLayers); ++c) {
                auto i = std::find_if(
                    availableLayers.begin(), availableLayers.end(),
                    [c](VkLayerProperties layer) { return XlEqString(layer.layerName, s_deviceLayers[c]); });
                if (i != availableLayers.end())
                    filteredLayers.push_back(s_deviceLayers[c]);
            }

		    device_info.enabledLayerCount = (uint32)filteredLayers.size();
		    device_info.ppEnabledLayerNames = AsPointer(filteredLayers.begin());
        #else
            device_info.enabledLayerCount = 0;
		    device_info.ppEnabledLayerNames = nullptr;
        #endif

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

	class EventBasedTracker : public Metal_Vulkan::IAsyncTracker
	{
	public:
		virtual Marker GetConsumerMarker() const;
		virtual Marker GetProducerMarker() const;

		void IncrementProducerFrame();
		void SetConsumerEndOfFrame(Metal_Vulkan::DeviceContext&);
		void UpdateConsumer();

		EventBasedTracker(Metal_Vulkan::ObjectFactory& factory, unsigned queueDepth);
		~EventBasedTracker();
	private:
		struct Tracker
		{
			VulkanUniquePtr<VkEvent> _event;
			Marker _frameMarker;
		};
		std::unique_ptr<Tracker[]> _trackers;
		unsigned _bufferCount;
		unsigned _producerBufferIndex;
		unsigned _consumerBufferIndex;
		bool _firstProducerFrame;
		Marker _currentProducerFrame;
		Marker _lastConsumerFrame;
		VkDevice _device;
	};

	auto EventBasedTracker::GetConsumerMarker() const -> Marker
	{
		return _lastConsumerFrame;
	}

	auto EventBasedTracker::GetProducerMarker() const -> Marker
	{
		return _currentProducerFrame;
	}

	void EventBasedTracker::SetConsumerEndOfFrame(Metal_Vulkan::DeviceContext& context)
	{
		// set the marker on the frame that has just finished --
		// Note that if we use VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, this is only going 
		// to be tracking rendering command progress -- not compute shaders!
		// Is ALL_COMMANDS fine?
		if (_trackers[_producerBufferIndex]._frameMarker != Marker_Invalid)
			context.GetActiveCommandList().SetEvent(_trackers[_producerBufferIndex]._event.get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}

	void EventBasedTracker::IncrementProducerFrame()
	{
		if (!_firstProducerFrame) {
			++_currentProducerFrame;
			_producerBufferIndex = (_producerBufferIndex + 1) % _bufferCount; 
			// If we start "eating our tail" (ie, we don't have enough buffers to support the queued GPU frames, we will get an assert
			// here... This needs to be replaced with something more robust
			// Probably higher level code should prevent the CPU from getting too far ahead of the GPU, so as to guarantee we never
			// end up eating our tail here...
			while (_trackers[_producerBufferIndex]._frameMarker != Marker_Invalid) {
				Threading::YieldTimeSlice();
				UpdateConsumer();
			}
			assert(_trackers[_producerBufferIndex]._frameMarker == Marker_Invalid); 
		}
		
		_firstProducerFrame = false;
		_trackers[_producerBufferIndex]._frameMarker = _currentProducerFrame;
	}

	void EventBasedTracker::UpdateConsumer()
	{
		for (;;) {
			if (_trackers[_consumerBufferIndex]._frameMarker == Marker_Invalid)
				break;
			auto status = vkGetEventStatus(_device, _trackers[_consumerBufferIndex]._event.get());
			if (status == VK_EVENT_RESET)
				break;
			assert(status == VK_EVENT_SET);

			auto res = vkResetEvent(_device, _trackers[_consumerBufferIndex]._event.get());
			assert(res == VK_SUCCESS); (void)res;

			assert(_trackers[_consumerBufferIndex]._frameMarker > _lastConsumerFrame);
			_lastConsumerFrame = _trackers[_consumerBufferIndex]._frameMarker;
			_trackers[_consumerBufferIndex]._frameMarker = Marker_Invalid;
			_consumerBufferIndex = (_consumerBufferIndex + 1) % _bufferCount;
		}
	}

	EventBasedTracker::EventBasedTracker(Metal_Vulkan::ObjectFactory& factory, unsigned queueDepth)
	{
		assert(queueDepth > 0);
		_trackers = std::make_unique<Tracker[]>(queueDepth);
		for (unsigned q = 0; q < queueDepth; ++q) {
			_trackers[q]._event = factory.CreateEvent();
			_trackers[q]._frameMarker = Marker_Invalid;
		}
		_currentProducerFrame = 1;
		_bufferCount = queueDepth;
		_consumerBufferIndex = 0;
		_producerBufferIndex = 0;
		_lastConsumerFrame = 0;
		_firstProducerFrame = true;
		_device = factory.GetDevice().get();
	}

	EventBasedTracker::~EventBasedTracker() {}

    void Device::DoSecondStageInit(VkSurfaceKHR surface)
    {
        if (!_underlying) {
			_physDev = SelectPhysicalDeviceForRendering(_instance.get(), surface);
			_underlying = CreateUnderlyingDevice(_physDev);
			_objectFactory = Metal_Vulkan::ObjectFactory(_physDev._dev, _underlying);

			// Set up the object factory with a default destroyer that tracks the current
			// GPU frame progress
			auto frameTracker = std::make_shared<EventBasedTracker>(_objectFactory, 5);
			auto destroyer = _objectFactory.CreateMarkerTrackingDestroyer(frameTracker);
			_objectFactory.SetDefaultDestroyer(destroyer);
            Metal_Vulkan::SetDefaultObjectFactory(&_objectFactory);

            _pools._mainDescriptorPool = Metal_Vulkan::DescriptorPool(_objectFactory, frameTracker);
            _pools._mainPipelineCache = _objectFactory.CreatePipelineCache();
            _pools._dummyResources = Metal_Vulkan::DummyResources(_objectFactory);

			auto tempBufferSpace = std::make_unique<Metal_Vulkan::TemporaryBufferSpace>(_objectFactory, frameTracker);

            _graphicsPipelineLayout = std::make_shared<Metal_Vulkan::PipelineLayout>(
                _objectFactory, "xleres/System/RootSignature.cfg",
                VK_SHADER_STAGE_ALL_GRAPHICS);

            _computePipelineLayout = std::make_shared<Metal_Vulkan::PipelineLayout>(
                _objectFactory, "xleres/System/RootSignatureCS.cfg",
                VK_SHADER_STAGE_COMPUTE_BIT);

            _foregroundPrimaryContext = std::make_shared<ThreadContextVulkan>(
				shared_from_this(), 
				GetQueue(_underlying.get(), _physDev._renderingQueueFamily),
                *_graphicsPipelineLayout,
                *_computePipelineLayout,
                Metal_Vulkan::CommandPool(_objectFactory, _physDev._renderingQueueFamily, false, frameTracker),
				Metal_Vulkan::CommandBufferType::Primary,
				std::move(tempBufferSpace));
			_foregroundPrimaryContext->AttachDestroyer(destroyer);
			_foregroundPrimaryContext->SetGPUTracker(frameTracker);
		}
    }

    struct SwapChainProperties
    {
        VkFormat                        _fmt;
        VkExtent2D                      _extent;
        uint32_t                        _desiredNumberOfImages;
        VkSurfaceTransformFlagBitsKHR   _preTransform;
        VkPresentModeKHR                _presentMode;
    };

    static SwapChainProperties DecideSwapChainProperties(
        VkPhysicalDevice phyDev, VkSurfaceKHR surface,
        unsigned requestedWidth, unsigned requestedHeight)
    {
        SwapChainProperties result;

        // The following is based on the "initswapchain" sample from the vulkan SDK
        auto fmts = GetSurfaceFormats(phyDev, surface);
        assert(!fmts.empty());  // expecting at least one

        // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
        // the surface has no preferred format.  Otherwise, at least one
        // supported format will be returned.
        result._fmt = 
            (fmts.empty() || (fmts.size() == 1 && fmts[0].format == VK_FORMAT_UNDEFINED)) 
            ? VK_FORMAT_B8G8R8A8_UNORM : fmts[0].format;

        VkSurfaceCapabilitiesKHR surfCapabilities;
        auto res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phyDev, surface, &surfCapabilities);
        assert(res == VK_SUCCESS); (void)res;

        auto presentModes = GetPresentModes(phyDev, surface);
        result._presentMode = SelectPresentMode(MakeIteratorRange(presentModes));

        // width and height are either both -1, or both not -1.
        if (surfCapabilities.currentExtent.width == (uint32_t)-1) {
            // If the surface size is undefined, the size is set to
            // the size of the images requested.
            result._extent.width = requestedWidth;
            result._extent.height = requestedHeight;
        } else {
            // If the surface size is defined, the swap chain size must match
            result._extent = surfCapabilities.currentExtent;
        }
        
        // Determine the number of VkImage's to use in the swap chain (we desire to
        // own only 1 image at a time, besides the images being displayed and
        // queued for display):
        result._desiredNumberOfImages = surfCapabilities.minImageCount + 1;
        if (surfCapabilities.maxImageCount > 0)
            result._desiredNumberOfImages = std::min(result._desiredNumberOfImages, surfCapabilities.maxImageCount);

        // setting "preTransform" to current transform... but clearing out other bits if the identity bit is set
        result._preTransform = 
            (surfCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfCapabilities.currentTransform;
        return result;
    }

    static VulkanSharedPtr<VkSwapchainKHR> CreateUnderlyingSwapChain(
        VkDevice dev, VkSurfaceKHR  surface, 
        const SwapChainProperties& props)
    {
        // finally, fill in our SwapchainCreate structure
        VkSwapchainCreateInfoKHR swapChainInfo = {};
        swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapChainInfo.pNext = nullptr;
        swapChainInfo.surface = surface;
        swapChainInfo.minImageCount = props._desiredNumberOfImages;
        swapChainInfo.imageFormat = props._fmt;
        swapChainInfo.imageExtent.width = props._extent.width;
        swapChainInfo.imageExtent.height = props._extent.height;
        swapChainInfo.preTransform = props._preTransform;
        swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapChainInfo.imageArrayLayers = 1;
        swapChainInfo.presentMode = props._presentMode;
        swapChainInfo.oldSwapchain = nullptr;
        swapChainInfo.clipped = true;
        swapChainInfo.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapChainInfo.queueFamilyIndexCount = 0;
        swapChainInfo.pQueueFamilyIndices = nullptr;

        VkSwapchainKHR swapChainRaw = nullptr;
        auto res = vkCreateSwapchainKHR(dev, &swapChainInfo, Metal_Vulkan::g_allocationCallbacks, &swapChainRaw);
        VulkanSharedPtr<VkSwapchainKHR> result(
            swapChainRaw,
            [dev](VkSwapchainKHR chain) { vkDestroySwapchainKHR(dev, chain, Metal_Vulkan::g_allocationCallbacks); } );
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure while creating swap chain"));
        return result;
    }

    std::unique_ptr<IPresentationChain> Device::CreatePresentationChain(
		const void* platformValue, const PresentationChainDesc& desc)
    {
		auto surface = CreateSurface(_instance.get(), platformValue);
		DoSecondStageInit(surface.get());

        // double check to make sure our physical device is compatible with this surface
        VkBool32 supportsPresent = false;
		auto res = vkGetPhysicalDeviceSurfaceSupportKHR(
			_physDev._dev, _physDev._renderingQueueFamily, surface.get(), &supportsPresent);
		if (res != VK_SUCCESS || !supportsPresent) 
            Throw(::Exceptions::BasicLabel("Presentation surface is not compatible with selected physical device. This may occur if the wrong physical device is selected, and it cannot render to the output window."));
        
        auto finalChain = std::make_unique<PresentationChain>(
            _objectFactory, std::move(surface), VectorPattern<unsigned, 2>{desc._width, desc._height}, _physDev._renderingQueueFamily, platformValue);

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
        // Note that when we do the second stage init through this path,
        // we will not verify the selected physical device against a
        // presentation surface.
        DoSecondStageInit();
		return _foregroundPrimaryContext;
    }

    std::unique_ptr<IThreadContext> Device::CreateDeferredContext()
    {
        // Note that when we do the second stage init through this path,
        // we will not verify the selected physical device against a
        // presentation surface.
        DoSecondStageInit();
		return std::make_unique<ThreadContextVulkan>(
            shared_from_this(), 
            nullptr, 
            *_graphicsPipelineLayout,
            *_computePipelineLayout,
            Metal_Vulkan::CommandPool(_objectFactory, _physDev._renderingQueueFamily, false, nullptr),
            Metal_Vulkan::CommandBufferType::Secondary, nullptr);
    }

	ResourcePtr Device::CreateResource(
		const ResourceDesc& desc,
		const std::function<SubResourceInitData(SubResourceId)>& initData)
	{
		return Metal_Vulkan::CreateResource(_objectFactory, desc, initData);
	}

	FormatCapability    Device::QueryFormatCapability(Format format, BindFlag::BitField bindingType)
	{
		return FormatCapability::Supported;
	}

    static const char* s_underlyingApi = "Vulkan";
        
    DeviceDesc Device::GetDesc()
    {
        return DeviceDesc{s_underlyingApi, VersionString, BuildDateString};
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////

	void*   DeviceVulkan::QueryInterface(size_t guid)
	{
		if (guid == typeid(IDeviceVulkan).hash_code())
			return (IDeviceVulkan*)this;
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

    const std::shared_ptr<Metal_Vulkan::PipelineLayout>& DeviceVulkan::ShareGraphicsPipelineLayout()
    {
        return _graphicsPipelineLayout;
    }

    const std::shared_ptr<Metal_Vulkan::PipelineLayout>& DeviceVulkan::ShareComputePipelineLayout()
    {
        return _computePipelineLayout;
    }

	DeviceVulkan::DeviceVulkan() { }
	DeviceVulkan::~DeviceVulkan() { }

    //////////////////////////////////////////////////////////////////////////////////////////////////

    void            PresentationChain::Resize(unsigned newWidth, unsigned newHeight)
    {
        // We need to destroy and recreate the presentation chain here.
        auto props = DecideSwapChainProperties(_factory->GetPhysicalDevice(), _surface.get(), newWidth, newHeight);
        if (newWidth == _bufferDesc._width && newHeight == _bufferDesc._height)
            return;

        // We can't delete the old swap chain while the device is using it. The easiest
        // way to get around this is to just synchronize with the GPU here.
        // Since a resize is uncommon, this should not be a issue. It might be better to wait for
        // a queue idle -- but we don't have access to the VkQueue from here.
        vkDeviceWaitIdle(_device.get());
        _swapChain.reset();
        _images.clear();

        _swapChain = CreateUnderlyingSwapChain(_device.get(), _surface.get(), props);
        _bufferDesc = TextureDesc::Plain2D(props._extent.width, props._extent.height, Metal_Vulkan::AsFormat(props._fmt));    

        *_desc = { _bufferDesc._width, _bufferDesc._height, _bufferDesc._format, _bufferDesc._samples };

        BuildImages();
    }

    const std::shared_ptr<PresentationChainDesc>& PresentationChain::GetDesc() const
    {
		return _desc;
    }

    Metal_Vulkan::RenderTargetView* PresentationChain::AcquireNextImage()
    {
        _activePresentSync = (_activePresentSync+1) % dimof(_presentSyncs);
        auto& sync = _presentSyncs[_activePresentSync];
		if (sync._fenceHasBeenQueued) {
			auto fence = sync._presentFence.get();
			auto res = vkWaitForFences(_device.get(), 1, &fence, true, UINT64_MAX);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failure while waiting for presentation fence"));
			res = vkResetFences(_device.get(), 1, &fence);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failure while resetting presentation fence"));
			sync._fenceHasBeenQueued = false;
		}

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
        auto res = vkAcquireNextImageKHR(
            _device.get(), _swapChain.get(), 
            timeout,
            sync._onAcquireComplete.get(), VK_NULL_HANDLE,
            &nextImageIndex);
        _activeImageIndex = nextImageIndex;

        // TODO: Deal with the VK_SUBOPTIMAL_KHR and VK_ERROR_OUT_OF_DATE_KHR
        // return codes
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure during acquire next image"));

        return &_images[_activeImageIndex]._rtv;
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
		_activeImageIndex = ~0x0u;
	}

    void PresentationChain::BuildImages()
    {
        auto images = GetImages(_device.get(), _swapChain.get());
        _images.reserve(images.size());
        for (auto& i:images) {
            TextureViewDesc window{
                _bufferDesc._format, 
                TextureViewDesc::SubResourceRange{0, _bufferDesc._mipCount},
				TextureViewDesc::SubResourceRange{0, _bufferDesc._arrayCount},
				_bufferDesc._dimensionality};
            auto resDesc = CreateDesc(
                BindFlag::RenderTarget|BindFlag::ShaderResource, 0u, GPUAccess::Read|GPUAccess::Write, 
                _bufferDesc, "presentationimage");
            auto resPtr = ResourcePtr(
				(RenderCore::Resource*)new Metal_Vulkan::Resource(i, resDesc),
				[](RenderCore::Resource* res) { delete (Metal_Vulkan::Resource*)res; });
            _images.emplace_back(
                Image { i, Metal_Vulkan::RenderTargetView(*_factory, resPtr, window) });
        }
    }

    PresentationChain::PresentationChain(
		const Metal_Vulkan::ObjectFactory& factory,
        VulkanSharedPtr<VkSurfaceKHR> surface, 
		VectorPattern<unsigned, 2> extent,
		unsigned queueFamilyIndex,
        const void* platformValue)
    : _surface(std::move(surface))
    , _device(factory.GetDevice())
    , _factory(&factory)
    , _platformValue(platformValue)
	, _primaryBufferPool(factory, queueFamilyIndex, true, nullptr)
    {
        _activeImageIndex = ~0x0u;
        auto props = DecideSwapChainProperties(factory.GetPhysicalDevice(), _surface.get(), extent[0], extent[1]);
        _swapChain = CreateUnderlyingSwapChain(_device.get(), _surface.get(), props);

        _bufferDesc = TextureDesc::Plain2D(props._extent.width, props._extent.height, Metal_Vulkan::AsFormat(props._fmt));
		_desc = std::make_shared<PresentationChainDesc>(
            _bufferDesc._width, _bufferDesc._height,
            _bufferDesc._format, _bufferDesc._samples);

        // We need to get pointers to each image and build the synchronization semaphores
        BuildImages();

        // Create the synchronisation primitives
        // This pattern is similar to the "Hologram" sample in the Vulkan SDK
        for (unsigned c=0; c<dimof(_presentSyncs); ++c) {
            _presentSyncs[c]._onCommandBufferComplete = factory.CreateSemaphore();
			_presentSyncs[c]._onCommandBufferComplete2 = factory.CreateSemaphore();
            _presentSyncs[c]._onAcquireComplete = factory.CreateSemaphore();
            _presentSyncs[c]._presentFence = factory.CreateFence(0);
			_presentSyncs[c]._fenceHasBeenQueued = false;
        }
		for (unsigned c = 0; c<dimof(_primaryBuffers); ++c)
			_primaryBuffers[c] = _primaryBufferPool.Allocate(Metal_Vulkan::CommandBufferType::Primary);
        _activePresentSync = 0;
    }

#if 0
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
#endif

    void PresentationChain::SetInitialLayout(
        const Metal_Vulkan::ObjectFactory& factory, 
        Metal_Vulkan::CommandPool& cmdPool, VkQueue queue)
    {
#if 0
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
		// Metal_Vulkan::SetImageLayout(
        //     cmd.get(), Metal_Vulkan::UnderlyingResourcePtr(_depthStencilResource).get()->GetImage(),
        //     VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,
        //     VK_IMAGE_LAYOUT_UNDEFINED,
        //     VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, 1);

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
#endif
    }

    PresentationChain::~PresentationChain()
    {
		_images.clear();
		_swapChain.reset();
		_device.reset();
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////

    render_dll_export std::shared_ptr<IDevice>    CreateDevice()
    {
        return std::make_shared<DeviceVulkan>();
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////

	ResourcePtr    ThreadContext::BeginFrame(IPresentationChain& presentationChain)
	{
		if (_gpuTracker)
			_gpuTracker->IncrementProducerFrame();

		PresentationChain* swapChain = checked_cast<PresentationChain*>(&presentationChain);
		auto nextImage = swapChain->AcquireNextImage();
		{
			auto cmdList = swapChain->SharePrimaryBuffer();
			auto res = vkResetCommandBuffer(cmdList.get(), VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failure while resetting command buffer"));

			_metalContext->BeginCommandList(std::move(cmdList));
		}
        _metalContext->Bind(Metal_Vulkan::ViewportDesc(0.f, 0.f, (float)swapChain->GetBufferDesc()._width, (float)swapChain->GetBufferDesc()._height));
        return nextImage->GetResource();
	}

	void            ThreadContext::Present(IPresentationChain& chain)
	{
		auto* swapChain = checked_cast<PresentationChain*>(&chain);
		auto& syncs = swapChain->GetSyncs();

		//////////////////////////////////////////////////////////////////

		// 2 options for setting the event that allows objects from this frame to
		// be destroyed:
		//	* at the end of the main command buffer
		//	* in a separate command buffer that waits on a completion 
		//		semaphore from the main command buffer
		// 
		// Note that it's not a good idea to add the signal into command buffer for
		// the next frame, because it's possible that there is some overlap between
		// one frame's commands and the next.
		// However, if we signal the event from the main command buffer, we have a
		// problem where the event that triggers the destruction of the command buffer
		// is executed by the command buffer itself. That could create problems -- so
		// one possible solution is to retain the command buffer for an extra frame.
		const bool trackingSeparateCommandBuffer = false;

		{
			if (!trackingSeparateCommandBuffer)
				_gpuTracker->SetConsumerEndOfFrame(*_metalContext);

			auto mainCmdBuffer = _metalContext->ResolveCommandList();

			VkSubmitInfo submitInfo;
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.pNext = nullptr;

			VkSemaphore waitSema[] = { syncs._onAcquireComplete.get() };
			VkSemaphore signalSema[] = { syncs._onCommandBufferComplete.get(), syncs._onCommandBufferComplete2.get() };
			VkCommandBuffer rawCmdBuffers[] = { mainCmdBuffer->GetUnderlying().get() };
			VkPipelineStageFlags stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			submitInfo.waitSemaphoreCount = dimof(waitSema);
			submitInfo.pWaitSemaphores = waitSema;
			submitInfo.signalSemaphoreCount = trackingSeparateCommandBuffer ? 2 : 1;
			submitInfo.pSignalSemaphores = signalSema;
			submitInfo.pWaitDstStageMask = &stage;
			submitInfo.commandBufferCount = dimof(rawCmdBuffers);
			submitInfo.pCommandBuffers = rawCmdBuffers;
		
			auto res = vkQueueSubmit(_queue, 1, &submitInfo, syncs._presentFence.get());
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failure while queuing semaphore signal"));

			syncs._fenceHasBeenQueued = true;
		}

		// Converting a semphore to an event (so we can query the progress from the CPU)
		if (constant_expression<trackingSeparateCommandBuffer>::result() && _gpuTracker) {
			_metalContext->BeginCommandList();
			_gpuTracker->SetConsumerEndOfFrame(*_metalContext);
			auto cmdBuffer = _metalContext->ResolveCommandList();

			VkSubmitInfo submitInfo;
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.pNext = nullptr;

			VkSemaphore waitSema[] = { syncs._onCommandBufferComplete2.get() };
			VkCommandBuffer rawCmdBuffers[] = { cmdBuffer->GetUnderlying().get() };
			VkPipelineStageFlags stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			submitInfo.waitSemaphoreCount = dimof(waitSema);
			submitInfo.pWaitSemaphores = waitSema;
			submitInfo.signalSemaphoreCount = 0;
			submitInfo.pSignalSemaphores = nullptr;
			submitInfo.pWaitDstStageMask = &stage;
			submitInfo.commandBufferCount = dimof(rawCmdBuffers);
			submitInfo.pCommandBuffers = rawCmdBuffers;

			auto res = vkQueueSubmit(_queue, 1, &submitInfo, VK_NULL_HANDLE);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failure while queuing frame complete signal"));
		}

		if (_gpuTracker) _gpuTracker->UpdateConsumer();
		// Maybe we should reset our finished command buffers here?
		if (_destrQueue) _destrQueue->Flush();
		_globalPools->_mainDescriptorPool.FlushDestroys();
		_renderingCommandPool.FlushDestroys();
		_tempBufferSpace->FlushDestroys();

		//////////////////////////////////////////////////////////////////
		// Finally, we can queue the present
		//		-- do it here to allow it to run in parallel as much as possible
		swapChain->PresentToQueue(_queue);
	}

    bool ThreadContext::IsImmediate() const
    {
        return _queue != nullptr;
    }

    auto ThreadContext::GetStateDesc() const -> ThreadContextStateDesc
    {
        const auto& view = _metalContext->GetBoundViewport();
        return ThreadContextStateDesc { {(unsigned)view.Width, (unsigned)view.Height}, _frameId };
    }

	void ThreadContext::InvalidateCachedState() const {}

	IAnnotator& ThreadContext::GetAnnotator()
	{
		if (!_annotator) {
			auto d = _device.lock();
			assert(d);
			_annotator = CreateAnnotator(*d);
		}
		return *_annotator;
	}

	void ThreadContext::SetGPUTracker(const std::shared_ptr<EventBasedTracker>& tracker) { _gpuTracker = tracker; }
	void ThreadContext::AttachDestroyer(const std::shared_ptr<Metal_Vulkan::IDestructionQueue>& queue) { _destrQueue = queue; }

    ThreadContext::ThreadContext(
		std::shared_ptr<Device> device,
		VkQueue queue,
        Metal_Vulkan::PipelineLayout& graphicsPipelineLayout,
        Metal_Vulkan::PipelineLayout& computePipelineLayout,
        Metal_Vulkan::CommandPool&& cmdPool,
		Metal_Vulkan::CommandBufferType cmdBufferType,
		std::unique_ptr<Metal_Vulkan::TemporaryBufferSpace>&& tempBufferSpace)
    : _device(device)
	, _frameId(0)
    , _renderingCommandPool(std::move(cmdPool))
	, _tempBufferSpace(std::move(tempBufferSpace))
	, _metalContext(
		std::make_shared<Metal_Vulkan::DeviceContext>(
			device->GetObjectFactory(), device->GetGlobalPools(), 
            graphicsPipelineLayout, computePipelineLayout,
            _renderingCommandPool, cmdBufferType, *_tempBufferSpace))
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

    void ThreadContext::IncrFrameId()
    {
        ++_frameId;
    }

    void*   ThreadContextVulkan::QueryInterface(size_t guid)
    {
        if (guid == typeid(IThreadContextVulkan).hash_code()) { return (IThreadContextVulkan*)this; }
        return nullptr;
    }

    const std::shared_ptr<Metal_Vulkan::DeviceContext>& ThreadContextVulkan::GetMetalContext()
    {
        return _metalContext;
    }

    ThreadContextVulkan::ThreadContextVulkan(
		std::shared_ptr<Device> device,
		VkQueue queue,
        Metal_Vulkan::PipelineLayout& graphicsPipelineLayout,
        Metal_Vulkan::PipelineLayout& computePipelineLayout,
        Metal_Vulkan::CommandPool&& cmdPool,
		Metal_Vulkan::CommandBufferType cmdBufferType,
		std::unique_ptr<Metal_Vulkan::TemporaryBufferSpace>&& tempBufferSpace)
    : ThreadContext(
        std::move(device), queue, 
        graphicsPipelineLayout, computePipelineLayout, 
        std::move(cmdPool), cmdBufferType,
		std::move(tempBufferSpace))
    {}

	ThreadContextVulkan::~ThreadContextVulkan() {}


	void RegisterCreation()
	{
		static_constructor<&RegisterCreation>::c;
		RegisterDeviceCreationFunction(UnderlyingAPI::Vulkan, &CreateDevice);
	}
}}

namespace RenderCore
{
	IDeviceVulkan::~IDeviceVulkan() {}
	IThreadContextVulkan::~IThreadContextVulkan() {}
}
