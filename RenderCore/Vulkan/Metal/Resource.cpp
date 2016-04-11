// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Resource.h"
#include "ObjectFactory.h"
#include "Format.h"
#include "DeviceContext.h"
#include "../../Format.h"
#include "../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{
	static VkBufferUsageFlags AsBufferUsageFlags(BindFlag::BitField bindFlags)
	{
		VkBufferUsageFlags result = 0;
		if (bindFlags & BindFlag::VertexBuffer) result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		if (bindFlags & BindFlag::IndexBuffer) result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		if (bindFlags & BindFlag::ConstantBuffer) result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		if (bindFlags & BindFlag::DrawIndirectArgs) result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

		// Other Vulkan flags:
		// VK_BUFFER_USAGE_TRANSFER_SRC_BIT
		// VK_BUFFER_USAGE_TRANSFER_DST_BIT
		// VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
		// VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
		// VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		return result;
	}
	
	static VkImageUsageFlags AsImageUsageFlags(BindFlag::BitField bindFlags)
	{
		// note -- we're assuming shader resources are sampled here (rather than storage type textures)
		VkImageUsageFlags result = 0;
		if (bindFlags & BindFlag::ShaderResource) result |= VK_IMAGE_USAGE_SAMPLED_BIT;
		if (bindFlags & BindFlag::RenderTarget) result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		if (bindFlags & BindFlag::DepthStencil) result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		if (bindFlags & BindFlag::UnorderedAccess) result |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		// Other Vulkan flags:
		// VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		// VK_IMAGE_USAGE_TRANSFER_DST_BIT
		// VK_IMAGE_USAGE_STORAGE_BIT
		// VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT
		return result;
	}

	static bool IsDepthStencilFormat(Format fmt)
	{
		return GetComponents(fmt) == FormatComponents::Depth;
	}

	static VkImageAspectFlags AsImageAspectMask(BindFlag::BitField bindFlags, Format fmt)
	{
		VkImageAspectFlags result = 0;
		if (bindFlags & BindFlag::RenderTarget) result |= VK_IMAGE_ASPECT_COLOR_BIT;
		if (bindFlags & BindFlag::DepthStencil) result |= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		if (result == 0) {
			if (IsDepthStencilFormat(fmt)) result |= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			else result |= VK_IMAGE_ASPECT_COLOR_BIT;
		}
		return result;
	}

	static VkImageLayout AsVkImageLayout(ImageLayout input) { return (VkImageLayout)input; }

	VkSampleCountFlagBits AsSampleCountFlagBits(TextureSamples samples)
	{
		return VK_SAMPLE_COUNT_1_BIT;
	}

	static VkImageType AsImageType(TextureDesc::Dimensionality::Enum dims)
	{
		switch (dims) {
		case TextureDesc::Dimensionality::T1D: return VK_IMAGE_TYPE_1D;
		default:
		case TextureDesc::Dimensionality::T2D: return VK_IMAGE_TYPE_2D;
		case TextureDesc::Dimensionality::T3D: return VK_IMAGE_TYPE_3D;
		case TextureDesc::Dimensionality::CubeMap: return VK_IMAGE_TYPE_2D;
		}
	}

	void SetImageLayout(
		VkCommandBuffer cmd, VkImage image,
		VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout)
	{
		VkImageMemoryBarrier image_memory_barrier = {};
		image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		image_memory_barrier.pNext = nullptr;
		image_memory_barrier.srcAccessMask = 0;
		image_memory_barrier.dstAccessMask = 0;
		image_memory_barrier.oldLayout = oldImageLayout;
		image_memory_barrier.newLayout = newImageLayout;
		image_memory_barrier.image = image;
		image_memory_barrier.subresourceRange.aspectMask = aspectMask;
		image_memory_barrier.subresourceRange.baseMipLevel = 0;
		image_memory_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		image_memory_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

		if (oldImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			|| oldImageLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
			image_memory_barrier.srcAccessMask =
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}

		if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}

		if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
			image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		}

		if (oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}

		if (oldImageLayout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
			image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		}

		if (newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			image_memory_barrier.srcAccessMask =
				VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}

		if (newImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			|| newImageLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
			image_memory_barrier.dstAccessMask =
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}

		if (newImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			image_memory_barrier.dstAccessMask =
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}

		VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dest_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		vkCmdPipelineBarrier(
			cmd, src_stages, dest_stages, 0, 0, nullptr, 0, nullptr,
			1, &image_memory_barrier);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////

	void SetImageLayout(
		DeviceContext& context, UnderlyingResourcePtr res,
		ImageLayout oldLayout, ImageLayout newLayout)
	{
		auto& r = *res.get();
		assert(r.GetDesc()._type == ResourceDesc::Type::Texture);
		// unforunately, we can't just blanket aspectMask with all bits enabled.
		// We must select a correct aspect mask. The nvidia drivers seem to be fine with all
		// bits enabled, but the documentation says that this is not allowed
		auto aspectMask = AsImageAspectMask(r.GetDesc()._bindFlags, r.GetDesc()._textureDesc._format);
		Metal_Vulkan::SetImageLayout(
			context.GetCommandList(),
			r.GetImage(), 
			aspectMask,
			AsVkImageLayout(oldLayout), AsVkImageLayout(newLayout));
	}

	Resource::Resource(
		const ObjectFactory& factory, const Desc& desc,
		const std::function<SubResourceInitData(unsigned, unsigned)>& initData)
	: _desc(desc)
	{
		// Our resource can either be a linear buffer, or an image
		// These correspond to the 2 types of Desc
		// We need to create the buffer/image first, so we can called vkGetXXXMemoryRequirements
		const bool hasInitData = !!initData;

		VkMemoryRequirements mem_reqs = {}; 
		if (desc._type == Desc::Type::LinearBuffer) {
			VkBufferCreateInfo buf_info = {};
			buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buf_info.pNext = nullptr;
			buf_info.usage = AsBufferUsageFlags(desc._bindFlags);
			buf_info.size = desc._linearBufferDesc._sizeInBytes;
			buf_info.queueFamilyIndexCount = 0;
			buf_info.pQueueFamilyIndices = nullptr;
			buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;   // sharing between queues
			buf_info.flags = 0;     // flags for sparse buffers

									// set this flag to enable usage with "vkCmdUpdateBuffer"
			if ((desc._cpuAccess & CPUAccess::Write)
				|| (desc._cpuAccess & CPUAccess::WriteDynamic))
				buf_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

			_underlyingBuffer = factory.CreateBuffer(buf_info);

			vkGetBufferMemoryRequirements(factory.GetDevice().get(), _underlyingBuffer.get(), &mem_reqs);
		} else {
			if (desc._type != Desc::Type::Texture)
				Throw(::Exceptions::BasicLabel("Invalid desc passed to buffer constructor"));

			const auto& tDesc = desc._textureDesc;
			const auto vkFormat = AsVkFormat(tDesc._format);

			VkImageCreateInfo image_create_info = {};
			image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image_create_info.pNext = nullptr;
			image_create_info.imageType = AsImageType(tDesc._dimensionality);
			image_create_info.format = vkFormat;
			image_create_info.extent.width = tDesc._width;
			image_create_info.extent.height = tDesc._height;
			image_create_info.extent.depth = tDesc._depth;
			image_create_info.mipLevels = std::max(1u, unsigned(tDesc._mipCount));
			image_create_info.arrayLayers = std::max(1u, unsigned(tDesc._arrayCount));
			image_create_info.samples = AsSampleCountFlagBits(tDesc._samples);
			image_create_info.queueFamilyIndexCount = 0;
			image_create_info.pQueueFamilyIndices = nullptr;
			image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image_create_info.flags = 0;

			if (tDesc._dimensionality == TextureDesc::Dimensionality::CubeMap)
				image_create_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

			// The tiling, initialLayout and usage flags depend on the bind flags and cpu/gpu usage
			// set in the input desc (and also if we have initData provided)
			// Tiling can only be OPTIMAL or LINEAR, 
			// and initialLayout can only be UNDEFINED or PREINITIALIZED at this stage
			image_create_info.tiling = (desc._cpuAccess != 0 || hasInitData) ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
			image_create_info.initialLayout = hasInitData ? VK_IMAGE_LAYOUT_PREINITIALIZED : VK_IMAGE_LAYOUT_UNDEFINED;
			image_create_info.usage = AsImageUsageFlags(desc._bindFlags);

			// minor validations
			if (image_create_info.tiling == VK_IMAGE_TILING_LINEAR && (image_create_info.usage & VK_IMAGE_USAGE_SAMPLED_BIT)) {
				const auto formatProps = factory.GetFormatProperties(vkFormat);
				const bool canSampleLinearTexture =
					!!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
				if (!canSampleLinearTexture)
					Throw(::Exceptions::BasicLabel("Hardware does not support sampling from a linear texture. A staging texture is required"));
			}

			_underlyingImage = factory.CreateImage(image_create_info);

			vkGetImageMemoryRequirements(factory.GetDevice().get(), _underlyingImage.get(), &mem_reqs);
		}

		VkMemoryPropertyFlags memoryRequirements = 0;
		if (hasInitData || desc._cpuAccess != 0)
			memoryRequirements |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		auto memoryTypeIndex = factory.FindMemoryType(mem_reqs.memoryTypeBits, memoryRequirements);
		if (memoryTypeIndex >= 32)
			Throw(::Exceptions::BasicLabel("Could not find valid memory type for buffer"));

		_mem = factory.AllocateMemory(mem_reqs.size, memoryTypeIndex);

		const VkDeviceSize offset = 0; 
		if (_underlyingBuffer) {

			// after allocation, we must initialise the data. Linear buffers don't have subresources,
			// so it's reasonably easy
			if (hasInitData) {
				auto subResData = initData(0, 0);
				if (subResData._data && subResData._size) {
					MemoryMap map(factory.GetDevice().get(), _mem.get());
					std::memcpy(map._data, subResData._data, std::min(subResData._size, (size_t)mem_reqs.size));
				}
			}

			auto res = vkBindBufferMemory(factory.GetDevice().get(), _underlyingBuffer.get(), _mem.get(), offset);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failed while binding a buffer to device memory"));
		} else if (_underlyingImage) {
			
			// Initialisation for images is more complex... We just iterate through each subresource
			// and copy in the data.
			if (hasInitData) {
				auto mipCount = std::max(1u, unsigned(desc._textureDesc._mipCount));
				auto arrayCount = std::max(1u, unsigned(desc._textureDesc._arrayCount));
				auto aspectFlags = AsImageAspectMask(desc._bindFlags, desc._textureDesc._format);
				for (unsigned m = 0; m < mipCount; ++m) {
					for (unsigned a = 0; a < arrayCount; ++a) {
						auto subResData = initData(m, a);
						if (!subResData._data || !subResData._size) continue;

						VkImageSubresource subRes = { aspectFlags, m, a };
						VkSubresourceLayout layout = {};
						vkGetImageSubresourceLayout(
							factory.GetDevice().get(), _underlyingImage.get(),
							&subRes, &layout);

						if (!layout.size) continue;	// couldn't find this subresource?

						// todo -- we should support cases where the pitches do now line up as expected!
						if (layout.rowPitch != subResData._rowPitch || layout.arrayPitch != subResData._slicePitch)
							Throw(::Exceptions::BasicLabel("Row or array pitch values are not as expected. These cases not supported."));

						// finally, map and copy
						MemoryMap map(factory.GetDevice().get(), _mem.get(), layout.offset, layout.size);
						XlCopyMemory(map._data, subResData._data, std::min(layout.size, subResData._size));
					}
				}
			}

			auto res = vkBindImageMemory(factory.GetDevice().get(), _underlyingImage.get(), _mem.get(), 0);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failed while binding an image to device memory"));
		}
	}

	Resource::Resource(
		const ObjectFactory& factory, const Desc& desc,
		const SubResourceInitData& initData)
	: Resource(factory, desc, [&initData](unsigned m, unsigned a) { return (m==0&&a==0) ? initData : SubResourceInitData{}; })
	{}
	Resource::Resource() {}
	Resource::~Resource() {}

	ResourceDesc ExtractDesc(UnderlyingResourcePtr res)
	{
		return res.get()->GetDesc();
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	void Copy(DeviceContext& context, UnderlyingResourcePtr dst, UnderlyingResourcePtr src, ImageLayout dstLayout, ImageLayout srcLayout)
	{
		// Each mipmap is treated as a separate copy operation (but multiple array layers can be handled
		// in a single operation).
		// The Vulkan API requires that the formats of each resource must be reasonably similiar
		//		-- in practice, that means that the size of the pixels in both cases must be the same.
		//		When copying between compressed and uncompressed images, the uncompressed pixel size must
		//		be equal to the compressed block size.
		assert(src.get() && dst.get());
		const auto& srcDesc = src.get()->GetDesc();
		const auto& dstDesc = dst.get()->GetDesc();
		assert(srcDesc._type == Resource::Desc::Type::Texture);
		assert(dstDesc._type == Resource::Desc::Type::Texture);

		VkImageCopy copyOps[16];

		unsigned copyOperations = 0;
		auto dstAspectMask = AsImageAspectMask(dstDesc._bindFlags, dstDesc._textureDesc._format);
		auto srcAspectMask = AsImageAspectMask(srcDesc._bindFlags, srcDesc._textureDesc._format);
		assert(srcAspectMask == dstAspectMask);

		auto arrayCount = std::max(1u, (unsigned)srcDesc._textureDesc._arrayCount);
		assert(arrayCount == std::max(1u, (unsigned)dstDesc._textureDesc._arrayCount));
		
		auto mips = std::max(1u, (unsigned)std::min(srcDesc._textureDesc._mipCount, dstDesc._textureDesc._mipCount));
		assert(mips <= dimof(copyOps));
		auto width = srcDesc._textureDesc._width, height = srcDesc._textureDesc._height, depth = srcDesc._textureDesc._depth;
		for (unsigned m = 0; m < mips; ++m) {
			auto& c = copyOps[copyOperations++];
			c.srcOffset = VkOffset3D { 0, 0, 0 };
			c.dstOffset = VkOffset3D { 0, 0, 0 };
			c.extent = VkExtent3D { width, height, depth };
			c.srcSubresource.aspectMask = srcAspectMask;
			c.srcSubresource.mipLevel = m;
			c.srcSubresource.baseArrayLayer = 0;
			c.srcSubresource.layerCount = arrayCount;
			c.dstSubresource.aspectMask = dstAspectMask;
			c.dstSubresource.mipLevel = m;
			c.dstSubresource.baseArrayLayer = 0;
			c.dstSubresource.layerCount = arrayCount;

			width = std::max(1u, width>>1);
			height = std::max(1u, height>>1);
			depth = std::max(1u, depth>>1);
		}

		vkCmdCopyImage(
			context.GetCommandList(),
			src.get()->GetImage(), AsVkImageLayout(srcLayout),
			dst.get()->GetImage(), AsVkImageLayout(dstLayout),
			copyOperations, copyOps);
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	MemoryMap::MemoryMap(
		VkDevice dev, VkDeviceMemory memory,
		VkDeviceSize offset, VkDeviceSize size)
		: _dev(dev), _mem(memory)
	{
		// There are many restrictions on this call -- see the Vulkan docs.
		// * we must ensure that the memory was allocated with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		// * we must ensure that the memory was allocated with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		//          (because we're not performing manual memory flushes)
		// * we must ensure that it is not being used by the GPU during the map
		auto res = vkMapMemory(dev, memory, offset, size, 0, &_data);
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failed while mapping device memory"));
	}

	MemoryMap::MemoryMap(
		VkDevice dev, UnderlyingResourcePtr resource,
		VkDeviceSize offset, VkDeviceSize size)
	: MemoryMap(dev, resource.get()->GetMemory(), offset, size)
	{}

	void MemoryMap::TryUnmap()
	{
		vkUnmapMemory(_dev, _mem);
	}

	MemoryMap::MemoryMap() : _dev(nullptr), _mem(nullptr), _data(nullptr) {}

	MemoryMap::~MemoryMap()
	{
		TryUnmap();
	}

	MemoryMap::MemoryMap(MemoryMap&& moveFrom)
	{
		_data = moveFrom._data; moveFrom._data = nullptr;
		_dev = moveFrom._dev; moveFrom._dev = nullptr;
		_mem = moveFrom._mem; moveFrom._mem = nullptr;
	}

	MemoryMap& MemoryMap::operator=(MemoryMap&& moveFrom)
	{
		TryUnmap();
		_data = moveFrom._data; moveFrom._data = nullptr;
		_dev = moveFrom._dev; moveFrom._dev = nullptr;
		_mem = moveFrom._mem; moveFrom._mem = nullptr;
		return *this;
	}


}}

