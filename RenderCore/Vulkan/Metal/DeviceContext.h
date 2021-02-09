// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Forward.h"
#include "State.h"
#include "InputLayout.h"		// for NumericUniformsInterface
#include "PipelineLayout.h"		// for PipelineDescriptorsLayoutBuilder
#include "VulkanCore.h"
#include "../../ResourceList.h"
#include "../../ResourceDesc.h"
#include "../../FrameBufferDesc.h"
#include "../../IDevice_Forward.h"
#include "../../Types_Forward.h"
#include "../../Utility/IteratorUtils.h"
#include <memory>
#include <sstream>

namespace RenderCore { class VertexBufferView; class IndexBufferView; }

namespace RenderCore { namespace Metal_Vulkan
{
	static const unsigned s_maxBoundVBs = 4;

	class GlobalPools;
	class FrameBuffer;
	class PipelineLayout;
	class DescriptorSetSignature;
	class CommandPool;
	class DescriptorPool;
	class DummyResources;
	enum class CommandBufferType;
	class TemporaryBufferSpace;
	class Resource;
	class PartialPipelineDescriptorsLayout;

	class GraphicsPipeline : public VulkanUniquePtr<VkPipeline>
	{
	public:
		uint64_t GetGUID() const;

		GraphicsPipeline(VulkanUniquePtr<VkPipeline>&&);
		~GraphicsPipeline();
	};

	class ComputePipeline : public VulkanUniquePtr<VkPipeline>
	{
	public:
		uint64_t GetGUID() const;

		ComputePipeline(VulkanUniquePtr<VkPipeline>&&);
		~ComputePipeline();
	};

	class GraphicsPipelineBuilder
	{
	public:
		// --------------- Cross-GFX-API interface --------------- 
		void        Bind(const ShaderProgram& shaderProgram);
		
		void        Bind(IteratorRange<const AttachmentBlendDesc*> blendStates);
		void        Bind(const DepthStencilDesc& depthStencilState);
		void        Bind(const RasterizationDesc& rasterizer);

		void 		Bind(const BoundInputLayout& inputLayout, Topology topology);
		void		UnbindInputLayout();

		void 		SetRenderPassConfiguration(const FrameBufferProperties& fbProps, const FrameBufferDesc& fbDesc, unsigned subPass);
		uint64_t 	GetRenderPassConfigurationHash() const;

		std::shared_ptr<GraphicsPipeline> CreatePipeline(
			ObjectFactory& factory, VkPipelineCache pipelineCache,
			VkRenderPass renderPass, unsigned subpass, 
			TextureSamples samples);
		bool IsPipelineStale() const { return _pipelineStale; }

		// const ShaderProgram* GetBoundShaderProgram() const { return _shaderProgram; }

		// --------------- Vulkan specific interface --------------- 

		GraphicsPipelineBuilder();
		~GraphicsPipelineBuilder();

	private:
		Internal::VulkanRasterizerState		_rasterizerState;
		Internal::VulkanBlendState			_blendState;
		Internal::VulkanDepthStencilState	_depthStencilState;
		VkPrimitiveTopology     			_topology;

		std::vector<VkVertexInputAttributeDescription> _iaAttributes;
		std::vector<VkVertexInputBindingDescription> _iaVBBindings;
		uint64_t _iaHash;

		const ShaderProgram*    _shaderProgram;

		bool                    _pipelineStale;

		/*void        SetBoundInputLayout(const BoundInputLayout& inputLayout);
		friend class BoundInputLayout;
		friend class ShaderProgram;*/

	protected:
		GraphicsPipelineBuilder(const GraphicsPipelineBuilder&);
		GraphicsPipelineBuilder& operator=(const GraphicsPipelineBuilder&);
	};

	class ComputePipelineBuilder
	{
	public:
		// --------------- Cross-GFX-API interface --------------- 
		void        Bind(const ComputeShader& shader);

		std::shared_ptr<ComputePipeline> CreatePipeline(
			ObjectFactory& factory, VkPipelineCache pipelineCache);
		bool IsPipelineStale() const { return _pipelineStale; }

		// const ComputeShader* GetBoundComputeShader() const { return _shader; }

		// --------------- Vulkan specific interface --------------- 

		ComputePipelineBuilder();
		~ComputePipelineBuilder();

		ComputePipelineBuilder(const ComputePipelineBuilder&);
		ComputePipelineBuilder& operator=(const ComputePipelineBuilder&);

	private:
		const ComputeShader*    _shader;
		bool                    _pipelineStale;
	};

