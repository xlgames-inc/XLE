// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeviceContext.h"
#include "ObjectFactory.h"
#include "InputLayout.h"
#include "Shader.h"
#include "Buffer.h"
#include "FrameBuffer.h"
#include "Pools.h"
#include "../../Format.h"
#include "../IDeviceVulkan.h"

namespace RenderCore { namespace Metal_Vulkan
{

    void        PipelineBuilder::Bind(const RasterizerState& rasterizer)
    {
        _pipelineStale = true;
        _rasterizerState = rasterizer;
    }
    
    void        PipelineBuilder::Bind(const BlendState& blendState)
    {
        _pipelineStale = true;
        _blendState = blendState;
    }
    
    void        PipelineBuilder::Bind(const DepthStencilState& depthStencilState, unsigned stencilRef)
    {
        _pipelineStale = true;
        _depthStencilState = depthStencilState;
    }

    void        PipelineBuilder::Bind(const BoundInputLayout& inputLayout)
    {
        if (_inputLayout != &inputLayout) {
            _pipelineStale = true;
            _inputLayout = &inputLayout;
        }
    }

	void        PipelineBuilder::Bind(const ShaderProgram& shaderProgram)
    {
        if (_shaderProgram != &shaderProgram) {
            _shaderProgram = &shaderProgram;
            _pipelineStale = true;
        }
    }

    static VkPrimitiveTopology AsNativeTopology(Topology::Enum topology)
    {
        return (VkPrimitiveTopology)topology;
    }
    
    void        PipelineBuilder::Bind(Topology::Enum topology)
    {
        auto native = AsNativeTopology(topology);
        if (native != _topology) {
            _pipelineStale = true;
            _topology = native;
        }
    }

    void        PipelineBuilder::SetVertexStrides(unsigned first, std::initializer_list<unsigned> vertexStrides)
    {
        for (unsigned c=0; (first+c)<vertexStrides.size() && c < dimof(_vertexStrides); ++c) {
            if (_vertexStrides[first+c] != vertexStrides.begin()[c]) {
                _vertexStrides[first+c] = vertexStrides.begin()[c];
                _pipelineStale = true;
            }
        }
    }

    static VkPipelineShaderStageCreateInfo BuildShaderStage(
        const Shader& shader, VkShaderStageFlagBits stage)
    {
        VkPipelineShaderStageCreateInfo result = {};
        result.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        result.pNext = nullptr;
        result.flags = 0;
        result.stage = stage;
        result.module = shader.GetUnderlying();
        result.pName = "main";
        result.pSpecializationInfo = nullptr;
        return result;
    }

    void				PipelineBuilder::SetPipelineLayout(VulkanSharedPtr<VkPipelineLayout> layout) 
    { 
        if (_pipelineLayout != layout) {
            _pipelineStale = true;
            _pipelineLayout = std::move(layout); 
        }
    }
    
    VkPipelineLayout    PipelineBuilder::GetPipelineLayout() 
    { 
        return _pipelineLayout.get(); 
    }

