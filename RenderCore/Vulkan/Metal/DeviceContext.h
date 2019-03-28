// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Forward.h"
#include "State.h"
#include "FrameBuffer.h"        // for AttachmentPool
#include "InputLayout.h"		// for NumericUniformsInterface
#include "DescriptorSet.h"
#include "VulkanCore.h"
#include "../../ResourceList.h"
#include "../../IDevice_Forward.h"
#include "../../Types_Forward.h"
#include "../../IThreadContext_Forward.h"
#include "../../Types_Forward.h"
#include "../../Utility/IteratorUtils.h"
#include <memory>
#include <sstream>

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

    class GraphicsPipelineBuilder
    {
    public:
        void        Bind(const RasterizerState& rasterizer);
        void        Bind(const BlendState& blendState);
        void        Bind(const DepthStencilState& depthStencilState, unsigned stencilRef = 0x0);

		void        Bind(const ShaderProgram& shaderProgram);
		void        Bind(const ShaderProgram& shaderProgram, const BoundClassInterfaces&);
        
        void        Bind(Topology topology);

		void		UnbindInputLayout();

        VulkanUniquePtr<VkPipeline> CreatePipeline(
            const ObjectFactory& factory,
            VkPipelineCache pipelineCache,
            VkPipelineLayout layout, 
            VkRenderPass renderPass, unsigned subpass, 
            TextureSamples samples);
        bool IsPipelineStale() const { return _pipelineStale; }

		const ShaderProgram* GetBoundShaderProgram() const { return _shaderProgram; }

        GraphicsPipelineBuilder();
        ~GraphicsPipelineBuilder();

        GraphicsPipelineBuilder(const GraphicsPipelineBuilder&);
        GraphicsPipelineBuilder& operator=(const GraphicsPipelineBuilder&);
    private:
        RasterizerState         _rasterizerState;
        BlendState              _blendState;
        DepthStencilState       _depthStencilState;
        VkPrimitiveTopology     _topology;

        const BoundInputLayout* _inputLayout;       // note -- unprotected pointer
        const ShaderProgram*    _shaderProgram;

        bool                    _pipelineStale;

		void        SetBoundInputLayout(const BoundInputLayout& inputLayout);
		friend class BoundInputLayout;
		friend class ShaderProgram;
    };

    class ComputePipelineBuilder
    {
    public:
        void        Bind(const ComputeShader& shader);

        VulkanUniquePtr<VkPipeline> CreatePipeline(
            const ObjectFactory& factory,
            VkPipelineCache pipelineCache,
            VkPipelineLayout layout);
        bool IsPipelineStale() const { return _pipelineStale; }

		const ComputeShader* GetBoundComputeShader() const { return _shader; }

        ComputePipelineBuilder();
        ~ComputePipelineBuilder();

        ComputePipelineBuilder(const ComputePipelineBuilder&);
        ComputePipelineBuilder& operator=(const ComputePipelineBuilder&);

    private:
        const ComputeShader*    _shader;
        bool                    _pipelineStale;
    };

	class DescriptorSet
	{
	public:
		VulkanUniquePtr<VkDescriptorSet>		_underlying;
		#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
			DescriptorSetVerboseDescription		_description;
		#endif		
	};

    class DescriptorCollection
    {
    public:
        NumericUniformsInterface			_numericBindings;
        unsigned                            _numericBindingsSlot;

        std::vector<VkDescriptorSet>		_descriptorSets;			// (can't use a smart pointer here because it's often bound to the descriptor set in NumericUniformsInterface, which we must share)
        bool                                _hasSetsAwaitingFlush;

		#if defined(VULKAN_VERBOSE_DESCRIPTIONS)
			std::vector<DescriptorSetVerboseDescription> _descriptorSetBindings;
		#endif

        std::shared_ptr<PipelineLayout>		_pipelineLayout;

        DescriptorCollection(
            const ObjectFactory&    factory, 
            GlobalPools&            globalPools,
            const std::shared_ptr<PipelineLayout>& pipelineLayout);
    };

	class CommandList
	{
	public:
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

    using CommandListPtr = std::shared_ptr<CommandList>;

	class DeviceContext : public GraphicsPipelineBuilder, ComputePipelineBuilder
    {
    public:
		void        Bind(const Resource& ib, Format indexFormat, unsigned offset=0);
        void        Bind(const ViewportDesc& viewport);
        const ViewportDesc& GetBoundViewport() const { return _boundViewport; }

        using GraphicsPipelineBuilder::Bind;        // we need to expose the "Bind" functions in the base class, as well
        using ComputePipelineBuilder::Bind;

        void        Draw(unsigned vertexCount, unsigned startVertexLocation=0);
        void        DrawIndexed(unsigned indexCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0);
        void        DrawAuto();
        void        Dispatch(unsigned countX, unsigned countY=1, unsigned countZ=1);

        void        Clear(const RenderTargetView& renderTargets, const VectorPattern<float,4>& clearColour) {}
        struct ClearFilter { enum Enum { Depth = 1<<0, Stencil = 1<<1 }; using BitField = unsigned; };
        void        Clear(const DepthStencilView& depthStencil, ClearFilter::BitField clearFilter, float depth, unsigned stencil) {}
        void        ClearUInt(const UnorderedAccessView& unorderedAccess, const VectorPattern<unsigned,4>& clearColour) {}
        void        ClearFloat(const UnorderedAccessView& unorderedAccess, const VectorPattern<float,4>& clearColour) {}
        void        ClearStencil(const DepthStencilView& depthStencil, unsigned stencil) {}

		NumericUniformsInterface& GetNumericUniforms(ShaderStage stage);

        static std::shared_ptr<DeviceContext> Get(IThreadContext& threadContext);
		std::shared_ptr<DeviceContext> Fork();

		void		BeginCommandList();
		void		BeginCommandList(const VulkanSharedPtr<VkCommandBuffer>& cmdList);
		void		CommitCommandList(CommandList&, bool);
		auto        ResolveCommandList() -> CommandListPtr;
		bool		IsImmediate() { return false; }

		CommandList& GetActiveCommandList();
		bool HasActiveCommandList();
		
		void		InvalidateCachedState() {}
		static void PrepareForDestruction(IDevice*, IPresentationChain*);

		void			BindDescriptorSet(PipelineType pipelineType, unsigned index, VkDescriptorSet set VULKAN_VERBOSE_DESCRIPTIONS_ONLY(, DescriptorSetVerboseDescription&& description));
        DescriptorSet	AllocateDescriptorSet(PipelineType pipelineType, unsigned descriptorSetIndex);
		void			PushConstants(VkShaderStageFlags stageFlags, IteratorRange<const void*> data);

		PipelineLayout* GetPipelineLayout(PipelineType pipelineType);

        GlobalPools&    GetGlobalPools();
        VkDevice        GetUnderlyingDevice();
		ObjectFactory&	GetFactory() const				{ return *_factory; }
		TemporaryBufferSpace& GetTemporaryBufferSpace()		{ return *_tempBufferSpace; }

        void BeginRenderPass(
            const FrameBuffer& fb,
            TextureSamples samples,
            VectorPattern<int, 2> offset, VectorPattern<unsigned, 2> extent,
            IteratorRange<const ClearValue*> clearValues);
        void EndRenderPass();
        bool IsInRenderPass() const;
		void NextSubpass(VkSubpassContents);
		unsigned RenderPassSubPassIndex() const { return _renderPassSubpass; }

		template<int Count> void    Bind(const ResourceList<RenderTargetView, Count>& renderTargets, const DepthStencilView* depthStencil);
        template<int Count1, int Count2> void    Bind(const ResourceList<RenderTargetView, Count1>& renderTargets, const DepthStencilView* depthStencil, const ResourceList<UnorderedAccessView, Count2>& unorderedAccess);

        DeviceContext(
            ObjectFactory& factory, 
            GlobalPools& globalPools,
            const std::shared_ptr<PipelineLayout>& globalPipelineLayout,
            const std::shared_ptr<PipelineLayout>& computePipelineLayout,
			CommandPool& cmdPool, 
			CommandBufferType cmdBufferType,
			TemporaryBufferSpace& tempBufferSpace);
		DeviceContext(const DeviceContext&) = delete;
		DeviceContext& operator=(const DeviceContext&) = delete;

    private:
        CommandList							_commandList;
        GlobalPools*                        _globalPools;
        ObjectFactory*						_factory;

        VkRenderPass                        _renderPass;
        TextureSamples                      _renderPassSamples;
        unsigned                            _renderPassSubpass;
        ViewportDesc                        _boundViewport;

        VulkanUniquePtr<VkPipeline>         _currentGraphicsPipeline;
        VulkanUniquePtr<VkPipeline>         _currentComputePipeline;

        DescriptorCollection                _graphicsDescriptors;
        DescriptorCollection                _computeDescriptors;

        CommandPool*                        _cmdPool;
		CommandBufferType					_cmdBufferType;

		TemporaryBufferSpace*				_tempBufferSpace;

        bool BindGraphicsPipeline();
        bool BindComputePipeline();
		void LogGraphicsPipeline();
		void LogComputePipeline();
		void BindDummyDescriptorSets(DescriptorCollection& collection);
    };

	inline CommandList& DeviceContext::GetActiveCommandList()
	{
		assert(_commandList.GetUnderlying());
		return _commandList;
	}

	inline bool DeviceContext::HasActiveCommandList()
	{
		return _commandList.GetUnderlying() != nullptr;
	}

	template<int Count> 
		void    DeviceContext::Bind(const ResourceList<RenderTargetView, Count>& renderTargets, const DepthStencilView* depthStencil) {}
    template<int Count1, int Count2> 
		void    DeviceContext::Bind(const ResourceList<RenderTargetView, Count1>& renderTargets, const DepthStencilView* depthStencil, const ResourceList<UnorderedAccessView, Count2>& unorderedAccess)
		{}

}}

