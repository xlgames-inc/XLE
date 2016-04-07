// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "IncludeVulkan.h"
#include "../../../Utility/IteratorUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{
    class ObjectFactory;

    class CommandPool
	{
	public:
		enum class BufferType { Primary, Secondary };
		VulkanSharedPtr<VkCommandBuffer> Allocate(BufferType type);

		CommandPool(const Metal_Vulkan::ObjectFactory& factory, unsigned queueFamilyIndex);
		CommandPool();
		~CommandPool();
	private:
		VulkanSharedPtr<VkCommandPool> _pool;
		VulkanSharedPtr<VkDevice> _device;
	};

    class DescriptorPool
    {
    public:
        void Allocate(
            IteratorRange<VulkanUniquePtr<VkDescriptorSet>*> dst,
            IteratorRange<const VkDescriptorSetLayout*> layouts);

        DescriptorPool(const Metal_Vulkan::ObjectFactory& factory);
        DescriptorPool();
        ~DescriptorPool();
    private:
        VulkanSharedPtr<VkDescriptorPool> _pool;
		VulkanSharedPtr<VkDevice> _device;
    };

    class GlobalPools
    {
    public:
        CommandPool						    _renderingCommandPool;
        DescriptorPool                      _mainDescriptorPool;
        VulkanSharedPtr<VkPipelineCache>    _mainPipelineCache;
    };
}}
