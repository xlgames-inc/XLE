// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#define FLEX_CONTEXT_Device					FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_DeviceVulkan			FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_PresentationChain		FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_ThreadContext			FLEX_CONTEXT_CONCRETE
#define FLEX_CONTEXT_ThreadContextVulkan	FLEX_CONTEXT_CONCRETE

#include "../IDevice.h"
#include "../IThreadContext.h"
#include "IDeviceVulkan.h"
#include "Metal/VulkanCore.h"
#include "Metal/ObjectFactory.h"
#include "Metal/DeviceContext.h"
#include "Metal/Pools.h"
#include "Metal/IncludeVulkan.h"
#include "Metal/FrameBuffer.h"
#include "Metal/TextureView.h"
#include "../../Utility/IntrusivePtr.h"
#include "../../Utility/IteratorUtils.h"
#include <memory>
#include <vector>
#include <type_traits>

namespace RenderCore { namespace ImplVulkan
{
    class SelectedPhysicalDevice
	{
	public:
		VkPhysicalDevice _dev;
		unsigned _renderingQueueFamily;
	};

    template<typename Type>
        using VulkanSharedPtr = Metal_Vulkan::VulkanSharedPtr<Type>;

    template<typename Type>
        using VulkanUniquePtr = Metal_Vulkan::VulkanUniquePtr<Type>;

////////////////////////////////////////////////////////////////////////////////

    class Device;

////////////////////////////////////////////////////////////////////////////////

    class PresentationChain : public Base_PresentationChain
    {
    public:
        void Resize(unsigned newWidth, unsigned newHeight) /*override*/;

        const std::shared_ptr<PresentationChainDesc>& GetDesc() const;
        Metal_Vulkan::RenderTargetView* AcquireNextImage();
        const TextureDesc& GetBufferDesc() { return _bufferDesc; }

		void PresentToQueue(VkQueue queue);
        void SetInitialLayout(
            const Metal_Vulkan::ObjectFactory& factory, 
            Metal_Vulkan::CommandPool& cmdPool, VkQueue queue);

		class PresentSync
		{
		public:
			VulkanUniquePtr<VkSemaphore>    _onAcquireComplete;
			VulkanUniquePtr<VkSemaphore>    _onCommandBufferComplete;
			VulkanUniquePtr<VkFence>        _presentFence;
		};
		PresentSync& GetSyncs() { return _presentSyncs[_activePresentSync]; }

        PresentationChain(
			const Metal_Vulkan::ObjectFactory& factory,
            VulkanSharedPtr<VkSurfaceKHR> surface, 
			VectorPattern<unsigned, 2> extent,
            const void* platformValue);
        ~PresentationChain();
    private:
		VulkanSharedPtr<VkSurfaceKHR>   _surface;
        VulkanSharedPtr<VkSwapchainKHR> _swapChain;
        VulkanSharedPtr<VkDevice>       _device;
        const Metal_Vulkan::ObjectFactory*    _factory;
        const void*		_platformValue;
        unsigned		_activeImageIndex;

        class Image
        {
        public:
            VkImage     _image;
			Metal_Vulkan::RenderTargetView      _rtv;
        };
        std::vector<Image> _images;

		TextureDesc     _bufferDesc;
		std::shared_ptr<PresentationChainDesc>	_desc;

        PresentSync     _presentSyncs[3];
        unsigned        _activePresentSync;

        void BuildImages();
    };

////////////////////////////////////////////////////////////////////////////////

	class EventBasedTracker;

    class ThreadContext : public Base_ThreadContext
    {
    public:
		void	        Present(IPresentationChain&);
		ResourcePtr     BeginFrame(IPresentationChain& presentationChain);

        bool                        IsImmediate() const;
        ThreadContextStateDesc      GetStateDesc() const;
        std::shared_ptr<IDevice>    GetDevice() const;
        void                        IncrFrameId();
		void						InvalidateCachedState() const;
        void                        BeginCommandList();

        Metal_Vulkan::CommandPool&  GetRenderingCommandPool()   { return _renderingCommandPool; }
        VkQueue                     GetQueue()                  { return _queue; }

		IAnnotator&					GetAnnotator();

		void SetGPUTracker(const std::shared_ptr<EventBasedTracker>&);
		void AttachDestroyer(const std::shared_ptr<Metal_Vulkan::IDestructionQueue>&);

