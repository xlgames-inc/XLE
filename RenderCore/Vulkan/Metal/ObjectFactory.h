// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "Resource.h"
#include "IncludeVulkan.h"
#include "../../IDevice_Forward.h"
#include "../../../Utility/IteratorUtils.h"
#include "../../../Core/Types.h"

#undef CreateSemaphore

namespace RenderCore { namespace Metal_Vulkan
{
    class IDestructionQueue
    {
    public:
        virtual void    Destroy(VkCommandPool) = 0;
        virtual void    Destroy(VkSemaphore) = 0;
        virtual void    Destroy(VkDeviceMemory) = 0;
        virtual void    Destroy(VkRenderPass) = 0;
        virtual void    Destroy(VkImage) = 0;
        virtual void    Destroy(VkImageView) = 0;
        virtual void    Destroy(VkFramebuffer) = 0;
        virtual void    Destroy(VkShaderModule) = 0;
        virtual void    Destroy(VkDescriptorSetLayout) = 0;
        virtual void    Destroy(VkDescriptorPool) = 0;
        virtual void    Destroy(VkPipeline) = 0;
        virtual void    Destroy(VkPipelineCache) = 0;
        virtual void    Destroy(VkPipelineLayout) = 0;
        virtual void    Destroy(VkBuffer) = 0;
        virtual void    Destroy(VkFence) = 0;
        virtual void    Destroy(VkSampler) = 0;
        virtual void    Flush() = 0;
        virtual ~IDestructionQueue();
    };

	class ObjectFactory
	{
	public:
		VkPhysicalDevice GetPhysicalDevice() const { return _physDev; }
		const VulkanSharedPtr<VkDevice>& GetDevice() const { return _device; }

        VulkanUniquePtr<VkCommandPool> CreateCommandPool(
            unsigned queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) const;

        VulkanUniquePtr<VkSemaphore> CreateSemaphore(
            VkSemaphoreCreateFlags flags = 0) const;

        VulkanUniquePtr<VkDeviceMemory> AllocateMemory(
            VkDeviceSize allocationSize, unsigned memoryTypeIndex) const;

        VulkanUniquePtr<VkRenderPass> CreateRenderPass(
            const VkRenderPassCreateInfo& createInfo) const;

        VulkanUniquePtr<VkImage> CreateImage(
            const VkImageCreateInfo& createInfo) const;

        VulkanUniquePtr<VkImageView> CreateImageView(
            const VkImageViewCreateInfo& createInfo) const;

        VulkanUniquePtr<VkFramebuffer> CreateFramebuffer(
            const VkFramebufferCreateInfo& createInfo) const;

        VulkanUniquePtr<VkShaderModule> CreateShaderModule(
            const void* byteCode, size_t size,
            VkShaderModuleCreateFlags flags = 0) const;

        VulkanUniquePtr<VkPipeline> CreateGraphicsPipeline(
            VkPipelineCache pipelineCache,
            const VkGraphicsPipelineCreateInfo& createInfo) const;

        VulkanUniquePtr<VkPipelineCache> CreatePipelineCache(
            const void* initialData = nullptr, size_t initialDataSize = 0,
            VkPipelineCacheCreateFlags flags = 0) const;

        VulkanUniquePtr<VkDescriptorSetLayout> CreateDescriptorSetLayout(
            IteratorRange<const VkDescriptorSetLayoutBinding*> bindings) const;

        VulkanUniquePtr<VkDescriptorPool> CreateDescriptorPool(
            const VkDescriptorPoolCreateInfo& createInfo) const;

        VulkanUniquePtr<VkPipelineLayout> CreatePipelineLayout(
            IteratorRange<const VkDescriptorSetLayout*> setLayouts,
            IteratorRange<const VkPushConstantRange*> pushConstants = IteratorRange<const VkPushConstantRange*>(),
            VkPipelineLayoutCreateFlags flags = 0) const;

        VulkanUniquePtr<VkBuffer> CreateBuffer(
            const VkBufferCreateInfo& createInfo) const;

        VulkanUniquePtr<VkFence> CreateFence(VkFenceCreateFlags flags = 0) const;

        VulkanUniquePtr<VkSampler> CreateSampler(const VkSamplerCreateInfo& createInfo) const;

		unsigned FindMemoryType(
            VkFlags memoryTypeBits, 
            VkMemoryPropertyFlags requirementsMask = 0) const;
		VkFormatProperties GetFormatProperties(VkFormat fmt) const;

        void FlushDestructionQueue() const;

		ObjectFactory(VkPhysicalDevice physDev, VulkanSharedPtr<VkDevice> device);
        ObjectFactory(IDevice*);
        ObjectFactory(Underlying::Resource&);
		ObjectFactory();
		~ObjectFactory();

	private:
		VkPhysicalDeviceMemoryProperties _memProps;
        VkPhysicalDevice            _physDev;
		VulkanSharedPtr<VkDevice>   _device;

        std::shared_ptr<IDestructionQueue> _destruction;
	};

    const ObjectFactory& GetDefaultObjectFactory();
    void SetDefaultObjectFactory(const ObjectFactory*);

    extern const VkAllocationCallbacks* g_allocationCallbacks;
}}
