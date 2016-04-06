// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "IncludeVulkan.h"
#include "../../../Utility/IteratorUtils.h"
#include "../../../Core/Types.h"

#undef CreateSemaphore

namespace RenderCore { namespace Metal_Vulkan
{
	class ObjectFactory
	{
	public:
		VkPhysicalDevice GetPhysicalDevice() const { return _physDev; }
		const VulkanSharedPtr<VkDevice>& GetDevice() const { return _device; }

        VulkanSharedPtr<VkCommandPool> CreateCommandPool(
            unsigned queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) const;

        VulkanSharedPtr<VkSemaphore> CreateSemaphore(
            VkSemaphoreCreateFlags flags = 0) const;

        VulkanSharedPtr<VkDeviceMemory> AllocateMemory(
            VkDeviceSize allocationSize, unsigned memoryTypeIndex) const;

        VulkanSharedPtr<VkRenderPass> CreateRenderPass(
            const VkRenderPassCreateInfo& createInfo) const;

        VulkanSharedPtr<VkImage> CreateImage(
            const VkImageCreateInfo& createInfo) const;

        VulkanSharedPtr<VkImageView> CreateImageView(
            const VkImageViewCreateInfo& createInfo) const;

        VulkanSharedPtr<VkFramebuffer> CreateFramebuffer(
            const VkFramebufferCreateInfo& createInfo) const;

        VulkanSharedPtr<VkShaderModule> CreateShaderModule(
            const void* byteCode, size_t size,
            VkShaderModuleCreateFlags flags = 0) const;

        VulkanSharedPtr<VkDescriptorSetLayout> CreateDescriptorSetLayout(
            IteratorRange<const VkDescriptorSetLayoutBinding*> bindings) const;

		unsigned FindMemoryType(
            VkFlags memoryTypeBits, 
            VkMemoryPropertyFlags requirementsMask = 0) const;

		ObjectFactory(VkPhysicalDevice physDev, VulkanSharedPtr<VkDevice> device);
		ObjectFactory();
		// ~ObjectFactory();

	private:
		VkPhysicalDeviceMemoryProperties _memProps;
        VkPhysicalDevice            _physDev;
		VulkanSharedPtr<VkDevice>   _device;
	};

    const ObjectFactory& GetDefaultObjectFactory();
    void SetDefaultObjectFactory(const ObjectFactory*);

    extern const VkAllocationCallbacks* g_allocationCallbacks;
}}
