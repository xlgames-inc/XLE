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
#include "ObjectFactory.h"
#include "VulkanCore.h"
#include "../../../Utility/IteratorUtils.h"
#include "../../../Utility/HeapUtils.h"
#include <vector>

#if defined(CHECK_COMMAND_POOL)
    #include "../../../Utility/Threading/Mutex.h"       // (cannot be included into CLR code)
#endif

namespace RenderCore { namespace Metal_Vulkan
{
    class ObjectFactory;

///////////////////////////////////////////////////////////////////////////////////////////////////

	enum class CommandBufferType { Primary, Secondary };

    class CommandPool
	{
	public:
		VulkanSharedPtr<VkCommandBuffer> Allocate(CommandBufferType type);

		void FlushDestroys();

		CommandPool(const Metal_Vulkan::ObjectFactory& factory, unsigned queueFamilyIndex, bool resetable, const std::shared_ptr<IAsyncTracker>& tracker);
		CommandPool();
		~CommandPool();

        CommandPool(CommandPool&& moveFrom) never_throws;
        CommandPool& operator=(CommandPool&& moveFrom) never_throws;
	private:
		VulkanSharedPtr<VkCommandPool> _pool;
		VulkanSharedPtr<VkDevice> _device;
		std::shared_ptr<IAsyncTracker> _gpuTracker;

		struct MarkedDestroys { IAsyncTracker::Marker _marker; unsigned _pendingCount; };
		CircularBuffer<MarkedDestroys, 8>	_markedDestroys;
		std::vector<VkCommandBuffer>		_pendingDestroys;
        #if defined(CHECK_COMMAND_POOL)
            Threading::Mutex _lock;
        #endif

		void QueueDestroy(VkCommandBuffer buffer);
	};

	class TemporaryBufferSpace
	{
	public:
		VkDescriptorBufferInfo	AllocateBuffer(IteratorRange<const void*> data);
		void FlushDestroys();
		void WriteBarrier(DeviceContext& context);

		TemporaryBufferSpace(
			const ObjectFactory& factory,
			const std::shared_ptr<IAsyncTracker>& asyncTracker);
		~TemporaryBufferSpace();
	private:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;
	};

    class DescriptorPool
    {
    public:
        void Allocate(
            IteratorRange<VulkanUniquePtr<VkDescriptorSet>*> dst,
            IteratorRange<const VkDescriptorSetLayout*> layouts);
		VulkanUniquePtr<VkDescriptorSet> Allocate(VkDescriptorSetLayout layout);

        void FlushDestroys();
        VkDevice GetDevice() { return _device.get(); }

        DescriptorPool(const Metal_Vulkan::ObjectFactory& factory, const std::shared_ptr<IAsyncTracker>& tracker);
        DescriptorPool();
        ~DescriptorPool();

        DescriptorPool(const DescriptorPool&) = delete;
        DescriptorPool& operator=(const DescriptorPool&) = delete;
        DescriptorPool(DescriptorPool&&) never_throws;
        DescriptorPool& operator=(DescriptorPool&&) never_throws;
    private:
        VulkanSharedPtr<VkDescriptorPool> _pool;
		VulkanSharedPtr<VkDevice> _device;
		std::shared_ptr<IAsyncTracker> _gpuTracker;

		struct MarkedDestroys { IAsyncTracker::Marker _marker; unsigned _pendingCount; };
		CircularBuffer<MarkedDestroys, 8> _markedDestroys;
        std::vector<VkDescriptorSet> _pendingDestroys;

		void QueueDestroy(VkDescriptorSet buffer);
    };

    class DummyResources
    {
    public:
        ResourcePtr         _blankTexture;
        ResourcePtr         _blankUAVImageRes;
        ResourcePtr         _blankUAVBufferRes;
        ShaderResourceView  _blankSrv;
        UnorderedAccessView _blankUavImage;
        UnorderedAccessView _blankUavBuffer;
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
        FrameBufferPool                    _mainFrameBufferCache;

        GlobalPools();
        ~GlobalPools();
        GlobalPools(const GlobalPools&) = delete;
        GlobalPools& operator=(const GlobalPools&) = delete;
    };
}}
