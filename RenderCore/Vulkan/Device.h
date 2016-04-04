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
#include "../../BufferUploads/IBufferUploads.h"
#include "../../Utility/IntrusivePtr.h"
#include "../../Utility/IteratorUtils.h"
#include "Metal/IncludeVulkan.h"
#include <memory>
#include <vector>
#include <type_traits>

namespace RenderCore
{
    namespace Internal
    {
        template<typename Type>
            struct VulkanShared
                { typedef std::shared_ptr<typename std::remove_reference<decltype(*std::declval<Type>())>::type> Ptr; };
    }

    template<typename Type>
	    using VulkanSharedPtr = typename Internal::VulkanShared<Type>::Ptr;

    class SelectedPhysicalDevice
	{
	public:
		VkPhysicalDevice _dev;
		unsigned _renderingQueueFamily;
	};

////////////////////////////////////////////////////////////////////////////////

    namespace Metal_DX11 { class DeviceContext; class ObjectFactory; }

    class Device;
    class ObjectFactory;

	class RenderPass
	{
	public:
		enum class PreviousState { DontCare, Clear };

		class TargetInfo
		{
		public:
			VkFormat _format;
			VkSampleCountFlagBits _samples;
			PreviousState _previousState;

			TargetInfo(
				VkFormat fmt = VK_FORMAT_UNDEFINED, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
				PreviousState previousState = PreviousState::DontCare)
				: _format(fmt), _samples(samples), _previousState() {}
		};

		VkRenderPass GetUnderlying() { return _underlying.get(); }

		RenderPass(
			const ObjectFactory& factory,
			IteratorRange<TargetInfo*> rtvAttachments,
			TargetInfo dsvAttachment = TargetInfo());
		RenderPass();
		~RenderPass();

	private:
		VulkanSharedPtr<VkRenderPass> _underlying;
	};

	class FrameBuffer
	{
	public:
		VkFramebuffer GetUnderlying() { return _underlying.get(); }

		FrameBuffer(
			const ObjectFactory& factory,
			IteratorRange<VkImageView*> views,
			RenderPass& renderPass,
			unsigned width, unsigned height);
		FrameBuffer();
		~FrameBuffer();
	private:
		VulkanSharedPtr<VkFramebuffer> _underlying;
	};

	class Resource
	{
	public:
		const BufferUploads::BufferDesc& GetDesc() const { return _desc; }
		VkImage GetImage() const { return _image.get(); }
		VkDeviceMemory GetDeviceMemory() const { return _mem.get(); }

		Resource(
			const ObjectFactory& factory,
			const BufferUploads::BufferDesc& desc);
		Resource();
		~Resource();
	private:
		VulkanSharedPtr<VkImage> _image;
		VulkanSharedPtr<VkDeviceMemory> _mem;
		BufferUploads::BufferDesc _desc;
	};

	class ImageView
	{
	public:
		VkImageView GetUnderlying() { return _underlying.get(); }
		~ImageView();
	protected:
		VulkanSharedPtr<VkImageView> _underlying;
	};

	class DepthStencilView : public ImageView
	{
	public:
		DepthStencilView(const ObjectFactory& factory, Resource& res);
		DepthStencilView();
	};

	class RenderTargetView : public ImageView
	{
	public:
		RenderTargetView(const ObjectFactory& factory, Resource& res);
		RenderTargetView(const ObjectFactory& factory, VkImage image, VkFormat fmt);
		RenderTargetView();
	};

	class CommandPool
	{
	public:
		enum class BufferType { Primary, Secondary };
		VulkanSharedPtr<VkCommandBuffer> CreateBuffer(BufferType type);

		CommandPool(const ObjectFactory& factory, unsigned queueFamilyIndex);
		CommandPool();
		~CommandPool();
	private:
		VulkanSharedPtr<VkCommandPool> _pool;
		VulkanSharedPtr<VkDevice> _device;
	};

	class ObjectFactory
	{
	public:
		VkPhysicalDevice            _physDev;
		VulkanSharedPtr<VkDevice>   _device;

		unsigned FindMemoryType(VkFlags memoryTypeBits, VkMemoryPropertyFlags requirementsMask = 0) const;

		ObjectFactory(VkPhysicalDevice physDev, VulkanSharedPtr<VkDevice> device);
		ObjectFactory();
		~ObjectFactory();