	class DescriptorCollection
	{
	public:
		NumericUniformsInterface			_numericBindings;
		unsigned                            _numericBindingsSlot = ~0u;

		std::vector<VkDescriptorSet>		_descriptorSets;			// (can't use a smart pointer here because it's often bound to the descriptor set in NumericUniformsInterface, which we must share)
		bool                                _hasSetsAwaitingFlush = false;

		struct DescInfo 
		{
			VulkanSharedPtr<VkDescriptorSet>	_dummy;

			#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
				DescriptorSetVerboseDescription _currentlyBoundDescription;
				DescriptorSetVerboseDescription _dummyDescription;
			#endif
		};
		std::vector<DescInfo> _descInfo;

		void BindNumericUniforms(
			const std::shared_ptr<DescriptorSetSignature>& signature,
			const LegacyRegisterBinding& bindings,
			VkShaderStageFlags stageFlags,
			unsigned descriptorSetIndex);

		DescriptorCollection(
			const ObjectFactory&    factory, 
			GlobalPools&            globalPools,
			unsigned				descriptorSetCount);

	private:
		const ObjectFactory*    _factory;
		GlobalPools*			_globalPools;
	};

	class CommandList
	{
	public:
		// --------------- Vulkan specific interface --------------- 
		void UpdateBuffer(
			VkBuffer buffer, VkDeviceSize offset, 
			VkDeviceSize byteCount, const void* data);
		void BindDescriptorSets(
			VkPipelineBindPoint pipelineBindPoint,
			VkPipelineLayout layout,
			uint32_t firstSet,
			uint32_t descriptorSetCount,
			const VkDescriptorSet* pDescriptorSets,
			uint32_t dynamicOffsetCount,
			const uint32_t* pDynamicOffsets);
		void CopyBuffer(
			VkBuffer srcBuffer,
			VkBuffer dstBuffer,
			uint32_t regionCount,
			const VkBufferCopy* pRegions);
		void CopyImage(
			VkImage srcImage,
			VkImageLayout srcImageLayout,
			VkImage dstImage,
			VkImageLayout dstImageLayout,
			uint32_t regionCount,
			const VkImageCopy* pRegions);
		void CopyBufferToImage(
			VkBuffer srcBuffer,
			VkImage dstImage,
			VkImageLayout dstImageLayout,
			uint32_t regionCount,
			const VkBufferImageCopy* pRegions);
		void PipelineBarrier(
			VkPipelineStageFlags            srcStageMask,
			VkPipelineStageFlags            dstStageMask,
			VkDependencyFlags               dependencyFlags,
			uint32_t                        memoryBarrierCount,
			const VkMemoryBarrier*          pMemoryBarriers,
			uint32_t                        bufferMemoryBarrierCount,
			const VkBufferMemoryBarrier*    pBufferMemoryBarriers,
			uint32_t                        imageMemoryBarrierCount,
			const VkImageMemoryBarrier*     pImageMemoryBarriers);
		void PushConstants(
			VkPipelineLayout layout,
			VkShaderStageFlags stageFlags,
			uint32_t offset,
			uint32_t size,
			const void* pValues);
		void WriteTimestamp(
			VkPipelineStageFlagBits pipelineStage, 
			VkQueryPool queryPool, uint32_t query);
		void BeginQuery(VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags = 0);
		void EndQuery(VkQueryPool queryPool, uint32_t query);
		void ResetQueryPool(
			VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount);
		void SetEvent(VkEvent evnt, VkPipelineStageFlags stageMask);
		void ResetEvent(VkEvent evnt, VkPipelineStageFlags stageMask);
		void BindVertexBuffers(
			uint32_t            firstBinding,
			uint32_t            bindingCount,
			const VkBuffer*     pBuffers,
			const VkDeviceSize*	pOffsets);

		const VulkanSharedPtr<VkCommandBuffer>& GetUnderlying() const { return _underlying; }

		CommandList();
		explicit CommandList(const VulkanSharedPtr<VkCommandBuffer>& underlying);
		~CommandList();

		CommandList(CommandList&&) = default;
		CommandList& operator=(CommandList&&) = default;
	private:
		VulkanSharedPtr<VkCommandBuffer> _underlying;
	};

	struct ClearFilter { enum Enum { Depth = 1<<0, Stencil = 1<<1 }; using BitField = unsigned; };

	class VulkanEncoderSharedState
	{
	public:
		void* 			_activeEncoder;
		CommandList 	_commandList;

		VkRenderPass	_renderPass;
		TextureSamples	_renderPassSamples;
		unsigned		_renderPassSubpass;

