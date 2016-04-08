// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Pools.h"
#include "ObjectFactory.h"

namespace RenderCore { namespace Metal_Vulkan
{
    VulkanSharedPtr<VkCommandBuffer> CommandPool::Allocate(BufferType type)
	{
		VkCommandBufferAllocateInfo cmd = {};
		cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd.pNext = nullptr;
		cmd.commandPool = _pool.get();
		cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd.commandBufferCount = 1;

		auto dev = _device.get();
		auto pool = _pool.get();
		VkCommandBuffer rawBuffer = nullptr;
		auto res = vkAllocateCommandBuffers(dev, &cmd, &rawBuffer);
		VulkanSharedPtr<VkCommandBuffer> result(
			rawBuffer,
			[dev, pool](VkCommandBuffer buffer) { vkFreeCommandBuffers(dev, pool, 1, &buffer); });
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while creating command buffer"));
		return result;
	}

	CommandPool::CommandPool(const Metal_Vulkan::ObjectFactory& factory, unsigned queueFamilyIndex)
	: _device(factory.GetDevice())
	{
		_pool = factory.CreateCommandPool(
            queueFamilyIndex, 
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	}

	CommandPool::CommandPool() {}
	CommandPool::~CommandPool() {}


    void DescriptorPool::Allocate(
        IteratorRange<VulkanUniquePtr<VkDescriptorSet>*> dst,
        IteratorRange<const VkDescriptorSetLayout*> layouts)
    {
        assert(dst.size() == layouts.size());
        assert(dst.size() > 0);

        VkDescriptorSetAllocateInfo desc_alloc_info;
        desc_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        desc_alloc_info.pNext = nullptr;
        desc_alloc_info.descriptorPool = _pool.get();
        desc_alloc_info.descriptorSetCount = (uint32_t)std::min(dst.size(), layouts.size());
        desc_alloc_info.pSetLayouts = layouts.begin();

        VkDescriptorSet rawDescriptorSets[4] = { nullptr, nullptr, nullptr, nullptr };

        VkResult res;
        if (desc_alloc_info.descriptorSetCount <= dimof(rawDescriptorSets)) {
            res = vkAllocateDescriptorSets(_device.get(), &desc_alloc_info, rawDescriptorSets);
            for (unsigned c=0; c<desc_alloc_info.descriptorSetCount; ++c)
                dst[c] = VulkanUniquePtr<VkDescriptorSet>(
                    rawDescriptorSets[c],
                    [this](VkDescriptorSet set) { this->_pendingDestroy.push_back(set); });
        } else {
            std::vector<VkDescriptorSet> rawDescriptorsOverflow;
            rawDescriptorsOverflow.resize(desc_alloc_info.descriptorSetCount, nullptr);
            res = vkAllocateDescriptorSets(_device.get(), &desc_alloc_info, AsPointer(rawDescriptorsOverflow.begin()));
            for (unsigned c=0; c<desc_alloc_info.descriptorSetCount; ++c)
                dst[c] = VulkanUniquePtr<VkDescriptorSet>(
                    rawDescriptorsOverflow[c],
                    [this](VkDescriptorSet set) { this->_pendingDestroy.push_back(set); });
        }

        if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while allocating descriptor set")); 
    }

    void DescriptorPool::FlushDestroys()
    {
        if (!_pendingDestroy.empty())
            vkFreeDescriptorSets(
                _device.get(), _pool.get(), 
                (uint32_t)_pendingDestroy.size(), AsPointer(_pendingDestroy.begin()));
        _pendingDestroy.clear();
    }

    DescriptorPool::DescriptorPool(const Metal_Vulkan::ObjectFactory& factory)
    : _device(factory.GetDevice())
    {
        VkDescriptorPoolSize type_count[1];
        type_count[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        type_count[0].descriptorCount = 128;

        VkDescriptorPoolCreateInfo descriptor_pool = {};
        descriptor_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptor_pool.pNext = nullptr;
        descriptor_pool.maxSets = 32;
        descriptor_pool.poolSizeCount = 1;
        descriptor_pool.pPoolSizes = type_count;
        descriptor_pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        _pool = factory.CreateDescriptorPool(descriptor_pool);
    }
    DescriptorPool::DescriptorPool() {}
    DescriptorPool::~DescriptorPool() 
    {
        FlushDestroys();
    }

    DescriptorPool::DescriptorPool(DescriptorPool&& moveFrom) never_throws
    : _pool(moveFrom._pool)
    , _device(moveFrom._device)
    , _pendingDestroy(moveFrom._pendingDestroy)
    {
    }

    DescriptorPool& DescriptorPool::operator=(DescriptorPool&& moveFrom) never_throws
    {
        _pool = std::move(moveFrom._pool);
        _device = std::move(moveFrom._device);
        _pendingDestroy = std::move(moveFrom._pendingDestroy);
        return *this;
    }

}}