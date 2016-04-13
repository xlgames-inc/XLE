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
#include "Pools.h"
#include "../../Format.h"
#include "../IDeviceVulkan.h"

namespace RenderCore { namespace Metal_Vulkan
{

    void        PipelineBuilder::Bind(const RasterizerState& rasterizer)
    {
        _rasterizerState = rasterizer;
    }
    
    void        PipelineBuilder::Bind(const BlendState& blendState)
    {
        _blendState = blendState;
    }
    
    void        PipelineBuilder::Bind(const DepthStencilState& depthStencilState, unsigned stencilRef)
    {
        _depthStencilState = depthStencilState;
    }

    void        PipelineBuilder::Bind(const BoundInputLayout& inputLayout)
    {
        _inputLayout = &inputLayout;
    }

	void        PipelineBuilder::Bind(const ShaderProgram& shaderProgram)
    {
        _shaderProgram = &shaderProgram;
    }

    static VkPrimitiveTopology AsNativeTopology(Topology::Enum topology)
    {
        return (VkPrimitiveTopology)topology;
    }
    
    void        PipelineBuilder::Bind(Topology::Enum topology)
    {
        _topology = AsNativeTopology(topology);
    }

    void        PipelineBuilder::SetVertexStrides(unsigned first, std::initializer_list<unsigned> vertexStrides)
    {
        for (unsigned c=0; (first+c)<vertexStrides.size() && c < dimof(_vertexStrides); ++c)
            _vertexStrides[first+c] = vertexStrides.begin()[c];
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
        pipeline.flags = 0;
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

        return _factory->CreateGraphicsPipeline(_globalPools->_mainPipelineCache.get(), pipeline);
    }

    PipelineBuilder::PipelineBuilder(const ObjectFactory& factory, GlobalPools& globalPools)
    : _factory(&factory), _globalPools(&globalPools)
    {
        _inputLayout = nullptr;
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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

    void        DeviceContext::BindPipeline()
    {
		assert(_commandList);
        auto pipeline = CreatePipeline(_renderPass.get());
        vkCmdBindPipeline(
			_commandList.get(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline.get());
    }

    void        DeviceContext::Draw(unsigned vertexCount, unsigned startVertexLocation)
    {
		assert(_commandList);
		BindPipeline();
        vkCmdDraw(
			_commandList.get(),
            vertexCount, 1,
            startVertexLocation, 0);
    }
    
    void        DeviceContext::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
		assert(_commandList);
		BindPipeline();
		vkCmdDrawIndexed(
			_commandList.get(),
			indexCount, 1,
			startIndexLocation, baseVertexLocation,
			0);
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

