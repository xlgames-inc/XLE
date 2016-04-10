// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "VulkanCore.h"
#include "IncludeVulkan.h"
#include "../../ResourceDesc.h"
#include "../../../Utility/IntrusivePtr.h"
#include "../../../Utility/Threading/ThreadingUtils.h"

namespace RenderCore { class Resource; }

namespace RenderCore { namespace Metal_Vulkan
{
	class ObjectFactory;
	class DeviceContext;
	class Resource;

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
		UnderlyingResourcePtr(RenderCore::Resource* res) { _res = (Resource*)_res; }
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
	class Resource : public RefCountedObject
	{
	public:
		using Desc = ResourceDesc;

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
	protected:
		VulkanSharedPtr<VkDeviceMemory> _mem;

		VulkanSharedPtr<VkBuffer> _underlyingBuffer;
		VulkanSharedPtr<VkImage> _underlyingImage;

		Desc _desc;
	};

	class MemoryMap
	{
	public:
		void*       _data;

		MemoryMap(
			VkDevice dev, VkDeviceMemory memory,
			VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
		MemoryMap(
			VkDevice dev, UnderlyingResourcePtr resource,
			VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
		MemoryMap();
		~MemoryMap();

		MemoryMap(const MemoryMap&) = delete;
		MemoryMap& operator=(const MemoryMap&) = delete;
		MemoryMap(MemoryMap&&);
		MemoryMap& operator=(MemoryMap&&);

	private:
		VkDevice _dev;
		VkDeviceMemory _mem;

		void TryUnmap();
	};

    class DeviceContext;

	void Copy(DeviceContext&, UnderlyingResourcePtr dst, UnderlyingResourcePtr src, ImageLayout dstLayout = ImageLayout::Undefined, ImageLayout srcLayout = ImageLayout::Undefined);

    namespace Internal { static std::true_type UnsignedTest(unsigned); static std::false_type UnsignedTest(...); }

    class PixelCoord
    {
    public:
        unsigned _x, _y, _z;
        PixelCoord(unsigned x=0, unsigned y=0, unsigned z=0)    { _x = x; _y = y; _z = z; }

            // We can initialize from anything that looks like a collection of unsigned values
            // This is a simple way to get casting from XLEMath::UInt2 (etc) types without
            // having to include XLEMath headers from here.
            // Plus, it will also work with any other types that expose a stl collection type
            // interface.
		template<
			typename Source,
			typename InternalTestType = decltype(Internal::UnsignedTest(std::declval<typename Source::value_type>())),
			std::enable_if<InternalTestType::value>* = nullptr>
            PixelCoord(const Source& src)
            {
                auto size = std::size(src);
                unsigned c=0;
                for (; c<std::min(unsigned(size), 3u); ++c) ((unsigned*)this)[c] = src[c];
                for (; c<3u; ++c) ((unsigned*)this)[c] = 0u;
            }
    };

    class CopyPartial_Dest
    {
    public:
        Resource*		_resource;
        unsigned        _subResource;
        PixelCoord      _leftTopFront;

        CopyPartial_Dest(
			UnderlyingResourcePtr dst, unsigned subres = 0u,
            const PixelCoord leftTopFront = PixelCoord())
        : _resource(dst.get()), _subResource(subres), _leftTopFront(leftTopFront) {}
    };

    class CopyPartial_Src
    {
    public:
		Resource* _resource;
        unsigned        _subResource;
        PixelCoord      _leftTopFront;
        PixelCoord      _rightBottomBack;

        CopyPartial_Src(
			UnderlyingResourcePtr dst, unsigned subres = 0u,
            const PixelCoord leftTopFront = PixelCoord(~0u,0,0),
            const PixelCoord rightBottomBack = PixelCoord(~0u,1,1))
        : _resource(dst.get()), _subResource(subres)
        , _leftTopFront(leftTopFront)
        , _rightBottomBack(rightBottomBack) {}
    };

	inline void CopyPartial(DeviceContext&, const CopyPartial_Dest& dst, const CopyPartial_Src& src) {}

	inline intrusive_ptr<Resource> Duplicate(DeviceContext& context, UnderlyingResourcePtr inputResource) { return nullptr; }

	VkSampleCountFlagBits AsSampleCountFlagBits(TextureSamples samples);
	void SetImageLayout(
		VkCommandBuffer cmd, VkImage image,
		VkImageAspectFlags aspectMask, VkImageLayout old_image_layout, VkImageLayout new_image_layout);
	void SetImageLayout(
		DeviceContext& context, UnderlyingResourcePtr res,
		ImageLayout oldLayout, ImageLayout newLayout);

	ResourceDesc ExtractDesc(UnderlyingResourcePtr res);
}}
