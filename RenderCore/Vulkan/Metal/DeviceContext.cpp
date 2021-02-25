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
#include "ShaderReflection.h"
#include "../IDeviceVulkan.h"
#include "../../Format.h"
#include "../../BufferView.h"
#include "../../../OSServices/Log.h"
#include "../../../Utility/MemoryUtils.h"
#include "../../../Utility/ArithmeticUtils.h"

namespace RenderCore { namespace Metal_Vulkan
{

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

	class VulkanEncoderSharedState
	{
	public:
		CommandList 	_commandList;

		VkRenderPass	_renderPass = 0;
		TextureSamples	_renderPassSamples = TextureSamples::Create(0);
		unsigned		_renderPassSubpass = 0;

		float			_renderTargetWidth = 0.f;
		float			_renderTargetHeight = 0.f;

		bool			_inBltPass = false;

		class DescriptorCollection
		{
		public:
			std::vector<VkDescriptorSet>		_descriptorSets;			// (can't use a smart pointer here because it's often bound to the descriptor set in NumericUniformsInterface, which we must share)
			bool                                _hasSetsAwaitingFlush = false;

			#if defined(VULKAN_VERBOSE_DEBUG)
				std::vector<DescriptorSetDebugInfo> _currentlyBoundDesc;
			#endif

			void ResetState(const CompiledPipelineLayout&);

			DescriptorCollection(
				const ObjectFactory&    factory, 
				GlobalPools&            globalPools,
				unsigned				descriptorSetCount);

		private:
			const ObjectFactory*    _factory;
			GlobalPools*			_globalPools;
		};

		DescriptorCollection	_graphicsDescriptors;
		DescriptorCollection	_computeDescriptors;

		void* _currentEncoder = nullptr;
		enum class EncoderType { None, Graphics, ProgressiveGraphics, ProgressiveCompute };
		EncoderType _currentEncoderType = EncoderType::None;

		bool _ibBound = false;		// (for debugging, validates that an index buffer actually is bound when calling DrawIndexed & alternatives)

		VulkanEncoderSharedState(
			const ObjectFactory&    factory, 
			GlobalPools&            globalPools);
		~VulkanEncoderSharedState();
	};

