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
#include "PipelineLayoutSignatureFile.h"
#include "ShaderReflection.h"
#include "../IDeviceVulkan.h"
#include "../../Format.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/ArithmeticUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{
	
///////////////////////////////////////////////////////////////////////////////////////////////////

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

    void        GraphicsPipelineBuilder::SetBoundInputLayout(const BoundInputLayout& inputLayout)
    {
        if (_inputLayout != &inputLayout) {
            _pipelineStale = true;
            _inputLayout = &inputLayout;
        }
    }

	void		GraphicsPipelineBuilder::UnbindInputLayout()
	{
		if (_inputLayout) {
			_pipelineStale = true;
			_inputLayout = nullptr;
		}
	}

	void        GraphicsPipelineBuilder::Bind(const ShaderProgram& shaderProgram)
	{
		shaderProgram.Apply(*this);
	}

	void        GraphicsPipelineBuilder::Bind(const ShaderProgram& shaderProgram, const BoundClassInterfaces& bci)
	{
		shaderProgram.Apply(*this, bci);
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

    static VkPipelineShaderStageCreateInfo BuildShaderStage(VkShaderModule shader, VkShaderStageFlagBits stage)
    {
        VkPipelineShaderStageCreateInfo result = {};
        result.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        result.pNext = nullptr;
        result.flags = 0;
        result.stage = stage;
        result.module = shader;
        result.pName = "main";
        result.pSpecializationInfo = nullptr;
        return result;
    }

    VulkanUniquePtr<VkPipeline> GraphicsPipelineBuilder::CreatePipeline(
        ObjectFactory& factory,
        VkPipelineCache pipelineCache,
        VkRenderPass renderPass, unsigned subpass, 
        TextureSamples samples)
    {
		assert(_shaderProgram && renderPass);

        VkPipelineShaderStageCreateInfo shaderStages[3];
        uint32_t shaderStageCount = 0;
		const auto& vs = _shaderProgram->GetModule(ShaderStage::Vertex);
		const auto& gs = _shaderProgram->GetModule(ShaderStage::Geometry);
		const auto& ps = _shaderProgram->GetModule(ShaderStage::Pixel);
        if (vs) shaderStages[shaderStageCount++] = BuildShaderStage(vs.get(), VK_SHADER_STAGE_VERTEX_BIT);
        if (gs) shaderStages[shaderStageCount++] = BuildShaderStage(gs.get(), VK_SHADER_STAGE_GEOMETRY_BIT);
		if (ps) shaderStages[shaderStageCount++] = BuildShaderStage(ps.get(), VK_SHADER_STAGE_FRAGMENT_BIT);

        VkDynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE];
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pNext = nullptr;
        dynamicState.pDynamicStates = dynamicStateEnables;
        dynamicState.dynamicStateCount = 0;

        VkPipelineVertexInputStateCreateInfo vi = {};
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.pNext = nullptr;
        vi.flags = 0;
        vi.vertexBindingDescriptionCount = 0;
		vi.pVertexBindingDescriptions = nullptr;
        vi.vertexAttributeDescriptionCount = 0;
        vi.pVertexAttributeDescriptions = nullptr;

        if (_inputLayout) {
            auto attribs = _inputLayout->GetAttributes();
            vi.vertexAttributeDescriptionCount = (uint32)attribs.size();
            vi.pVertexAttributeDescriptions = attribs.begin();

			auto vbBindings = _inputLayout->GetVBBindings();
			vi.vertexBindingDescriptionCount = (uint32)vbBindings.size();
			vi.pVertexBindingDescriptions = vbBindings.begin();
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
		pipeline.layout = _pipelineLayoutBuilder.GetPipelineLayout();
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

        _pipelineStale = cloneFrom._pipelineStale;
        return *this;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void        ComputePipelineBuilder::Bind(const ComputeShader& shader)
    {
        _shader = &shader;
		_pipelineLayoutBuilder.SetShaderBasedDescriptorSets(*shader._pipelineLayoutConfig);
        _pipelineStale = true;
    }

    VulkanUniquePtr<VkPipeline> ComputePipelineBuilder::CreatePipeline(
        ObjectFactory& factory,
        VkPipelineCache pipelineCache)
    {
        VkComputePipelineCreateInfo pipeline = {};
        pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline.pNext = nullptr;
        pipeline.flags = 0; // VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
		pipeline.layout = _pipelineLayoutBuilder.GetPipelineLayout();
        pipeline.basePipelineHandle = VK_NULL_HANDLE;
        pipeline.basePipelineIndex = 0;

        assert(_shader);
        pipeline.stage = BuildShaderStage(_shader->GetModule().get(), VK_SHADER_STAGE_COMPUTE_BIT);

        auto result = factory.CreateComputePipeline(pipelineCache, pipeline);
        _pipelineStale = false;
        return std::move(result);
    }

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
		assert(_commandList.GetUnderlying());
        _boundViewport = viewport;
        vkCmdSetViewport(_commandList.GetUnderlying().get(), 0, 1, (VkViewport*)&viewport);

            // todo -- get this right for non-integer coords
        VkRect2D scissor = {
            {int(viewport.TopLeftX), int(viewport.TopLeftY)},
            {unsigned(viewport.Width), unsigned(viewport.Height)},
        };
        vkCmdSetScissor(
			_commandList.GetUnderlying().get(),
            0, 1,
            &scissor);
    }

	void        DeviceContext::Bind(const Resource& ib, Format indexFormat, unsigned offset)
	{
		assert(_commandList.GetUnderlying());
        vkCmdBindIndexBuffer(
			_commandList.GetUnderlying().get(),
			ib.GetBuffer(),
			offset,
			indexFormat == Format::R32_UINT ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
	}

    void        DeviceContext::BindDescriptorSet(
		PipelineType pipelineType, unsigned index, VkDescriptorSet set
		VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, DescriptorSetVerboseDescription&& description))
    {
        // compute descriptors must be bound outside of a render pass, but graphics descriptors must be bound inside of it
        if (pipelineType == PipelineType::Compute) { assert(!_renderPass); } 

        auto& collection = (pipelineType == PipelineType::Compute) ? _computeDescriptors : _graphicsDescriptors;
        if (index < (unsigned)collection._descriptorSets.size() && collection._descriptorSets[index] != set) {
            collection._descriptorSets[index] = set;
			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				collection._descInfo[index]._currentlyBoundDescription = std::move(description);
			#endif

            if (_renderPass || pipelineType == PipelineType::Compute) {
				if (pipelineType == PipelineType::Compute) {
					if (GetBoundComputeShader()) {
						assert(index < ComputePipelineBuilder::_pipelineLayoutBuilder.GetDescriptorSetCount());
						_commandList.BindDescriptorSets(
							VK_PIPELINE_BIND_POINT_COMPUTE,
							ComputePipelineBuilder::_pipelineLayoutBuilder.GetPipelineLayout(),
							index, 1, &collection._descriptorSets[index], 
							0, nullptr);
						return;
					}
				} else {
					if (GetBoundShaderProgram()) {
						assert(index < GraphicsPipelineBuilder::_pipelineLayoutBuilder.GetDescriptorSetCount());
						_commandList.BindDescriptorSets(
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							GraphicsPipelineBuilder::_pipelineLayoutBuilder.GetPipelineLayout(),
							index, 1, &collection._descriptorSets[index], 
							0, nullptr);
						return;
					}
				}
            }

            collection._hasSetsAwaitingFlush = true;    // (will be bound when the render pass begins)
        }
    }

	void			DeviceContext::PushConstants(VkShaderStageFlags stageFlags, IteratorRange<const void*> data)
	{
		assert(!(stageFlags & VK_SHADER_STAGE_COMPUTE_BIT));
		GetActiveCommandList().PushConstants(
			GraphicsPipelineBuilder::_pipelineLayoutBuilder.GetPipelineLayout(),
            stageFlags, 0, (uint32_t)data.size(), data.begin());
	}

	void		DeviceContext::RebindNumericDescriptorSet(PipelineType pipelineType)
	{
		if (pipelineType == PipelineType::Graphics) {
			VkDescriptorSet descSets[1];
			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				DescriptorSetVerboseDescription* descriptions[1];
				_graphicsDescriptors._numericBindings.GetDescriptorSets(MakeIteratorRange(descSets), MakeIteratorRange(descriptions));
				BindDescriptorSet(
					PipelineType::Graphics, _graphicsDescriptors._numericBindingsSlot, descSets[0], 
					DescriptorSetVerboseDescription{*descriptions[0]});
			#else
				_graphicsDescriptors._numericBindings.GetDescriptorSets(MakeIteratorRange(descSets));
				BindDescriptorSet(PipelineType::Graphics, _graphicsDescriptors._numericBindingsSlot, descSets[0]);
			#endif
		} else {
			VkDescriptorSet descSets[1];
			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				DescriptorSetVerboseDescription* descriptions[1];
				_computeDescriptors._numericBindings.GetDescriptorSets(MakeIteratorRange(descSets), MakeIteratorRange(descriptions));
				BindDescriptorSet(
					PipelineType::Compute, _computeDescriptors._numericBindingsSlot, descSets[0], 
					DescriptorSetVerboseDescription{*descriptions[0]});
			#else
				_computeDescriptors._numericBindings.GetDescriptorSets(MakeIteratorRange(descSets));
				BindDescriptorSet(PipelineType::Compute, _computeDescriptors._numericBindingsSlot, descSets[0]);
			#endif
		}
	}

    bool        DeviceContext::BindGraphicsPipeline()
    {
		assert(_commandList.GetUnderlying());

        // If we've been using the pipeline layout builder directly, then we
        // must flush those changes down to the GraphicsPipelineBuilder
        if (_graphicsDescriptors._numericBindings.HasChanges()) {
			RebindNumericDescriptorSet(PipelineType::Graphics);
        }

        if (_currentGraphicsPipeline && !GraphicsPipelineBuilder::IsPipelineStale()) return true;

		if (!_renderPass) {
			Log(Warning) << "Attempting to bind graphics pipeline without a render pass" << std::endl;
			return false;
		}

		_currentGraphicsPipeline = GraphicsPipelineBuilder::CreatePipeline(
            *_factory, _globalPools->_mainPipelineCache.get(),
            _renderPass, _renderPassSubpass, _renderPassSamples);
        assert(_currentGraphicsPipeline);
		LogGraphicsPipeline();

		#if defined(_DEBUG)
			// check for unbound descriptor sets
			const auto& sig = *GetBoundShaderProgram()->_pipelineLayoutConfig;
			for (unsigned c=0; c<sig._descriptorSets.size(); ++c)
				if (c >= _graphicsDescriptors._descriptorSets.size() || !_graphicsDescriptors._descriptorSets[c]) {
					Log(Warning) << "Graphics descriptor set index [" << c << "] (" << sig._descriptorSets[c]._name << ") is unbound when creating pipeline. This will probably result in a crash." << std::endl;
				}
		#endif

        vkCmdBindPipeline(
			_commandList.GetUnderlying().get(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            _currentGraphicsPipeline.get());
        Bind(_boundViewport);
        return true;
    }

    bool DeviceContext::BindComputePipeline()
    {
        assert(_commandList.GetUnderlying());

        // If we've been using the pipeline layout builder directly, then we
        // must flush those changes down to the ComputePipelineBuilder
        if (_computeDescriptors._numericBindings.HasChanges()) {
			RebindNumericDescriptorSet(PipelineType::Compute);
        }

		if (_computeDescriptors._hasSetsAwaitingFlush) {
            _commandList.BindDescriptorSets(
                VK_PIPELINE_BIND_POINT_COMPUTE,
				ComputePipelineBuilder::_pipelineLayoutBuilder.GetPipelineLayout(),
                0, ComputePipelineBuilder::_pipelineLayoutBuilder.GetDescriptorSetCount(), AsPointer(_computeDescriptors._descriptorSets.begin()), 
                0, nullptr);
            _computeDescriptors._hasSetsAwaitingFlush = false;
        }

        if (_currentComputePipeline && !ComputePipelineBuilder::IsPipelineStale()) return true;

        _currentComputePipeline = ComputePipelineBuilder::CreatePipeline(
            *_factory, _globalPools->_mainPipelineCache.get());
        assert(_currentComputePipeline);
		LogComputePipeline();

		#if defined(_DEBUG)
			// check for unbound descriptor sets
			const auto& sig = *GetBoundShaderProgram()->_pipelineLayoutConfig;
			for (unsigned c=0; c<sig._descriptorSets.size(); ++c)
				if (c >= _computeDescriptors._descriptorSets.size() || !_computeDescriptors._descriptorSets[c]) {
					Log(Warning) << "Compute descriptor set index [" << c << "] (" << sig._descriptorSets[c]._name << ") is unbound when creating pipeline. This will probably result in a crash." << std::endl;
				}
		#endif

        vkCmdBindPipeline(
			_commandList.GetUnderlying().get(),
            VK_PIPELINE_BIND_POINT_COMPUTE,
            _currentComputePipeline.get());

        // note -- currently crashing the GPU if we don't rebind here...?
        /*_commandList.BindDescriptorSets(
            VK_PIPELINE_BIND_POINT_COMPUTE,
            _computeDescriptors._pipelineLayout->GetUnderlying(), 
            0, (uint32_t)_computeDescriptors._descriptorSets.size(), AsPointer(_computeDescriptors._descriptorSets.begin()), 
            0, nullptr);*/
        return true;
    }

	void DeviceContext::LogGraphicsPipeline()
	{
		#if defined(_DEBUG)
			if (!Verbose.IsEnabled()) return;

			Log(Verbose) << "-------------VertexShader------------" << std::endl;
			Log(Verbose) << SPIRVReflection(GetBoundShaderProgram()->GetCompiledCode(ShaderStage::Vertex).GetByteCode()) << std::endl;
			Log(Verbose) << "-------------PixelShader------------" << std::endl;
			Log(Verbose) << SPIRVReflection(GetBoundShaderProgram()->GetCompiledCode(ShaderStage::Pixel).GetByteCode()) << std::endl;
			Log(Verbose) << "-------------Descriptors------------" << std::endl;

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				const CompiledShaderByteCode* shaders[ShaderProgram::s_maxShaderStages] = {};
				for (unsigned c=0; c<dimof(shaders); ++c)
					shaders[c] = &GetBoundShaderProgram()->GetCompiledCode((ShaderStage)c);

				#pragma warning(disable:4239) // HACK -- workaround -- nonstandard extension used: 'argument': conversion from '_Ostr' to 'std::ostream &')
				auto& pipelineLayoutHelper = *GetBoundShaderProgram()->_pipelineLayoutConfig;
				for (unsigned c=0; c<(unsigned)pipelineLayoutHelper._descriptorSets.size(); ++c) {
					auto descSetIdx = pipelineLayoutHelper._descriptorSets[c]._pipelineLayoutBindingIndex;
					WriteDescriptorSet(
						Log(Verbose),
						_graphicsDescriptors._descInfo[descSetIdx]._currentlyBoundDescription,
						*pipelineLayoutHelper._descriptorSets[c]._signature,
						*pipelineLayoutHelper._legacyRegisterBinding,
						MakeIteratorRange(shaders),
						descSetIdx, descSetIdx < _graphicsDescriptors._descriptorSets.size() && _graphicsDescriptors._descriptorSets[descSetIdx] != nullptr);
				}
			#endif
		#endif
	}

	void DeviceContext::LogComputePipeline()
	{
		#if defined(_DEBUG)
			if (!Verbose.IsEnabled()) return;

			Log(Verbose) << "-------------ComputeShader------------" << std::endl;
			Log(Verbose) << SPIRVReflection(GetBoundComputeShader()->GetCompiledShaderByteCode().GetByteCode()) << std::endl;
			Log(Verbose) << "-------------Descriptors------------" << std::endl;

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				const CompiledShaderByteCode* shaders[(unsigned)ShaderStage::Max] = {};
				shaders[(unsigned)ShaderStage::Compute] = &GetBoundComputeShader()->GetCompiledShaderByteCode();

				#pragma warning(disable:4239) // HACK -- workaround -- nonstandard extension used: 'argument': conversion from '_Ostr' to 'std::ostream &')
				auto& pipelineLayoutHelper = *GetBoundComputeShader()->_pipelineLayoutConfig;
				for (unsigned c=0; c<(unsigned)pipelineLayoutHelper._descriptorSets.size(); ++c) {
					auto descSetIdx = pipelineLayoutHelper._descriptorSets[c]._pipelineLayoutBindingIndex;
					WriteDescriptorSet(
						Log(Verbose),
						_computeDescriptors._descInfo[descSetIdx]._currentlyBoundDescription,
						*pipelineLayoutHelper._descriptorSets[c]._signature,
						*pipelineLayoutHelper._legacyRegisterBinding,
						MakeIteratorRange(shaders),
						descSetIdx, descSetIdx < _computeDescriptors._descriptorSets.size() && _computeDescriptors._descriptorSets[descSetIdx] != nullptr);
				}
			#endif
		#endif
	}

    void DeviceContext::Draw(unsigned vertexCount, unsigned startVertexLocation)
    {
		assert(_commandList.GetUnderlying());
		if (BindGraphicsPipeline()) {
            assert(vertexCount);
            vkCmdDraw(
			    _commandList.GetUnderlying().get(),
                vertexCount, 1,
                startVertexLocation, 0);
        }
    }
    
    void        DeviceContext::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
		assert(_commandList.GetUnderlying());
		if (BindGraphicsPipeline()) {
			assert(indexCount);
		    vkCmdDrawIndexed(
			    _commandList.GetUnderlying().get(),
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
        assert(_commandList.GetUnderlying());
        assert(!_renderPass);   // dispatch should not happen in a render pass
		if (BindComputePipeline()) {
            vkCmdDispatch(
                _commandList.GetUnderlying().get(),
                countX, countY, countZ);
        }
    }

    std::shared_ptr<DeviceContext> DeviceContext::Get(IThreadContext& threadContext)
    {
        IThreadContextVulkan* vulkanContext = 
            (IThreadContextVulkan*)threadContext.QueryInterface(
                typeid(IThreadContextVulkan).hash_code());
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
			*_cmdPool, _cmdBufferType, *_tempBufferSpace);
	}

	void		DeviceContext::BeginCommandList()
	{
		if (!_cmdPool) return;
		BeginCommandList(_cmdPool->Allocate(_cmdBufferType));
	}

	void		DeviceContext::BeginCommandList(const VulkanSharedPtr<VkCommandBuffer>& cmdList)
	{
        // hack -- clear some state
        *(GraphicsPipelineBuilder*)this = GraphicsPipelineBuilder();
        *(ComputePipelineBuilder*)this = ComputePipelineBuilder();
        _currentGraphicsPipeline.reset();
        _currentComputePipeline.reset();
		SetupPipelineBuilders();

		for (unsigned c=0; c<_graphicsDescriptors._descriptorSets.size(); ++c) {
			_graphicsDescriptors._descriptorSets[c] = _graphicsDescriptors._descInfo[c]._dummy.get();
			_graphicsDescriptors._descInfo[c]._currentlyBoundDescription = _graphicsDescriptors._descInfo[c]._dummyDescription;
		}
		_graphicsDescriptors._numericBindings.Reset();
		RebindNumericDescriptorSet(PipelineType::Graphics);
        _graphicsDescriptors._hasSetsAwaitingFlush = true;

		for (unsigned c=0; c<_computeDescriptors._descriptorSets.size(); ++c) {
			_computeDescriptors._descriptorSets[c] = _computeDescriptors._descInfo[c]._dummy.get();
			_computeDescriptors._descInfo[c]._currentlyBoundDescription = _computeDescriptors._descInfo[c]._dummyDescription;
		}
		_computeDescriptors._numericBindings.Reset();
		RebindNumericDescriptorSet(PipelineType::Compute);
        _computeDescriptors._hasSetsAwaitingFlush = true;

        _boundViewport = ViewportDesc();

		assert(!_commandList.GetUnderlying());
		_commandList = CommandList(cmdList);

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
		cmd_buf_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		cmd_buf_info.pInheritanceInfo = &inheritInfo;
        auto res = vkBeginCommandBuffer(_commandList.GetUnderlying().get(), &cmd_buf_info);
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while beginning command buffer"));
	}

	void		DeviceContext::ExecuteCommandList(CommandList& cmdList, bool preserveState)
	{
		assert(_commandList.GetUnderlying());
		(void)preserveState;		// we can't handle this properly in Vulkan

		const VkCommandBuffer buffers[] = { cmdList.GetUnderlying().get() };
        vkCmdExecuteCommands(
			_commandList.GetUnderlying().get(),
			dimof(buffers), buffers);
	}

	void		DeviceContext::QueueCommandList(IDevice& device, QueueCommandListFlags::BitField flags)
	{
		IDeviceVulkan* deviceVulkan = (IDeviceVulkan*)device.QueryInterface(typeid(IDeviceVulkan).hash_code());
		if (!deviceVulkan) {
			assert(0);
			return;
		}

		assert(deviceVulkan->GetUnderlyingDevice() == GetUnderlyingDevice());

		auto cmdList = ResolveCommandList();
		auto renderingQueue = deviceVulkan->GetRenderingQueue();

		VkSubmitInfo submitInfo;
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;

		VkCommandBuffer rawCmdBuffers[] = { cmdList->GetUnderlying().get() };
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = 0;
		submitInfo.commandBufferCount = dimof(rawCmdBuffers);
		submitInfo.pCommandBuffers = rawCmdBuffers;

		// Use a fence to stall this method until the execute of the command list is complete
		VkFence f = VK_NULL_HANDLE;
		if (flags & QueueCommandListFlags::Stall) {
			f = _utilityFence.get();
			auto res = vkResetFences(deviceVulkan->GetUnderlyingDevice(), 1, &f);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failure while resetting utility fence"));
		}
		
		auto res = vkQueueSubmit(renderingQueue, 1, &submitInfo, f);
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while queuing command list"));

		if (flags & QueueCommandListFlags::Stall) {
			res = vkWaitForFences(deviceVulkan->GetUnderlyingDevice(), 1, &f, true, UINT64_MAX);
			if (res != VK_SUCCESS)
				Throw(VulkanAPIFailure(res, "Failure while waiting for command list fence"));
		}
	}

	auto        DeviceContext::ResolveCommandList() -> CommandListPtr
	{
		assert(_commandList.GetUnderlying());
		auto res = vkEndCommandBuffer(_commandList.GetUnderlying().get());
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while ending command buffer"));

		// We will release our reference on _command list here.
		auto result = std::make_shared<CommandList>(std::move(_commandList));
		assert(!_commandList.GetUnderlying());
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

		// hack -- todo -- properly support cases where in the number of entries in "clearValues" is too few
		// for this renderpass
		VkClearValue temp[8];
		rp_begin.pClearValues = temp;
		rp_begin.clearValueCount = 8;
		for (unsigned c=0; c<8; ++c) {
			temp[c].depthStencil.depth = 1.0f;
			temp[c].depthStencil.stencil = 0;
		}

        vkCmdBeginRenderPass(_commandList.GetUnderlying().get(), &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
        _renderPass = fb.GetLayout();
        _renderPassSamples = samples;
        _renderPassSubpass = 0u;
        _currentGraphicsPipeline.reset();

        // bind descriptor sets that are pending
        if (_graphicsDescriptors._hasSetsAwaitingFlush) {
            _commandList.BindDescriptorSets(
                VK_PIPELINE_BIND_POINT_GRAPHICS,
				GraphicsPipelineBuilder::_pipelineLayoutBuilder.GetPipelineLayout(),
                0, GraphicsPipelineBuilder::_pipelineLayoutBuilder.GetDescriptorSetCount(), AsPointer(_graphicsDescriptors._descriptorSets.begin()), 
                0, nullptr);
            _graphicsDescriptors._hasSetsAwaitingFlush = false;
        }
    }

    void DeviceContext::EndRenderPass()
    {
		vkCmdEndRenderPass(_commandList.GetUnderlying().get());
        _renderPass = nullptr;
        _renderPassSamples = TextureSamples::Create();
        _renderPassSubpass = 0u;
        _currentGraphicsPipeline.reset();
    }

    bool DeviceContext::IsInRenderPass() const
    {
        return _renderPass != nullptr;
    }

	void DeviceContext::NextSubpass(VkSubpassContents contents)
    {
        vkCmdNextSubpass(_commandList.GetUnderlying().get(), contents);
        _currentGraphicsPipeline.reset();
        ++_renderPassSubpass;
    }

	NumericUniformsInterface& DeviceContext::GetNumericUniforms(ShaderStage stage)
	{
		switch (stage) {
		case ShaderStage::Pixel:
			return _graphicsDescriptors._numericBindings;
		case ShaderStage::Compute:
			return _computeDescriptors._numericBindings;
		default:
			// since the numeric uniforms are associated with a descriptor set, we don't
			// distinguish between different shader stages (unlike some APIs where constants
			// are set for each stage independantly)
			// Hence we can't return anything sensible here.
			Throw(::Exceptions::BasicLabel("Numeric uniforms only supported for pixel shader in Vulkan"));
		}
	}

	static std::shared_ptr<DescriptorSetSignature> GetNumericBindingsDescriptorSet(const PipelineLayoutSignatureFile& source)
	{
		for (const auto&d:source._descriptorSets)
			if (d->_name == "Numeric") return d;
		return nullptr;
	}

	void DeviceContext::SetupPipelineBuilders()
	{
		auto& globals = VulkanGlobalsTemp::GetInstance();

		{
			GraphicsPipelineBuilder::_pipelineLayoutBuilder._shaderStageMask = VK_SHADER_STAGE_ALL_GRAPHICS;
			GraphicsPipelineBuilder::_pipelineLayoutBuilder._factory = _factory;

			const auto& root = *globals._graphicsRootSignatureFile->GetRootSignature(Hash64(globals._graphicsRootSignatureFile->_mainRootSignature));
			unsigned q=0;
			for (unsigned c=0; c<root._descriptorSets.size(); ++c) {
				const auto&d = root._descriptorSets[c];
				if (d._type == RootSignature::DescriptorSetType::Numeric) {
					GraphicsPipelineBuilder::_pipelineLayoutBuilder._fixedDescriptorSetLayout[q]._bindingIndex = c;
					GraphicsPipelineBuilder::_pipelineLayoutBuilder._fixedDescriptorSetLayout[q]._descriptorSet =
						globals._boundGraphicsSignatures->GetDescriptorSet(VulkanGlobalsTemp::s_mainSignature, d._hashName)->_layout;
					++q;
				}
			}

			PipelineLayoutShaderConfig helper { *_factory, *globals._graphicsRootSignatureFile, VulkanGlobalsTemp::s_mainSignature, PipelineType::Graphics };
			GraphicsPipelineBuilder::_pipelineLayoutBuilder.SetShaderBasedDescriptorSets(helper);

			for (const auto&d:helper._descriptorSets) {
				_graphicsDescriptors._descInfo[d._pipelineLayoutBindingIndex]._dummy = d._bound._blankBindings;
				_graphicsDescriptors._descInfo[d._pipelineLayoutBindingIndex]._dummyDescription = d._bound._blankBindingsDescription;
			}
		}

		_graphicsDescriptors.BindNumericUniforms(
			GetNumericBindingsDescriptorSet(*globals._graphicsRootSignatureFile),
			*globals._graphicsRootSignatureFile->_legacyRegisterBindingSettings[0],	// hack -- just selecting first
			VK_SHADER_STAGE_ALL_GRAPHICS, 3);

		{
			ComputePipelineBuilder::_pipelineLayoutBuilder._shaderStageMask = VK_SHADER_STAGE_COMPUTE_BIT;
			ComputePipelineBuilder::_pipelineLayoutBuilder._factory = _factory;

			const auto& root = *globals._computeRootSignatureFile->GetRootSignature(Hash64(globals._computeRootSignatureFile->_mainRootSignature));
			unsigned q=0;
			for (unsigned c=0; c<root._descriptorSets.size(); ++c) {
				const auto&d = root._descriptorSets[c];
				if (d._type == RootSignature::DescriptorSetType::Numeric) {
					ComputePipelineBuilder::_pipelineLayoutBuilder._fixedDescriptorSetLayout[q]._bindingIndex = c;
					ComputePipelineBuilder::_pipelineLayoutBuilder._fixedDescriptorSetLayout[q]._descriptorSet =
						globals._boundComputeSignatures->GetDescriptorSet(VulkanGlobalsTemp::s_mainSignature, d._hashName)->_layout;
					++q;
				}
			}

			PipelineLayoutShaderConfig helper { *_factory, *globals._computeRootSignatureFile, VulkanGlobalsTemp::s_mainSignature, PipelineType::Compute };
			ComputePipelineBuilder::_pipelineLayoutBuilder.SetShaderBasedDescriptorSets(helper);

			for (const auto&d:helper._descriptorSets) {
				_computeDescriptors._descInfo[d._pipelineLayoutBindingIndex]._dummy = d._bound._blankBindings;
				_computeDescriptors._descInfo[d._pipelineLayoutBindingIndex]._dummyDescription = d._bound._blankBindingsDescription;
			}
		}

		_computeDescriptors.BindNumericUniforms(
			GetNumericBindingsDescriptorSet(*globals._computeRootSignatureFile),
			*globals._computeRootSignatureFile->_legacyRegisterBindingSettings[0],	// hack -- just selecting first
			VK_SHADER_STAGE_COMPUTE_BIT, 2);
	}

	DeviceContext::DeviceContext(
        ObjectFactory&			factory, 
        GlobalPools&            globalPools,
		CommandPool&            cmdPool, 
		CommandBufferType		cmdBufferType,
		TemporaryBufferSpace&	tempBufferSpace)
    : _cmdPool(&cmdPool), _cmdBufferType(cmdBufferType)
    , _factory(&factory)
    , _globalPools(&globalPools)
    , _renderPass(nullptr)
    , _renderPassSubpass(0u)
    , _renderPassSamples(TextureSamples::Create())
    , _graphicsDescriptors(factory, globalPools, 4)
    , _computeDescriptors(factory, globalPools, 4)
	, _tempBufferSpace(&tempBufferSpace)
    {
		_utilityFence = _factory->CreateFence(0);

		auto& globals = VulkanGlobalsTemp::GetInstance();
		globals._globalPools = &globalPools;

        globals._graphicsRootSignatureFile = std::make_shared<Metal_Vulkan::PipelineLayoutSignatureFile>("xleres/System/RootSignature.cfg");
        globals._computeRootSignatureFile = std::make_shared<Metal_Vulkan::PipelineLayoutSignatureFile>("xleres/System/RootSignatureCS.cfg");

		globals._boundGraphicsSignatures = std::make_shared<BoundSignatureFile>(*_factory, *_globalPools, VK_SHADER_STAGE_ALL_GRAPHICS);
		globals._boundGraphicsSignatures->RegisterSignatureFile(VulkanGlobalsTemp::s_mainSignature, *globals._graphicsRootSignatureFile);
		globals._boundComputeSignatures = std::make_shared<BoundSignatureFile>(*_factory, *_globalPools, VK_SHADER_STAGE_COMPUTE_BIT);
		globals._boundComputeSignatures->RegisterSignatureFile(VulkanGlobalsTemp::s_mainSignature, *globals._computeRootSignatureFile);

		SetupPipelineBuilders();
    }

	void DeviceContext::PrepareForDestruction(IDevice*, IPresentationChain*) {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    void CommandList::UpdateBuffer(
        VkBuffer buffer, VkDeviceSize offset, 
        VkDeviceSize byteCount, const void* data)
    {
        assert(byteCount <= 65536); // this restriction is imposed by Vulkan
		assert((byteCount & (4 - 1)) == 0);  // must be a multiple of 4
		assert(byteCount > 0 && data);
        vkCmdUpdateBuffer(
			_underlying.get(),
			buffer, 0,
			byteCount, (const uint32_t*)data);
    }

    void CommandList::BindDescriptorSets(
        VkPipelineBindPoint pipelineBindPoint,
        VkPipelineLayout layout,
        uint32_t firstSet,
        uint32_t descriptorSetCount,
        const VkDescriptorSet* pDescriptorSets,
        uint32_t dynamicOffsetCount,
        const uint32_t* pDynamicOffsets)
    {
        vkCmdBindDescriptorSets(
            _underlying.get(),
            pipelineBindPoint, layout, firstSet, 
            descriptorSetCount, pDescriptorSets,
            dynamicOffsetCount, pDynamicOffsets);
    }

    void CommandList::CopyBuffer(
        VkBuffer srcBuffer,
        VkBuffer dstBuffer,
        uint32_t regionCount,
        const VkBufferCopy* pRegions)
    {
        vkCmdCopyBuffer(_underlying.get(), srcBuffer, dstBuffer, regionCount, pRegions);
    }

    void CommandList::CopyImage(
        VkImage srcImage,
        VkImageLayout srcImageLayout,
        VkImage dstImage,
        VkImageLayout dstImageLayout,
        uint32_t regionCount,
        const VkImageCopy* pRegions)
    {
        vkCmdCopyImage(
            _underlying.get(), 
            srcImage, srcImageLayout, 
            dstImage, dstImageLayout, 
            regionCount, pRegions);
    }

    void CommandList::CopyBufferToImage(
        VkBuffer srcBuffer,
        VkImage dstImage,
        VkImageLayout dstImageLayout,
        uint32_t regionCount,
        const VkBufferImageCopy* pRegions)
    {
        vkCmdCopyBufferToImage(
            _underlying.get(),
            srcBuffer, 
            dstImage, dstImageLayout,
            regionCount, pRegions);
    }

    void CommandList::PipelineBarrier(
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
            _underlying.get(),
            srcStageMask, dstStageMask,
            dependencyFlags, 
            memoryBarrierCount, pMemoryBarriers,
            bufferMemoryBarrierCount, pBufferMemoryBarriers,
            imageMemoryBarrierCount, pImageMemoryBarriers);
    }

    void CommandList::PushConstants(
        VkPipelineLayout layout,
        VkShaderStageFlags stageFlags,
        uint32_t offset,
        uint32_t size,
        const void* pValues)
    {
        vkCmdPushConstants(
            _underlying.get(),
            layout, stageFlags,
            offset, size, pValues);
    }

	void CommandList::WriteTimestamp(
		VkPipelineStageFlagBits pipelineStage,
		VkQueryPool queryPool, uint32_t query)
	{
		vkCmdWriteTimestamp(_underlying.get(), pipelineStage, queryPool, query);
	}

	void CommandList::BeginQuery(VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags)
	{
		vkCmdBeginQuery(_underlying.get(), queryPool, query, flags);
	}

	void CommandList::EndQuery(VkQueryPool queryPool, uint32_t query)
	{
		vkCmdEndQuery(_underlying.get(), queryPool, query);
	}

	void CommandList::ResetQueryPool(
		VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
	{
		vkCmdResetQueryPool(_underlying.get(), queryPool, firstQuery, queryCount);
	}

	void CommandList::SetEvent(VkEvent evnt, VkPipelineStageFlags stageMask)
	{
		vkCmdSetEvent(_underlying.get(), evnt, stageMask);
	}

	void CommandList::BindVertexBuffers(
		uint32_t            firstBinding,
		uint32_t            bindingCount,
		const VkBuffer*     pBuffers,
		const VkDeviceSize*	pOffsets)
	{
		vkCmdBindVertexBuffers(
			_underlying.get(), 
			firstBinding, bindingCount,
			pBuffers, pOffsets);
	}

	CommandList::CommandList() {}
	CommandList::CommandList(const VulkanSharedPtr<VkCommandBuffer>& underlying)
	: _underlying(underlying) {}
	CommandList::~CommandList() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	void DescriptorCollection::BindNumericUniforms(
		const std::shared_ptr<DescriptorSetSignature>& signature,
		const LegacyRegisterBinding& bindings,
		VkShaderStageFlags stageFlags,
		unsigned descriptorSetIndex)
	{
		_numericBindings = NumericUniformsInterface {
			*_factory, *_globalPools,
			signature, bindings, stageFlags, descriptorSetIndex };
		_numericBindingsSlot = descriptorSetIndex;
	}

    DescriptorCollection::DescriptorCollection(
        const ObjectFactory&    factory, 
        GlobalPools&            globalPools,
		unsigned				descriptorSetCount)
	: _factory(&factory), _globalPools(&globalPools)
    {
        _descriptorSets.resize(descriptorSetCount, nullptr);
        _descInfo.resize(descriptorSetCount);
		_hasSetsAwaitingFlush = false;
    }

}}