        ThreadContext(
            std::shared_ptr<Device> device, 
			VkQueue queue,
            Metal_Vulkan::PipelineLayout& graphicsPipelineLayout,
            Metal_Vulkan::PipelineLayout& computePipelineLayout,
            Metal_Vulkan::CommandPool&& cmdPool,
			Metal_Vulkan::CommandPool::BufferType cmdBufferType);
        ~ThreadContext();
    protected:
        std::weak_ptr<Device>           _device;  // (must be weak, because Device holds a shared_ptr to the immediate context)
		unsigned                        _frameId;
        Metal_Vulkan::CommandPool		_renderingCommandPool;
		std::shared_ptr<Metal_Vulkan::DeviceContext>     _metalContext;
		std::unique_ptr<IAnnotator>		_annotator;

		VkDevice							_underlyingDevice;
		VkQueue								_queue;
		const Metal_Vulkan::ObjectFactory*	_factory;
		Metal_Vulkan::GlobalPools*			_globalPools;

		std::shared_ptr<EventBasedTracker>	_gpuTracker;
		std::shared_ptr<Metal_Vulkan::IDestructionQueue> _destrQueue;
    };

    class ThreadContextVulkan : public ThreadContext, public Base_ThreadContextVulkan
    {
    public:
        virtual void*   QueryInterface(const GUID& guid);
        const std::shared_ptr<Metal_Vulkan::DeviceContext>& GetMetalContext();

		ThreadContextVulkan(
			std::shared_ptr<Device> device,
			VkQueue queue,
            Metal_Vulkan::PipelineLayout& graphicsPipelineLayout,
            Metal_Vulkan::PipelineLayout& computePipelineLayout,
            Metal_Vulkan::CommandPool&& cmdPool,
			Metal_Vulkan::CommandPool::BufferType cmdBufferType);
        ~ThreadContextVulkan();
    };

////////////////////////////////////////////////////////////////////////////////

	class Device : public Base_Device, public std::enable_shared_from_this<Device>
    {
    public:
        std::unique_ptr<IPresentationChain>     CreatePresentationChain(
			const void* platformValue, unsigned width, unsigned height) /*override*/;

        DeviceDesc                              GetDesc();

        std::shared_ptr<IThreadContext>         GetImmediateContext();
        std::unique_ptr<IThreadContext>         CreateDeferredContext();

        Metal_Vulkan::GlobalPools&              GetGlobalPools() { return _pools; }
		Metal_Vulkan::ObjectFactory&			GetObjectFactory() { return _objectFactory; }

		ResourcePtr CreateResource(
			const ResourceDesc& desc, 
			const std::function<SubResourceInitData(SubResourceId)>&);

		VkDevice	    GetUnderlyingDevice() { return _underlying.get(); }

        Device();
        ~Device();
    protected:
		VulkanSharedPtr<VkInstance>         _instance;
		VulkanSharedPtr<VkDevice>		    _underlying;
        SelectedPhysicalDevice              _physDev;
		Metal_Vulkan::ObjectFactory		    _objectFactory;
        Metal_Vulkan::GlobalPools           _pools;

        std::shared_ptr<Metal_Vulkan::PipelineLayout> _graphicsPipelineLayout;
        std::shared_ptr<Metal_Vulkan::PipelineLayout> _computePipelineLayout;
		std::shared_ptr<ThreadContextVulkan>	_foregroundPrimaryContext;

        void DoSecondStageInit(VkSurfaceKHR surface = nullptr);
    };

    class DeviceVulkan : public Device, public Base_DeviceVulkan
    {
    public:
        virtual void*   QueryInterface(const GUID& guid);
		VkInstance	    GetVulkanInstance();
		VkDevice	    GetUnderlyingDevice();
        VkQueue         GetRenderingQueue();
        Metal_Vulkan::GlobalPools& GetGlobalPools();
        const std::shared_ptr<Metal_Vulkan::PipelineLayout>& ShareGraphicsPipelineLayout();
        const std::shared_ptr<Metal_Vulkan::PipelineLayout>& ShareComputePipelineLayout();
        
        DeviceVulkan();
        ~DeviceVulkan();
    };

////////////////////////////////////////////////////////////////////////////////
}}
