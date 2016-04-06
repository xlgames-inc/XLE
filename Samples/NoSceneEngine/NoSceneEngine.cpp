// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define SELECT_VULKAN
#define _CRT_SECURE_NO_WARNINGS

#include "../../PlatformRig/OverlappedWindow.h"
#include "../../PlatformRig/MainInputHandler.h"
#include "../../PlatformRig/InputTranslator.h"
#include "../../PlatformRig/DebuggingDisplays/GPUProfileDisplay.h"
#include "../../PlatformRig/DebuggingDisplays/CPUProfileDisplay.h"
#include "../../PlatformRig/FrameRig.h"
#include "../../PlatformRig/PlatformRigUtil.h"
#include "../../PlatformRig/OverlaySystem.h"

#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/GPUProfiler.h"
#include "../../RenderCore/Assets/Services.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderOverlays/Font.h"
#include "../../RenderOverlays/DebugHotKeys.h"
#include "../../BufferUploads/IBufferUploads.h"

#include "../../Assets/AssetServices.h"
#include "../../Assets/AssetSetManager.h"

#include "../../ConsoleRig/Console.h"
#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/Profiling/CPUProfiler.h"
#include "../../Utility/Streams/FileSystemMonitor.h"

#include <functional>

#include "../../RenderCore/ShaderService.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Vulkan/Metal/ObjectFactory.h"
#include "../../RenderCore/Vulkan/IDeviceVulkan.h"

unsigned FrameRenderCount = 0;

namespace VulkanTest
{
    struct texture_object 
    {
        VkSampler sampler;

        VkImage image;
        VkImageLayout imageLayout;

        VkDeviceMemory mem;
        VkImageView view;
        int32_t tex_width, tex_height;

        VkDeviceMemory stagingMemory;
        VkImage stagingImage;
    };

    bool read_ppm(char const *const filename, int &width, int &height,
                  uint64_t rowPitch, unsigned char *dataPtr) {
        // PPM format expected from http://netpbm.sourceforge.net/doc/ppm.html
        //  1. magic number
        //  2. whitespace
        //  3. width
        //  4. whitespace
        //  5. height
        //  6. whitespace
        //  7. max color value
        //  8. whitespace
        //  7. data

        // Comments are not supported, but are detected and we kick out
        // Only 8 bits per channel is supported
        // If dataPtr is nullptr, only width and height are returned

        // Read in values from the PPM file as characters to check for comments
        char magicStr[3] = {}, heightStr[6] = {}, widthStr[6] = {},
             formatStr[6] = {};

        FILE *fPtr = fopen(filename, "rb");
        if (!fPtr) {
            printf("Bad filename in read_ppm: %s\n", filename);
            return false;
        }

        // Read the four values from file, accounting with any and all whitepace
        fscanf(fPtr, "%s %s %s %s ", magicStr, widthStr, heightStr, formatStr);

        // Kick out if comments present
        if (magicStr[0] == '#' || widthStr[0] == '#' || heightStr[0] == '#' ||
            formatStr[0] == '#') {
            printf("Unhandled comment in PPM file\n");
            return false;
        }

        // Only one magic value is valid
        if (strncmp(magicStr, "P6", sizeof(magicStr))) {
            printf("Unhandled PPM magic number: %s\n", magicStr);
            return false;
        }

        width = atoi(widthStr);
        height = atoi(heightStr);

        // Ensure we got something sane for width/height
        static const int saneDimension = 32768; //??
        if (width <= 0 || width > saneDimension) {
            printf("Width seems wrong.  Update read_ppm if not: %u\n", width);
            return false;
        }
        if (height <= 0 || height > saneDimension) {
            printf("Height seems wrong.  Update read_ppm if not: %u\n", height);
            return false;
        }

        if (dataPtr == nullptr) {
            // If no destination pointer, caller only wanted dimensions
            return true;
        }

        // Now read the data
        for (int y = 0; y < height; y++) {
            unsigned char *rowPtr = dataPtr;
            for (int x = 0; x < width; x++) {
                fread(rowPtr, 3, 1, fPtr);
                rowPtr[3] = 255; /* Alpha of 1 */
                rowPtr += 4;
            }
            dataPtr += rowPitch;
        }
        fclose(fPtr);

        return true;
    }