	private:
		VkPhysicalDeviceMemoryProperties _memProps;
	};

////////////////////////////////////////////////////////////////////////////////

    class PresentationChain : public Base_PresentationChain
    {
    public:
        void	Present() /*override*/;
        void	Resize(unsigned newWidth, unsigned newHeight) /*override*/;

        std::shared_ptr<ViewportContext> GetViewportContext() const;
        void    AcquireNextImage();
		void	BindDefaultRenderPass(VkCommandBuffer cmdBuffer);

        PresentationChain(
			VulkanSharedPtr<VkSurfaceKHR> surface, 
			VulkanSharedPtr<VkSwapchainKHR> swapChain,			
            const ObjectFactory& factory,
            VkQueue queue, 
            const BufferUploads::TextureDesc& bufferDesc,
            const void* platformValue);
        ~PresentationChain();
    private:
		VulkanSharedPtr<VkSurfaceKHR> _surface;
        VulkanSharedPtr<VkSwapchainKHR> _swapChain;
        VkQueue			_queue;
        VulkanSharedPtr<VkDevice> _device;
        const void*		_platformValue;
        unsigned		_activeImageIndex;

        class Image
        {
        public:
            VkImage _underlying;
            VulkanSharedPtr<VkSemaphore> _presentSemaphore;
			RenderTargetView _rtv;
			FrameBuffer _defaultFrameBuffer;
        };
        std::vector<Image> _images;

		Resource _depthStencilResource;
		DepthStencilView _dsv;

		RenderPass		_defaultRenderPass;
		BufferUploads::TextureDesc _bufferDesc;

		VkCommandBuffer _cmdBufferPendingCommit;
    };

////////////////////////////////////////////////////////////////////////////////

    class ThreadContext : public Base_ThreadContext
    {
    public:
        bool                        IsImmediate() const;
        ThreadContextStateDesc      GetStateDesc() const;
        std::shared_ptr<IDevice>    GetDevice() const;
        void                        ClearAllBoundTargets() const;
        void                        IncrFrameId();
		void						InvalidateCachedState() const;

		VkCommandBuffer				GetCommandBuffer() { return _primaryCommandBuffer.get(); }

        ThreadContext(std::shared_ptr<Device> device, VulkanSharedPtr<VkCommandBuffer> primaryCommandBuffer);
        ~ThreadContext();
    protected:
        std::weak_ptr<Device>   _device;  // (must be weak, because Device holds a shared_ptr to the immediate context)
		unsigned                _frameId;
		VulkanSharedPtr<VkCommandBuffer> _primaryCommandBuffer;
    };

    class ThreadContextVulkan : public ThreadContext, public Base_ThreadContextVulkan
    {
    public:
        virtual void*       QueryInterface(const GUID& guid);

		ThreadContextVulkan(std::shared_ptr<Device> device, VulkanSharedPtr<VkCommandBuffer> primaryCommandBuffer);
        ~ThreadContextVulkan();
    };

////////////////////////////////////////////////////////////////////////////////

	class Device : public Base_Device, public std::enable_shared_from_this<Device>
    {
    public:
        std::unique_ptr<IPresentationChain>     CreatePresentationChain(const void* platformValue, unsigned width, unsigned height) /*override*/;
        void    BeginFrame(IPresentationChain* presentationChain);

        std::pair<const char*, const char*>     GetVersionInformation();

        std::shared_ptr<IThreadContext>         GetImmediateContext();
        std::unique_ptr<IThreadContext>         CreateDeferredContext();

        Device();
        ~Device();
    protected:
		VulkanSharedPtr<VkInstance>     _instance;
		VulkanSharedPtr<VkDevice>		_underlying;
        SelectedPhysicalDevice          _physDev;
		CommandPool						_renderingCommandPool;
		ObjectFactory					_objectFactory;

		std::shared_ptr<ThreadContext>	_foregroundPrimaryContext;
    };

    class DeviceVulkan : public Device, public Base_DeviceVulkan
    {
    public:
        virtual void*   QueryInterface(const GUID& guid);
		VkInstance		GetVulkanInstance();
		VkDevice		GetUnderlyingDevice();
        
        DeviceVulkan();
        ~DeviceVulkan();
    };

////////////////////////////////////////////////////////////////////////////////
}
