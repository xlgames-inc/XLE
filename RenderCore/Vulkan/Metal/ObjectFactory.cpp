// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ObjectFactory.h"
#include "../../../Core/Prefix.h"

namespace RenderCore { namespace Metal_Vulkan
{
    const VkAllocationCallbacks* g_allocationCallbacks = nullptr;

    VulkanUniquePtr<VkCommandPool> ObjectFactory::CreateCommandPool(
        unsigned queueFamilyIndex, VkCommandPoolCreateFlags flags) const
    {
        VkCommandPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.queueFamilyIndex = queueFamilyIndex;
		createInfo.flags = flags;

        auto dev = _device.get();
		VkCommandPool rawPool = nullptr;
		auto res = vkCreateCommandPool(dev, &createInfo, g_allocationCallbacks, &rawPool);
		auto pool = VulkanUniquePtr<VkCommandPool>(
			rawPool,
			[dev](VkCommandPool pool) { vkDestroyCommandPool(dev, pool, g_allocationCallbacks); });
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while creating command pool"));
        return std::move(pool);
    }

    VulkanUniquePtr<VkSemaphore> ObjectFactory::CreateSemaphore(
        VkSemaphoreCreateFlags flags) const
    {
        VkSemaphoreCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = flags;

        auto dev = _device.get();
        VkSemaphore rawPtr = nullptr;
        auto res = vkCreateSemaphore(
            dev, &createInfo,
            Metal_Vulkan::g_allocationCallbacks, &rawPtr);
        VulkanUniquePtr<VkSemaphore> result(
            rawPtr,
            [dev](VkSemaphore sem) { vkDestroySemaphore(dev, sem, g_allocationCallbacks); });
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure while creating Vulkan semaphore"));
        return std::move(result);
    }

    VulkanUniquePtr<VkDeviceMemory> ObjectFactory::AllocateMemory(
        VkDeviceSize allocationSize, unsigned memoryTypeIndex) const
    {
        VkMemoryAllocateInfo mem_alloc = {};
        mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mem_alloc.pNext = nullptr;
        mem_alloc.allocationSize = allocationSize;
        mem_alloc.memoryTypeIndex = memoryTypeIndex;

        auto dev = _device.get();
        VkDeviceMemory rawMem = nullptr;
        auto res = vkAllocateMemory(dev, &mem_alloc, Metal_Vulkan::g_allocationCallbacks, &rawMem);
        auto mem = VulkanUniquePtr<VkDeviceMemory>(
            rawMem,
            [dev](VkDeviceMemory mem) { vkFreeMemory(dev, mem, Metal_Vulkan::g_allocationCallbacks); });
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failed while allocating device memory for image"));

        return std::move(mem);
    }

    VulkanUniquePtr<VkRenderPass> ObjectFactory::CreateRenderPass(
        const VkRenderPassCreateInfo& createInfo) const
    {
        auto dev = _device.get();
        VkRenderPass rawPtr = nullptr;
        auto res = vkCreateRenderPass(dev, &createInfo, Metal_Vulkan::g_allocationCallbacks, &rawPtr);
        auto renderPass = VulkanUniquePtr<VkRenderPass>(
            rawPtr,
            [dev](VkRenderPass pass) { vkDestroyRenderPass(dev, pass, Metal_Vulkan::g_allocationCallbacks ); });
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failure while creating render pass"));
        return std::move(renderPass);
    }

    VulkanUniquePtr<VkImage> ObjectFactory::CreateImage(
        const VkImageCreateInfo& createInfo) const
    {
        auto dev = _device.get();
        VkImage rawImage = nullptr;
        auto res = vkCreateImage(dev, &createInfo, Metal_Vulkan::g_allocationCallbacks, &rawImage);
        auto image = VulkanUniquePtr<VkImage>(
            rawImage,
            [dev](VkImage image) { vkDestroyImage(dev, image, Metal_Vulkan::g_allocationCallbacks); });
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failed while creating image"));
        return std::move(image);
    }

