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
#include "State.h"
#include "TextureView.h"
#include "FrameBuffer.h"
#include "Pools.h"
#include "PipelineLayout.h"
#include "../../Format.h"
#include "../IDeviceVulkan.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/ArithmeticUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{

    void        GraphicsPipelineBuilder::Bind(const RasterizerState& rasterizer)
    {
        _pipelineStale = true;
        _rasterizerState = rasterizer;
    }
    
    void        GraphicsPipelineBuilder::Bind(const BlendState& blendState)
    {
        _pipelineStale = true;
        _blendState = blendState;
    }
    
    void        GraphicsPipelineBuilder::Bind(const DepthStencilState& depthStencilState, unsigned stencilRef)
    {
        _pipelineStale = true;
        _depthStencilState = depthStencilState;
        _depthStencilState.front.reference = stencilRef;
        _depthStencilState.back.reference = stencilRef;
    }

    void        GraphicsPipelineBuilder::Bind(const BoundInputLayout& inputLayout)
    {
        if (_inputLayout != &inputLayout) {
            _pipelineStale = true;
            _inputLayout = &inputLayout;
        }
    }

	void        GraphicsPipelineBuilder::Bind(const ShaderProgram& shaderProgram)
    {
        if (_shaderProgram != &shaderProgram) {
            _shaderProgram = &shaderProgram;
            _pipelineStale = true;
        }
    }

    static VkPrimitiveTopology AsNative(Topology topo)
    {
        switch (topo)
        {
        case Topology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case Topology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case Topology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        default:
        case Topology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case Topology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case Topology::LineListAdj: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;

        case Topology::PatchList1:
        /*case Topology::PatchList2:
        case Topology::PatchList3:
        case Topology::PatchList4:
        case Topology::PatchList5:
        case Topology::PatchList6:
        case Topology::PatchList7:
        case Topology::PatchList8:
        case Topology::PatchList9:
        case Topology::PatchList10:
        case Topology::PatchList11:
        case Topology::PatchList12:
        case Topology::PatchList13:
        case Topology::PatchList14:
        case Topology::PatchList15:
        case Topology::PatchList16:*/
            return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
        }

        // other Vulkan topologies:
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN
        // VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY
    }
    
    void        GraphicsPipelineBuilder::Bind(Topology topology)
    {
        auto native = AsNative(topology);
        if (native != _topology) {
            _pipelineStale = true;
            _topology = native;
        }
    }

    void        GraphicsPipelineBuilder::SetVertexStrides(unsigned first, std::initializer_list<unsigned> vertexStrides)
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

    VulkanUniquePtr<VkPipeline> GraphicsPipelineBuilder::CreatePipeline(
        const ObjectFactory& factory,
        VkPipelineCache pipelineCache,
        VkPipelineLayout layout, 
        VkRenderPass renderPass, unsigned subpass, 
        TextureSamples samples)
    {
        if (!_shaderProgram || !renderPass) return nullptr;

        VkPipelineShaderStageCreateInfo shaderStages[3];
        uint32_t shaderStageCount = 0;
        if (_shaderProgram->GetVertexShader().IsGood())
            shaderStages[shaderStageCount++] = BuildShaderStage(_shaderProgram->GetVertexShader(), VK_SHADER_STAGE_VERTEX_BIT);
        if (_shaderProgram->GetPixelShader().IsGood())
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
        ia.topology = _topology;

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
        ms.rasterizationSamples = AsSampleCountFlagBits(samples);
        ms.sampleShadingEnable = VK_FALSE;
        ms.alphaToCoverageEnable = VK_FALSE;
        ms.alphaToOneEnable = VK_FALSE;
        ms.minSampleShading = 0.0f;

        VkGraphicsPipelineCreateInfo pipeline = {};
        pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline.pNext = nullptr;
        pipeline.layout = layout;
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

        auto result = factory.CreateGraphicsPipeline(pipelineCache, pipeline);
        _pipelineStale = false;
        return std::move(result);
    }

    GraphicsPipelineBuilder::GraphicsPipelineBuilder()
    {
        _inputLayout = nullptr;
        _shaderProgram = nullptr;
        _topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        _pipelineStale = true;
        for (auto& v:_vertexStrides) v = 0;
    }

    GraphicsPipelineBuilder::~GraphicsPipelineBuilder() {}

    GraphicsPipelineBuilder::GraphicsPipelineBuilder(const GraphicsPipelineBuilder& cloneFrom)
    {
        *this = cloneFrom;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::operator=(const GraphicsPipelineBuilder& cloneFrom)
    {
        _rasterizerState = cloneFrom._rasterizerState;
        _blendState = cloneFrom._blendState;
        _depthStencilState = cloneFrom._depthStencilState;
        _topology = cloneFrom._topology;

        _inputLayout = cloneFrom._inputLayout;
        _shaderProgram = cloneFrom._shaderProgram;

        for (unsigned c=0; c<s_maxBoundVBs; ++c)
            _vertexStrides[c] = cloneFrom._vertexStrides[c];

        _pipelineStale = cloneFrom._pipelineStale;
        return *this;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void        ComputePipelineBuilder::Bind(const ComputeShader& shader)
    {
        _shader = &shader;
        _pipelineStale = true;
    }

    VulkanUniquePtr<VkPipeline> ComputePipelineBuilder::CreatePipeline(
        const ObjectFactory& factory,
        VkPipelineCache pipelineCache,
        VkPipelineLayout layout)
    {
        VkComputePipelineCreateInfo pipeline = {};
        pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline.pNext = nullptr;
        pipeline.flags = 0; // VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
        pipeline.layout = layout;
        pipeline.basePipelineHandle = VK_NULL_HANDLE;
        pipeline.basePipelineIndex = 0;

        assert(_shader);
        pipeline.stage = BuildShaderStage(*_shader, VK_SHADER_STAGE_COMPUTE_BIT);

        auto result = factory.CreateComputePipeline(pipelineCache, pipeline);
        _pipelineStale = false;
        return std::move(result);
    }

    void        Reset();

    ComputePipelineBuilder::ComputePipelineBuilder()
    {
        _shader = nullptr;
        _pipelineStale = true;
    }
    ComputePipelineBuilder::~ComputePipelineBuilder() {}

    ComputePipelineBuilder::ComputePipelineBuilder(const ComputePipelineBuilder& cloneFrom)
    {
        *this = cloneFrom;
    }

    ComputePipelineBuilder& ComputePipelineBuilder::operator=(const ComputePipelineBuilder& cloneFrom)
    {
        _shader = cloneFrom._shader;
        _pipelineStale = cloneFrom._pipelineStale;
        return *this;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void        DeviceContext::Bind(const ViewportDesc& viewport)
    {
		assert(_commandList);
        _boundViewport = viewport;
        vkCmdSetViewport(_commandList.get(), 0, 1, (VkViewport*)&viewport);

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

    void        DeviceContext::BindDescriptorSet(PipelineType pipelineType, unsigned index, VkDescriptorSet set)
    {
        // compute descriptors must be bound outside of a render pass, but graphics descriptors must be bound inside of it
        if (pipelineType == PipelineType::Compute) { assert(!_renderPass); } 

        auto& collection = (pipelineType == PipelineType::Compute) ? _computeDescriptors : _graphicsDescriptors;
        if (index < (unsigned)collection._descriptorSets.size() && collection._descriptorSets[index] != set) {
            collection._descriptorSets[index] = set;

            if (_renderPass || pipelineType == PipelineType::Compute) {
                CmdBindDescriptorSets(
                    (pipelineType == PipelineType::Compute) ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
                    collection._pipelineLayout->GetUnderlying(), 
                    index, 1, &set, 
                    0, nullptr);
            } else {
                collection._hasSetsAwaitingFlush = true;    // (will be bound when the render pass begins)
            }
        }
    }

    PipelineLayout* DeviceContext::GetPipelineLayout(PipelineType pipelineType)
    {
        return (pipelineType == PipelineType::Compute) 
            ? _computeDescriptors._pipelineLayout
            : _graphicsDescriptors._pipelineLayout;
    }

    bool        DeviceContext::BindGraphicsPipeline()
    {
		assert(_commandList);

        // If we've been using the pipeline layout builder directly, then we
        // must flush those changes down to the GraphicsPipelineBuilder
        if (_graphicsDescriptors._dynamicBindings.HasChanges()) {
            VkDescriptorSet descSets[1];
            _graphicsDescriptors._dynamicBindings.GetDescriptorSets(MakeIteratorRange(descSets));
            BindDescriptorSet(
                PipelineType::Graphics, 
                _graphicsDescriptors._dynamicBindingsSlot, descSets[0]);
        }

        if (_graphicsDescriptors._globalBindings.HasChanges()) {
            VkDescriptorSet descSets[1];
            _graphicsDescriptors._globalBindings.GetDescriptorSets(MakeIteratorRange(descSets));
            BindDescriptorSet(
                PipelineType::Graphics,
                _graphicsDescriptors._globalBindingsSlot, descSets[0]);
        }

        if (_currentGraphicsPipeline && !GraphicsPipelineBuilder::IsPipelineStale()) return true;

        assert(_renderPass);
        _currentGraphicsPipeline = GraphicsPipelineBuilder::CreatePipeline(
            *_factory,
            _globalPools->_mainPipelineCache.get(),
            _graphicsDescriptors._pipelineLayout->GetUnderlying(),
            _renderPass, _renderPassSubpass, _renderPassSamples);
        if (_currentGraphicsPipeline) {
            vkCmdBindPipeline(
			    _commandList.get(),
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                _currentGraphicsPipeline.get());
            Bind(_boundViewport);
            return true;
        }

        assert(0);
        return false;
    }

    bool DeviceContext::BindComputePipeline()
    {
        assert(_commandList);

        // If we've been using the pipeline layout builder directly, then we
        // must flush those changes down to the ComputePipelineBuilder
        if (_computeDescriptors._dynamicBindings.HasChanges()) {
            VkDescriptorSet descSets[1];
            _computeDescriptors._dynamicBindings.GetDescriptorSets(MakeIteratorRange(descSets));
            BindDescriptorSet(
                PipelineType::Compute, 
                _computeDescriptors._dynamicBindingsSlot, descSets[0]);
        }

        if (_computeDescriptors._globalBindings.HasChanges()) {
            VkDescriptorSet descSets[1];
            _computeDescriptors._globalBindings.GetDescriptorSets(MakeIteratorRange(descSets));
            BindDescriptorSet(
                PipelineType::Compute,
                _computeDescriptors._globalBindingsSlot, descSets[0]);
        }

        if (_currentComputePipeline && !ComputePipelineBuilder::IsPipelineStale()) return true;

        _currentComputePipeline = ComputePipelineBuilder::CreatePipeline(
            *_factory,
            _globalPools->_mainPipelineCache.get(),
            _computeDescriptors._pipelineLayout->GetUnderlying());
        if (_currentComputePipeline) {
            vkCmdBindPipeline(
			    _commandList.get(),
                VK_PIPELINE_BIND_POINT_COMPUTE,
                _currentComputePipeline.get());

            // note -- currently crashing the GPU if we don't rebind here...?
            CmdBindDescriptorSets(
                VK_PIPELINE_BIND_POINT_COMPUTE,
                _computeDescriptors._pipelineLayout->GetUnderlying(), 
                0, (uint32_t)_computeDescriptors._descriptorSets.size(), AsPointer(_computeDescriptors._descriptorSets.begin()), 
                0, nullptr);
            return true;
        }

        assert(0);
        return false;
    }

    void DeviceContext::Draw(unsigned vertexCount, unsigned startVertexLocation)
    {
		assert(_commandList);
        assert(_renderPass);
		if (BindGraphicsPipeline()) {
            assert(vertexCount);
            vkCmdDraw(
			    _commandList.get(),
                vertexCount, 1,
                startVertexLocation, 0);
        }
    }
    
    void        DeviceContext::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
		assert(_commandList);
        assert(_renderPass);
		if (BindGraphicsPipeline()) {
		    vkCmdDrawIndexed(
			    _commandList.get(),
			    indexCount, 1,
			    startIndexLocation, baseVertexLocation,
			    0);
        }
    }

    void        DeviceContext::DrawAuto() 
    {
        assert(0);      // not implemented
    }

    void        DeviceContext::Dispatch(unsigned countX, unsigned countY, unsigned countZ)
    {
        assert(_commandList);
        assert(!_renderPass);   // dispatch should not happen in a render pass
		if (BindComputePipeline()) {
            vkCmdDispatch(
                _commandList.get(),
                countX, countY, countZ);
        }
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

	std::shared_ptr<DeviceContext> DeviceContext::Fork()
	{
		return std::make_shared<DeviceContext>(
			*_factory, *_globalPools,
			*_graphicsDescriptors._pipelineLayout,
			*_computeDescriptors._pipelineLayout,
			*_cmdPool, _cmdBufferType);
	}

	void		DeviceContext::BeginCommandList()
	{
        // hack -- clear some state
        *(GraphicsPipelineBuilder*)this = GraphicsPipelineBuilder();
        *(ComputePipelineBuilder*)this = ComputePipelineBuilder();
        _currentGraphicsPipeline.reset();
        _currentComputePipeline.reset();

        for (auto& s:_graphicsDescriptors._descriptorSets) s = nullptr;
        _graphicsDescriptors._hasSetsAwaitingFlush = false;
        _graphicsDescriptors._dynamicBindings.Reset();
        _graphicsDescriptors._globalBindings.Reset();
        _graphicsDescriptors._pipelineLayout->RebuildLayout(*_factory); // (rebuild if necessary)

        for (auto& s:_computeDescriptors._descriptorSets) s = nullptr;
        _computeDescriptors._hasSetsAwaitingFlush = false;
        _computeDescriptors._dynamicBindings.Reset();
        _computeDescriptors._globalBindings.Reset();
        _computeDescriptors._pipelineLayout->RebuildLayout(*_factory); // (rebuild if necessary)

        _boundViewport = ViewportDesc();

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

    void        DeviceContext::BeginRenderPass(
        const FrameBuffer& fb,
        TextureSamples samples,
        VectorPattern<int, 2> offset, VectorPattern<unsigned, 2> extent,
        IteratorRange<const ClearValue*> clearValues)
    {
        if (_renderPass)
            Throw(::Exceptions::BasicLabel("Attempting to begin a render pass while another render pass is already in progress"));
        VkRenderPassBeginInfo rp_begin;
		rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rp_begin.pNext = nullptr;
		rp_begin.renderPass = fb.GetLayout();
		rp_begin.framebuffer = fb.GetUnderlying();
		rp_begin.renderArea.offset.x = offset[0];
		rp_begin.renderArea.offset.y = offset[1];
		rp_begin.renderArea.extent.width = extent[0];
		rp_begin.renderArea.extent.height = extent[1];
		
		rp_begin.pClearValues = (const VkClearValue*)clearValues.begin();
		rp_begin.clearValueCount = (uint32_t)clearValues.size();

        vkCmdBeginRenderPass(_commandList.get(), &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
        _renderPass = fb.GetLayout();
        _renderPassSamples = samples;
        _renderPassSubpass = 0u;
        _currentGraphicsPipeline.reset();

        // bind descriptor sets that are pending
        if (_graphicsDescriptors._hasSetsAwaitingFlush) {
            CmdBindDescriptorSets(
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                _graphicsDescriptors._pipelineLayout->GetUnderlying(), 
                0, (uint32_t)_graphicsDescriptors._descriptorSets.size(), AsPointer(_graphicsDescriptors._descriptorSets.begin()), 
                0, nullptr);
            _graphicsDescriptors._hasSetsAwaitingFlush = false;
        }
    }

    void DeviceContext::EndRenderPass()
    {
		vkCmdEndRenderPass(_commandList.get());
        _renderPass = nullptr;
        _renderPassSamples = TextureSamples::Create();
        _renderPassSubpass = 0u;
        _currentGraphicsPipeline.reset();
    }

    bool DeviceContext::IsInRenderPass() const
    {
        return _renderPass != nullptr;
    }

    void                        DeviceContext::SetPresentationTarget(RenderTargetView* presentationTarget, const VectorPattern<unsigned,2>& dims)
    {
        // _namedResources.Bind(0u, *presentationTarget);
        _presentationTargetDims = dims;
    }

    VectorPattern<unsigned,2>   DeviceContext::GetPresentationTargetDims()
    {
        return _presentationTargetDims;
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

    void DeviceContext::CmdNextSubpass(VkSubpassContents contents)
    {
        vkCmdNextSubpass(_commandList.get(), contents);
        _currentGraphicsPipeline.reset();
        ++_renderPassSubpass;
    }

    void DeviceContext::CmdPipelineBarrier(
        VkPipelineStageFlags            srcStageMask,
        VkPipelineStageFlags            dstStageMask,
        VkDependencyFlags               dependencyFlags,
        uint32_t                        memoryBarrierCount,
        const VkMemoryBarrier*          pMemoryBarriers,
        uint32_t                        bufferMemoryBarrierCount,
        const VkBufferMemoryBarrier*    pBufferMemoryBarriers,
        uint32_t                        imageMemoryBarrierCount,
        const VkImageMemoryBarrier*     pImageMemoryBarriers)
    {
        vkCmdPipelineBarrier(
            _commandList.get(),
            srcStageMask, dstStageMask,
            dependencyFlags, 
            memoryBarrierCount, pMemoryBarriers,
            bufferMemoryBarrierCount, pBufferMemoryBarriers,
            imageMemoryBarrierCount, pImageMemoryBarriers);
    }

    void DeviceContext::CmdPushConstants(
        VkPipelineLayout layout,
        VkShaderStageFlags stageFlags,
        uint32_t offset,
        uint32_t size,
        const void* pValues)
    {
        vkCmdPushConstants(
            _commandList.get(),
            layout, stageFlags,
            offset, size, pValues);
    }

	void DeviceContext::CmdWriteTimestamp(
		VkPipelineStageFlagBits pipelineStage,
		VkQueryPool queryPool, uint32_t query)
	{
		vkCmdWriteTimestamp(_commandList.get(), pipelineStage, queryPool, query);
	}

	void DeviceContext::CmdResetQueryPool(
		VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
	{
		vkCmdResetQueryPool(_commandList.get(), queryPool, firstQuery, queryCount);
	}

	void DeviceContext::CmdSetEvent(VkEvent evnt, VkPipelineStageFlags stageMask)
	{
		vkCmdSetEvent(_commandList.get(), evnt, stageMask);
	}

    DeviceContext::DeviceContext(
        const ObjectFactory&    factory, 
        GlobalPools&            globalPools,
        PipelineLayout&         globalPipelineLayout,
        PipelineLayout&         computePipelineLayout,
		CommandPool&            cmdPool, 
		CommandBufferType		cmdBufferType)
    : _cmdPool(&cmdPool), _cmdBufferType(cmdBufferType)
    , _factory(&factory)
    , _globalPools(&globalPools)
    , _renderPass(nullptr)
    , _renderPassSubpass(0u)
    , _renderPassSamples(TextureSamples::Create())
    , _graphicsDescriptors(factory, globalPools, globalPipelineLayout)
    , _computeDescriptors(factory, globalPools, computePipelineLayout)
    {
    }

	void DeviceContext::PrepareForDestruction(IDevice*, IPresentationChain*) {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    DescriptorCollection::DescriptorCollection(
        const ObjectFactory&    factory, 
        GlobalPools&            globalPools,
        PipelineLayout&         pipelineLayout)
    : _pipelineLayout(&pipelineLayout)
    , _dynamicBindings(
        factory, globalPools._mainDescriptorPool, globalPools._dummyResources, 
        pipelineLayout.GetDescriptorSetLayout(1), pipelineLayout.ShareRootSignature()->_descriptorSets[1],
        10, 30, 0, 0)
    , _dynamicBindingsSlot(1u)
    , _globalBindings(
        factory, globalPools._mainDescriptorPool, globalPools._dummyResources, 
        pipelineLayout.GetDescriptorSetLayout(2), pipelineLayout.ShareRootSignature()->_descriptorSets[2],
        0, 0, 0, 0)
    , _globalBindingsSlot(2u)
    {
        _descriptorSets.resize(pipelineLayout.GetDescriptorSetCount(), nullptr);
        _hasSetsAwaitingFlush = false;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class DescriptorSetBuilder::Pimpl
    {
    public:
        static const unsigned s_pendingBufferLength = 32;
        static const unsigned s_descriptorSetCount = 1;
        static const unsigned s_maxBindings = 64u;

        VkDescriptorBufferInfo  _bufferInfo[s_pendingBufferLength];
        VkDescriptorImageInfo   _imageInfo[s_pendingBufferLength];
        VkWriteDescriptorSet    _writes[s_pendingBufferLength];

        unsigned _pendingWrites;
        unsigned _pendingImageInfos;
        unsigned _pendingBufferInfos;

        VkDescriptorSetLayout               _layouts[s_descriptorSetCount];
        VulkanUniquePtr<VkDescriptorSet>    _activeDescSets[s_descriptorSetCount];
        VulkanUniquePtr<VkDescriptorSet>    _defaultDescSets[s_descriptorSetCount];

        uint64              _sinceLastFlush[s_descriptorSetCount];
        uint64              _slotsFilled[s_descriptorSetCount];

        DescriptorPool*     _descriptorPool;
        VkSampler           _dummySampler;

        unsigned    _srvMapping[s_maxBindings];
        unsigned    _srvBufferMapping[s_maxBindings];
        unsigned    _uavMapping[s_maxBindings];
        unsigned    _uavBufferMapping[s_maxBindings];
        unsigned    _cbMapping[s_maxBindings];
        unsigned    _samplerMapping[s_maxBindings];
        int         _srvBindingOffset;
        int         _uavBindingOffset;
        int         _cbBindingOffset;
        int         _samplerBindingOffset;

        template<typename BindingInfo> void WriteBinding(unsigned bindingPoint, unsigned descriptorSet, VkDescriptorType type, const BindingInfo& bindingInfo);
        template<typename BindingInfo> BindingInfo& AllocateInfo(const BindingInfo& init);
    };

    template<typename BindingInfo> static BindingInfo*& InfoPtr(VkWriteDescriptorSet& writeDesc);
    template<> static VkDescriptorImageInfo*& InfoPtr(VkWriteDescriptorSet& writeDesc)
    {
        return *const_cast<VkDescriptorImageInfo**>(&writeDesc.pImageInfo);
    }

    template<> static VkDescriptorBufferInfo*& InfoPtr(VkWriteDescriptorSet& writeDesc)
    {
        return *const_cast<VkDescriptorBufferInfo**>(&writeDesc.pBufferInfo);
    }

    template<> 
        VkDescriptorImageInfo& DescriptorSetBuilder::Pimpl::AllocateInfo(const VkDescriptorImageInfo& init)
    {
        assert(_pendingImageInfos < s_pendingBufferLength);
        auto& i = _imageInfo[_pendingImageInfos++];
        i = init;
        return i;
    }

    template<> 
        VkDescriptorBufferInfo& DescriptorSetBuilder::Pimpl::AllocateInfo(const VkDescriptorBufferInfo& init)
    {
        assert(_pendingBufferInfos < s_pendingBufferLength);
        auto& i = _bufferInfo[_pendingBufferInfos++];
        i = init;
        return i;
    }

    template<typename BindingInfo>
        void    DescriptorSetBuilder::Pimpl::WriteBinding(unsigned bindingPoint, unsigned descriptorSet, VkDescriptorType type, const BindingInfo& bindingInfo)
    {
            // (we're limited by the number of bits in _sinceLastFlush)
        if (bindingPoint >= 64u) {
            LogWarning << "Cannot bind to binding point " << bindingPoint;
            return;
        }

        if (_sinceLastFlush[descriptorSet] & (1ull<<bindingPoint)) {
            // we already have a pending write to this slot. Let's find it, and just
            // update the details with the new view.
            bool foundExisting = false; (void)foundExisting;
            for (unsigned p=0; p<_pendingWrites; ++p) {
                auto& w = _writes[p];
                if (w.descriptorType == type && w.dstBinding == bindingPoint) {
                    *InfoPtr<BindingInfo>(w) = bindingInfo;
                    foundExisting = true;
                    break;
                }
            }
            assert(foundExisting);
        } else {
            _sinceLastFlush[descriptorSet] |= 1ull<<bindingPoint;

            assert(_pendingWrites < Pimpl::s_pendingBufferLength);
            auto& w = _writes[_pendingWrites++];
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.pNext = nullptr;
            w.dstSet = nullptr;
            w.dstBinding = bindingPoint;
            w.dstArrayElement = 0;
            w.descriptorCount = 1;
            w.descriptorType = type;

            InfoPtr<BindingInfo>(w) = &AllocateInfo(bindingInfo);
        }
    }

    void    DescriptorSetBuilder::BindSRV(Stage stage, unsigned startingPoint, IteratorRange<const TextureView*const*> resources)
    {
        const auto descriptorSet = 0u;
        for (unsigned c=0; c<unsigned(resources.size()); ++c) {
            if (!resources[c] || (!resources[c]->GetImageView() && !resources[c]->GetResource())) continue;
            unsigned binding = startingPoint + c + _pimpl->_srvBindingOffset;
            assert(binding < Pimpl::s_maxBindings);

            if (resources[c]->GetImageView()) {
                if (_pimpl->_srvMapping[binding] == ~0u) {
                    LogWarning << "SRV image mapping is off root signature";
                    continue;
                }

                _pimpl->WriteBinding(
                    _pimpl->_srvMapping[binding], descriptorSet, 
                    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        // (could be a COMBINED_IMAGE_SAMPLER or just a SAMPLED_IMAGE)
                    VkDescriptorImageInfo { nullptr, resources[c]->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
            } else {
                if (_pimpl->_srvBufferMapping[binding] == ~0u) {
                    LogWarning << "SRV buffer mapping is off root signature";
                    continue;
                }
            
                // This is a "structured buffer" in the DirectX terminology
                auto buffer = UnderlyingResourcePtr(resources[c]->GetResource()).get()->GetBuffer();
                assert(buffer);
                _pimpl->WriteBinding(
                    _pimpl->_srvBufferMapping[binding], descriptorSet, 
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VkDescriptorBufferInfo { buffer, 0, VK_WHOLE_SIZE });
            }
        }
    }

    void    DescriptorSetBuilder::BindUAV(Stage stage, unsigned startingPoint, IteratorRange<const TextureView*const*> resources)
    {
        const auto descriptorSet = 0u;
        for (unsigned c=0; c<unsigned(resources.size()); ++c) {
            if (!resources[c] || (!resources[c]->GetImageView() && !resources[c]->GetResource())) continue;
            unsigned binding = startingPoint + c + _pimpl->_uavBindingOffset;
            assert(binding < Pimpl::s_maxBindings);

            // We could be binding either a "gimageXX" type, or a "buffer" type. Check the texture 
            // to see if there is an image view attached.
            // The type must correspond to what the layout is expecting.

            if (resources[c]->GetImageView()) {
                if (_pimpl->_uavMapping[binding] == ~0u) {
                    LogWarning << "UAV image mapping is off root signature";
                    continue;
                }
            
                // note --  load and store operations can only be performed in VK_IMAGE_LAYOUT_GENERAL
                _pimpl->WriteBinding(
                    _pimpl->_uavMapping[binding], descriptorSet, 
                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    VkDescriptorImageInfo { nullptr, resources[c]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL });
            } else {
                if (_pimpl->_uavBufferMapping[binding] == ~0u) {
                    LogWarning << "UAV buffer mapping is off root signature";
                    continue;
                }
            
                auto buffer = UnderlyingResourcePtr(resources[c]->GetResource()).get()->GetBuffer();
                assert(buffer);
                _pimpl->WriteBinding(
                    _pimpl->_uavBufferMapping[binding], descriptorSet, 
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    VkDescriptorBufferInfo { buffer, 0, VK_WHOLE_SIZE });
            }
        }
    }

    void    DescriptorSetBuilder::Bind(Stage stage, unsigned startingPoint, IteratorRange<const VkBuffer*> uniformBuffers)
    {
        const auto descriptorSet = 0u;
        for (unsigned c=0; c<unsigned(uniformBuffers.size()); ++c) {
            if (!uniformBuffers[c]) continue;
            unsigned binding = startingPoint + c + _pimpl->_cbBindingOffset;
            assert(binding < Pimpl::s_maxBindings);

            if (_pimpl->_cbMapping[binding] == ~0u) {
                LogWarning << "Uniform buffer mapping is off root signature";
                continue;
            }

            _pimpl->WriteBinding(
                _pimpl->_cbMapping[binding], descriptorSet, 
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VkDescriptorBufferInfo { uniformBuffers[c], 0, VK_WHOLE_SIZE });
        }
    }

    void    DescriptorSetBuilder::Bind(Stage stage, unsigned startingPoint, IteratorRange<const VkSampler*> samplers)
    {
        const auto descriptorSet = 0u;
        for (unsigned c=0; c<unsigned(samplers.size()); ++c) {
            if (!samplers[c]) continue;
            unsigned binding = startingPoint + c + _pimpl->_samplerBindingOffset;
            assert(binding < Pimpl::s_maxBindings);

            if (_pimpl->_samplerMapping[binding] == ~0u) {
                LogWarning << "Sampler mapping is off root signature";
                continue;
            }

            _pimpl->WriteBinding(
                _pimpl->_samplerMapping[binding], descriptorSet, 
                VK_DESCRIPTOR_TYPE_SAMPLER,
                VkDescriptorImageInfo { samplers[c] });
        }
    }

    void    DescriptorSetBuilder::GetDescriptorSets(IteratorRange<VkDescriptorSet*> dst)
    {
        // If we've had any changes this last time, we must create new
        // descriptor sets. We will use vkUpdateDescriptorSets to fill in these
        // sets with the latest changes. Note that this will require copy across the
        // bindings that haven't changed.
        // It turns out that copying using VkCopyDescriptorSet is probably going to be
        // slow. We should try a different approach.
        if (_pimpl->_pendingWrites || !_pimpl->_activeDescSets[0]) {
            VulkanUniquePtr<VkDescriptorSet> newSets[Pimpl::s_descriptorSetCount];
            _pimpl->_descriptorPool->Allocate(MakeIteratorRange(newSets), MakeIteratorRange(_pimpl->_layouts));

            for (unsigned c=0; c<_pimpl->_pendingWrites; ++c)
                _pimpl->_writes[c].dstSet = newSets[0].get();

            VkCopyDescriptorSet copies[Pimpl::s_maxBindings * Pimpl::s_descriptorSetCount];
            unsigned copyCount = 0;
            for (unsigned s=0; s<Pimpl::s_descriptorSetCount; ++s) {
                auto set = _pimpl->_activeDescSets[s].get();
                if (!set) set = _pimpl->_defaultDescSets[s].get();

                auto filledButNotWritten = _pimpl->_slotsFilled[s] & ~_pimpl->_sinceLastFlush[s];
                unsigned msbBit = 64u - xl_clz8(filledButNotWritten);
                unsigned lsbBit = xl_ctz8(filledButNotWritten);
                for (unsigned b=lsbBit; b<=msbBit; ++b) {
                    if (filledButNotWritten & (1ull<<b)) {
                        assert(copyCount < dimof(copies));
                        auto& cpy = copies[copyCount++];
                        cpy.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
                        cpy.pNext = nullptr;

                        cpy.srcSet = set;
                        cpy.srcBinding = b;
                        cpy.srcArrayElement = 0;

                        cpy.dstSet = newSets[s].get();
                        cpy.dstBinding = b;
                        cpy.dstArrayElement = 0;
                        cpy.descriptorCount = 1;        // (we can set this higher to set multiple sequential descriptors)
                    }
                }
            }

            vkUpdateDescriptorSets(
                _pimpl->_descriptorPool->GetDevice(), 
                _pimpl->_pendingWrites, _pimpl->_writes, 
                copyCount, copies);

            _pimpl->_pendingWrites = 0;
            _pimpl->_pendingImageInfos = 0;
            _pimpl->_pendingBufferInfos = 0;

            for (unsigned c=0; c<Pimpl::s_descriptorSetCount; ++c) {
                _pimpl->_slotsFilled[c] |= _pimpl->_sinceLastFlush[c];
                _pimpl->_sinceLastFlush[c] = 0ull;
                _pimpl->_activeDescSets[c] = std::move(newSets[c]);
            }
        }

        for (unsigned c=0; c<unsigned(dst.size()); ++c)
            dst[c] = (c < Pimpl::s_descriptorSetCount) ? _pimpl->_activeDescSets[c].get() : nullptr;
    }

    bool    DescriptorSetBuilder::HasChanges() const
    {
        // note --  we have to bind some descriptor set for the first draw of the frame,
        //          even if nothing has been bound! So, when _activeDescSets is empty
        //          we must return true here.
        return _pimpl->_pendingWrites != 0 || !_pimpl->_activeDescSets[0];
    }

    void    DescriptorSetBuilder::Reset()
    {
        _pimpl->_pendingWrites = 0u;
        _pimpl->_pendingImageInfos = 0u;
        _pimpl->_pendingBufferInfos = 0u;

        XlZeroMemory(_pimpl->_bufferInfo);
        XlZeroMemory(_pimpl->_imageInfo);
        XlZeroMemory(_pimpl->_writes);

        for (unsigned c=0; c<Pimpl::s_descriptorSetCount; ++c) {
            _pimpl->_sinceLastFlush[c] = 0x0u;
            _pimpl->_activeDescSets[c] = nullptr;
        }
    }

    DescriptorSetBuilder::DescriptorSetBuilder(
        const ObjectFactory& factory,
        DescriptorPool& descPool, 
        DummyResources& defResources,
        VkDescriptorSetLayout layout,
        const DescriptorSetSignature& signature,
        int cbBindingOffset, int srvBindingOffset, 
        int samplerBindingOffset, int uavBindingOffset)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_descriptorPool = &descPool;
        _pimpl->_dummySampler = defResources._blankSampler->GetUnderlying();
        _pimpl->_layouts[0] = layout;
        _pimpl->_cbBindingOffset = cbBindingOffset;
        _pimpl->_srvBindingOffset = srvBindingOffset;
        _pimpl->_samplerBindingOffset = samplerBindingOffset;
        _pimpl->_uavBindingOffset = uavBindingOffset;

        for (unsigned c=0; c<Pimpl::s_descriptorSetCount; ++c) {
            _pimpl->_slotsFilled[c] = 0x0ull;
            _pimpl->_sinceLastFlush[c] = 0x0ull;
        }

        // We need to make a mapping between the HLSL binding numbers and types
        // and the binding index in our descriptor set

        for (auto& m:_pimpl->_srvMapping) m = ~0u;
        for (auto& m:_pimpl->_srvBufferMapping) m = ~0u;
        for (auto& m:_pimpl->_uavMapping) m = ~0u;
        for (auto& m:_pimpl->_uavBufferMapping) m = ~0u;
        for (auto& m:_pimpl->_cbMapping) m = ~0u;
        for (auto& m:_pimpl->_samplerMapping) m = ~0u;

        for (unsigned bIndex=0; bIndex<(unsigned)signature._bindings.size(); ++bIndex) {
            const auto& b = signature._bindings[bIndex];
            assert(b._hlslBindingIndex < Pimpl::s_maxBindings);
            if (b._type == DescriptorSetBindingSignature::Type::Texture) {
                assert(_pimpl->_srvMapping[b._hlslBindingIndex] == ~0u);
                _pimpl->_srvMapping[b._hlslBindingIndex] = bIndex;
            } else if (b._type == DescriptorSetBindingSignature::Type::TextureAsBuffer) {
                assert(_pimpl->_srvBufferMapping[b._hlslBindingIndex] == ~0u);
                _pimpl->_srvBufferMapping[b._hlslBindingIndex] = bIndex;
            }  else if (b._type == DescriptorSetBindingSignature::Type::ConstantBuffer) {
                assert(_pimpl->_cbMapping[b._hlslBindingIndex] == ~0u);
                _pimpl->_cbMapping[b._hlslBindingIndex] = bIndex;
            } else if (b._type == DescriptorSetBindingSignature::Type::Sampler) {
                assert(_pimpl->_samplerMapping[b._hlslBindingIndex] == ~0u);
                _pimpl->_samplerMapping[b._hlslBindingIndex] = bIndex;
            } else if (b._type == DescriptorSetBindingSignature::Type::UnorderedAccess) {
                assert(_pimpl->_uavMapping[b._hlslBindingIndex] == ~0u);
                _pimpl->_uavMapping[b._hlslBindingIndex] = bIndex;
            } else if (b._type == DescriptorSetBindingSignature::Type::UnorderedAccessAsBuffer) {
                assert(_pimpl->_uavBufferMapping[b._hlslBindingIndex] == ~0u);
                _pimpl->_uavBufferMapping[b._hlslBindingIndex] = bIndex;
            }
        }

        Reset();

        // Create the default resources binding sets...
        VkBuffer defaultBuffers[Pimpl::s_maxBindings];
        const TextureView* defaultSRVImages[Pimpl::s_maxBindings];
        const TextureView* defaultSRVBuffers[Pimpl::s_maxBindings];
        const TextureView* defaultUAVImages[Pimpl::s_maxBindings];
        const TextureView* defaultUAVBuffers[Pimpl::s_maxBindings];
        VkSampler defaultSamplers[Pimpl::s_maxBindings];
        for (unsigned c=0; c<Pimpl::s_maxBindings; ++c) {
            defaultBuffers[c] = (_pimpl->_cbMapping[c+_pimpl->_cbBindingOffset] != ~0u) ? defResources._blankBuffer.GetUnderlying() : nullptr;
            defaultSRVImages[c] = (_pimpl->_srvMapping[c+_pimpl->_srvBindingOffset] != ~0u) ? &defResources._blankSrv : nullptr;
            defaultSRVBuffers[c] = (_pimpl->_srvBufferMapping[c+_pimpl->_srvBindingOffset] != ~0u) ? &defResources._blankUavBuffer : nullptr;
            defaultUAVImages[c] = (_pimpl->_uavMapping[c+_pimpl->_uavBindingOffset] != ~0u) ? &defResources._blankUavImage : nullptr;
            defaultUAVBuffers[c] = (_pimpl->_uavBufferMapping[c+_pimpl->_uavBindingOffset] != ~0u) ? &defResources._blankUavBuffer : nullptr;
            defaultSamplers[c] = (_pimpl->_samplerMapping[c+_pimpl->_samplerBindingOffset] != ~0u) ? defResources._blankSampler->GetUnderlying() : nullptr;
        }
        Bind(Stage::Vertex, 0, IteratorRange<const VkBuffer*>(defaultBuffers, &defaultBuffers[dimof(defaultBuffers) - _pimpl->_cbBindingOffset]));
        Bind(Stage::Vertex, 0, IteratorRange<const VkSampler*>(defaultSamplers, &defaultSamplers[dimof(defaultSamplers) - _pimpl->_samplerBindingOffset]));
        BindSRV(Stage::Vertex, 0, IteratorRange<const TextureView**>(defaultSRVImages, &defaultSRVImages[dimof(defaultSRVImages) - _pimpl->_srvBindingOffset]));
        BindSRV(Stage::Vertex, 0, IteratorRange<const TextureView**>(defaultSRVBuffers, &defaultSRVBuffers[dimof(defaultSRVBuffers) - _pimpl->_srvBindingOffset]));
        BindUAV(Stage::Vertex, 0, IteratorRange<const TextureView**>(defaultUAVImages, &defaultUAVImages[dimof(defaultUAVImages) - _pimpl->_uavBindingOffset]));
        BindUAV(Stage::Vertex, 0, IteratorRange<const TextureView**>(defaultUAVBuffers, &defaultUAVBuffers[dimof(defaultUAVBuffers) - _pimpl->_uavBindingOffset]));

        // Create the descriptor sets, and then move them into "_defaultDescSets"
        // Note that these sets must come from a non-resetable permanent pool
        GetDescriptorSets(IteratorRange<VkDescriptorSet*>());
        for (unsigned c=0; c<Pimpl::s_descriptorSetCount; ++c)
            _pimpl->_defaultDescSets[c] = std::move(_pimpl->_activeDescSets[c]);
    }

    DescriptorSetBuilder::~DescriptorSetBuilder()
    {
    }

}}