		float			_renderTargetWidth;
		float			_renderTargetHeight;

		DescriptorCollection	_graphicsDescriptors;
		DescriptorCollection	_computeDescriptors;

		void* _currentEncoder;
		enum class EncoderType { None, Graphics, ProgressiveGraphics, ProgressiveCompute };
		EncoderType _currentEncoderType;
	};

	class SharedGraphicsEncoder
	{
	public:
		//	------ Draw & Clear -------
		void        Clear(const RenderTargetView& renderTargets, const VectorPattern<float,4>& clearColour);
		void        Clear(const DepthStencilView& depthStencil, ClearFilter::BitField clearFilter, float depth, unsigned stencil);
		void        ClearUInt(const UnorderedAccessView& unorderedAccess, const VectorPattern<unsigned,4>& clearColour);
		void        ClearFloat(const UnorderedAccessView& unorderedAccess, const VectorPattern<float,4>& clearColour);
		void        ClearStencil(const DepthStencilView& depthStencil, unsigned stencil);

		//	------ Non-pipeline states (that can be changed mid-render pass) -------
		void 		Bind(IteratorRange<const VertexBufferView*> vertexBuffers);
		void        Bind(const IndexBufferView& ibView);
		void		SetStencilRef(unsigned stencilRef);
		void 		Bind(IteratorRange<const Viewport*> viewports, IteratorRange<const ScissorRect*> scissorRects);

		// --------------- Vulkan specific interface --------------- 
		void		BindDescriptorSet(unsigned index, VkDescriptorSet set VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, DescriptorSetVerboseDescription&& description));
		void		PushConstants(VkShaderStageFlags stageFlags, IteratorRange<const void*> data);
		void		RebindNumericDescriptorSet();

	protected:
		SharedGraphicsEncoder(const std::shared_ptr<VulkanEncoderSharedState>& sharedState);
		~SharedGraphicsEncoder();
		SharedGraphicsEncoder(const SharedGraphicsEncoder&);		// (hide these to avoid slicing in derived types)
		SharedGraphicsEncoder& operator=(const SharedGraphicsEncoder&);

		VkPipelineLayout GetPipelineLayout();
		unsigned GetDescriptorSetCount();

		std::shared_ptr<VulkanEncoderSharedState> _sharedState;
	};
	
	class GraphicsEncoder_ProgressivePipeline : public SharedGraphicsEncoder, public GraphicsPipelineBuilder
	{
	public:
		//	------ Draw & Clear -------
		void        Draw(unsigned vertexCount, unsigned startVertexLocation=0);
		void        DrawIndexed(unsigned indexCount, unsigned startIndexLocation=0);
		void    	DrawInstances(unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation=0);
		void    	DrawIndexedInstances(unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation=0);
		void        DrawAuto();

	protected:
		GraphicsEncoder_ProgressivePipeline(
			const std::shared_ptr<VulkanEncoderSharedState>& sharedState,
			ObjectFactory& objectFactory,
			GlobalPools& globalPools);
		~GraphicsEncoder_ProgressivePipeline();
	
		bool 		BindGraphicsPipeline();
		std::shared_ptr<GraphicsPipeline>	_currentGraphicsPipeline;
		ObjectFactory*						_factory;
		GlobalPools*                        _globalPools;

		friend class DeviceContext;
	};

	class GraphicsEncoder : public SharedGraphicsEncoder, public GraphicsPipelineBuilder
	{
	public:
		//	------ Draw & Clear -------
		void        Draw(const GraphicsPipeline& pipeline, unsigned vertexCount, unsigned startVertexLocation=0);
		void        DrawIndexed(const GraphicsPipeline& pipeline, unsigned indexCount, unsigned startIndexLocation=0);
		void    	DrawInstances(const GraphicsPipeline& pipeline, unsigned vertexCount, unsigned instanceCount, unsigned startVertexLocation=0);
		void    	DrawIndexedInstances(const GraphicsPipeline& pipeline, unsigned indexCount, unsigned instanceCount, unsigned startIndexLocation=0);
		void        DrawAuto(const GraphicsPipeline& pipeline);

	protected:
		GraphicsEncoder(const std::shared_ptr<VulkanEncoderSharedState>& sharedState);
		~GraphicsEncoder();

		friend class DeviceContext;
	};

	class ComputeEncoder_ProgressivePipeline : public ComputePipelineBuilder
	{
	public:
		void        Dispatch(unsigned countX, unsigned countY=1, unsigned countZ=1);