    VulkanUniquePtr<VkImageView> ObjectFactory::CreateImageView(
        const VkImageViewCreateInfo& createInfo) const
    {
        auto dev = _device.get();
        VkImageView viewRaw = nullptr;
        auto result = vkCreateImageView(dev, &createInfo, Metal_Vulkan::g_allocationCallbacks, &viewRaw);
        auto imageView = VulkanUniquePtr<VkImageView>(
            viewRaw,
            [dev](VkImageView view) { vkDestroyImageView(dev, view, Metal_Vulkan::g_allocationCallbacks); });
        if (result != VK_SUCCESS)
            Throw(VulkanAPIFailure(result, "Failed while creating depth stencil view of resource"));
        return std::move(imageView);
    }

    VulkanUniquePtr<VkFramebuffer> ObjectFactory::CreateFramebuffer(
        const VkFramebufferCreateInfo& createInfo) const
    {
        auto dev = _device.get();
        VkFramebuffer rawFB = nullptr;
        auto res = vkCreateFramebuffer(dev, &createInfo, Metal_Vulkan::g_allocationCallbacks, &rawFB);
        auto framebuffer = VulkanUniquePtr<VkFramebuffer>(
            rawFB,
            [dev](VkFramebuffer fb) { vkDestroyFramebuffer(dev, fb, Metal_Vulkan::g_allocationCallbacks); });
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failed while allocating frame buffer"));
        return std::move(framebuffer);
    }

    VulkanUniquePtr<VkShaderModule> ObjectFactory::CreateShaderModule(
        const void* byteCode, size_t size,
        VkShaderModuleCreateFlags flags) const
    {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = flags;
        createInfo.codeSize = size;
        createInfo.pCode = (const uint32_t*)byteCode;

        auto dev = _device.get();
        VkShaderModule rawShader = nullptr;
        auto res = vkCreateShaderModule(dev, &createInfo, Metal_Vulkan::g_allocationCallbacks, &rawShader);
        auto shader = VulkanUniquePtr<VkShaderModule>(
            rawShader,
            [dev](VkShaderModule shdr) { vkDestroyShaderModule(dev, shdr, Metal_Vulkan::g_allocationCallbacks); });
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failed while creating shader module"));
        return std::move(shader);
    }

    VulkanUniquePtr<VkDescriptorSetLayout> ObjectFactory::CreateDescriptorSetLayout(
        IteratorRange<const VkDescriptorSetLayoutBinding*> bindings) const
    {
        VkDescriptorSetLayoutCreateInfo createInfo = {};
        createInfo.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.bindingCount = (uint32)bindings.size();
        createInfo.pBindings = bindings.begin();

        auto dev = _device.get();
        VkDescriptorSetLayout rawLayout = nullptr;
        auto res = vkCreateDescriptorSetLayout(dev, &createInfo, g_allocationCallbacks, &rawLayout);
        auto shader = VulkanUniquePtr<VkDescriptorSetLayout>(
            rawLayout,
            [dev](VkDescriptorSetLayout layout) { vkDestroyDescriptorSetLayout(dev, layout, Metal_Vulkan::g_allocationCallbacks); });
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failed while creating descriptor set layout"));
        return std::move(shader);
    }

    VulkanUniquePtr<VkPipeline> ObjectFactory::CreateGraphicsPipeline(
        VkPipelineCache pipelineCache,
        const VkGraphicsPipelineCreateInfo& createInfo) const
    {
        auto dev = _device.get();
        VkPipeline rawPipeline = nullptr;
        auto res = vkCreateGraphicsPipelines(dev, pipelineCache, 1, &createInfo, g_allocationCallbacks, &rawPipeline);
        auto pipeline = VulkanUniquePtr<VkPipeline>(
            rawPipeline,
            [dev](VkPipeline pipeline) { vkDestroyPipeline(dev, pipeline, Metal_Vulkan::g_allocationCallbacks); });
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failed while creating graphics pipeline"));
        return std::move(pipeline);
    }

