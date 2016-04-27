// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TextureView.h"
#include "Buffer.h"
#include "State.h"
#include "FrameBuffer.h"
#include "VulkanCore.h"
#include "IncludeVulkan.h"
#include "../../../Utility/IteratorUtils.h"
#include "../../../Utility/Threading/Mutex.h"

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

        CommandPool(CommandPool&& moveFrom);
        CommandPool& operator=(CommandPool&& moveFrom);
	private:
		VulkanSharedPtr<VkCommandPool> _pool;
		VulkanSharedPtr<VkDevice> _device;

		std::vector<VkCommandBuffer> _pendingDestroy;
        Threading::Mutex _lock;
	};

    class DescriptorPool
    {
    public:
        void Allocate(
            IteratorRange<VulkanUniquePtr<VkDescriptorSet>*> dst,
            IteratorRange<const VkDescriptorSetLayout*> layouts);

        void FlushDestroys();
        VkDevice GetDevice() { return _device.get(); }

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

    class DummyResources
    {
    public:
        ResourcePtr         _blankTexture;
        ResourcePtr         _blankUAVTexture;
        ShaderResourceView  _blankSrv;
        UnorderedAccessView _blankUav;
        Buffer              _blankBuffer;
        std::unique_ptr<SamplerState> _blankSampler;

        DummyResources(const ObjectFactory& factory);
        DummyResources();
        ~DummyResources();

        DummyResources(DummyResources&& moveFrom) never_throws;
        DummyResources& operator=(DummyResources&& moveFrom) never_throws;
    };

    class GlobalPools
    {
    public:
        DescriptorPool                      _mainDescriptorPool;
        VulkanSharedPtr<VkPipelineCache>    _mainPipelineCache;
        DummyResources                      _dummyResources;
        FrameBufferCache                    _mainFrameBufferCache;

        GlobalPools();
        ~GlobalPools();
        GlobalPools(const GlobalPools&) = delete;
        GlobalPools& operator=(const GlobalPools&) = delete;
    };
}}