	void        GraphicsEncoder::Bind(
		IteratorRange<const Viewport*> viewports,
		IteratorRange<const ScissorRect*> scissorRects)
	{
		// maxviewports: VkPhysicalDeviceLimits::maxViewports
		// VkPhysicalDeviceFeatures::multiViewport must be enabled
		// need VK_DYNAMIC_STATE_VIEWPORT & VK_DYNAMIC_STATE_SCISSOR set
		assert(viewports.size() == 1);		// to allow multiple viewports, we need to set the flag VkPhysicalDeviceFeatures::multiViewport during construction
		assert(scissorRects.size() == 1);

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

	void        GraphicsEncoder::Bind(IteratorRange<const VertexBufferView*> vbViews, const IndexBufferView& ibView)
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

	void        GraphicsEncoder::BindDescriptorSet(
		unsigned index, VkDescriptorSet set
		VULKAN_VERBOSE_DEBUG_ONLY(, DescriptorSetDebugInfo&& description))
	{
		auto& collection = _sharedState->_graphicsDescriptors;
		if (index < (unsigned)collection._descriptorSets.size() && collection._descriptorSets[index] != set) {
			collection._descriptorSets[index] = set;
			assert(index < GetDescriptorSetCount());
			_sharedState->_commandList.BindDescriptorSets(
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				GetUnderlyingPipelineLayout(),
				index, 1, &collection._descriptorSets[index], 
				0, nullptr);

			#if defined(VULKAN_VERBOSE_DEBUG)
				collection._currentlyBoundDesc[index] = description;
			#endif
		}
	}

	void        ComputeEncoder_ProgressivePipeline::BindDescriptorSet(
		unsigned index, VkDescriptorSet set
		VULKAN_VERBOSE_DEBUG_ONLY(, DescriptorSetDebugInfo&& description))
	{
		auto& collection = _sharedState->_computeDescriptors;
		if (index < (unsigned)collection._descriptorSets.size() && collection._descriptorSets[index] != set) {
			collection._descriptorSets[index] = set;
			assert(index < GetDescriptorSetCount());
			_sharedState->_commandList.BindDescriptorSets(
				VK_PIPELINE_BIND_POINT_COMPUTE,
				GetUnderlyingPipelineLayout(),
				index, 1, &collection._descriptorSets[index], 
				0, nullptr);

			#if defined(VULKAN_VERBOSE_DEBUG)
				collection._currentlyBoundDesc[index] = description;
			#endif
		}
	}

	void			GraphicsEncoder::PushConstants(VkShaderStageFlags stageFlags, unsigned offset, IteratorRange<const void*> data)
	{
		assert(!(stageFlags & VK_SHADER_STAGE_COMPUTE_BIT));
		_sharedState->_commandList.PushConstants(
			GetUnderlyingPipelineLayout(),
			stageFlags, offset, (uint32_t)data.size(), data.begin());
	}

	unsigned GraphicsEncoder::GetDescriptorSetCount()
	{
		return _pipelineLayout->GetDescriptorSetCount();
	}

	VkPipelineLayout GraphicsEncoder::GetUnderlyingPipelineLayout()
	{
		return _pipelineLayout->GetUnderlying();
	}

	unsigned ComputeEncoder_ProgressivePipeline::GetDescriptorSetCount()
	{
		return _pipelineLayout->GetDescriptorSetCount();
	}

	VkPipelineLayout ComputeEncoder_ProgressivePipeline::GetUnderlyingPipelineLayout()
	{
		return _pipelineLayout->GetUnderlying();
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

	void GraphicsEncoder_ProgressivePipeline::LogPipeline()
	{
		#if defined(_DEBUG)
			if (!Verbose.IsEnabled()) return;

			const CompiledShaderByteCode* shaders[(unsigned)ShaderStage::Max] = {};
			Log(Verbose) << "-------------VertexShader------------" << std::endl;
			Log(Verbose) << SPIRVReflection(GetBoundShaderProgram()->GetCompiledCode(ShaderStage::Vertex).GetByteCode()) << std::endl;
			Log(Verbose) << "-------------PixelShader------------" << std::endl;
			Log(Verbose) << SPIRVReflection(GetBoundShaderProgram()->GetCompiledCode(ShaderStage::Pixel).GetByteCode()) << std::endl;
			static_assert(ShaderProgram::s_maxShaderStages <= dimof(shaders));
			for (unsigned c=0; c<ShaderProgram::s_maxShaderStages; ++c)
				shaders[c] = &GetBoundShaderProgram()->GetCompiledCode((ShaderStage)c);

			/*} else {
				assert(pipeline == PipelineType::Compute);
				Log(Verbose) << "-------------ComputeShader------------" << std::endl;
				Log(Verbose) << SPIRVReflection(GetBoundComputeShader()->GetCompiledShaderByteCode().GetByteCode()) << std::endl;
				shaders[(unsigned)ShaderStage::Compute] = &GetBoundComputeShader()->GetCompiledShaderByteCode();
			}*/

			#if defined(VULKAN_VERBOSE_DEBUG)
				const auto& descriptors = _sharedState->_graphicsDescriptors;
				_pipelineLayout->WriteDebugInfo(
					Log(Verbose),
					MakeIteratorRange(shaders),
					MakeIteratorRange(descriptors._currentlyBoundDesc));
			#endif
		#endif
	}

	void ComputeEncoder_ProgressivePipeline::LogPipeline()
	{
		#if defined(_DEBUG)
			if (!Verbose.IsEnabled()) return;

			const CompiledShaderByteCode* shaders[(unsigned)ShaderStage::Max] = {};
			Log(Verbose) << "-------------ComputeShader------------" << std::endl;
			Log(Verbose) << SPIRVReflection(GetBoundComputeShader()->GetCompiledShaderByteCode().GetByteCode()) << std::endl;
			shaders[(unsigned)ShaderStage::Compute] = &GetBoundComputeShader()->GetCompiledShaderByteCode();

			#if defined(VULKAN_VERBOSE_DEBUG)
				const auto& descriptors = _sharedState->_computeDescriptors;
				_pipelineLayout->WriteDebugInfo(
					Log(Verbose),
					MakeIteratorRange(shaders),
					MakeIteratorRange(descriptors._currentlyBoundDesc));
			#endif
		#endif
	}

	void GraphicsEncoder_ProgressivePipeline::Bind(const ShaderProgram& shaderProgram)
	{
		assert(&shaderProgram.GetPipelineLayout() == _pipelineLayout.get());
		GraphicsPipelineBuilder::Bind(shaderProgram);
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
		// Vulkan does have a per-instance data rate concept, but to access it we need to use
		// the draw indirect commands. That allows us to put instance count and offset information
		// into the VkDrawIndirectCommand, VkDrawIndexedIndirectCommand data structures, which
		// are read from VkBuffer.
		//
		// We can emulate that functionality here by creating a buffer and just calling 
		// the vkCmdDrawIndirect. Or alternatively having some large buffer that we just
		// stream commands to over time. But neither of those really ideal. 
		// We should try to avoid creating and uploading buffer data during render passes,
		// and where possible move that to construction time.
		Log(Verbose) << "DrawInstances is very inefficient on Vulkan. Prefer pre-building buffers and vkCmdDrawIndirect" << std::endl;
		assert(_sharedState->_commandList.GetUnderlying());
		if (BindGraphicsPipeline()) {
			VkDrawIndirectCommand indirectCommands[] {
				VkDrawIndirectCommand { vertexCount, instanceCount, startVertexLocation, 0 }
			};
			Resource temporaryBuffer(
				*_factory,
				CreateDesc(
					BindFlag::DrawIndirectArgs, 0, GPUAccess::Read,
					LinearBufferDesc::Create(sizeof(indirectCommands)),
					"temp-DrawInstances-buffer"),
				SubResourceInitData{MakeIteratorRange(indirectCommands)});
			vkCmdDrawIndirect(
				_sharedState->_commandList.GetUnderlying().get(),
				temporaryBuffer.GetBuffer(),
				0, 1, sizeof(VkDrawIndirectCommand));
		}
	}

	void GraphicsEncoder_ProgressivePipeline::DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation)
	{
		Log(Verbose) << "DrawIndexedInstances is very inefficient on Vulkan. Prefer pre-building buffers and vkCmdDrawIndirect" << std::endl;
		assert(_sharedState->_commandList.GetUnderlying());
		if (BindGraphicsPipeline()) {
			VkDrawIndexedIndirectCommand indirectCommands[] {
				VkDrawIndexedIndirectCommand { indexCount, instanceCount, startIndexLocation, 0, 0 }
			};
			Resource temporaryBuffer(
				*_factory,
				CreateDesc(
					BindFlag::DrawIndirectArgs, 0, GPUAccess::Read,
					LinearBufferDesc::Create(sizeof(indirectCommands)),
					"temp-DrawInstances-buffer"),
				SubResourceInitData{MakeIteratorRange(indirectCommands)});
			vkCmdDrawIndexedIndirect(
				_sharedState->_commandList.GetUnderlying().get(),
				temporaryBuffer.GetBuffer(),
				0, 1, sizeof(VkDrawIndexedIndirectCommand));
		}
	}

	void GraphicsEncoder_ProgressivePipeline::DrawAuto() 
	{
		assert(0);      // not implemented
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

	GraphicsEncoder::GraphicsEncoder(
		const std::shared_ptr<CompiledPipelineLayout>& pipelineLayout,
		const std::shared_ptr<VulkanEncoderSharedState>& sharedState)
	: _pipelineLayout(pipelineLayout)
	, _sharedState(sharedState)
	{
		if (_pipelineLayout && _sharedState) {
			assert(_sharedState->_currentEncoder == nullptr && _sharedState->_currentEncoderType == VulkanEncoderSharedState::EncoderType::None);
			assert(_sharedState->_renderPass != nullptr);
			_sharedState->_currentEncoder = this;
			_sharedState->_currentEncoderType = VulkanEncoderSharedState::EncoderType::Graphics;

			_sharedState->_graphicsDescriptors.ResetState(*_pipelineLayout);

			// bind descriptor sets that are pending
			// If we've been using the pipeline layout builder directly, then we
			// must flush those changes down to the GraphicsPipelineBuilder
			if (_sharedState->_graphicsDescriptors._hasSetsAwaitingFlush) {
				_sharedState->_commandList.BindDescriptorSets(
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					GetUnderlyingPipelineLayout(),
					0, GetDescriptorSetCount(), AsPointer(_sharedState->_graphicsDescriptors._descriptorSets.begin()), 
					0, nullptr);
				_sharedState->_graphicsDescriptors._hasSetsAwaitingFlush = false;
			}
		}
	}

	GraphicsEncoder::~GraphicsEncoder()
	{
		if (_sharedState) {
			assert(_sharedState->_currentEncoder == this);
			_sharedState->_currentEncoder = nullptr;
			_sharedState->_currentEncoderType = VulkanEncoderSharedState::EncoderType::None;
		}
	}

	GraphicsEncoder::GraphicsEncoder(GraphicsEncoder&& moveFrom)
	{
		if (moveFrom._sharedState) {
			assert(moveFrom._sharedState->_currentEncoder == &moveFrom);
			_sharedState = std::move(moveFrom._sharedState);
			_sharedState->_currentEncoder = this;
		}
	}

	GraphicsEncoder& GraphicsEncoder::operator=(GraphicsEncoder&& moveFrom)
	{
		if (_sharedState) {
			assert(_sharedState->_currentEncoder == this);
			_sharedState->_currentEncoder = nullptr;
			_sharedState->_currentEncoderType = VulkanEncoderSharedState::EncoderType::None;
			_sharedState.reset();
		}

		if (moveFrom._sharedState) {
			assert(moveFrom._sharedState->_currentEncoder == &moveFrom);
			_sharedState = std::move(moveFrom._sharedState);
			_sharedState->_currentEncoder = this;
		}
		return *this;
	}

	GraphicsEncoder_ProgressivePipeline::GraphicsEncoder_ProgressivePipeline(GraphicsEncoder_ProgressivePipeline&& moveFrom)
	: GraphicsEncoder(std::move(moveFrom))
	, GraphicsPipelineBuilder(moveFrom)
	{
		_currentGraphicsPipeline = std::move(moveFrom._currentGraphicsPipeline);
		_factory = moveFrom._factory;
		_globalPools = moveFrom._globalPools;
		moveFrom._factory = nullptr;
		moveFrom._globalPools = nullptr;
	}

	GraphicsEncoder_ProgressivePipeline& GraphicsEncoder_ProgressivePipeline::operator=(GraphicsEncoder_ProgressivePipeline&& moveFrom)
	{
		GraphicsEncoder::operator=(std::move(moveFrom));
		GraphicsPipelineBuilder::operator=(std::move(moveFrom));
		_currentGraphicsPipeline = std::move(moveFrom._currentGraphicsPipeline);
		_factory = moveFrom._factory;
		_globalPools = moveFrom._globalPools;
		moveFrom._factory = nullptr;
		moveFrom._globalPools = nullptr;
		return *this;
	}

	GraphicsEncoder_ProgressivePipeline::GraphicsEncoder_ProgressivePipeline(
		const std::shared_ptr<CompiledPipelineLayout>& pipelineLayout,
		const std::shared_ptr<VulkanEncoderSharedState>& sharedState,
		ObjectFactory& objectFactory,
		GlobalPools& globalPools)
	: GraphicsEncoder(pipelineLayout, sharedState)
	, _factory(&objectFactory)
	, _globalPools(&globalPools)
	{
		_sharedState->_currentEncoderType = VulkanEncoderSharedState::EncoderType::ProgressiveGraphics;
	}

	GraphicsEncoder_ProgressivePipeline::GraphicsEncoder_ProgressivePipeline()
	{
		_factory = nullptr;
		_globalPools = nullptr;
	}

	GraphicsEncoder_ProgressivePipeline::~GraphicsEncoder_ProgressivePipeline()
	{}

	GraphicsEncoder_Optimized::GraphicsEncoder_Optimized(GraphicsEncoder_Optimized&& moveFrom)
	: GraphicsEncoder(std::move(moveFrom))
	{
	}

	GraphicsEncoder_Optimized::GraphicsEncoder_Optimized() {}

	GraphicsEncoder_Optimized& GraphicsEncoder_Optimized::operator=(GraphicsEncoder_Optimized&& moveFrom)
	{
		GraphicsEncoder::operator=(std::move(moveFrom));
		return *this;
	}

	GraphicsEncoder_Optimized::GraphicsEncoder_Optimized(
		const std::shared_ptr<CompiledPipelineLayout>& pipelineLayout,
		const std::shared_ptr<VulkanEncoderSharedState>& sharedState)
	: GraphicsEncoder(pipelineLayout, sharedState)
	{}
	GraphicsEncoder_Optimized::~GraphicsEncoder_Optimized()
	{}

	ComputeEncoder_ProgressivePipeline::ComputeEncoder_ProgressivePipeline(
		const std::shared_ptr<CompiledPipelineLayout>& pipelineLayout,
		const std::shared_ptr<VulkanEncoderSharedState>& sharedState,
		ObjectFactory& objectFactory,
		GlobalPools& globalPools)
	: _pipelineLayout(pipelineLayout)
	, _factory(&objectFactory)
	, _globalPools(&globalPools)
	{
		assert(_sharedState->_currentEncoder == nullptr && _sharedState->_currentEncoderType == VulkanEncoderSharedState::EncoderType::None);
		assert(_sharedState->_renderPass == nullptr);	// don't start compute encoding during a render pass
		_sharedState->_currentEncoder = this;
		_sharedState->_currentEncoderType = VulkanEncoderSharedState::EncoderType::ProgressiveCompute;

		_sharedState->_computeDescriptors.ResetState(*_pipelineLayout);

		// bind descriptor sets that are pending
		if (_sharedState->_computeDescriptors._hasSetsAwaitingFlush) {
			_sharedState->_commandList.BindDescriptorSets(
				VK_PIPELINE_BIND_POINT_COMPUTE,
				GetUnderlyingPipelineLayout(),
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

	GraphicsEncoder_Optimized DeviceContext::BeginGraphicsEncoder(const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout)
	{
		if (_sharedState->_inBltPass)
			Throw(::Exceptions::BasicLabel("Attempting to begin a compute encoder while a blt pass is in progress"));
		return GraphicsEncoder_Optimized { checked_pointer_cast<CompiledPipelineLayout>(pipelineLayout), _sharedState };
	}

	GraphicsEncoder_ProgressivePipeline DeviceContext::BeginGraphicsEncoder_ProgressivePipeline(const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout)
	{
		if (_sharedState->_inBltPass)
			Throw(::Exceptions::BasicLabel("Attempting to begin a compute encoder while a blt pass is in progress"));
		return GraphicsEncoder_ProgressivePipeline { checked_pointer_cast<CompiledPipelineLayout>(pipelineLayout), _sharedState, *_factory, *_globalPools };
	}

	ComputeEncoder_ProgressivePipeline DeviceContext::BeginComputeEncoder(const std::shared_ptr<ICompiledPipelineLayout>& pipelineLayout)
	{
		if (_sharedState->_renderPass)
			Throw(::Exceptions::BasicLabel("Attempting to begin a compute encoder while another render pass is in progress"));
		if (_sharedState->_inBltPass)
			Throw(::Exceptions::BasicLabel("Attempting to begin a compute encoder while a blt pass is in progress"));
		return ComputeEncoder_ProgressivePipeline { checked_pointer_cast<CompiledPipelineLayout>(pipelineLayout), _sharedState, *_factory, *_globalPools };
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

	void		DeviceContext::BeginCommandList(const VulkanSharedPtr<VkCommandBuffer>& cmdList)
	{
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

	auto        DeviceContext::ResolveCommandList() -> std::shared_ptr<CommandList>
	{
		assert(_sharedState->_commandList.GetUnderlying());
		if (_captureForBindRecords)
			Internal::ValidateIsEmpty(*_captureForBindRecords);		// always complete these captures before completing a command list
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
		if (_sharedState->_inBltPass)
			Throw(::Exceptions::BasicLabel("Attempting to begin a render pass while a blt pass is in progress"));
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

		// Set the default viewport & scissor
		VkViewport defaultViewport { (float)offset[0], (float)offset[1], (float)extent[0], (float)extent[1], 0.f, 1.0f };
		VkRect2D defaultScissor { offset[0], offset[1], extent[0], extent[1] };
		vkCmdSetViewport(_sharedState->_commandList.GetUnderlying().get(), 0, 1, &defaultViewport);
		vkCmdSetScissor(_sharedState->_commandList.GetUnderlying().get(), 0, 1, &defaultScissor);
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

	BlitEncoder DeviceContext::BeginBlitEncoder()
	{
		if (_sharedState->_renderPass)
			Throw(::Exceptions::BasicLabel("Attempting to begin a blt pass while a render pass is in progress"));
		if (_sharedState->_inBltPass)
			Throw(::Exceptions::BasicLabel("Attempting to begin a blt pass while another blt pass is already in progress"));
		if (_sharedState->_currentEncoder)
			Throw(::Exceptions::BasicLabel("Attempting to begin a blt pass while an encoder is in progress"));
		_sharedState->_inBltPass = true;
		return BlitEncoder(*this);
	}

	void DeviceContext::EndBlitEncoder()
	{
		assert(_sharedState->_inBltPass);
		_sharedState->_inBltPass = false;
	}

	CommandList& DeviceContext::GetActiveCommandList()
	{
		assert(_sharedState->_commandList.GetUnderlying());
		return _sharedState->_commandList;
	}

	bool DeviceContext::HasActiveCommandList()
	{
		return _sharedState->_commandList.GetUnderlying() != nullptr;
	}

	void DeviceContext::SetupPipelineBuilders()
	{
		#if 0
		auto& globals = Internal::VulkanGlobalsTemp::GetInstance();

		if (!globals._compiledDescriptorSetLayoutCache)
			globals._compiledDescriptorSetLayoutCache = Internal::CreateCompiledDescriptorSetLayoutCache(*_factory, *_globalPools);

		{
			#if defined(_DEBUG)
				Internal::ValidateRootSignature(_factory->GetPhysicalDevice(), *globals._graphicsRootSignatureFile);
			#endif

			auto graphicsPartialLayout = Internal::CreatePartialPipelineDescriptorsLayout(
				*globals._graphicsRootSignatureFile, PipelineType::Graphics);

			globals._graphicsPipelineLayout = Internal::CreateCompiledPipelineLayout(
				*_factory, *globals._compiledDescriptorSetLayoutCache, 
				MakeIteratorRange(graphicsPartialLayout.get(), graphicsPartialLayout.get()+1),
				VK_SHADER_STAGE_ALL_GRAPHICS);
		}

		{
			#if defined(_DEBUG)
				Internal::ValidateRootSignature(_factory->GetPhysicalDevice(), *globals._computeRootSignatureFile);
			#endif

			auto computePartialLayout = Internal::CreatePartialPipelineDescriptorsLayout(
				*globals._computeRootSignatureFile, PipelineType::Compute);

			globals._computePipelineLayout = Internal::CreateCompiledPipelineLayout(
				*_factory, *globals._compiledDescriptorSetLayoutCache, MakeIteratorRange(computePartialLayout.get(), computePartialLayout.get()+1),
				VK_SHADER_STAGE_COMPUTE_BIT);
		}
		#endif
	}

	void DeviceContext::RequireResourceVisbility(IteratorRange<const uint64_t*> resourceGuids)
	{
		#if defined(VULKAN_VALIDATE_RESOURCE_VISIBILITY)
			for (auto r:resourceGuids) {
				// Don't record the guid for any resources that are already marked as becoming visible 
				// during this command list (this is the only way we can check relative ordering of 
				// initialization and use within the same command list)
				auto i = std::find(_resourcesBecomingVisible.begin(), _resourcesBecomingVisible.end(), r);
				if (i != _resourcesBecomingVisible.end()) continue;
				_resourcesThatMustBeVisible.push_back(r);
			}
		#endif
	}

	void DeviceContext::MakeResourcesVisible(IteratorRange<const uint64_t*> resourceGuids)
	{
		#if defined(VULKAN_VALIDATE_RESOURCE_VISIBILITY)
			_resourcesBecomingVisible.insert(_resourcesBecomingVisible.end(), resourceGuids.begin(), resourceGuids.end());
		#endif
	}

	void DeviceContext::ValidateCommitToQueue()
	{
		#if defined(VULKAN_VALIDATE_RESOURCE_VISIBILITY)
			// We're going to commit the current command list to the queue. Let's validate resource visibility
			// All resources in _resourcesBecomingVisible must be on the "_resourcesVisibleToQueue" list in ObjectFactory
			// If they are not, it means one of the following:
			//   - that the resource was never made visible on a command list
			//   - the command list in which it was made visible hasn't yet been commited to the queue
			//   - it's made visible after it was used on this command list
			std::sort(_resourcesThatMustBeVisible.begin(), _resourcesThatMustBeVisible.end());
			std::sort(_resourcesBecomingVisible.begin(), _resourcesBecomingVisible.end());
			auto becomingVisibleEnd = std::unique(_resourcesBecomingVisible.begin(), _resourcesBecomingVisible.end());

			auto factoryi = _factory->_resourcesVisibleToQueue.begin();
			auto searchi = _resourcesThatMustBeVisible.begin();
			while (searchi != _resourcesThatMustBeVisible.end()) {
				while (factoryi != _factory->_resourcesVisibleToQueue.end() && *factoryi < *searchi)
					++factoryi;

				if (factoryi == _factory->_resourcesVisibleToQueue.end() || *factoryi != *searchi)
					Throw(std::runtime_error("Attempting to use resource that hasn't been made visible. Ensure that all used resources have had Metal::CompleteInitialization() called on them"));

				++searchi;
			}
			_resourcesThatMustBeVisible.clear();

			// Now register the resources in _resourcesBecomingVisible as visible to the queue
			if (_resourcesBecomingVisible.begin() != becomingVisibleEnd) {
				std::vector<uint64_t> newVisibleToQueue;
				newVisibleToQueue.reserve(becomingVisibleEnd - _resourcesBecomingVisible.begin() + _factory->_resourcesVisibleToQueue.size());
				std::set_union(
					_factory->_resourcesVisibleToQueue.begin(), _factory->_resourcesVisibleToQueue.end(),
					_resourcesBecomingVisible.begin(), becomingVisibleEnd,
					std::back_inserter(newVisibleToQueue));

				std::swap(newVisibleToQueue, _factory->_resourcesVisibleToQueue);
			}
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

		auto& globals = Internal::VulkanGlobalsTemp::GetInstance();
		globals._globalPools = &globalPools;

		SetupPipelineBuilders();

		/*
		globals._boundGraphicsSignatures = std::make_shared<BoundSignatureFile>(*_factory, *_globalPools, VK_SHADER_STAGE_ALL_GRAPHICS);
		globals._boundGraphicsSignatures->RegisterSignatureFile(VulkanGlobalsTemp::s_mainSignature, *globals._graphicsRootSignatureFile);
		globals._boundComputeSignatures = std::make_shared<BoundSignatureFile>(*_factory, *_globalPools, VK_SHADER_STAGE_COMPUTE_BIT);
		globals._boundComputeSignatures->RegisterSignatureFile(VulkanGlobalsTemp::s_mainSignature, *globals._computeRootSignatureFile);

		globals._mainGraphicsConfig = std::make_shared<PartialPipelineDescriptorsLayout>(factory, *globals._graphicsRootSignatureFile, globals.s_mainSignature, PipelineType::Graphics);
		globals._mainComputeConfig = std::make_shared<PartialPipelineDescriptorsLayout>(factory, *globals._computeRootSignatureFile, globals.s_mainSignature, PipelineType::Compute);
		*/
	}

	DeviceContext::~DeviceContext()
	{
		if (_captureForBindRecords)
			Internal::ValidateIsEmpty(*_captureForBindRecords);
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
		_inBltPass = false;
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

	void VulkanEncoderSharedState::DescriptorCollection::ResetState(const CompiledPipelineLayout& layout)
	{
		_descriptorSets.resize(layout.GetDescriptorSetCount());
		for (unsigned c=0; c<_descriptorSets.size(); ++c) {
			_descriptorSets[c] = layout.GetBlankDescriptorSet(c).get();
			#if defined(VULKAN_VERBOSE_DEBUG)
				_currentlyBoundDesc[c] = layout.GetBlankDescriptorSetDebugInfo(c);
			#endif
		}
		_hasSetsAwaitingFlush = true;
	}

	VulkanEncoderSharedState::DescriptorCollection::DescriptorCollection(
		const ObjectFactory&    factory, 
		GlobalPools&            globalPools,
		unsigned				descriptorSetCount)
	: _factory(&factory), _globalPools(&globalPools)
	{
		_descriptorSets.resize(descriptorSetCount, nullptr);
		#if defined(VULKAN_VERBOSE_DEBUG)
			_currentlyBoundDesc.resize(descriptorSetCount);
		#endif
		_hasSetsAwaitingFlush = false;
	}

}}

