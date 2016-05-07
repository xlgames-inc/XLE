// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Resource.h"
#include "ObjectFactory.h"
#include "Format.h"
#include "DeviceContext.h"
#include "TextureView.h"
#include "../IDeviceVulkan.h"
#include "../../ResourceUtils.h"
#include "../../Format.h"
#include "../../Utility/BitUtils.h"
#include "../../Utility/MemoryUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{
    static VkDevice ExtractUnderlyingDevice(IDevice& idev)
    {
        auto* vulkanDevice = (RenderCore::IDeviceVulkan*)idev.QueryInterface(__uuidof(RenderCore::IDeviceVulkan));
		return vulkanDevice ? vulkanDevice->GetUnderlyingDevice() : nullptr;
    }

	static VkBufferUsageFlags AsBufferUsageFlags(BindFlag::BitField bindFlags)
	{
		VkBufferUsageFlags result = 0;
		if (bindFlags & BindFlag::VertexBuffer) result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		if (bindFlags & BindFlag::IndexBuffer) result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		if (bindFlags & BindFlag::ConstantBuffer) result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		if (bindFlags & BindFlag::DrawIndirectArgs) result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        if (bindFlags & BindFlag::TransferSrc) result |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if (bindFlags & BindFlag::TransferDst) result |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (bindFlags & BindFlag::StructuredBuffer) result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		// Other Vulkan flags:
		// VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
		// VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
		return result;
	}
	
	static VkImageUsageFlags AsImageUsageFlags(BindFlag::BitField bindFlags)
	{
		// note -- we're assuming shader resources are sampled here (rather than storage type textures)
		VkImageUsageFlags result = 0;
		if (bindFlags & BindFlag::ShaderResource) result |= VK_IMAGE_USAGE_SAMPLED_BIT;
		if (bindFlags & BindFlag::RenderTarget) result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		if (bindFlags & BindFlag::DepthStencil) result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		if (bindFlags & BindFlag::UnorderedAccess) result |= VK_IMAGE_USAGE_STORAGE_BIT;
        if (bindFlags & BindFlag::TransferSrc) result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (bindFlags & BindFlag::TransferDst) result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		// Other Vulkan flags:
		// VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
		// VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT
		return result;
	}

	VkImageAspectFlags AsImageAspectMask(Format fmt)
	{
        // if (view_info.format == VK_FORMAT_D16_UNORM_S8_UINT ||
        //     view_info.format == VK_FORMAT_D24_UNORM_S8_UINT ||
        //     view_info.format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
        //     view_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        // }

        auto components = GetComponents(fmt);
        switch (components) {
        case FormatComponents::Depth:           return VK_IMAGE_ASPECT_DEPTH_BIT;
        case FormatComponents::DepthStencil:    return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        case FormatComponents::Stencil:         return VK_IMAGE_ASPECT_STENCIL_BIT;
        default:                                return VK_IMAGE_ASPECT_COLOR_BIT;
        }
	}

	static VkImageLayout AsVkImageLayout(ImageLayout input) { return (VkImageLayout)input; }

	VkSampleCountFlagBits AsSampleCountFlagBits(TextureSamples samples)
	{
        // we just want to isolate the most significant bit. If it's already a power
        // of two, then we can just return as is.
        assert(IsPowerOfTwo(samples._sampleCount));
        assert(samples._sampleCount > 0);
        return (VkSampleCountFlagBits)samples._sampleCount;
	}

	static VkImageType AsImageType(TextureDesc::Dimensionality dims)
	{
		switch (dims) {
		case TextureDesc::Dimensionality::T1D: return VK_IMAGE_TYPE_1D;
		default:
		case TextureDesc::Dimensionality::T2D: return VK_IMAGE_TYPE_2D;
		case TextureDesc::Dimensionality::T3D: return VK_IMAGE_TYPE_3D;
		case TextureDesc::Dimensionality::CubeMap: return VK_IMAGE_TYPE_2D;
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////

    static VkAccessFlags GetAccessForOldLayout(VkImageLayout oldImageLayout)
    {
        VkAccessFlags flags = 0;
        if (oldImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			|| oldImageLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
			flags =
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}

        if (oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			flags = VK_ACCESS_TRANSFER_WRITE_BIT;
		}

        if (oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
			flags = VK_ACCESS_TRANSFER_READ_BIT;
		}

		if (oldImageLayout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
			flags = VK_ACCESS_HOST_WRITE_BIT;
		}

        if (oldImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			flags = VK_ACCESS_SHADER_READ_BIT;
            // (or VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)
		}

        // note --  the "General" case is tricky here! General is used for storage buffers, which
        //          can be read or written. It's also used for transfers that read and write from
        //          the same buffer. And it can be used when mapping textures.
        //          So we need to lay down some blanket flags...
        if (oldImageLayout == VK_IMAGE_LAYOUT_GENERAL) {
			flags = 
                  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
                | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT
                | VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
		}

        if (oldImageLayout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
            flags = VK_ACCESS_HOST_WRITE_BIT;
        }

        return flags;
    }

    static VkAccessFlags GetAccessForNewLayout(VkImageLayout newImageLayout)
    {
        VkAccessFlags flags = 0;
        if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			flags = VK_ACCESS_TRANSFER_WRITE_BIT;
		}

		if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
			flags = VK_ACCESS_TRANSFER_READ_BIT;
		}

		if (newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            // These flags are set in the samples, but we're handling when switching
            // away from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_PREINITIALIZED
			// image_memory_barrier.srcAccessMask =
			// 	VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			flags = VK_ACCESS_SHADER_READ_BIT;
		}

		if (newImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			|| newImageLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
			flags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}

		if (newImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			flags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}

        if (newImageLayout == VK_IMAGE_LAYOUT_GENERAL) {
			flags = 
                  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
                | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT
                | VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
		}
        return flags;
    }

	void SetImageLayouts(
		DeviceContext& context, 
        IteratorRange<const LayoutTransition*> changes)
	{
        VkImageMemoryBarrier barriers[16];
        assert(changes.size() > 0 && changes.size() < dimof(barriers));

        unsigned barrierCount = 0;
        for (unsigned c=0; c<(unsigned)changes.size(); ++c) {
		    auto& r = *changes[c]._res.get();
		    assert(r.GetDesc()._type == ResourceDesc::Type::Texture);
            if (!r.GetImage()) continue;   // (staging buffer case)

            auto& b = barriers[barrierCount++];

		    // unforunately, we can't just blanket aspectMask with all bits enabled.
		    // We must select a correct aspect mask. The nvidia drivers seem to be fine with all
		    // bits enabled, but the documentation says that this is not allowed
            const auto& desc = r.GetDesc();

            b = {};
		    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		    b.pNext = nullptr;
		    b.oldLayout = AsVkImageLayout(changes[c]._oldLayout);
		    b.newLayout = AsVkImageLayout(changes[c]._newLayout);
            b.srcAccessMask = GetAccessForOldLayout(b.oldLayout);
		    b.dstAccessMask = GetAccessForNewLayout(b.newLayout);
		    b.image = r.GetImage();
		    b.subresourceRange.aspectMask = AsImageAspectMask(desc._textureDesc._format);
		    b.subresourceRange.baseMipLevel = 0;
		    b.subresourceRange.levelCount = std::max(1u, (unsigned)desc._textureDesc._mipCount);
		    b.subresourceRange.layerCount = std::max(1u, (unsigned)desc._textureDesc._arrayCount);
        }

        if (barrierCount) {
            const VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            const VkPipelineStageFlags dest_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            context.CmdPipelineBarrier(
                src_stages, dest_stages,
                0, 
                0, nullptr, 0, nullptr,
                barrierCount, barriers);
        }
	}

    static VulkanSharedPtr<VkDeviceMemory> AllocateDeviceMemory(
        const Metal_Vulkan::ObjectFactory& factory, VkMemoryRequirements memReqs, VkMemoryPropertyFlags requirementMask)
    {
        auto type = factory.FindMemoryType(memReqs.memoryTypeBits, requirementMask);
        if (type >= 32)
            Throw(Exceptions::BasicLabel("Could not find compatible memory type for image"));
        return factory.AllocateMemory(memReqs.size, type);
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
			if ((desc._cpuAccess & CPUAccess::Write) || (desc._cpuAccess & CPUAccess::WriteDynamic))
				buf_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

			_underlyingBuffer = factory.CreateBuffer(buf_info);

			vkGetBufferMemoryRequirements(factory.GetDevice().get(), _underlyingBuffer.get(), &mem_reqs);
		} else {
			if (desc._type != Desc::Type::Texture)
				Throw(::Exceptions::BasicLabel("Invalid desc passed to buffer constructor"));

			const auto& tDesc = desc._textureDesc;
			const auto vkFormat = AsVkFormat(tDesc._format);

			assert(vkFormat != VK_FORMAT_UNDEFINED);

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

            if (HasLinearAndSRGBFormats(tDesc._format))
                image_create_info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

			// The tiling, initialLayout and usage flags depend on the bind flags and cpu/gpu usage
			// set in the input desc (and also if we have initData provided)
			// Tiling can only be OPTIMAL or LINEAR, 
			// and initialLayout can only be UNDEFINED or PREINITIALIZED at this stage
			image_create_info.tiling = (desc._cpuAccess != 0 || hasInitData) ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
			image_create_info.initialLayout = hasInitData ? VK_IMAGE_LAYOUT_PREINITIALIZED : VK_IMAGE_LAYOUT_UNDEFINED;
			image_create_info.usage = AsImageUsageFlags(desc._bindFlags);

			// minor validations
            if (image_create_info.tiling == VK_IMAGE_TILING_OPTIMAL && (image_create_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
                // For depth/stencil textures, if the device doesn't support optimal tiling, they switch back to linear
                // Maybe this is unnecessary, because the device could just define "optimal" to mean linear in this case.
                // But the vulkan samples do something similar (though they prefer to use linear mode when it's available...?)
                const auto formatProps = factory.GetFormatProperties(vkFormat);
                if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
                    image_create_info.tiling = VK_IMAGE_TILING_LINEAR;
                    if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
                        Throw(Exceptions::BasicLabel("Format (%i) can't be used for a depth stencil", unsigned(vkFormat)));
                }
            }

			if (image_create_info.tiling == VK_IMAGE_TILING_LINEAR && (image_create_info.usage & VK_IMAGE_USAGE_SAMPLED_BIT)) {
				const auto formatProps = factory.GetFormatProperties(vkFormat);
				const bool canSampleLinearTexture =
					!!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
				if (!canSampleLinearTexture)
					Throw(::Exceptions::BasicLabel("Hardware does not support sampling from a linear texture. A staging texture is required"));
			}

            // When constructing a staging (or readback) texture with multiple mip levels or array layers,
            // we must actually allocate a "buffer". We will treat this buffer as a linear texture, and we
            // will manually layout the miplevels within the device memory.
            //
            // This is because Vulkan doesn't support creating VK_IMAGE_TILING_LINEAR with more than 1
            // mip level or array layers. And linear texture must be 2D (they can't be 1d or 3d textures)
			// However, our solution more or less emulates what would happen
            // if it did...?! (Except, of course, we can never bind it as a sampled texture)
            //
            // See (for example) this post from nvidia:
            // https://devtalk.nvidia.com/default/topic/926085/texture-memory-management/

            if (image_create_info.tiling == VK_IMAGE_TILING_LINEAR && (desc._gpuAccess == 0)) {

                VkBufferCreateInfo buf_info = {};
			    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			    buf_info.pNext = nullptr;
			    buf_info.usage = AsBufferUsageFlags(desc._bindFlags);
			    buf_info.size = ByteCount(tDesc);
			    buf_info.queueFamilyIndexCount = 0;
			    buf_info.pQueueFamilyIndices = nullptr;
			    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			    buf_info.flags = 0;

                _underlyingBuffer = factory.CreateBuffer(buf_info);
    			vkGetBufferMemoryRequirements(factory.GetDevice().get(), _underlyingBuffer.get(), &mem_reqs);

            } else {
    			_underlyingImage = factory.CreateImage(image_create_info);
	    		vkGetImageMemoryRequirements(factory.GetDevice().get(), _underlyingImage.get(), &mem_reqs);
            }
		}

        const auto hostVisibleReqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        auto memoryRequirements = (hasInitData || desc._cpuAccess != 0) ? hostVisibleReqs : 0;
        _mem = AllocateDeviceMemory(factory, mem_reqs, memoryRequirements);

		const VkDeviceSize offset = 0; 
		if (_underlyingBuffer) {

			// After allocation, we must initialise the data. True linear buffers don't have subresources,
			// so it's reasonably easy. But if this buffer is really a staging texture, then we need to 
            // copy in all of the sub resources.
			if (hasInitData) {
                if (desc._type == Desc::Type::LinearBuffer) {
				    auto subResData = initData(0, 0);
				    if (subResData._data && subResData._size) {
					    ResourceMap map(factory.GetDevice().get(), _mem.get());
					    std::memcpy(map.GetData(), subResData._data, std::min(subResData._size, (size_t)mem_reqs.size));
				    }
                } else {
                    // This is the staging texture path. Rather that getting the arrangement of subresources from
                    // the VkImage, we specify it ourselves.
                    CopyViaMemoryMap(
                        factory.GetDevice().get(), nullptr, _mem.get(), 
                        _desc._textureDesc, initData);
                }
			}

			auto res = vkBindBufferMemory(factory.GetDevice().get(), _underlyingBuffer.get(), _mem.get(), offset);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failed while binding a buffer to device memory"));
		} else if (_underlyingImage) {
			
			// Initialisation for images is more complex... We just iterate through each subresource
			// and copy in the data. We're going to do this with a single map -- assuming that all
            // if any of the subresources have init data, then they must all have. 
            // Alternatively, we could map each subresource separately... But there doesn't seem to
            // be any advantage to that.
			if (hasInitData) {
                CopyViaMemoryMap(
                    factory.GetDevice().get(), _underlyingImage.get(), _mem.get(), 
                    _desc._textureDesc, initData);
			}

			auto res = vkBindImageMemory(factory.GetDevice().get(), _underlyingImage.get(), _mem.get(), 0);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failed while binding an image to device memory"));
		}
	}

	Resource::Resource(
		const ObjectFactory& factory, const Desc& desc,
		const SubResourceInitData& initData)
	: Resource(factory, desc, (initData._size && initData._data) ? ([&initData](unsigned m, unsigned a) { return (m==0&&a==0) ? initData : SubResourceInitData{}; }) : std::function<SubResourceInitData(unsigned, unsigned)>())
	{}
	Resource::Resource() {}
	Resource::~Resource() {}

	ResourceDesc ExtractDesc(UnderlyingResourcePtr res)
	{
		return res.get()->GetDesc();
	}

	ResourceDesc ExtractDesc(const TextureView& res)
	{
		auto resource = res.GetResource();
		if (!resource) return ResourceDesc();
		return ExtractDesc(resource);
	}

	RenderCore::ResourcePtr ExtractResource(const TextureView& res)
	{
		return res.ShareResource();
	}

    namespace Internal
	{
		class ResourceAllocator : public std::allocator<Metal_Vulkan::Resource>
		{
		public:
			pointer allocate(size_type n, std::allocator<void>::const_pointer ptr)
			{
				Throw(::Exceptions::BasicLabel("Allocation attempted via ResourceAllocator"));
			}

			void deallocate(pointer p, size_type n)
			{
				delete (Metal_Vulkan::Resource*)p;
			}
		};
	}

    ResourcePtr Resource::Allocate(
        const ObjectFactory& factory,
		const ResourceDesc& desc,
		const std::function<SubResourceInitData(unsigned, unsigned)>& initData)
    {
        const bool useAllocateShared = true;
		if (constant_expression<useAllocateShared>::result()) {
			auto res = std::allocate_shared<Metal_Vulkan::Resource>(
				Internal::ResourceAllocator(),
				std::ref(factory), std::ref(desc), std::ref(initData));
			return *reinterpret_cast<ResourcePtr*>(&res);
		}
		else {
			auto res = std::make_unique<Metal_Vulkan::Resource>(factory, desc, initData);
			return ResourcePtr(
				(RenderCore::Resource*)res.release(),
				[](RenderCore::Resource* res) { delete (Metal_Vulkan::Resource*)res; });
		}
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	void Copy(DeviceContext& context, UnderlyingResourcePtr dst, UnderlyingResourcePtr src, ImageLayout dstLayout, ImageLayout srcLayout)
	{
		assert(src.get() && dst.get());
        if (dst.get()->GetImage() && src.get()->GetImage()) {
            // image to image copy

            // Each mipmap is treated as a separate copy operation (but multiple array layers can be handled
		    // in a single operation).
		    // The Vulkan API requires that the formats of each resource must be reasonably similiar
		    //		-- in practice, that means that the size of the pixels in both cases must be the same.
		    //		When copying between compressed and uncompressed images, the uncompressed pixel size must
		    //		be equal to the compressed block size.

		    const auto& srcDesc = src.get()->GetDesc();
		    const auto& dstDesc = dst.get()->GetDesc();
		    assert(srcDesc._type == Resource::Desc::Type::Texture);
		    assert(dstDesc._type == Resource::Desc::Type::Texture);

		    VkImageCopy copyOps[16];

		    unsigned copyOperations = 0;
		    auto dstAspectMask = AsImageAspectMask(dstDesc._textureDesc._format);
		    auto srcAspectMask = AsImageAspectMask(srcDesc._textureDesc._format);

		    auto arrayCount = std::max(1u, (unsigned)srcDesc._textureDesc._arrayCount);
		    assert(arrayCount == std::max(1u, (unsigned)dstDesc._textureDesc._arrayCount));
		
		    auto mips = std::max(1u, (unsigned)std::min(srcDesc._textureDesc._mipCount, dstDesc._textureDesc._mipCount));
		    assert(mips <= dimof(copyOps));
		    auto width = srcDesc._textureDesc._width, height = srcDesc._textureDesc._height, depth = srcDesc._textureDesc._depth;
		    for (unsigned m = 0; m < mips; ++m) {
                assert(copyOperations < dimof(copyOps));
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

		    context.CmdCopyImage(
			    src.get()->GetImage(), AsVkImageLayout(srcLayout),
			    dst.get()->GetImage(), AsVkImageLayout(dstLayout),
			    copyOperations, copyOps);

        } else if (dst.get()->GetBuffer() && src.get()->GetBuffer()) {
            // buffer to buffer copy
            const auto& srcDesc = src.get()->GetDesc();
		    const auto& dstDesc = dst.get()->GetDesc();
            assert(srcDesc._type == Resource::Desc::Type::Texture);
		    assert(dstDesc._type == Resource::Desc::Type::Texture);
            VkBufferCopy copyOps[] = 
            {
                VkBufferCopy{0, 0, std::min(srcDesc._linearBufferDesc._sizeInBytes, dstDesc._linearBufferDesc._sizeInBytes)}
            };
            context.CmdCopyBuffer(
                src.get()->GetBuffer(),
                dst.get()->GetBuffer(),
                dimof(copyOps), copyOps);
        } else if (dst.get()->GetImage() && src.get()->GetBuffer()) {
            // This copy operation is typically used when initializing a texture via staging
            // resource. The buffer probably has a "Texture" type Desc, even though the underlying
            // resource is a buffer.
            if (src.get()->GetDesc()._type != ResourceDesc::Type::Texture)
                Throw(::Exceptions::BasicLabel("Buffer to image copy not implemented, except for staging resources"));

            const auto& srcDesc = src.get()->GetDesc();
		    const auto& dstDesc = dst.get()->GetDesc();
		    assert(srcDesc._type == Resource::Desc::Type::Texture);
		    assert(dstDesc._type == Resource::Desc::Type::Texture);

            auto dstAspectMask = AsImageAspectMask(dstDesc._textureDesc._format);

            VkBufferImageCopy copyOps[96];

            auto arrayCount = std::max(1u, (unsigned)srcDesc._textureDesc._arrayCount);
		    auto mips = std::max(1u, (unsigned)std::min(srcDesc._textureDesc._mipCount, dstDesc._textureDesc._mipCount));
            unsigned width = srcDesc._textureDesc._width, height = srcDesc._textureDesc._height, depth = srcDesc._textureDesc._depth;
            auto minDims = (GetCompressionType(srcDesc._textureDesc._format) == FormatCompressionType::BlockCompression) ? 4u : 1u;

            assert(dstDesc._textureDesc._width == width);
            assert(dstDesc._textureDesc._height == height);
            assert(dstDesc._textureDesc._depth == depth);
		    assert(mips*arrayCount <= dimof(copyOps));

            for (unsigned m=0; m<mips; ++m) {
                auto mipOffset = GetSubResourceOffset(srcDesc._textureDesc, m, 0);
                for (unsigned a=0; a<arrayCount; ++a) {
                    auto& c = copyOps[m+a*mips];
                    c.bufferOffset = mipOffset._offset + mipOffset._pitches._arrayPitch * a;
                    c.bufferRowLength = std::max(width, minDims);
                    c.bufferImageHeight = std::max(height, minDims);
                    c.imageSubresource = VkImageSubresourceLayers{ dstAspectMask, m, a, 1 };
                    c.imageOffset = VkOffset3D{0,0,0};
                    c.imageExtent = VkExtent3D{std::max(width, minDims), std::max(height, minDims), depth};
                }

                width >>= 1u;
                height >>= 1u;
                depth >>= 1u;
            }

            const auto copyOperations = mips*arrayCount;
            context.CmdCopyBufferToImage(
                src.get()->GetBuffer(),
                dst.get()->GetImage(), AsVkImageLayout(dstLayout),
                copyOperations, copyOps);
        } else {
            // copies from buffer to image, or image to buffer are supported by Vulkan, but
            // not implemented here.
            Throw(::Exceptions::BasicLabel("Image to buffer copy not implemented"));
        }
	}

    void CopyPartial(
        DeviceContext& context, 
        const CopyPartial_Dest& dst, const CopyPartial_Src& src,
        ImageLayout dstLayout, ImageLayout srcLayout)
    {
        assert(src._resource && dst._resource);
        if (dst._resource->GetImage() && src._resource->GetImage()) {
            // image to image copy
            // In this case, we're going to generate only a single copy operation. This is 
            // similar to CopySubresourceRegion in D3D

            const auto& srcDesc = src._resource->GetDesc();
		    const auto& dstDesc = dst._resource->GetDesc();
		    assert(srcDesc._type == Resource::Desc::Type::Texture);
		    assert(dstDesc._type == Resource::Desc::Type::Texture);

		    auto dstAspectMask = AsImageAspectMask(dstDesc._textureDesc._format);
		    auto srcAspectMask = AsImageAspectMask(srcDesc._textureDesc._format);

            VkImageCopy c;
            c.srcOffset = VkOffset3D { (int)src._leftTopFront._values[0], (int)src._leftTopFront._values[1], (int)src._leftTopFront._values[2] };
			c.dstOffset = VkOffset3D { (int)dst._leftTopFront._values[0], (int)dst._leftTopFront._values[1], (int)dst._leftTopFront._values[2] };
			c.extent = VkExtent3D { 
                src._rightBottomBack._values[0] - src._leftTopFront._values[0],
                src._rightBottomBack._values[1] - src._leftTopFront._values[1],
                src._rightBottomBack._values[2] - src._leftTopFront._values[2]
            };
			c.srcSubresource.aspectMask = srcAspectMask;
			c.srcSubresource.mipLevel = src._subResource._mip;
			c.srcSubresource.baseArrayLayer = src._subResource._arrayLayer;
			c.srcSubresource.layerCount = 1;
			c.dstSubresource.aspectMask = dstAspectMask;
			c.dstSubresource.mipLevel = dst._subResource._mip;
			c.dstSubresource.baseArrayLayer = dst._subResource._arrayLayer;
			c.dstSubresource.layerCount = 1;

            context.CmdCopyImage(
			    src._resource->GetImage(), AsVkImageLayout(srcLayout),
			    dst._resource->GetImage(), AsVkImageLayout(dstLayout),
			    1, &c);
        } else if (dst._resource->GetBuffer() && src._resource->GetBuffer()) {
            // buffer to buffer copy
            const auto& srcDesc = src._resource->GetDesc();
		    const auto& dstDesc = dst._resource->GetDesc();
            assert(srcDesc._type == Resource::Desc::Type::Texture);
		    assert(dstDesc._type == Resource::Desc::Type::Texture);
            VkBufferCopy c;
            c.srcOffset = src._leftTopFront._values[0];
            c.dstOffset = dst._leftTopFront._values[0];
            auto end = std::min(src._rightBottomBack._values[0], std::min(srcDesc._linearBufferDesc._sizeInBytes, dstDesc._linearBufferDesc._sizeInBytes));
            c.size = end - src._rightBottomBack._values[0];
            context.CmdCopyBuffer(
                src._resource->GetBuffer(),
                dst._resource->GetBuffer(),
                1, &c);
        } else if (dst._resource->GetImage() && src._resource->GetBuffer()) {
            // This copy operation is typically used when initializing a texture via staging
            // resource. The buffer probably has a "Texture" type Desc, even though the underlying
            // resource is a buffer.
            if (src._resource->GetDesc()._type != ResourceDesc::Type::Texture)
                Throw(::Exceptions::BasicLabel("Buffer to image copy not implemented, except for staging resources"));

            const auto& srcDesc = src._resource->GetDesc();
		    const auto& dstDesc = dst._resource->GetDesc();
		    assert(srcDesc._type == Resource::Desc::Type::Texture);
		    assert(dstDesc._type == Resource::Desc::Type::Texture);

            auto dstAspectMask = AsImageAspectMask(dstDesc._textureDesc._format);

            VkBufferImageCopy copyOps[8];

            auto arrayCount = std::max(1u, (unsigned)srcDesc._textureDesc._arrayCount);
		    auto mips = std::max(1u, (unsigned)std::min(srcDesc._textureDesc._mipCount, dstDesc._textureDesc._mipCount));
            unsigned width = srcDesc._textureDesc._width, height = srcDesc._textureDesc._height, depth = srcDesc._textureDesc._depth;
            auto minDims = (GetCompressionType(srcDesc._textureDesc._format) == FormatCompressionType::BlockCompression) ? 4u : 1u;

            assert(dstDesc._textureDesc._width == width);
            assert(dstDesc._textureDesc._height == height);
            assert(dstDesc._textureDesc._depth == depth);
		    assert(mips*arrayCount <= dimof(copyOps));

            // todo -- not adjusting the offsets/extents for mipmaps. This won't
            // work correctly in the mipmapped case.
            assert(mips <= 1);
            for (unsigned m=0; m<mips; ++m) {
                auto mipOffset = GetSubResourceOffset(srcDesc._textureDesc, m, 0);
                for (unsigned a=0; a<arrayCount; ++a) {
                    auto& c = copyOps[m+a*mips];
                    c.bufferOffset = mipOffset._offset + mipOffset._pitches._arrayPitch * a;
                    if (src._leftTopFront[0] != ~0u) {
                        c.bufferOffset += 
                              src._leftTopFront[2] * mipOffset._pitches._slicePitch
                            + src._leftTopFront[1] * mipOffset._pitches._rowPitch
                            + src._leftTopFront[0] * BitsPerPixel(srcDesc._textureDesc._format) / 8;
                    }
                    c.bufferRowLength = std::max(width, minDims);
                    c.bufferImageHeight = std::max(height, minDims);

                    c.imageSubresource = VkImageSubresourceLayers{ dstAspectMask, m, a, 1 };
                    c.imageOffset = VkOffset3D{(int32_t)dst._leftTopFront[0], (int32_t)dst._leftTopFront[1], (int32_t)dst._leftTopFront[2]};

                    if (src._leftTopFront[0] != ~0u && src._rightBottomBack[0] != ~0u) {
                        c.imageExtent = VkExtent3D{
                            src._rightBottomBack[0] - src._leftTopFront[0],
                            src._rightBottomBack[1] - src._leftTopFront[1],
                            src._rightBottomBack[2] - src._leftTopFront[2]};
                    } else {
                        c.imageExtent = VkExtent3D{
                            srcDesc._textureDesc._width,
                            srcDesc._textureDesc._height,
                            srcDesc._textureDesc._depth};
                    }
                }

                width >>= 1u;
                height >>= 1u;
                depth >>= 1u;
            }

            const auto copyOperations = mips*arrayCount;
            context.CmdCopyBufferToImage(
                src._resource->GetBuffer(),
                dst._resource->GetImage(), AsVkImageLayout(dstLayout),
                copyOperations, copyOps);
        } else {
            // copies from buffer to image, or image to buffer are supported by Vulkan, but
            // not implemented here.
            Throw(::Exceptions::BasicLabel("Buffer to image and image to buffer copy not implemented"));
        }
    }

    unsigned CopyViaMemoryMap(
        VkDevice device, VkImage image, VkDeviceMemory mem,
        const TextureDesc& desc,
        const std::function<SubResourceInitData(unsigned, unsigned)>& initData)
    {
        // Copy all of the subresources to device member, using a MemoryMap path.
        // If "image" is not null, we will get the arrangement of subresources from
        // the images. Otherwise, we will use a default arrangement of subresources.
        ResourceMap map(device, mem);
        unsigned bytesUploaded = 0;

		auto mipCount = std::max(1u, unsigned(desc._mipCount));
		auto arrayCount = std::max(1u, unsigned(desc._arrayCount));
		auto aspectFlags = AsImageAspectMask(desc._format);
		for (unsigned m = 0; m < mipCount; ++m) {
            auto mipDesc = CalculateMipMapDesc(desc, m);
			for (unsigned a = 0; a < arrayCount; ++a) {
				auto subResData = initData(m, a);
				if (!subResData._data || !subResData._size) continue;

				VkSubresourceLayout layout = {};
                if (image) {
                    VkImageSubresource subRes = { aspectFlags, m, a };
				    vkGetImageSubresourceLayout(
					    device, image,
					    &subRes, &layout);
                } else {
                    auto offset = GetSubResourceOffset(desc, m, a);
                    layout = VkSubresourceLayout { 
                        offset._offset, offset._size, 
                        offset._pitches._rowPitch, offset._pitches._arrayPitch, offset._pitches._slicePitch };
                }

				if (!layout.size) continue;	// couldn't find this subresource?

                CopyMipLevel(
                    PtrAdd(map.GetData(), layout.offset), size_t(layout.size),
                    TexturePitches{unsigned(layout.rowPitch), unsigned(layout.depthPitch), unsigned(layout.arrayPitch)},
                    mipDesc, subResData);
			}
		}
        return bytesUploaded;
    }

    unsigned CopyViaMemoryMap(
        IDevice& dev, UnderlyingResourcePtr resource,
        const std::function<SubResourceInitData(unsigned, unsigned)>& initData)
    {
        assert(resource.get()->GetDesc()._type == ResourceDesc::Type::Texture);
        return CopyViaMemoryMap(
            ExtractUnderlyingDevice(dev), resource.get()->GetImage(), resource.get()->GetMemory(),
            resource.get()->GetDesc()._textureDesc, initData);
    }

	ResourcePtr Duplicate(ObjectFactory&, UnderlyingResourcePtr inputResource) 
    { 
        Throw(::Exceptions::BasicLabel("Resource duplication not implemented"));
    }

	ResourcePtr Duplicate(DeviceContext&, UnderlyingResourcePtr inputResource)
	{
		Throw(::Exceptions::BasicLabel("Resource duplication not implemented"));
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	ResourceMap::ResourceMap(
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

        // we don't actually know the size or pitches in this case
        _dataSize = 0;
        _pitches = {};
	}

	ResourceMap::ResourceMap(
		IDevice& idev, UnderlyingResourcePtr resource,
        Resource::SubResource subResource,
		VkDeviceSize offset, VkDeviceSize size)
	{
        auto dev = ExtractUnderlyingDevice(idev);

        VkDeviceSize finalOffset = offset, finalSize = size;
        _pitches = Pitches { unsigned(size), unsigned(size) };

        // special case for images, where we need to take into account the requested "subresource"
        auto* image = resource.get()->GetImage();
        const auto& desc = resource.get()->GetDesc();
        if (image) {
            auto aspectMask = AsImageAspectMask(desc._textureDesc._format);
            VkImageSubresource sub = { aspectMask, subResource._mip, subResource._arrayLayer };
            VkSubresourceLayout layout = {};
            vkGetImageSubresourceLayout(dev, image, &sub, &layout);
            finalOffset += layout.offset;
            finalSize = std::min(layout.size, finalSize);
            _pitches = Pitches { unsigned(layout.rowPitch), unsigned(layout.depthPitch) };
            _dataSize = finalSize;
        } else {
            if (desc._type == ResourceDesc::Type::Texture) {
                // This is the staging texture case. We can use GetSubResourceOffset to
                // calculate the arrangement of subresources
                auto subResOffset = GetSubResourceOffset(desc._textureDesc, subResource._mip, subResource._arrayLayer);
                finalOffset = subResOffset._offset;
                finalSize = subResOffset._size;
                _pitches = subResOffset._pitches;
                _dataSize = finalSize;
            } else {
                _dataSize = desc._linearBufferDesc._sizeInBytes;
            }
        }

        auto res = vkMapMemory(dev, resource.get()->GetMemory(), finalOffset, finalSize, 0, &_data);
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failed while mapping device memory"));

        _dev = dev;
        _mem = resource.get()->GetMemory();
    }

	void ResourceMap::TryUnmap()
	{
        if (_dev && _mem)
		    vkUnmapMemory(_dev, _mem);
	}

    ResourceMap::ResourceMap() : _dev(nullptr), _mem(nullptr), _data(nullptr), _pitches{} {}

	ResourceMap::~ResourceMap()
	{
		TryUnmap();
	}

	ResourceMap::ResourceMap(ResourceMap&& moveFrom)
	{
		_data = moveFrom._data; moveFrom._data = nullptr;
		_dev = moveFrom._dev; moveFrom._dev = nullptr;
		_mem = moveFrom._mem; moveFrom._mem = nullptr;
        _pitches = moveFrom._pitches;
	}

	ResourceMap& ResourceMap::operator=(ResourceMap&& moveFrom)
	{
		TryUnmap();
		_data = moveFrom._data; moveFrom._data = nullptr;
		_dev = moveFrom._dev; moveFrom._dev = nullptr;
		_mem = moveFrom._mem; moveFrom._mem = nullptr;
        _pitches = moveFrom._pitches;
		return *this;
	}


}}