    void set_image_layout(VkCommandBuffer cmd, VkImage image,
                          VkImageAspectFlags aspectMask,
                          VkImageLayout old_image_layout,
                          VkImageLayout new_image_layout) 
    {
        VkImageMemoryBarrier image_memory_barrier = {};
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.pNext = NULL;
        image_memory_barrier.srcAccessMask = 0;
        image_memory_barrier.dstAccessMask = 0;
        image_memory_barrier.oldLayout = old_image_layout;
        image_memory_barrier.newLayout = new_image_layout;
        image_memory_barrier.image = image;
        image_memory_barrier.subresourceRange.aspectMask = aspectMask;
        image_memory_barrier.subresourceRange.baseMipLevel = 0;
        image_memory_barrier.subresourceRange.levelCount = 1;
        image_memory_barrier.subresourceRange.layerCount = 1;

        if (old_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            image_memory_barrier.srcAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }

        if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        }

        if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        }

        if (old_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        }

        if (old_image_layout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
            image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        }

        if (new_image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            image_memory_barrier.srcAccessMask =
                VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        }

        if (new_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            image_memory_barrier.dstAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }

        if (new_image_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            image_memory_barrier.dstAccessMask =
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }

        VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dest_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        vkCmdPipelineBarrier(cmd, src_stages, dest_stages, 0, 0, NULL, 0, NULL,
                             1, &image_memory_barrier);
    }

    void init_image(
        const RenderCore::Metal_Vulkan::ObjectFactory& factory,
        VkQueue queue,
        VkCommandBuffer cmd, 
        texture_object &texObj) 
    {
        VkResult res;
        std::string filename = "lunarg.ppm";

        if (!read_ppm(filename.c_str(), texObj.tex_width, texObj.tex_height, 0,
                      NULL)) {
            std::cout << "Could not read texture file lunarg.ppm\n";
            exit(-1);
        }

        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(
            factory.GetPhysicalDevice(), VK_FORMAT_R8G8B8A8_UNORM, &formatProps);

        /* See if we can use a linear tiled image for a texture, if not, we will
         * need a staging image for the texture data */
        bool needStaging = (!(formatProps.linearTilingFeatures &
                              VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
                               ? true
                               : false;

        VkImageCreateInfo image_create_info = {};
        image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_create_info.pNext = NULL;
        image_create_info.imageType = VK_IMAGE_TYPE_2D;
        image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        image_create_info.extent.width = texObj.tex_width;
        image_create_info.extent.height = texObj.tex_height;
        image_create_info.extent.depth = 1;
        image_create_info.mipLevels = 1;
        image_create_info.arrayLayers = 1;
        image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_create_info.tiling = VK_IMAGE_TILING_LINEAR;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        image_create_info.usage = needStaging ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                              : VK_IMAGE_USAGE_SAMPLED_BIT;
        image_create_info.queueFamilyIndexCount = 0;
        image_create_info.pQueueFamilyIndices = NULL;
        image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_create_info.flags = 0;

        VkMemoryAllocateInfo mem_alloc = {};
        mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mem_alloc.pNext = NULL;
        mem_alloc.allocationSize = 0;
        mem_alloc.memoryTypeIndex = 0;

        VkImage mappableImage;
        VkDeviceMemory mappableMemory;

        VkMemoryRequirements mem_reqs;

        /* Create a mappable image.  It will be the texture if linear images are ok
         * to be textures or it will be the staging image if they are not. */
        auto device = factory.GetDevice().get();
        res = vkCreateImage(device, &image_create_info, NULL, &mappableImage);
        assert(res == VK_SUCCESS);

        vkGetImageMemoryRequirements(device, mappableImage, &mem_reqs);
        assert(res == VK_SUCCESS);

        mem_alloc.allocationSize = mem_reqs.size;

        /* Find the memory type that is host mappable */
        mem_alloc.memoryTypeIndex = factory.FindMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        assert(mem_alloc.memoryTypeIndex < 32);

        /* allocate memory */
        res = vkAllocateMemory(device, &mem_alloc, NULL, &(mappableMemory));
        assert(res == VK_SUCCESS);

        /* bind memory */
        res = vkBindImageMemory(device, mappableImage, mappableMemory, 0);
        assert(res == VK_SUCCESS);

        #if 0
            set_image_layout(cmd, mappableImage, VK_IMAGE_ASPECT_COLOR_BIT,
                             VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_GENERAL);

            res = vkEndCommandBuffer(cmd);
            assert(res == VK_SUCCESS);
            const VkCommandBuffer cmd_bufs[] = {cmd};
            VkFenceCreateInfo fenceInfo;
            VkFence cmdFence;
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.pNext = NULL;
            fenceInfo.flags = 0;
            vkCreateFence(device, &fenceInfo, NULL, &cmdFence);

            VkSubmitInfo submit_info[1] = {};
            submit_info[0].pNext = NULL;
            submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info[0].waitSemaphoreCount = 0;
            submit_info[0].pWaitSemaphores = NULL;
            submit_info[0].pWaitDstStageMask = NULL;
            submit_info[0].commandBufferCount = 1;
            submit_info[0].pCommandBuffers = cmd_bufs;
            submit_info[0].signalSemaphoreCount = 0;
            submit_info[0].pSignalSemaphores = NULL;

            /* Queue the command buffer for execution */
            res = vkQueueSubmit(queue, 1, submit_info, cmdFence);
            assert(res == VK_SUCCESS);
        #endif

        VkImageSubresource subres = {};
        subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subres.mipLevel = 0;
        subres.arrayLayer = 0;

        VkSubresourceLayout layout;
        void *data;

        /* Get the subresource layout so we know what the row pitch is */
        vkGetImageSubresourceLayout(device, mappableImage, &subres, &layout);

        /* Make sure command buffer is finished before mapping */
        #if 0
            #define FENCE_TIMEOUT 100000000
            do {
                res =
                    vkWaitForFences(device, 1, &cmdFence, VK_TRUE, FENCE_TIMEOUT);
            } while (res == VK_TIMEOUT);
            assert(res == VK_SUCCESS);

            vkDestroyFence(device, cmdFence, NULL);
        #endif

        res = vkMapMemory(device, mappableMemory, 0, mem_reqs.size, 0, &data);
        assert(res == VK_SUCCESS);

        /* Read the ppm file into the mappable image's memory */
        if (!read_ppm(filename.c_str(), texObj.tex_width, texObj.tex_height,
                      layout.rowPitch, (unsigned char *)data)) {
            std::cout << "Could not load texture file lunarg.ppm\n";
            exit(-1);
        }

        vkUnmapMemory(device, mappableMemory);

        #if 0
            VkCommandBufferBeginInfo cmd_buf_info = {};
            cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            cmd_buf_info.pNext = NULL;
            cmd_buf_info.flags = 0;
            cmd_buf_info.pInheritanceInfo = NULL;

            res = vkResetCommandBuffer(cmd, 0);
            res = vkBeginCommandBuffer(cmd, &cmd_buf_info);
            assert(res == VK_SUCCESS);
        #endif

        if (!needStaging) {
            /* If we can use the linear tiled image as a texture, just do it */
            texObj.image = mappableImage;
            texObj.mem = mappableMemory;
            texObj.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            set_image_layout(cmd, texObj.image, VK_IMAGE_ASPECT_COLOR_BIT,
                             VK_IMAGE_LAYOUT_GENERAL, texObj.imageLayout);
            /* No staging resources to free later */
            texObj.stagingImage = VK_NULL_HANDLE;
            texObj.stagingMemory = VK_NULL_HANDLE;
        } else {
            /* The mappable image cannot be our texture, so create an optimally
             * tiled image and blit to it */
            image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            image_create_info.usage =
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            res =
                vkCreateImage(device, &image_create_info, NULL, &texObj.image);
            assert(res == VK_SUCCESS);

            vkGetImageMemoryRequirements(device, texObj.image, &mem_reqs);

            mem_alloc.allocationSize = mem_reqs.size;

            /* Find memory type - dont specify any mapping requirements */
            mem_alloc.memoryTypeIndex = factory.FindMemoryType(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            assert(mem_alloc.memoryTypeIndex < 32);

            /* allocate memory */
            res = vkAllocateMemory(device, &mem_alloc, NULL, &texObj.mem);
            assert(res == VK_SUCCESS);

            /* bind memory */
            res = vkBindImageMemory(device, texObj.image, texObj.mem, 0);
            assert(res == VK_SUCCESS);

            /* Since we're going to blit from the mappable image, set its layout to
             * SOURCE_OPTIMAL. Side effect is that this will create info.cmd */
            set_image_layout(cmd, mappableImage, VK_IMAGE_ASPECT_COLOR_BIT,
                             VK_IMAGE_LAYOUT_GENERAL,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            /* Since we're going to blit to the texture image, set its layout to
             * DESTINATION_OPTIMAL */
            set_image_layout(cmd, texObj.image, VK_IMAGE_ASPECT_COLOR_BIT,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkImageCopy copy_region;
            copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.srcSubresource.mipLevel = 0;
            copy_region.srcSubresource.baseArrayLayer = 0;
            copy_region.srcSubresource.layerCount = 1;
            copy_region.srcOffset.x = 0;
            copy_region.srcOffset.y = 0;
            copy_region.srcOffset.z = 0;
            copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.dstSubresource.mipLevel = 0;
            copy_region.dstSubresource.baseArrayLayer = 0;
            copy_region.dstSubresource.layerCount = 1;
            copy_region.dstOffset.x = 0;
            copy_region.dstOffset.y = 0;
            copy_region.dstOffset.z = 0;
            copy_region.extent.width = texObj.tex_width;
            copy_region.extent.height = texObj.tex_height;
            copy_region.extent.depth = 1;

            /* Put the copy command into the command buffer */
            vkCmdCopyImage(cmd, mappableImage,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texObj.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

            /* Set the layout for the texture image from DESTINATION_OPTIMAL to
             * SHADER_READ_ONLY */
            texObj.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            set_image_layout(cmd, texObj.image, VK_IMAGE_ASPECT_COLOR_BIT,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             texObj.imageLayout);

            /* Remember staging resources to free later */
            texObj.stagingImage = mappableImage;
            texObj.stagingMemory = mappableMemory;
        }

        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.pNext = NULL;
        view_info.image = VK_NULL_HANDLE;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        view_info.components.r = VK_COMPONENT_SWIZZLE_R;
        view_info.components.g = VK_COMPONENT_SWIZZLE_G;
        view_info.components.b = VK_COMPONENT_SWIZZLE_B;
        view_info.components.a = VK_COMPONENT_SWIZZLE_A;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        /* create image view */
        view_info.image = texObj.image;
        res = vkCreateImageView(device, &view_info, NULL, &texObj.view);
        assert(res == VK_SUCCESS);
    }

}

namespace Sample
{
///////////////////////////////////////////////////////////////////////////////////////////////////

        // "GPU profiler" doesn't have a place to live yet. We just manage it here, at 
        //  the top level
    RenderCore::GPUProfiler::Ptr g_gpuProfiler;
    Utility::HierarchicalCPUProfiler g_cpuProfiler;

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void InitProfilerDisplays(RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSys);

    static void RunShaderTest(RenderCore::IThreadContext& genericThreadContext)
    {
        TRY
        {
            auto threadContext = (RenderCore::IThreadContextVulkan*)genericThreadContext.QueryInterface(__uuidof(RenderCore::IThreadContextVulkan));
            if (!threadContext) return;

            auto genericDevice = genericThreadContext.GetDevice();
            auto device = (RenderCore::IDeviceVulkan*)genericDevice->QueryInterface(__uuidof(RenderCore::IDeviceVulkan));
            if (!device) return;

            auto& shader = ::Assets::GetAsset<RenderCore::Metal::ShaderProgram>(
                    "game/xleres/deferred/basic.vsh:main:vs_*",
                    "game/xleres/deferred/basic.psh:main:ps_*",
                    "GEO_HAS_TANGENT_FRAME=1;GEO_HAS_NORMAL=1;GEO_HAS_TEXCOORD=1;RES_HAS_DiffuseTexture=1;RES_HAS_NormalsTexture=1");

            RenderCore::Metal::BoundInputLayout inputLayout(
                RenderCore::Metal::GlobalInputLayouts::PNTT,
                shader.GetCompiledVertexShader());

            RenderCore::Metal::BoundUniforms boundUniforms(shader);
            boundUniforms.BindConstantBuffers(0, {"GlobalTransform", "LocalTransform"});
            boundUniforms.BindShaderResources(0, {"DiffuseTexture", "NormalsTexture", "ParametersTexture"});

            auto& factory = RenderCore::Metal::GetDefaultObjectFactory();
            auto underlyingLayout0 = boundUniforms.CreateLayout(factory, 0);
            auto underlyingLayout1 = boundUniforms.CreateLayout(factory, 1);
            (void)underlyingLayout0;
            (void)underlyingLayout1;

            VkDescriptorSetLayout layouts[] = { underlyingLayout0.get() }; //, underlyingLayout1.get() };

            #define g_allocationCallbacks RenderCore::Metal_Vulkan::g_allocationCallbacks
            using RenderCore::Metal_Vulkan::VulkanAPIFailure;
            using RenderCore::Metal_Vulkan::VulkanSharedPtr;
            
            // -------- pipeline layout --------
            VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
            pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pPipelineLayoutCreateInfo.pNext = nullptr;
            pPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
            pPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
            pPipelineLayoutCreateInfo.setLayoutCount = dimof(layouts);
            pPipelineLayoutCreateInfo.pSetLayouts = layouts;

            auto dev = factory.GetDevice().get();
            VkPipelineLayout rawPipelineLayout = nullptr;
            auto res = vkCreatePipelineLayout(dev, &pPipelineLayoutCreateInfo, g_allocationCallbacks, &rawPipelineLayout);
            auto pipelineLayout = VulkanSharedPtr<VkPipelineLayout>(
                rawPipelineLayout,
                [dev](VkPipelineLayout layout) { vkDestroyPipelineLayout(dev, layout, g_allocationCallbacks); });
            if (res != VK_SUCCESS)
                Throw(VulkanAPIFailure(res, "Failed while creating descriptor set layout"));

            // -------- descriptor pool --------
            VkDescriptorPoolSize type_count[1];
            type_count[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            type_count[0].descriptorCount = 1;

            VkDescriptorPoolCreateInfo descriptor_pool = {};
            descriptor_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptor_pool.pNext = nullptr;
            descriptor_pool.maxSets = 2;
            descriptor_pool.poolSizeCount = 1;
            descriptor_pool.pPoolSizes = type_count;

            // auto dev = factory.GetDevice().get();
            VkDescriptorPool rawDescriptorPool = nullptr;
            res = vkCreateDescriptorPool(dev, &descriptor_pool, g_allocationCallbacks, &rawDescriptorPool);
            auto descriptorPool = VulkanSharedPtr<VkDescriptorPool>(
                rawDescriptorPool,
                [dev](VkDescriptorPool pool) { vkDestroyDescriptorPool(dev, pool, g_allocationCallbacks); });
            if (res != VK_SUCCESS)
                Throw(VulkanAPIFailure(res, "Failed while creating descriptor pool"));

            // -------- descriptor set --------
            VkDescriptorSetAllocateInfo desc_alloc_info;
            desc_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            desc_alloc_info.pNext = nullptr;
            desc_alloc_info.descriptorPool = descriptorPool.get();
            desc_alloc_info.descriptorSetCount = dimof(layouts);
            desc_alloc_info.pSetLayouts = layouts;

            // auto dev = factory.GetDevice().get();
            auto pool = descriptorPool.get();
            VkDescriptorSet rawDescriptorSets[2] = { nullptr, nullptr };
            res = vkAllocateDescriptorSets(dev, &desc_alloc_info, rawDescriptorSets);
            auto descriptorSet0 = VulkanSharedPtr<VkDescriptorSet>(
                rawDescriptorSets[0],
                [dev, pool](VkDescriptorSet set) { vkFreeDescriptorSets(dev, pool, 1, &set); });
            // auto descriptorSet1 = VulkanSharedPtr<VkDescriptorSet>(
            //     rawDescriptorSets[1],
            //     [dev, pool](VkDescriptorSet set) { vkFreeDescriptorSets(dev, pool, 1, &set); });
            if (res != VK_SUCCESS)
                Throw(VulkanAPIFailure(res, "Failed while creating descriptor set"));

            // -------- demo image --------
            VulkanTest::texture_object texObj = {};
            VulkanTest::init_image(
                factory, device->GetRenderingQueue(), threadContext->GetPrimaryCommandBuffer(), 
                texObj);

            VkSamplerCreateInfo samplerCreateInfo = {};
            samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
            samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.mipLodBias = 0.0;
            samplerCreateInfo.anisotropyEnable = VK_FALSE,
            samplerCreateInfo.maxAnisotropy = 0;
            samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
            samplerCreateInfo.minLod = 0.0;
            samplerCreateInfo.maxLod = 0.0;
            samplerCreateInfo.compareEnable = VK_FALSE;
            samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

            /* create sampler */
            res = vkCreateSampler(dev, &samplerCreateInfo, g_allocationCallbacks, &texObj.sampler);
            assert(res == VK_SUCCESS);

            // -------- write descriptor set --------
            VkDescriptorImageInfo imageInfo;
            imageInfo.imageLayout = texObj.imageLayout;
            imageInfo.imageView = texObj.view;
            imageInfo.sampler = texObj.sampler;

            VkWriteDescriptorSet writes[1];
            writes[0] = {};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = descriptorSet0.get();
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[0].pImageInfo = &imageInfo;
            writes[0].dstArrayElement = 0;

            vkUpdateDescriptorSets(factory.GetDevice().get(), dimof(writes), writes, 0, nullptr);

            VkDescriptorSet descriptorSets[] = { descriptorSet0.get() };
            vkCmdBindDescriptorSets(
                threadContext->GetPrimaryCommandBuffer(),
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout.get(), 
                0, 1, descriptorSets, 0, nullptr);
        }
        CATCH(const ::Assets::Exceptions::AssetException&) {}
        CATCH_END
    }

    void ExecuteSample()
    {
        using namespace PlatformRig;
        using namespace Sample;

            // We need to startup some basic objects:
            //      * OverlappedWindow (corresponds to a single basic window on Windows)
            //      * RenderDevice & presentation chain
            //      * BufferUploads
            //      * CompileAndAsyncManager
            //
            // Note that the render device should be created first, so that the window
            // object is destroyed before the device is destroyed.
        LogInfo << "Building primary managers";
        auto renderDevice = RenderCore::CreateDevice();

        PlatformRig::OverlappedWindow window;
        auto clientRect = window.GetRect();
        std::shared_ptr<RenderCore::IPresentationChain> presentationChain = 
            renderDevice->CreatePresentationChain(
                window.GetUnderlyingHandle(), 
                clientRect.second[0] - clientRect.first[0], clientRect.second[1] - clientRect.first[1]);

        auto assetServices = std::make_unique<::Assets::Services>(0);
        auto renderAssetServices = std::make_unique<RenderCore::Assets::Services>(renderDevice.get());

            //  Tie in the window handler so we get presentation chain resizes, and give our
            //  window a title
            //  Here, I show 2 different ways to do dynamic string formatting.
            //      (note that XlDynFormatString will always allocate at least once!)
        window.AddWindowHandler(std::make_shared<PlatformRig::ResizePresentationChain>(presentationChain));
        auto v = renderDevice->GetVersionInformation();
        window.SetTitle(XlDynFormatString("XLE sample [RenderCore: %s : %s]", v.first, v.second).c_str());
        window.SetTitle(StringMeld<128>() << "XLE sample [RenderCore: " << v.first << ", " << v.second << "]");

            // Some secondary initalisation:
            //  * attach compilers
            //  * pass buffer uploads pointer to the scene engine
            //  * init the gpu profiler (this init step will probably change someday)
            //  * the font system needs an explicit init (and shutdown)
            //  * the global technique context contains some global rendering settings
        renderAssetServices->InitColladaCompilers();
        g_gpuProfiler = RenderCore::GPUProfiler::CreateProfiler();
        RenderOverlays::InitFontSystem(renderDevice.get(), &renderAssetServices->GetBufferUploads());
        auto globalTechniqueContext = std::make_shared<PlatformRig::GlobalTechniqueContext>();
        
        {
                // currently we need to maintain a reference on these two fonts -- 
            auto defaultFont0 = RenderOverlays::GetX2Font("Raleway", 16);
            auto defaultFont1 = RenderOverlays::GetX2Font("Vera", 16);

                //  Create the debugging system, and add any "displays"
                //  If we have any custom displays to add, we can add them here. Often it's 
                //  useful to create a debugging display to go along with any new feature. 
                //  It just provides a convenient architecture for visualizing important information.
            LogInfo << "Setup tools and debugging";
            FrameRig frameRig;
            // InitDebugDisplays(*frameRig.GetDebugSystem());
            InitProfilerDisplays(*frameRig.GetDebugSystem());

            auto overlaySwitch = std::make_shared<PlatformRig::OverlaySystemSwitch>();
            overlaySwitch->AddSystem(RenderOverlays::DebuggingDisplay::KeyId_Make("~"), PlatformRig::CreateConsoleOverlaySystem());
            frameRig.GetMainOverlaySystem()->AddSystem(overlaySwitch);

                //  Setup input:
                //      * We create a main input handler, and tie that to the window to receive inputs
                //      * We can add secondary input handles to the main input handler as required
                //      * The order in which we add handlers determines their priority in intercepting messages
            LogInfo << "Setup input";
            auto mainInputHandler = std::make_shared<PlatformRig::MainInputHandler>();
            mainInputHandler->AddListener(RenderOverlays::MakeHotKeysHandler("game/xleres/hotkey.txt"));
            mainInputHandler->AddListener(frameRig.GetMainOverlaySystem()->GetInputListener());
            window.GetInputTranslator().AddListener(mainInputHandler);

                //  We can log the active assets at any time using this method.
                //  At this point during startup, we should only have a few assets loaded.
            assetServices->GetAssetSets().LogReport();

                //  We need 2 final objects for rendering:
                //      * the FrameRig schedules continuous rendering. It will take care
                //          of timing and some thread management taskes
                //      * the DeviceContext provides the methods we need for rendering.
            LogInfo << "Setup frame rig and rendering context";
            auto context = renderDevice->GetImmediateContext();

                //  Finally, we execute the frame loop
            for (;;) {
                if (OverlappedWindow::DoMsgPump() == OverlappedWindow::PumpResult::Terminate) {
                    break;
                }

                renderDevice->BeginFrame(presentationChain.get());

                RunShaderTest(*context);

                presentationChain->Present();

                    // ------- Update ----------------------------------------
                RenderCore::Assets::Services::GetBufferUploads().Update(*context, false);
                g_cpuProfiler.EndFrame();
                ++FrameRenderCount;
            }
        }

            //  There are some manual destruction operations we need to perform...
            //  (note that currently some shutdown steps might get skipped if we get 
            //  an unhandled exception)
            //  Before we go too far, though, let's log a list of active assets.
        LogInfo << "Starting shutdown";
        assetServices->GetAssetSets().LogReport();
        // RenderCore::Metal::DeviceContext::PrepareForDestruction(renderDevice.get(), presentationChain.get());

        g_gpuProfiler.reset();

        assetServices->GetAssetSets().Clear();
        RenderCore::Techniques::ResourceBoxes_Shutdown();
        RenderOverlays::CleanupFontSystem();

        renderAssetServices.reset();
        assetServices.reset();
        TerminateFileSystemMonitoring();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    static void InitProfilerDisplays(RenderOverlays::DebuggingDisplay::DebugScreensSystem& debugSys)
    {
        // if (g_gpuProfiler) {
        //     auto gpuProfilerDisplay = std::make_shared<PlatformRig::Overlays::GPUProfileDisplay>(g_gpuProfiler.get());
        //     debugSys.Register(gpuProfilerDisplay, "[Profiler] GPU Profiler");
        // }
        debugSys.Register(
            std::make_shared<PlatformRig::Overlays::CPUProfileDisplay>(&g_cpuProfiler), 
            "[Profiler] CPU Profiler");
    }
}

