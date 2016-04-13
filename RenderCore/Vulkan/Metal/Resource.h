// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "IncludeVulkan.h"
#include "../../IDevice.h"
#include "../../ResourceDesc.h"
#include "../../Types.h"
#include "../../../Utility/IteratorUtils.h"

namespace RenderCore { class Resource; }

namespace RenderCore { namespace Metal_Vulkan
{
	class ObjectFactory;
	class DeviceContext;
	class Resource;
	class TextureView;

	enum class ImageLayout
	{
		Undefined						= VK_IMAGE_LAYOUT_UNDEFINED,
		General							= VK_IMAGE_LAYOUT_GENERAL,
		ColorAttachmentOptimal			= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		DepthStencilAttachmentOptimal	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		DepthStencilReadOnlyOptimal		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		ShaderReadOnlyOptimal			= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		TransferSrcOptimal				= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		TransferDstOptimal				= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		Preinitialized					= VK_IMAGE_LAYOUT_PREINITIALIZED,
		PresentSrc						= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};

	/// <summary>Helper object to catch multiple similar pointers</summary>
	/// To help with platform abstraction, RenderCore::Resource* is actually the
	/// same as a Metal::Resource*. This helper allows us to catch both equally.
	class UnderlyingResourcePtr
	{
	public:
		Resource* get() { return _res; }
	
		UnderlyingResourcePtr(Resource* res) { _res = res; }
		UnderlyingResourcePtr(RenderCore::Resource* res) { _res = (Resource*)res; }
		UnderlyingResourcePtr(const std::shared_ptr<RenderCore::Resource>& res) { _res = (Resource*)res.get(); }
	protected:
		Resource* _res;
	};

	/// <summary>Abstraction for a device memory resource</summary>
	/// A Resource can either be a buffer or an image. In Vulkan, both types reference a VkDeviceMemory
	/// object that represents the actual allocation. This object maintains that allocation, and provides
	/// interfaces for copying data.
	///
	/// Images and buffers are combined into a single object for convenience. This allows us to use the 
	/// single "Desc" object to describe both, and it also fits in better with other APIs (eg, DirectX).
	/// This adds a small amount of redundancy to the Resource object -- but it seems to be trivial.
	class Resource
	{
	public:
		using Desc = ResourceDesc;
        struct SubResource { unsigned _mip; unsigned _arrayLayer; };

		Resource(
			const ObjectFactory& factory, const Desc& desc,
			const SubResourceInitData& initData = SubResourceInitData{});
		Resource(
			const ObjectFactory& factory, const Desc& desc,
			const std::function<SubResourceInitData(unsigned, unsigned)>&);
		Resource(UnderlyingResourcePtr copyFrom) : Resource(*copyFrom.get()) {}
		Resource();
		~Resource();

		VkDeviceMemory GetMemory() { return _mem.get(); }
		VkImage GetImage() { return _underlyingImage.get(); }
		VkBuffer GetBuffer() { return _underlyingBuffer.get(); }
		const Desc& GetDesc() const { return _desc; }

		const VulkanSharedPtr<VkImage>& ShareImage() const { return _underlyingImage; }
	protected:
		VulkanSharedPtr<VkDeviceMemory> _mem;

		VulkanSharedPtr<VkBuffer> _underlyingBuffer;
		VulkanSharedPtr<VkImage> _underlyingImage;

		Desc _desc;
	};

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      M E M O R Y   M A P       //
///////////////////////////////////////////////////////////////////////////////////////////////////

    /// <summary>Locks a resource's memory for access from the CPU</summary>
    /// This is a low level mapping operation that happens immediately. The GPU must not
    /// be using the resource at the same time. If the GPU attempts to read while the CPU
    /// is written, the results will be undefined.
    /// A resource cannot be mapped more than once at the same time. However, multiple 
    /// subresources can be mapped in a single mapping operation.
    /// The caller is responsible for ensuring that the map is safe.
	class ResourceMap
	{
	public:
        using Pitches = TexturePitches;

		void*           GetData()               { return _data; }
        const void*     GetData() const         { return _data; }
        size_t          GetDataSize() const     { return _dataSize; }
        Pitches         GetPitches() const      { return _pitches; }