    VulkanUniquePtr<VkPipeline> PipelineBuilder::CreatePipeline(VkRenderPass renderPass, unsigned subpass)
    {
        if (!_shaderProgram) return nullptr;

        VkPipelineShaderStageCreateInfo shaderStages[3];
        uint32_t shaderStageCount = 0;
        shaderStages[shaderStageCount++] = BuildShaderStage(_shaderProgram->GetVertexShader(), VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[shaderStageCount++] = BuildShaderStage(_shaderProgram->GetPixelShader(), VK_SHADER_STAGE_FRAGMENT_BIT);
        if (_shaderProgram->GetGeometryShader().IsGood())
            shaderStages[shaderStageCount++] = BuildShaderStage(_shaderProgram->GetGeometryShader(), VK_SHADER_STAGE_GEOMETRY_BIT);

        VkDynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE];
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pNext = nullptr;
        dynamicState.pDynamicStates = dynamicStateEnables;
        dynamicState.dynamicStateCount = 0;

        VkVertexInputBindingDescription vertexBinding = { 0, _vertexStrides[0], VK_VERTEX_INPUT_RATE_VERTEX };

        VkPipelineVertexInputStateCreateInfo vi = {};
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.pNext = nullptr;
        vi.flags = 0;
        vi.vertexBindingDescriptionCount = 1;
        vi.pVertexBindingDescriptions = &vertexBinding;
        vi.vertexAttributeDescriptionCount = 0;
        vi.pVertexAttributeDescriptions = nullptr;

        if (_inputLayout) {
            auto attribs = _inputLayout->GetAttributes();
            vi.vertexAttributeDescriptionCount = (uint32)attribs.size();
            vi.pVertexAttributeDescriptions = attribs.begin();
        }

        VkPipelineInputAssemblyStateCreateInfo ia = {};
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.pNext = nullptr;
        ia.flags = 0;
        ia.primitiveRestartEnable = VK_FALSE;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo vp = {};
        vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.pNext = nullptr;
        vp.flags = 0;
        vp.viewportCount = 1;
        dynamicStateEnables[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
        vp.scissorCount = 1;
        dynamicStateEnables[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;
        vp.pScissors = nullptr;
        vp.pViewports = nullptr;

        VkPipelineMultisampleStateCreateInfo ms = {};
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.pNext = nullptr;
        ms.flags = 0;
        ms.pSampleMask = nullptr;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        ms.sampleShadingEnable = VK_FALSE;
        ms.alphaToCoverageEnable = VK_FALSE;
        ms.alphaToOneEnable = VK_FALSE;
        ms.minSampleShading = 0.0f;

        VkGraphicsPipelineCreateInfo pipeline = {};
        pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline.pNext = nullptr;
        pipeline.layout = _pipelineLayout.get();
        pipeline.basePipelineHandle = VK_NULL_HANDLE;
        pipeline.basePipelineIndex = 0;
        pipeline.flags = 0; // VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
        pipeline.pVertexInputState = &vi;
        pipeline.pInputAssemblyState = &ia;
        pipeline.pRasterizationState = &_rasterizerState;
        pipeline.pColorBlendState = &_blendState;
        pipeline.pTessellationState = nullptr;
        pipeline.pMultisampleState = &ms;
        pipeline.pDynamicState = &dynamicState;
        pipeline.pViewportState = &vp;
        pipeline.pDepthStencilState = &_depthStencilState;
        pipeline.pStages = shaderStages;
        pipeline.stageCount = shaderStageCount;
        pipeline.renderPass = renderPass;
        pipeline.subpass = subpass;

        auto result = _factory->CreateGraphicsPipeline(_globalPools->_mainPipelineCache.get(), pipeline);
        _pipelineStale = false;
        return std::move(result);
    }

    PipelineBuilder::PipelineBuilder(const ObjectFactory& factory, GlobalPools& globalPools)
    : _factory(&factory), _globalPools(&globalPools)
    {
        _inputLayout = nullptr;
        _shaderProgram = nullptr;
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        _pipelineStale = true;
        for (auto& v:_vertexStrides) v = 0;
    }

    PipelineBuilder::~PipelineBuilder() {}

    void        DeviceContext::Bind(const ViewportDesc& viewport)
    {
		assert(_commandList);
        vkCmdSetViewport(
            _commandList.get(),
            0, 1,
            (VkViewport*)&viewport);

            // todo -- get this right for non-integer coords
        VkRect2D scissor = {
            {int(viewport.TopLeftX), int(viewport.TopLeftY)},
            {int(viewport.Width), int(viewport.Height)},
        };
        vkCmdSetScissor(
			_commandList.get(),
            0, 1,
            &scissor);
    }

	void        DeviceContext::Bind(const IndexBuffer& ib, Format indexFormat, unsigned offset)
	{
		assert(_commandList);
        vkCmdBindIndexBuffer(
			_commandList.get(),
			ib.GetUnderlying(),
			offset,
			indexFormat == Format::R32_UINT ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
	}

	void        DeviceContext::Bind(
		unsigned startSlot, unsigned bufferCount, const VertexBuffer* VBs[], 
		const unsigned strides[], const unsigned offsets[])
	{
		assert(_commandList);
		assert(bufferCount <= s_maxBoundVBs);
		SetVertexStrides(startSlot, std::initializer_list<unsigned>(strides, &strides[bufferCount]));
		VkBuffer buffers[s_maxBoundVBs];
		VkDeviceSize vkOffsets[s_maxBoundVBs];
		for (unsigned c=0; c<bufferCount; ++c) {
			buffers[c] = VBs[c]->GetUnderlying();
			vkOffsets[c] = offsets[c];
		}
        vkCmdBindVertexBuffers(
			_commandList.get(),
			startSlot, bufferCount,
			buffers, vkOffsets);
	}


    void        DeviceContext::Bind(VulkanSharedPtr<VkRenderPass> renderPass)
    {
        _renderPass = std::move(renderPass);
    }

    bool        DeviceContext::BindPipeline()
    {
		assert(_commandList);
        if (!_pipelineStale) return true;

        auto pipeline = CreatePipeline(_renderPass.get());
        if (pipeline) {
            vkCmdBindPipeline(
			    _commandList.get(),
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline.get());
            Bind(ViewportDesc(0.f, 0.f, 512.f, 512.f));
            return true;
        }

        assert(0);
        return false;
    }

    void        DeviceContext::Draw(unsigned vertexCount, unsigned startVertexLocation)
    {
		assert(_commandList);
		if (BindPipeline()) {
            vkCmdDraw(
			    _commandList.get(),
                vertexCount, 1,
                startVertexLocation, 0);
        }
    }
    
    void        DeviceContext::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
		assert(_commandList);
		if (BindPipeline()) {
		    vkCmdDrawIndexed(
			    _commandList.get(),
			    indexCount, 1,
			    startIndexLocation, baseVertexLocation,
			    0);
        }
    }

    void        DeviceContext::DrawAuto() 
    {
    }

    void        DeviceContext::Dispatch(unsigned countX, unsigned countY, unsigned countZ)
    {
    }

    std::shared_ptr<DeviceContext> DeviceContext::Get(IThreadContext& threadContext)
    {
        IThreadContextVulkan* vulkanContext = 
            (IThreadContextVulkan*)threadContext.QueryInterface(
                __uuidof(IThreadContextVulkan));
        if (vulkanContext)
            return vulkanContext->GetMetalContext();
        return nullptr;
    }

    GlobalPools&    DeviceContext::GetGlobalPools()
    {
        return *_globalPools;
    }

    VkDevice        DeviceContext::GetUnderlyingDevice()
    {
        return _factory->GetDevice().get();
    }

	void		DeviceContext::BeginCommandList()
	{
        // hack -- clear some state
        _inputLayout = nullptr;
        _shaderProgram = nullptr;
        _pipelineLayout = nullptr;
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        for (auto& v:_vertexStrides) v = 0;
        _pipelineStale = true;

		// Unless the context is already tied to a primary command list, we will always
		// create a secondary command list here.
		// Also, all command lists are marked as "one time submit"
		if (!_commandList) {
            if (!_cmdPool) return;
			_commandList = _cmdPool->Allocate(_cmdBufferType);
		} else {
			auto res = vkResetCommandBuffer(_commandList.get(), 0);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failure while resetting command buffer"));
		}

		VkCommandBufferInheritanceInfo inheritInfo = {};
		inheritInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inheritInfo.pNext = nullptr;
		inheritInfo.renderPass = VK_NULL_HANDLE;
		inheritInfo.subpass = 0;
		inheritInfo.framebuffer = VK_NULL_HANDLE;
		inheritInfo.occlusionQueryEnable = false;
		inheritInfo.queryFlags = 0;
		inheritInfo.pipelineStatistics = 0;

		VkCommandBufferBeginInfo cmd_buf_info = {};
		cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmd_buf_info.pNext = nullptr;
		cmd_buf_info.flags = 0; // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		cmd_buf_info.pInheritanceInfo = &inheritInfo;
        auto res = vkBeginCommandBuffer(_commandList.get(), &cmd_buf_info);
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while beginning command buffer"));
	}

	void		DeviceContext::CommitCommandList(VkCommandBuffer_T& cmdList, bool preserveState)
	{
		assert(_commandList);
		(void)preserveState;		// we can't handle this properly in Vulkan

		const VkCommandBuffer buffers[] = { &cmdList };
        vkCmdExecuteCommands(
			_commandList.get(),
			dimof(buffers), buffers);
	}

	auto        DeviceContext::ResolveCommandList() -> CommandListPtr
	{
		assert(_commandList);
		auto res = vkEndCommandBuffer(_commandList.get());
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while ending command buffer"));

		// We will release our reference on _command list here.
		auto result = std::move(_commandList);
		return result;
	}

    static VkClearValue ClearDepthStencil(float depth, uint32_t stencil) { VkClearValue result; result.depthStencil = VkClearDepthStencilValue { depth, stencil }; return result; }
	static VkClearValue ClearColor(float r, float g, float b, float a) { VkClearValue result; result.color.float32[0] = r; result.color.float32[1] = g; result.color.float32[2] = b; result.color.float32[3] = a; return result; }

    void        DeviceContext::BeginRenderPass(
        FrameBufferLayout& fbLayout, FrameBuffer& fb,
        VectorPattern<int, 2> offset, VectorPattern<unsigned, 2> extent)
    {
        VkRenderPassBeginInfo rp_begin;
		rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rp_begin.pNext = nullptr;
		rp_begin.renderPass = fbLayout.GetUnderlying();
		rp_begin.framebuffer = fb.GetUnderlying();
		rp_begin.renderArea.offset.x = offset[0];
		rp_begin.renderArea.offset.y = offset[1];
		rp_begin.renderArea.extent.width = extent[0];
		rp_begin.renderArea.extent.height = extent[1];
		
		VkClearValue clearValues[] = { ClearColor(0.5f, 0.25f, 1.f, 1.f), ClearDepthStencil(1.f, 0) };
		rp_begin.pClearValues = clearValues;
		rp_begin.clearValueCount = dimof(clearValues);

        vkCmdBeginRenderPass(_commandList.get(), &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    }

    void DeviceContext::EndRenderPass()
    {
		vkCmdEndRenderPass(_commandList.get());
    }

    void DeviceContext::SetImageLayout(
		VkImage image,
		VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
        unsigned mipCount,
        unsigned layerCount)
	{
        Metal_Vulkan::SetImageLayout(
            _commandList.get(), image, aspectMask, 
            oldImageLayout, newImageLayout,
            mipCount, layerCount);
    }

    void SetImageLayout(
        VkCommandBuffer commandBuffer,
		VkImage image,
		VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
        unsigned mipCount,
        unsigned layerCount)
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
		image_memory_barrier.subresourceRange.levelCount = mipCount;
		image_memory_barrier.subresourceRange.layerCount = layerCount;

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

        if (oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
			image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
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
			commandBuffer, src_stages, dest_stages, 0, 0, nullptr, 0, nullptr,
			1, &image_memory_barrier);
	}

    void DeviceContext::CmdUpdateBuffer(
        VkBuffer buffer, VkDeviceSize offset, 
        VkDeviceSize byteCount, const void* data)
    {
        assert(byteCount <= 65536); // this restriction is imposed by Vulkan
		assert((byteCount & (4 - 1)) == 0);  // must be a multiple of 4
		assert(byteCount > 0 && data);
        vkCmdUpdateBuffer(
			_commandList.get(),
			buffer, 0,
			byteCount, (const uint32_t*)data);
    }

    void DeviceContext::CmdBindDescriptorSets(
        VkPipelineBindPoint pipelineBindPoint,
        VkPipelineLayout layout,
        uint32_t firstSet,
        uint32_t descriptorSetCount,
        const VkDescriptorSet* pDescriptorSets,
        uint32_t dynamicOffsetCount,
        const uint32_t* pDynamicOffsets)
    {
        vkCmdBindDescriptorSets(
            _commandList.get(),
            pipelineBindPoint, layout, firstSet, 
            descriptorSetCount, pDescriptorSets,
            dynamicOffsetCount, pDynamicOffsets);
    }

    void DeviceContext::CmdCopyBuffer(
        VkBuffer srcBuffer,
        VkBuffer dstBuffer,
        uint32_t regionCount,
        const VkBufferCopy* pRegions)
    {
        vkCmdCopyBuffer(_commandList.get(), srcBuffer, dstBuffer, regionCount, pRegions);
    }

    void DeviceContext::CmdCopyImage(
        VkImage srcImage,
        VkImageLayout srcImageLayout,
        VkImage dstImage,
        VkImageLayout dstImageLayout,
        uint32_t regionCount,
        const VkImageCopy* pRegions)
    {
        vkCmdCopyImage(
            _commandList.get(), 
            srcImage, srcImageLayout, 
            dstImage, dstImageLayout, 
            regionCount, pRegions);
    }

    void DeviceContext::CmdCopyBufferToImage(
        VkBuffer srcBuffer,
        VkImage dstImage,
        VkImageLayout dstImageLayout,
        uint32_t regionCount,
        const VkBufferImageCopy* pRegions)
    {
        vkCmdCopyBufferToImage(
            _commandList.get(),
            srcBuffer, 
            dstImage, dstImageLayout,
            regionCount, pRegions);
    }

    DeviceContext::DeviceContext(
        const ObjectFactory& factory, 
        GlobalPools& globalPools,
		CommandPool& cmdPool, 
        CommandPool::BufferType cmdBufferType)
    : PipelineBuilder(factory, globalPools)
    , _cmdPool(&cmdPool), _cmdBufferType(cmdBufferType)
    {}

	void DeviceContext::PrepareForDestruction(IDevice*, IPresentationChain*) {}

}}