		// --------------- Vulkan specific interface --------------- 
		void		BindDescriptorSet(unsigned index, VkDescriptorSet set VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, DescriptorSetVerboseDescription&& description));
		void		RebindNumericDescriptorSet();

	protected:
		bool 		BindComputePipeline();
		std::shared_ptr<VulkanEncoderSharedState> _sharedState;
		std::shared_ptr<ComputePipeline> 	_currentComputePipeline;
		ObjectFactory*						_factory;
		GlobalPools*                        _globalPools;

		VkPipelineLayout GetPipelineLayout();
		unsigned GetDescriptorSetCount();

		ComputeEncoder_ProgressivePipeline(
			const std::shared_ptr<VulkanEncoderSharedState>& sharedState,
			ObjectFactory& objectFactory,
			GlobalPools& globalPools);
		~ComputeEncoder_ProgressivePipeline();

		friend class DeviceContext;
	};

	class DeviceContext
	{
	public:
		// --------------- Cross-GFX-API interface --------------- 

		void BeginRenderPass(
			FrameBuffer& frameBuffer,
			IteratorRange<const ClearValue*> clearValues = {});
		void BeginNextSubpass(FrameBuffer& frameBuffer);
		void EndRenderPass();
		unsigned GetCurrentSubpassIndex() const;

		GraphicsEncoder BeginGraphicsEncoder();
		GraphicsEncoder_ProgressivePipeline BeginGraphicsEncoder_ProgressivePipeline();
		ComputeEncoder_ProgressivePipeline BeginComputeEncoder();

		static std::shared_ptr<DeviceContext> Get(IThreadContext& threadContext);

		// --------------- Vulkan specific interface --------------- 

		void BeginRenderPass(
			const FrameBuffer& fb,
			TextureSamples samples,
			VectorPattern<int, 2> offset, VectorPattern<unsigned, 2> extent,
			IteratorRange<const ClearValue*> clearValues);
		
		NumericUniformsInterface& GetNumericUniforms(ShaderStage stage);

		void		BeginCommandList();
		void		BeginCommandList(const VulkanSharedPtr<VkCommandBuffer>& cmdList);
		void		ExecuteCommandList(CommandList&, bool);

		struct QueueCommandListFlags
		{
			enum Bits { Stall = 1 << 0 };
			using BitField = unsigned;
		};
		void		QueueCommandList(IDevice& device, QueueCommandListFlags::BitField flags = 0);
		auto        ResolveCommandList() -> std::shared_ptr<CommandList>;

		CommandList& GetActiveCommandList();
		bool HasActiveCommandList();

		GlobalPools&    GetGlobalPools();
		VkDevice        GetUnderlyingDevice();
		ObjectFactory&	GetFactory() const				{ return *_factory; }
		TemporaryBufferSpace& GetTemporaryBufferSpace()		{ return *_tempBufferSpace; }

		void BeginRenderPass(
			const FrameBuffer& fb,
			TextureSamples samples,
			VectorPattern<int, 2> offset, VectorPattern<unsigned, 2> extent,
			IteratorRange<const ClearValue*> clearValues);
		bool IsInRenderPass() const;
		void NextSubpass(VkSubpassContents);

		DeviceContext(
			ObjectFactory& factory, 
			GlobalPools& globalPools,
			CommandPool& cmdPool, 
			CommandBufferType cmdBufferType,
			TemporaryBufferSpace& tempBufferSpace);
		DeviceContext(const DeviceContext&) = delete;
		DeviceContext& operator=(const DeviceContext&) = delete;

		// --------------- Legacy interface --------------- 
		void			InvalidateCachedState() {}
		static void		PrepareForDestruction(IDevice*, IPresentationChain*);
		bool			IsImmediate() { return false; }

	private:
		ObjectFactory*						_factory;
		GlobalPools*                        _globalPools;

		std::shared_ptr<VulkanEncoderSharedState> _sharedState;

		CommandPool*                        _cmdPool;
		CommandBufferType					_cmdBufferType;

		TemporaryBufferSpace*				_tempBufferSpace;

		VulkanUniquePtr<VkFence>			_utilityFence;

		void SetupPipelineBuilders();
		void ResetDescriptorSetState();
	};

	inline CommandList& DeviceContext::GetActiveCommandList()
	{
		assert(_sharedState->_commandList.GetUnderlying());
		return _sharedState->_commandList;
	}

	inline bool DeviceContext::HasActiveCommandList()
	{
		return _sharedState->_commandList.GetUnderlying() != nullptr;
	}

}}

