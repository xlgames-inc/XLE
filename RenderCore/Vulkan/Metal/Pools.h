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

		void FlushDestroys();

		CommandPool(const Metal_Vulkan::ObjectFactory& factory, unsigned queueFamilyIndex);
		CommandPool();
		~CommandPool();
	private:
		VulkanSharedPtr<VkCommandPool> _pool;
		VulkanSharedPtr<VkDevice> _device;

		std::vector<VkCommandBuffer> _pendingDestroy;
	};

    class DescriptorPool
    {
    public:
        void Allocate(
            IteratorRange<VulkanUniquePtr<VkDescriptorSet>*> dst,
            IteratorRange<const VkDescriptorSetLayout*> layouts);

        void FlushDestroys();

        DescriptorPool(const Metal_Vulkan::ObjectFactory& factory);
        DescriptorPool();
        ~DescriptorPool();

        DescriptorPool(const DescriptorPool&) = delete;
        DescriptorPool& operator=(const DescriptorPool&) = delete;
        DescriptorPool(DescriptorPool&&) never_throws;
        DescriptorPool& operator=(DescriptorPool&&) never_throws;
    private:
        VulkanSharedPtr<VkDescriptorPool> _pool;
		VulkanSharedPtr<VkDevice> _device;

        std::vector<VkDescriptorSet> _pendingDestroy;
    };

    class GlobalPools
    {
    public:
        CommandPool						    _renderingCommandPool;
        DescriptorPool                      _mainDescriptorPool;
        VulkanSharedPtr<VkPipelineCache>    _mainPipelineCache;

        GlobalPools() {}
        GlobalPools(const GlobalPools&) = delete;
        GlobalPools& operator=(const GlobalPools&) = delete;
    };
}}
