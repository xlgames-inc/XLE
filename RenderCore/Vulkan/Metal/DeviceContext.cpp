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
#include "DescriptorSetSignatureFile.h"
#include "ShaderReflection.h"
#include "../IDeviceVulkan.h"
#include "../../Format.h"
#include "../../BufferView.h"
#include "../../../OSServices/Log.h"
#include "../../../Utility/MemoryUtils.h"
#include "../../../Utility/ArithmeticUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{

	static void LogPipeline();

///////////////////////////////////////////////////////////////////////////////////////////////////

	static VkViewport AsVkViewport(const Viewport& viewport, const float renderTargetHeight)
	{
		VkViewport vp;
		vp.x = viewport._x;
		vp.y = viewport._y;
		vp.width = viewport._width;
		vp.height = viewport._height;
		vp.minDepth = viewport._minDepth;
		vp.maxDepth = viewport._maxDepth;
		if (!viewport._originIsUpperLeft) {
			// Vulkan window coordinate space has origin in upper-left, so we must account for that in the viewport
			vp.y = renderTargetHeight - viewport._y - viewport._height;
		}
		return vp;
	}

	static VkRect2D AsVkRect2D(const ScissorRect& input, const float renderTargetHeight)
	{
		VkRect2D scissor;
		scissor.offset.x = input._x;
		scissor.offset.y = input._y;
		scissor.extent.width = input._width;
		scissor.extent.height = input._height;
		if (!input._originIsUpperLeft) {
			// Vulkan window coordinate space has origin in upper-left, so we must account for that in the viewport
			scissor.offset.y = renderTargetHeight - input._y - input._height;
		}
		return scissor;
	}

	void        SharedGraphicsEncoder::Bind(
		IteratorRange<const Viewport*> viewports,
		IteratorRange<const ScissorRect*> scissorRects)
	{
		// maxviewports: VkPhysicalDeviceLimits::maxViewports
		// VkPhysicalDeviceFeatures::multiViewport must be enabled
		// need VK_DYNAMIC_STATE_VIEWPORT & VK_DYNAMIC_STATE_SCISSOR set

		assert(_sharedState->_commandList.GetUnderlying());
		VkViewport vkViewports[viewports.size()];
		for (unsigned c=0; c<viewports.size(); ++c)
			vkViewports[c] = AsVkViewport(viewports[c], _sharedState->_renderTargetHeight);

		VkRect2D vkScissors[scissorRects.size()];
		for (unsigned c=0; c<scissorRects.size(); ++c)
			vkScissors[c] = AsVkRect2D(scissorRects[c], _sharedState->_renderTargetHeight);

		vkCmdSetViewport(_sharedState->_commandList.GetUnderlying().get(), 0, viewports.size(), vkViewports);
		vkCmdSetScissor(_sharedState->_commandList.GetUnderlying().get(), 0, scissorRects.size(), vkScissors);
	}

	void        SharedGraphicsEncoder::Bind(IteratorRange<const VertexBufferView*> vbViews, const IndexBufferView& ibView)
	{
		assert(_sharedState->_commandList.GetUnderlying());

		VkBuffer buffers[s_maxBoundVBs];
		VkDeviceSize offsets[s_maxBoundVBs];
		// auto count = (unsigned)std::min(std::min(vertexBuffers.size(), dimof(buffers)), _vbBindingDescriptions.size());
		for (unsigned c=0; c<vbViews.size(); ++c) {
			offsets[c] = vbViews[c]._offset;
			assert(const_cast<IResource*>(vbViews[c]._resource)->QueryInterface(typeid(Resource).hash_code()));
			buffers[c] = checked_cast<const Resource*>(vbViews[c]._resource)->GetBuffer();
		}
		vkCmdBindVertexBuffers(
			_sharedState->_commandList.GetUnderlying().get(), 
			0, vbViews.size(),
			buffers, offsets);

		if (ibView._resource) {
			assert(ibView._resource);
			vkCmdBindIndexBuffer(
				_sharedState->_commandList.GetUnderlying().get(),
				checked_cast<const Resource*>(ibView._resource)->GetBuffer(),
				ibView._offset,
				ibView._indexFormat == Format::R32_UINT ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
			_sharedState->_ibBound = true;
		} else {
			_sharedState->_ibBound = false;
		}
	}

	void        SharedGraphicsEncoder::BindDescriptorSet(
		unsigned index, VkDescriptorSet set
		VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, DescriptorSetVerboseDescription&& description))
	{
		auto& collection = _sharedState->_graphicsDescriptors;
		if (index < (unsigned)collection._descriptorSets.size() && collection._descriptorSets[index] != set) {
			collection._descriptorSets[index] = set;
			assert(index < GetDescriptorSetCount());
			_sharedState->_commandList.BindDescriptorSets(
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				GetPipelineLayout(),
				index, 1, &collection._descriptorSets[index], 
				0, nullptr);
		}
	}

	void        ComputeEncoder_ProgressivePipeline::BindDescriptorSet(
		unsigned index, VkDescriptorSet set
		VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, DescriptorSetVerboseDescription&& description))
	{
		auto& collection = _sharedState->_computeDescriptors;
		if (index < (unsigned)collection._descriptorSets.size() && collection._descriptorSets[index] != set) {
			collection._descriptorSets[index] = set;
			assert(index < GetDescriptorSetCount());
			_sharedState->_commandList.BindDescriptorSets(
				VK_PIPELINE_BIND_POINT_COMPUTE,
				GetPipelineLayout(),
				index, 1, &collection._descriptorSets[index], 
				0, nullptr);
		}
	}

	void			SharedGraphicsEncoder::PushConstants(VkShaderStageFlags stageFlags, IteratorRange<const void*> data)
	{
		assert(!(stageFlags & VK_SHADER_STAGE_COMPUTE_BIT));
		_sharedState->_commandList.PushConstants(
			GetPipelineLayout(),
			stageFlags, 0, (uint32_t)data.size(), data.begin());
	}

	void			SharedGraphicsEncoder::RebindNumericDescriptorSet()
	{
		VkDescriptorSet descSets[1];
		#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
			DescriptorSetVerboseDescription* descriptions[1];
			_sharedState->_graphicsDescriptors._numericBindings.GetDescriptorSets(MakeIteratorRange(descSets), MakeIteratorRange(descriptions));
			BindDescriptorSet(
				_sharedState->_graphicsDescriptors._numericBindingsSlot, descSets[0], 
				DescriptorSetVerboseDescription{*descriptions[0]});
		#else
			_sharedState->_graphicsDescriptors._numericBindings.GetDescriptorSets(MakeIteratorRange(descSets));
			BindDescriptorSet(_sharedState->_graphicsDescriptors._numericBindingsSlot, descSets[0]);
		#endif
	}

	unsigned SharedGraphicsEncoder::GetDescriptorSetCount()
	{
		return 4;
	}

	VkPipelineLayout SharedGraphicsEncoder::GetPipelineLayout()
	{
		return Internal::VulkanGlobalsTemp::GetInstance()._graphicsPipelineLayout->GetUnderlying();
	}

	unsigned ComputeEncoder_ProgressivePipeline::GetDescriptorSetCount()
	{
		return 4;
	}

	VkPipelineLayout ComputeEncoder_ProgressivePipeline::GetPipelineLayout()
	{
		return Internal::VulkanGlobalsTemp::GetInstance()._graphicsPipelineLayout->GetUnderlying();
	}

	void			ComputeEncoder_ProgressivePipeline::RebindNumericDescriptorSet()
	{
		VkDescriptorSet descSets[1];
		#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
			DescriptorSetVerboseDescription* descriptions[1];
			_sharedState->_computeDescriptors._numericBindings.GetDescriptorSets(MakeIteratorRange(descSets), MakeIteratorRange(descriptions));
			BindDescriptorSet(
				_sharedState->_computeDescriptors._numericBindingsSlot, descSets[0], 
				DescriptorSetVerboseDescription{*descriptions[0]});
		#else
			_sharedState->_computeDescriptors._numericBindings.GetDescriptorSets(MakeIteratorRange(descSets));
			BindDescriptorSet(_sharedState->_computeDescriptors._numericBindingsSlot, descSets[0]);
		#endif
	}

	bool GraphicsEncoder_ProgressivePipeline::BindGraphicsPipeline()
	{
		assert(_sharedState->_commandList.GetUnderlying());

		if (_currentGraphicsPipeline && !GraphicsPipelineBuilder::IsPipelineStale()) return true;

		_currentGraphicsPipeline = GraphicsPipelineBuilder::CreatePipeline(
			*_factory, _globalPools->_mainPipelineCache.get(),
			_sharedState->_renderPass, _sharedState->_renderPassSubpass, _sharedState->_renderPassSamples);
		assert(_currentGraphicsPipeline);
		LogPipeline();

		#if 0 // defined(_DEBUG)
			// check for unbound descriptor sets
			const auto& sig = *GetBoundShaderProgram()->_pipelineLayoutConfig;
			for (unsigned c=0; c<sig._descriptorSets.size(); ++c)
				if (c >= _sharedState->_graphicsDescriptors._descriptorSets.size() || !_sharedState->_graphicsDescriptors._descriptorSets[c]) {
					Log(Warning) << "Graphics descriptor set index [" << c << "] (" << sig._descriptorSets[c]._name << ") is unbound when creating pipeline. This will probably result in a crash." << std::endl;
				}
		#endif

		vkCmdBindPipeline(
			_sharedState->_commandList.GetUnderlying().get(),
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			_currentGraphicsPipeline->get());
		// Bind(_boundViewport);
		return true;
	}

	bool ComputeEncoder_ProgressivePipeline::BindComputePipeline()
	{
		assert(_sharedState->_commandList.GetUnderlying());

		if (_currentComputePipeline && !ComputePipelineBuilder::IsPipelineStale()) return true;

		_currentComputePipeline = ComputePipelineBuilder::CreatePipeline(
			*_factory, _globalPools->_mainPipelineCache.get());
		assert(_currentComputePipeline);
		LogPipeline();

		#if 0 // defined(_DEBUG)
			// check for unbound descriptor sets
			const auto& sig = *GetBoundShaderProgram()->_pipelineLayoutConfig;
			for (unsigned c=0; c<sig._descriptorSets.size(); ++c)
				if (c >= _sharedState->_computeDescriptors._descriptorSets.size() || !_sharedState->_computeDescriptors._descriptorSets[c]) {
					Log(Warning) << "Compute descriptor set index [" << c << "] (" << sig._descriptorSets[c]._name << ") is unbound when creating pipeline. This will probably result in a crash." << std::endl;
				}
		#endif

		vkCmdBindPipeline(
			_sharedState->_commandList.GetUnderlying().get(),
			VK_PIPELINE_BIND_POINT_COMPUTE,
			_currentComputePipeline->get());

		return true;
	}

	static void LogPipeline()
	{
		#if 0 // defined(_DEBUG)		todo -- we need to redo this
			if (!Verbose.IsEnabled()) return;

			const CompiledShaderByteCode* shaders[(unsigned)ShaderStage::Max] = {};
			if (pipeline == PipelineType::Graphics) {
				Log(Verbose) << "-------------VertexShader------------" << std::endl;
				Log(Verbose) << SPIRVReflection(GetBoundShaderProgram()->GetCompiledCode(ShaderStage::Vertex).GetByteCode()) << std::endl;
				Log(Verbose) << "-------------PixelShader------------" << std::endl;
				Log(Verbose) << SPIRVReflection(GetBoundShaderProgram()->GetCompiledCode(ShaderStage::Pixel).GetByteCode()) << std::endl;
				static_assert(ShaderProgram::s_maxShaderStages <= dimof(shaders));
				for (unsigned c=0; c<ShaderProgram::s_maxShaderStages; ++c)
					shaders[c] = &GetBoundShaderProgram()->GetCompiledCode((ShaderStage)c);
			} else {
				assert(pipeline == PipelineType::Compute);
				Log(Verbose) << "-------------ComputeShader------------" << std::endl;
				Log(Verbose) << SPIRVReflection(GetBoundComputeShader()->GetCompiledShaderByteCode().GetByteCode()) << std::endl;
				shaders[(unsigned)ShaderStage::Compute] = &GetBoundComputeShader()->GetCompiledShaderByteCode();
			}
			Log(Verbose) << "-------------Descriptors------------" << std::endl;

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				
				// get bindings from either graphics or compute configuration, depending on what we're logging
				const auto& PipelineDescriptorsLayoutBuilder = (pipeline == PipelineType::Graphics) ? GraphicsPipelineBuilder::_PipelineDescriptorsLayoutBuilder : ComputePipelineBuilder::_PipelineDescriptorsLayoutBuilder;
				const auto& shaderPipelineLayoutConfig = *((pipeline == PipelineType::Graphics) ? GetBoundShaderProgram()->_pipelineLayoutConfig : GetBoundComputeShader()->_pipelineLayoutConfig);
				const auto& descriptors = (pipeline == PipelineType::Graphics) ? _graphicsDescriptors : _computeDescriptors;

				#pragma warning(disable:4239) // HACK -- workaround -- nonstandard extension used: 'argument': conversion from '_Ostr' to 'std::ostream &')
				for (unsigned descSetIdx=0; descSetIdx<PipelineDescriptorsLayoutBuilder.GetDescriptorSetCount(); ++descSetIdx) {

					// Check if it's bound via the shader pipeline layout config
					auto i = std::find_if(shaderPipelineLayoutConfig._descriptorSets.begin(), shaderPipelineLayoutConfig._descriptorSets.end(),
						[descSetIdx](const PartialPipelineDescriptorsLayout::DescriptorSet& descSet) {
							return descSet._pipelineLayoutBindingIndex == descSetIdx;
						});
					if (i != shaderPipelineLayoutConfig._descriptorSets.end()) {
						WriteDescriptorSet(
							Log(Verbose),
							descriptors._descInfo[descSetIdx]._currentlyBoundDescription,
							*i->_signature,
							*shaderPipelineLayoutConfig._legacyRegisterBinding,
							MakeIteratorRange(shaders),
							descSetIdx, descSetIdx < descriptors._descriptorSets.size() && descriptors._descriptorSets[descSetIdx] != nullptr);
						continue;
					}

					// Check if it's bound via the numeric bindings
					if (descSetIdx == descriptors._numericBindingsSlot) {
						WriteDescriptorSet(
							Log(Verbose),
							descriptors._descInfo[descSetIdx]._currentlyBoundDescription,
							descriptors._numericBindings.GetSignature(),
							descriptors._numericBindings.GetLegacyRegisterBindings(),
							MakeIteratorRange(shaders),
							descSetIdx, descSetIdx < descriptors._descriptorSets.size() && descriptors._descriptorSets[descSetIdx] != nullptr);
						continue;
					}

					Log(Verbose) << "Could not find descriptor set signature for descriptor set index (" << descSetIdx << ")" << std::endl;
				}
			#endif
		#endif
	}

	void GraphicsEncoder_ProgressivePipeline::Draw(unsigned vertexCount, unsigned startVertexLocation)
	{
		assert(_sharedState->_commandList.GetUnderlying());
		if (BindGraphicsPipeline()) {
			assert(vertexCount);
			vkCmdDraw(
				_sharedState->_commandList.GetUnderlying().get(),
				vertexCount, 1,
				startVertexLocation, 0);
		}
	}
	
	void GraphicsEncoder_ProgressivePipeline::DrawIndexed(unsigned indexCount, unsigned startIndexLocation)
	{
		assert(_sharedState->_commandList.GetUnderlying());
		assert(_sharedState->_ibBound);
		if (BindGraphicsPipeline()) {
			assert(indexCount);
			vkCmdDrawIndexed(
				_sharedState->_commandList.GetUnderlying().get(),
				indexCount, 1,
				startIndexLocation, 0,
				0);
		}
	}

	void GraphicsEncoder_ProgressivePipeline::DrawInstances(unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation)
	{
		assert(0);      // not implemented
	}

	void GraphicsEncoder_ProgressivePipeline::DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation)
	{
		assert(0);      // not implemented
	}

	void GraphicsEncoder_ProgressivePipeline::DrawAuto() 
	{
		assert(0);      // not implemented
		assert(_sharedState->_ibBound);
	}

	void ComputeEncoder_ProgressivePipeline::Dispatch(unsigned countX, unsigned countY, unsigned countZ)
	{
		assert(_sharedState->_commandList.GetUnderlying());
		if (BindComputePipeline()) {
			vkCmdDispatch(
				_sharedState->_commandList.GetUnderlying().get(),
				countX, countY, countZ);
		}
	}

	SharedGraphicsEncoder::SharedGraphicsEncoder(const std::shared_ptr<VulkanEncoderSharedState>& sharedState)
	: _sharedState(sharedState)
	{
		assert(_sharedState->_currentEncoder == nullptr && _sharedState->_currentEncoderType == VulkanEncoderSharedState::EncoderType::None);
		assert(_sharedState->_renderPass != nullptr);
		_sharedState->_currentEncoder = this;
		_sharedState->_currentEncoderType = VulkanEncoderSharedState::EncoderType::Graphics;

		// bind descriptor sets that are pending
		// If we've been using the pipeline layout builder directly, then we
		// must flush those changes down to the GraphicsPipelineBuilder
		if (_sharedState->_graphicsDescriptors._numericBindings.HasChanges()) {
			RebindNumericDescriptorSet();
		}
		if (_sharedState->_graphicsDescriptors._hasSetsAwaitingFlush) {
			_sharedState->_commandList.BindDescriptorSets(
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				GetPipelineLayout(),
				0, GetDescriptorSetCount(), AsPointer(_sharedState->_graphicsDescriptors._descriptorSets.begin()), 
				0, nullptr);
			_sharedState->_graphicsDescriptors._hasSetsAwaitingFlush = false;
		}
	}

	SharedGraphicsEncoder::~SharedGraphicsEncoder()
	{
		assert(_sharedState->_currentEncoder == this);
		_sharedState->_currentEncoder = nullptr;
		_sharedState->_currentEncoderType = VulkanEncoderSharedState::EncoderType::None;
	}

	GraphicsEncoder_ProgressivePipeline::GraphicsEncoder_ProgressivePipeline(
		const std::shared_ptr<VulkanEncoderSharedState>& sharedState,
		ObjectFactory& objectFactory,
		GlobalPools& globalPools)
	: SharedGraphicsEncoder(sharedState)
	, _factory(&objectFactory)
	, _globalPools(&globalPools)
	{
		_sharedState->_currentEncoderType = VulkanEncoderSharedState::EncoderType::ProgressiveGraphics;
	}

	GraphicsEncoder_ProgressivePipeline::~GraphicsEncoder_ProgressivePipeline()
	{}

	GraphicsEncoder::GraphicsEncoder(const std::shared_ptr<VulkanEncoderSharedState>& sharedState)
	: SharedGraphicsEncoder(sharedState)
	{}
	GraphicsEncoder::~GraphicsEncoder()
	{}

	ComputeEncoder_ProgressivePipeline::ComputeEncoder_ProgressivePipeline(
		const std::shared_ptr<VulkanEncoderSharedState>& sharedState,
		ObjectFactory& objectFactory,
		GlobalPools& globalPools)
	: _factory(&objectFactory)
	, _globalPools(&globalPools)
	{
		assert(_sharedState->_currentEncoder == nullptr && _sharedState->_currentEncoderType == VulkanEncoderSharedState::EncoderType::None);
		assert(_sharedState->_renderPass == nullptr);	// don't start compute encoding during a render pass
		_sharedState->_currentEncoder = this;
		_sharedState->_currentEncoderType = VulkanEncoderSharedState::EncoderType::ProgressiveCompute;

		// bind descriptor sets that are pending
		if (_sharedState->_computeDescriptors._numericBindings.HasChanges()) {
			RebindNumericDescriptorSet();
		}
		if (_sharedState->_computeDescriptors._hasSetsAwaitingFlush) {
			_sharedState->_commandList.BindDescriptorSets(
				VK_PIPELINE_BIND_POINT_COMPUTE,
				GetPipelineLayout(),
				0, GetDescriptorSetCount(), AsPointer(_sharedState->_computeDescriptors._descriptorSets.begin()), 
				0, nullptr);
			_sharedState->_computeDescriptors._hasSetsAwaitingFlush = false;
		}
	}

	ComputeEncoder_ProgressivePipeline::~ComputeEncoder_ProgressivePipeline()
	{
		assert(_sharedState->_currentEncoder == this);
		_sharedState->_currentEncoder = nullptr;
		_sharedState->_currentEncoderType = VulkanEncoderSharedState::EncoderType::None;
	}

	GraphicsEncoder DeviceContext::BeginGraphicsEncoder()
	{
		return GraphicsEncoder { _sharedState };
	}

	GraphicsEncoder_ProgressivePipeline DeviceContext::BeginGraphicsEncoder_ProgressivePipeline()
	{
		return GraphicsEncoder_ProgressivePipeline { _sharedState, *_factory, *_globalPools };
	}

	ComputeEncoder_ProgressivePipeline DeviceContext::BeginComputeEncoder()
	{
		return ComputeEncoder_ProgressivePipeline { _sharedState, *_factory, *_globalPools };
	}

	std::shared_ptr<DeviceContext> DeviceContext::Get(IThreadContext& threadContext)
	{
		IThreadContextVulkan* vulkanContext = 
			(IThreadContextVulkan*)threadContext.QueryInterface(
				typeid(IThreadContextVulkan).hash_code());
		if (vulkanContext) {
			auto res = vulkanContext->GetMetalContext();
			if (!res->HasActiveCommandList())
				res->BeginCommandList();
			return res;
		}
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
		if (!_cmdPool) return;
		BeginCommandList(_cmdPool->Allocate(_cmdBufferType));
	}

	void 		DeviceContext::ResetDescriptorSetState()
	{
		assert(!_sharedState->_currentEncoder);

		for (unsigned c=0; c<_sharedState->_graphicsDescriptors._descriptorSets.size(); ++c) {
			_sharedState->_graphicsDescriptors._descriptorSets[c] = _sharedState->_graphicsDescriptors._descInfo[c]._dummy.get();
			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				_sharedState->_graphicsDescriptors._descInfo[c]._currentlyBoundDescription = _sharedState->_graphicsDescriptors._descInfo[c]._dummyDescription;
			#endif
		}
		_sharedState->_graphicsDescriptors._numericBindings.Reset();
		_sharedState->_graphicsDescriptors._hasSetsAwaitingFlush = true;

		for (unsigned c=0; c<_sharedState->_computeDescriptors._descriptorSets.size(); ++c) {
			_sharedState->_computeDescriptors._descriptorSets[c] = _sharedState->_computeDescriptors._descInfo[c]._dummy.get();
			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				_sharedState->_computeDescriptors._descInfo[c]._currentlyBoundDescription = _sharedState->_computeDescriptors._descInfo[c]._dummyDescription;
			#endif
		}
		_sharedState->_computeDescriptors._numericBindings.Reset();
		_sharedState->_computeDescriptors._hasSetsAwaitingFlush = true;
	}

	void		DeviceContext::BeginCommandList(const VulkanSharedPtr<VkCommandBuffer>& cmdList)
	{
		SetupPipelineBuilders();
		ResetDescriptorSetState();

		assert(!_sharedState->_commandList.GetUnderlying());
		_sharedState->_commandList = CommandList(cmdList);
		_sharedState->_ibBound = false;

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
		auto res = vkBeginCommandBuffer(_sharedState->_commandList.GetUnderlying().get(), &cmd_buf_info);
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while beginning command buffer"));
	}

	void		DeviceContext::ExecuteCommandList(CommandList& cmdList, bool preserveState)
	{
		assert(_sharedState->_commandList.GetUnderlying());
		(void)preserveState;		// we can't handle this properly in Vulkan

		const VkCommandBuffer buffers[] = { cmdList.GetUnderlying().get() };
		vkCmdExecuteCommands(
			_sharedState->_commandList.GetUnderlying().get(),
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

	auto        DeviceContext::ResolveCommandList() -> std::shared_ptr<CommandList>
	{
		assert(_sharedState->_commandList.GetUnderlying());
		auto res = vkEndCommandBuffer(_sharedState->_commandList.GetUnderlying().get());
		if (res != VK_SUCCESS)
			Throw(VulkanAPIFailure(res, "Failure while ending command buffer"));

		// We will release our reference on _command list here.
		auto result = std::make_shared<CommandList>(std::move(_sharedState->_commandList));
		assert(!_sharedState->_commandList.GetUnderlying());
		return result;
	}

	void        DeviceContext::BeginRenderPass(
		const FrameBuffer& fb,
		TextureSamples samples,
		VectorPattern<int, 2> offset, VectorPattern<unsigned, 2> extent,
		IteratorRange<const ClearValue*> clearValues)
	{
		if (_sharedState->_renderPass)
			Throw(::Exceptions::BasicLabel("Attempting to begin a render pass while another render pass is already in progress"));
		assert(!_sharedState->_currentEncoder);
		assert(_sharedState->_commandList.GetUnderlying() != nullptr);

		VkRenderPassBeginInfo rp_begin;
		rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rp_begin.pNext = nullptr;
		rp_begin.renderPass = fb.GetLayout();
		rp_begin.framebuffer = fb.GetUnderlying();
		rp_begin.renderArea.offset.x = offset[0];
		rp_begin.renderArea.offset.y = offset[1];
		rp_begin.renderArea.extent.width = extent[0];
		rp_begin.renderArea.extent.height = extent[1];
		
		VkClearValue vkClearValues[fb._clearValuesOrdering.size()];
		for (unsigned c=0; c<fb._clearValuesOrdering.size(); ++c) {
			if (fb._clearValuesOrdering[c]._originalAttachmentIndex < clearValues.size()) {
				vkClearValues[c] = *(const VkClearValue*)&clearValues[fb._clearValuesOrdering[c]._originalAttachmentIndex];
			} else {
				vkClearValues[c] = *(const VkClearValue*)&fb._clearValuesOrdering[c]._defaultClearValue;
			}
		}
		rp_begin.pClearValues = vkClearValues;
		rp_begin.clearValueCount = (uint32_t)fb._clearValuesOrdering.size();

		vkCmdBeginRenderPass(_sharedState->_commandList.GetUnderlying().get(), &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
		_sharedState->_renderPass = fb.GetLayout();
		_sharedState->_renderPassSamples = samples;
		_sharedState->_renderPassSubpass = 0u;
	}

	void DeviceContext::EndRenderPass()
	{
		assert(!_sharedState->_currentEncoder);
		vkCmdEndRenderPass(_sharedState->_commandList.GetUnderlying().get());
		_sharedState->_renderPass = nullptr;
		_sharedState->_renderPassSamples = TextureSamples::Create();
		_sharedState->_renderPassSubpass = 0u;
	}

	bool DeviceContext::IsInRenderPass() const
	{
		return _sharedState->_renderPass != nullptr;
	}

	void DeviceContext::NextSubpass(VkSubpassContents contents)
	{
		assert(!_sharedState->_currentEncoder);
		vkCmdNextSubpass(_sharedState->_commandList.GetUnderlying().get(), contents);
		++_sharedState->_renderPassSubpass;
	}

	unsigned DeviceContext::GetCurrentSubpassIndex() const
	{
		return _sharedState->_renderPassSubpass;
	}

	NumericUniformsInterface& DeviceContext::GetNumericUniforms(ShaderStage stage)
	{
		switch (stage) {
		case ShaderStage::Pixel:
			return _sharedState->_graphicsDescriptors._numericBindings;
		case ShaderStage::Compute:
			return _sharedState->_computeDescriptors._numericBindings;
		default:
			// since the numeric uniforms are associated with a descriptor set, we don't
			// distinguish between different shader stages (unlike some APIs where constants
			// are set for each stage independantly)
			// Hence we can't return anything sensible here.
			Throw(::Exceptions::BasicLabel("Numeric uniforms only supported for pixel shader in Vulkan"));
		}
	}

	static std::shared_ptr<DescriptorSetSignature> GetNumericBindingsDescriptorSet(const DescriptorSetSignatureFile& source)
	{
		for (const auto&d:source._descriptorSets)
			if (d->_name == "Numeric") return d;
		return nullptr;
	}

	void DeviceContext::SetupPipelineBuilders()
	{
		auto& globals = Internal::VulkanGlobalsTemp::GetInstance();

		if (!globals._compiledDescriptorSetLayoutCache)
			globals._compiledDescriptorSetLayoutCache = Internal::CreateCompiledDescriptorSetLayoutCache(*_factory, *_globalPools);

		{
			#if defined(_DEBUG)
				Internal::ValidateRootSignature(_factory->GetPhysicalDevice(), *globals._graphicsRootSignatureFile);
			#endif

			auto graphicsPartialLayout = Internal::CreatePartialPipelineDescriptorsLayout(
				*globals._graphicsRootSignatureFile, PipelineType::Graphics);

			globals._graphicsPipelineLayout = std::make_shared<Internal::VulkanPipelineLayout>(
				*_factory, *globals._compiledDescriptorSetLayoutCache, 
				MakeIteratorRange(graphicsPartialLayout.get(), graphicsPartialLayout.get()+1),
				VK_SHADER_STAGE_ALL_GRAPHICS);

			for (unsigned d=0; d<globals._graphicsPipelineLayout->GetDescriptorSetCount(); ++d) {
				_sharedState->_graphicsDescriptors._descInfo[d]._dummy = globals._graphicsPipelineLayout->GetBlankDescriptorSet(d);
				#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
					_sharedState->_graphicsDescriptors._descInfo[d]._dummyDescription = globals._graphicsPipelineLayout->GetDescriptorSetVerboseDescription(d);
				#endif
			}
			
			_sharedState->_graphicsDescriptors.BindNumericUniforms(
				GetNumericBindingsDescriptorSet(*globals._graphicsRootSignatureFile),
				*globals._graphicsRootSignatureFile->_legacyRegisterBindingSettings[0],	// hack -- just selecting first
				VK_SHADER_STAGE_ALL_GRAPHICS, 3);

			// find the uniform stream bindings
			const auto& root = *globals._graphicsRootSignatureFile->GetRootSignature(Hash64(globals._graphicsRootSignatureFile->_mainRootSignature));
			for (unsigned c=0; c<root._descriptorSets.size(); ++c) {
				const auto&d = root._descriptorSets[c];
				if (d._type == RootSignature::DescriptorSetType::Adaptive && d._uniformStream < 4)
					globals._graphicsUniformStreamToDescriptorSetBinding[d._uniformStream] = c;
			}
		}

		{
			#if defined(_DEBUG)
				Internal::ValidateRootSignature(_factory->GetPhysicalDevice(), *globals._computeRootSignatureFile);
			#endif

			auto computePartialLayout = Internal::CreatePartialPipelineDescriptorsLayout(
				*globals._computeRootSignatureFile, PipelineType::Compute);

			globals._computePipelineLayout = std::make_shared<Internal::VulkanPipelineLayout>(
				*_factory, *globals._compiledDescriptorSetLayoutCache, MakeIteratorRange(computePartialLayout.get(), computePartialLayout.get()+1),
				VK_SHADER_STAGE_COMPUTE_BIT);

			for (unsigned d=0; d<globals._graphicsPipelineLayout->GetDescriptorSetCount(); ++d) {
				_sharedState->_computeDescriptors._descInfo[d]._dummy = globals._computePipelineLayout->GetBlankDescriptorSet(d);
				#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
					_sharedState->_computeDescriptors._descInfo[d]._dummyDescription = globals._computePipelineLayout->GetDescriptorSetVerboseDescription(d);
				#endif
			}

			_sharedState->_computeDescriptors.BindNumericUniforms(
				GetNumericBindingsDescriptorSet(*globals._computeRootSignatureFile),
				*globals._computeRootSignatureFile->_legacyRegisterBindingSettings[0],	// hack -- just selecting first
				VK_SHADER_STAGE_COMPUTE_BIT, 2);

			const auto& root = *globals._computeRootSignatureFile->GetRootSignature(Hash64(globals._computeRootSignatureFile->_mainRootSignature));
			for (unsigned c=0; c<root._descriptorSets.size(); ++c) {
				const auto&d = root._descriptorSets[c];
				if (d._type == RootSignature::DescriptorSetType::Adaptive && d._uniformStream < 4)
					globals._computeUniformStreamToDescriptorSetBinding[d._uniformStream] = c;
			}
		}

#if 0
		{
			GraphicsPipelineBuilder::_PipelineDescriptorsLayoutBuilder._shaderStageMask = VK_SHADER_STAGE_ALL_GRAPHICS;
			GraphicsPipelineBuilder::_PipelineDescriptorsLayoutBuilder._factory = _factory;

			const auto& root = *globals._graphicsRootSignatureFile->GetRootSignature(Hash64(globals._graphicsRootSignatureFile->_mainRootSignature));
			unsigned q=0;
			for (unsigned c=0; c<root._descriptorSets.size(); ++c) {
				const auto&d = root._descriptorSets[c];
				if (d._type == RootSignature::DescriptorSetType::Numeric) {
					GraphicsPipelineBuilder::_PipelineDescriptorsLayoutBuilder._fixedDescriptorSetLayout[q]._bindingIndex = c;
					GraphicsPipelineBuilder::_PipelineDescriptorsLayoutBuilder._fixedDescriptorSetLayout[q]._descriptorSet =
						globals._boundGraphicsSignatures->GetDescriptorSet(VulkanGlobalsTemp::s_mainSignature, d._hashName)->_layout;
					++q;
				}
			}

			PartialPipelineDescriptorsLayout helper { *_factory, *globals._graphicsRootSignatureFile, VulkanGlobalsTemp::s_mainSignature, PipelineType::Graphics };
			GraphicsPipelineBuilder::_PipelineDescriptorsLayoutBuilder.SetShaderBasedDescriptorSets(helper);

			for (const auto&d:helper._descriptorSets) {
				_sharedState->_graphicsDescriptors._descInfo[d._pipelineLayoutBindingIndex]._dummy = d._bound._blankBindings;
				#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
					_sharedState->_graphicsDescriptors._descInfo[d._pipelineLayoutBindingIndex]._dummyDescription = d._bound._blankBindingsDescription;
				#endif
			}
		}

		_sharedState->_graphicsDescriptors.BindNumericUniforms(
			GetNumericBindingsDescriptorSet(*globals._graphicsRootSignatureFile),
			*globals._graphicsRootSignatureFile->_legacyRegisterBindingSettings[0],	// hack -- just selecting first
			VK_SHADER_STAGE_ALL_GRAPHICS, 3);

		GraphicsPipelineBuilder::_PipelineDescriptorsLayoutBuilder.SetShaderBasedDescriptorSets(*globals._mainGraphicsConfig);

		{
			ComputePipelineBuilder::_PipelineDescriptorsLayoutBuilder._shaderStageMask = VK_SHADER_STAGE_COMPUTE_BIT;
			ComputePipelineBuilder::_PipelineDescriptorsLayoutBuilder._factory = _factory;

			const auto& root = *globals._computeRootSignatureFile->GetRootSignature(Hash64(globals._computeRootSignatureFile->_mainRootSignature));
			unsigned q=0;
			for (unsigned c=0; c<root._descriptorSets.size(); ++c) {
				const auto&d = root._descriptorSets[c];
				if (d._type == RootSignature::DescriptorSetType::Numeric) {
					ComputePipelineBuilder::_PipelineDescriptorsLayoutBuilder._fixedDescriptorSetLayout[q]._bindingIndex = c;
					ComputePipelineBuilder::_PipelineDescriptorsLayoutBuilder._fixedDescriptorSetLayout[q]._descriptorSet =
						globals._boundComputeSignatures->GetDescriptorSet(VulkanGlobalsTemp::s_mainSignature, d._hashName)->_layout;
					++q;
				}
			}

			PartialPipelineDescriptorsLayout helper { *_factory, *globals._computeRootSignatureFile, VulkanGlobalsTemp::s_mainSignature, PipelineType::Compute };
			ComputePipelineBuilder::_PipelineDescriptorsLayoutBuilder.SetShaderBasedDescriptorSets(helper);

			for (const auto&d:helper._descriptorSets) {
				_sharedState->_computeDescriptors._descInfo[d._pipelineLayoutBindingIndex]._dummy = d._bound._blankBindings;
				#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
					_sharedState->_computeDescriptors._descInfo[d._pipelineLayoutBindingIndex]._dummyDescription = d._bound._blankBindingsDescription;
				#endif
			}
		}

		_sharedState->_computeDescriptors.BindNumericUniforms(
			GetNumericBindingsDescriptorSet(*globals._computeRootSignatureFile),
			*globals._computeRootSignatureFile->_legacyRegisterBindingSettings[0],	// hack -- just selecting first
			VK_SHADER_STAGE_COMPUTE_BIT, 2);

		ComputePipelineBuilder::_PipelineDescriptorsLayoutBuilder.SetShaderBasedDescriptorSets(*globals._mainComputeConfig);
#endif
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
	, _tempBufferSpace(&tempBufferSpace)
	{
		_sharedState = std::make_shared<VulkanEncoderSharedState>(*_factory, *_globalPools);
		_utilityFence = _factory->CreateFence(0);

		auto& globals = Internal::VulkanGlobalsTemp::GetInstance();
		globals._globalPools = &globalPools;

		/*
		globals._boundGraphicsSignatures = std::make_shared<BoundSignatureFile>(*_factory, *_globalPools, VK_SHADER_STAGE_ALL_GRAPHICS);
		globals._boundGraphicsSignatures->RegisterSignatureFile(VulkanGlobalsTemp::s_mainSignature, *globals._graphicsRootSignatureFile);
		globals._boundComputeSignatures = std::make_shared<BoundSignatureFile>(*_factory, *_globalPools, VK_SHADER_STAGE_COMPUTE_BIT);
		globals._boundComputeSignatures->RegisterSignatureFile(VulkanGlobalsTemp::s_mainSignature, *globals._computeRootSignatureFile);

		globals._mainGraphicsConfig = std::make_shared<PartialPipelineDescriptorsLayout>(factory, *globals._graphicsRootSignatureFile, globals.s_mainSignature, PipelineType::Graphics);
		globals._mainComputeConfig = std::make_shared<PartialPipelineDescriptorsLayout>(factory, *globals._computeRootSignatureFile, globals.s_mainSignature, PipelineType::Compute);
		*/
	}

	void DeviceContext::PrepareForDestruction(IDevice*, IPresentationChain*) {}

	VulkanEncoderSharedState::VulkanEncoderSharedState(
		const ObjectFactory&    factory, 
		GlobalPools&            globalPools)
	: _graphicsDescriptors(factory, globalPools, 4)
	, _computeDescriptors(factory, globalPools, 4)
	{
		_renderPass = nullptr;
		_renderPassSubpass = 0u;
		_renderPassSamples = TextureSamples::Create();
		_currentEncoder = nullptr;
		_currentEncoderType = EncoderType::None;
		_ibBound = false;
	}
	VulkanEncoderSharedState::~VulkanEncoderSharedState() {}

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

	void CommandList::CopyImageToBuffer(
		VkImage srcImage,
		VkImageLayout srcImageLayout,
		VkBuffer dstBuffer,
		uint32_t regionCount,
		const VkBufferImageCopy* pRegions)
	{
		vkCmdCopyImageToBuffer(
			_underlying.get(),
			srcImage, srcImageLayout,
			dstBuffer,
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