    VulkanUniquePtr<VkPipelineCache> ObjectFactory::CreatePipelineCache(
        const void* initialData, size_t initialDataSize,
        VkPipelineCacheCreateFlags flags) const
    {
        VkPipelineCacheCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.initialDataSize = initialDataSize;
        createInfo.pInitialData = initialData;
        createInfo.flags = flags;

        auto dev = _device.get();
        VkPipelineCache rawCache = nullptr;
        auto res = vkCreatePipelineCache(dev, &createInfo, g_allocationCallbacks, &rawCache);
        auto cache = VulkanUniquePtr<VkPipelineCache>(
            rawCache,
            [dev](VkPipelineCache cache) { vkDestroyPipelineCache(dev, cache, Metal_Vulkan::g_allocationCallbacks); });
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failed while creating descriptor set layout"));
        return std::move(cache);
    }

    VulkanUniquePtr<VkPipelineLayout> ObjectFactory::CreatePipelineLayout(
        IteratorRange<const VkDescriptorSetLayout*> setLayouts,
        IteratorRange<const VkPushConstantRange*> pushConstants,
        VkPipelineLayoutCreateFlags flags) const
    {
        VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
        pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pPipelineLayoutCreateInfo.pNext = nullptr;
        pPipelineLayoutCreateInfo.flags = flags;
        pPipelineLayoutCreateInfo.setLayoutCount = (uint32_t)setLayouts.size();
        pPipelineLayoutCreateInfo.pSetLayouts = setLayouts.begin();
        pPipelineLayoutCreateInfo.pushConstantRangeCount = (uint32_t)pushConstants.size();
        pPipelineLayoutCreateInfo.pPushConstantRanges = pushConstants.begin();

        auto dev = _device.get();
        VkPipelineLayout rawPipelineLayout = nullptr;
        auto res = vkCreatePipelineLayout(dev, &pPipelineLayoutCreateInfo, g_allocationCallbacks, &rawPipelineLayout);
        auto pipelineLayout = VulkanUniquePtr<VkPipelineLayout>(
            rawPipelineLayout,
            [dev](VkPipelineLayout layout) { vkDestroyPipelineLayout(dev, layout, g_allocationCallbacks); });
        if (res != VK_SUCCESS)
            Throw(VulkanAPIFailure(res, "Failed while creating descriptor set layout"));
        return std::move(pipelineLayout);
    }

    unsigned ObjectFactory::FindMemoryType(VkFlags memoryTypeBits, VkMemoryPropertyFlags requirementsMask) const
    {
        // Search memtypes to find first index with those properties
        for (uint32_t i=0; i<dimof(_memProps.memoryTypes); i++) {
            if ((memoryTypeBits & 1) == 1) {
                // Type is available, does it match user properties?
                if ((_memProps.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask)
                    return i;
            }
            memoryTypeBits >>= 1;
        }
        return ~0x0u;
    }

    ObjectFactory::ObjectFactory(VkPhysicalDevice physDev, VulkanSharedPtr<VkDevice> device)
    : _physDev(physDev), _device(device)
    {
        _memProps = {};
        vkGetPhysicalDeviceMemoryProperties(physDev, &_memProps);
    }

    ObjectFactory::ObjectFactory(IDevice*) {}
    ObjectFactory::ObjectFactory(Underlying::Resource&) {}

	ObjectFactory::ObjectFactory() {}
	// ObjectFactory::~ObjectFactory() {}


    static const ObjectFactory* s_defaultObjectFactory = nullptr;

    void SetDefaultObjectFactory(const ObjectFactory* factory)
    {
        s_defaultObjectFactory = factory;
    }

    const ObjectFactory& GetDefaultObjectFactory()
    {
        return *s_defaultObjectFactory;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

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

    VulkanAPIFailure::VulkanAPIFailure(VkResult res, const char message[])
        : Exceptions::BasicLabel("%s [%s, %i]", message, AsString(res), res) {}
}}