		ResourceMap(
			VkDevice dev, VkDeviceMemory memory,
			VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
		ResourceMap(
			IDevice& dev, UnderlyingResourcePtr resource,
            Resource::SubResource subResource,
			VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
		ResourceMap();
		~ResourceMap();

		ResourceMap(const ResourceMap&) = delete;
		ResourceMap& operator=(const ResourceMap&) = delete;
		ResourceMap(ResourceMap&&) never_throws;
		ResourceMap& operator=(ResourceMap&&) never_throws;

	private:
		VkDevice        _dev;
		VkDeviceMemory  _mem;
        void*           _data;
        size_t          _dataSize;
        Pitches         _pitches;

		void TryUnmap();
	};

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      C O P Y I N G       //
///////////////////////////////////////////////////////////////////////////////////////////////////

	void Copy(
        DeviceContext&, 
        UnderlyingResourcePtr dst, UnderlyingResourcePtr src, 
        ImageLayout dstLayout = ImageLayout::TransferDstOptimal, ImageLayout srcLayout = ImageLayout::TransferSrcOptimal);

    using UInt3Pattern = VectorPattern<unsigned, 3>;

    class CopyPartial_Dest
    {
    public:
        Resource*		        _resource;
        Resource::SubResource   _subResource;
        UInt3Pattern            _leftTopFront;

        CopyPartial_Dest(
            UnderlyingResourcePtr dst, Resource::SubResource subres = {},
            const UInt3Pattern& leftTopFront = UInt3Pattern())
        : _resource(dst.get()), _subResource(subres), _leftTopFront(leftTopFront) {}
    };

    class CopyPartial_Src
    {
    public:
		Resource*               _resource;
        Resource::SubResource   _subResource;
        UInt3Pattern            _leftTopFront;
        UInt3Pattern            _rightBottomBack;

        CopyPartial_Src(
            UnderlyingResourcePtr dst, Resource::SubResource subres = {},
            const UInt3Pattern& leftTopFront = UInt3Pattern(~0u,0,0),
            const UInt3Pattern& rightBottomBack = UInt3Pattern(~0u,1,1))
        : _resource(dst.get()), _subResource(subres)
        , _leftTopFront(leftTopFront)
        , _rightBottomBack(rightBottomBack) {}
    };

	void CopyPartial(
        DeviceContext&, 
        const CopyPartial_Dest& dst, const CopyPartial_Src& src,
        ImageLayout dstLayout = ImageLayout::Undefined, ImageLayout srcLayout = ImageLayout::Undefined);

	ResourcePtr Duplicate(ObjectFactory&, UnderlyingResourcePtr inputResource);
	ResourcePtr Duplicate(DeviceContext&, UnderlyingResourcePtr inputResource);

    unsigned CopyViaMemoryMap(
        VkDevice device, VkImage image, VkDeviceMemory mem,
        const TextureDesc& desc,
        const std::function<SubResourceInitData(unsigned, unsigned)>& initData);

    unsigned CopyViaMemoryMap(
        IDevice& dev, UnderlyingResourcePtr resource,
        const std::function<SubResourceInitData(unsigned, unsigned)>& initData);

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      G E T   D E S C       //
///////////////////////////////////////////////////////////////////////////////////////////////////

    ResourceDesc ExtractDesc(UnderlyingResourcePtr res);
	ResourceDesc ExtractDesc(const TextureView& res);
	RenderCore::ResourcePtr ExtractResource(TextureView&);

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      U T I L S       //
///////////////////////////////////////////////////////////////////////////////////////////////////

	void SetImageLayout(
		VkCommandBuffer cmd, VkImage image,
		VkImageAspectFlags aspectMask, VkImageLayout old_image_layout, VkImageLayout new_image_layout);
	void SetImageLayout(
		DeviceContext& context, UnderlyingResourcePtr res,
		ImageLayout oldLayout, ImageLayout newLayout);

    VkSampleCountFlagBits   AsSampleCountFlagBits(TextureSamples samples);
    VkImageAspectFlags      AsImageAspectMask(Format fmt);
}}
