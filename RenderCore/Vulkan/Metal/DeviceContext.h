// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Forward.h"
#include "State.h"
#include "FrameBuffer.h"        // for AttachmentPool
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
        void        SetVertexStrides(IteratorRange<const unsigned*> vertexStrides);

        VulkanUniquePtr<VkPipeline> CreatePipeline(
            const ObjectFactory& factory,
            VkPipelineCache pipelineCache,
            VkPipelineLayout layout, 
            VkRenderPass renderPass, unsigned subpass, 
            TextureSamples samples);
        bool IsPipelineStale() const { return _pipelineStale; }

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

        unsigned                _vertexStrides[s_maxBoundVBs];

        bool                    _pipelineStale;

		void        SetBoundInputLayout(const BoundInputLayout& inputLayout);
		friend class BoundInputLayout;
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

        ComputePipelineBuilder();
        ~ComputePipelineBuilder();

        ComputePipelineBuilder(const ComputePipelineBuilder&);
        ComputePipelineBuilder& operator=(const ComputePipelineBuilder&);

    private:
        const ComputeShader*    _shader;
        bool                    _pipelineStale;
    };

    class DescriptorSetBuilder
    {
    public:
        enum class Stage { Vertex, Pixel, Geometry, Compute, Hull, Domain, Max };
        void    BindSRV(Stage stage, unsigned startingPoint, IteratorRange<const TextureView*const*> resources);
        void    BindUAV(Stage stage, unsigned startingPoint, IteratorRange<const TextureView*const*> resources);
        void    Bind(Stage stage, unsigned startingPoint, IteratorRange<const VkBuffer*> uniformBuffers);
        void    Bind(Stage stage, unsigned startingPoint, IteratorRange<const VkSampler*> samplers);

        void    GetDescriptorSets(IteratorRange<VkDescriptorSet*> dst);
        bool    HasChanges() const;
        void    Reset();

        DescriptorSetBuilder(
            const ObjectFactory& factory, DescriptorPool& descPool, 
            DummyResources& dummyResources,
            VkDescriptorSetLayout layout,
            const DescriptorSetSignature& signature, 
            int cbBindingOffset, int srvBindingOffset, 
            int samplerBindingOffset, int uavBindingOffset);
        ~DescriptorSetBuilder();

        DescriptorSetBuilder(const DescriptorSetBuilder&) = delete;
        DescriptorSetBuilder& operator=(const DescriptorSetBuilder&) = delete;
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    class DescriptorCollection
    {
    public:
        DescriptorSetBuilder                _dynamicBindings;
        unsigned                            _dynamicBindingsSlot;

        DescriptorSetBuilder                _globalBindings;
        unsigned                            _globalBindingsSlot;

        std::vector<VkDescriptorSet>        _descriptorSets;
        bool                                _hasSetsAwaitingFlush;

        PipelineLayout*                     _pipelineLayout;

        DescriptorCollection(
            const ObjectFactory&    factory, 
            GlobalPools&            globalPools,
            PipelineLayout&         pipelineLayout);
    };

    using CommandList = VkCommandBuffer;
    using CommandListPtr = VulkanSharedPtr<VkCommandBuffer>;

	class DeviceContext : public GraphicsPipelineBuilder, ComputePipelineBuilder
    {
    public:
        template<int Count> void    BindVS(const ResourceList<ShaderResourceView, Count>& shaderResources);
        template<int Count> void    BindPS(const ResourceList<ShaderResourceView, Count>& shaderResources);
        template<int Count> void    BindCS(const ResourceList<ShaderResourceView, Count>& shaderResources);
        template<int Count> void    BindGS(const ResourceList<ShaderResourceView, Count>& shaderResources);
        template<int Count> void    BindHS(const ResourceList<ShaderResourceView, Count>& shaderResources);
        template<int Count> void    BindDS(const ResourceList<ShaderResourceView, Count>& shaderResources);

        template<int Count> void    BindVS(const ResourceList<SamplerState, Count>& samplerStates);
        template<int Count> void    BindPS(const ResourceList<SamplerState, Count>& samplerStates);
        template<int Count> void    BindGS(const ResourceList<SamplerState, Count>& samplerStates);
        template<int Count> void    BindCS(const ResourceList<SamplerState, Count>& samplerStates);
        template<int Count> void    BindHS(const ResourceList<SamplerState, Count>& samplerStates);
        template<int Count> void    BindDS(const ResourceList<SamplerState, Count>& samplerStates);

        template<int Count> void    BindVS(const ResourceList<ConstantBuffer, Count>& constantBuffers);
        template<int Count> void    BindPS(const ResourceList<ConstantBuffer, Count>& constantBuffers);
        template<int Count> void    BindCS(const ResourceList<ConstantBuffer, Count>& constantBuffers);
        template<int Count> void    BindGS(const ResourceList<ConstantBuffer, Count>& constantBuffers);
        template<int Count> void    BindHS(const ResourceList<ConstantBuffer, Count>& constantBuffers);
        template<int Count> void    BindDS(const ResourceList<ConstantBuffer, Count>& constantBuffers);

        template<int Count> void    BindVS_G(const ResourceList<ShaderResourceView, Count>& shaderResources);
        template<int Count> void    BindPS_G(const ResourceList<ShaderResourceView, Count>& shaderResources);
        template<int Count> void    BindCS_G(const ResourceList<ShaderResourceView, Count>& shaderResources);
        template<int Count> void    BindGS_G(const ResourceList<ShaderResourceView, Count>& shaderResources);
        template<int Count> void    BindHS_G(const ResourceList<ShaderResourceView, Count>& shaderResources);
        template<int Count> void    BindDS_G(const ResourceList<ShaderResourceView, Count>& shaderResources);

        template<int Count> void    BindVS_G(const ResourceList<SamplerState, Count>& samplerStates);
        template<int Count> void    BindPS_G(const ResourceList<SamplerState, Count>& samplerStates);
        template<int Count> void    BindGS_G(const ResourceList<SamplerState, Count>& samplerStates);
        template<int Count> void    BindCS_G(const ResourceList<SamplerState, Count>& samplerStates);
        template<int Count> void    BindHS_G(const ResourceList<SamplerState, Count>& samplerStates);
        template<int Count> void    BindDS_G(const ResourceList<SamplerState, Count>& samplerStates);

        template<int Count> void    BindVS_G(const ResourceList<ConstantBuffer, Count>& constantBuffers);
        template<int Count> void    BindPS_G(const ResourceList<ConstantBuffer, Count>& constantBuffers);
        template<int Count> void    BindCS_G(const ResourceList<ConstantBuffer, Count>& constantBuffers);
        template<int Count> void    BindGS_G(const ResourceList<ConstantBuffer, Count>& constantBuffers);
        template<int Count> void    BindHS_G(const ResourceList<ConstantBuffer, Count>& constantBuffers);
        template<int Count> void    BindDS_G(const ResourceList<ConstantBuffer, Count>& constantBuffers);

		template<int Count> void    BindCS(const ResourceList<UnorderedAccessView, Count>& unorderedAccess);

		template<int Count> void    Bind(const ResourceList<RenderTargetView, Count>& renderTargets, const DepthStencilView* depthStencil);
        template<int Count1, int Count2> void    Bind(const ResourceList<RenderTargetView, Count1>& renderTargets, const DepthStencilView* depthStencil, const ResourceList<UnorderedAccessView, Count2>& unorderedAccess) {}

		void        Bind(const Resource& ib, Format indexFormat, unsigned offset=0);
        void        Bind(const ViewportDesc& viewport);
        const ViewportDesc& GetBoundViewport() const { return _boundViewport; }

        using GraphicsPipelineBuilder::Bind;        // we need to expose the "Bind" functions in the base class, as well
        using ComputePipelineBuilder::Bind;

        T1(Type) void   UnbindVS(unsigned startSlot, unsigned count) {}
        T1(Type) void   UnbindGS(unsigned startSlot, unsigned count) {}
        T1(Type) void   UnbindPS(unsigned startSlot, unsigned count) {}
        T1(Type) void   UnbindCS(unsigned startSlot, unsigned count) {}
		T1(Type) void   UnbindDS(unsigned startSlot, unsigned count) {}
        T1(Type) void   Unbind() {}
        void            UnbindSO() {}

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

        static std::shared_ptr<DeviceContext> Get(IThreadContext& threadContext);
		std::shared_ptr<DeviceContext> Fork();

		void		BeginCommandList();
		void		BeginCommandList(CommandListPtr cmdList);
		void		CommitCommandList(VkCommandBuffer_T&, bool);
		auto        ResolveCommandList() -> CommandListPtr;
		bool		IsImmediate() { return false; }
		
		void		InvalidateCachedState() {}
		static void PrepareForDestruction(IDevice*, IPresentationChain*);

        enum class PipelineType { Graphics, Compute };
        void        BindDescriptorSet(PipelineType pipelineType, unsigned index, VkDescriptorSet set);
        PipelineLayout* GetPipelineLayout(PipelineType pipelineType);

        GlobalPools&    GetGlobalPools();
        VkDevice        GetUnderlyingDevice();
		ObjectFactory&	GetFactory() const				{ return *_factory; }
		TemporaryBufferSpace& GetTemporaryBufferSpace()		{ return *_tempBufferSpace; }

        void                        SetPresentationTarget(RenderTargetView* presentationTarget, const VectorPattern<unsigned,2>& dims);
        VectorPattern<unsigned,2>   GetPresentationTargetDims();

        void BeginRenderPass(
            const FrameBuffer& fb,
            TextureSamples samples,
            VectorPattern<int, 2> offset, VectorPattern<unsigned, 2> extent,
            IteratorRange<const ClearValue*> clearValues);
        void EndRenderPass();
        bool IsInRenderPass() const;

        ///////////// Command buffer layer /////////////
        //      (todo -- consider moving to a utility class)
        void CmdUpdateBuffer(
            VkBuffer buffer, VkDeviceSize offset, 
            VkDeviceSize byteCount, const void* data);
        void CmdBindDescriptorSets(
            VkPipelineBindPoint pipelineBindPoint,
            VkPipelineLayout layout,
            uint32_t firstSet,
            uint32_t descriptorSetCount,
            const VkDescriptorSet* pDescriptorSets,
            uint32_t dynamicOffsetCount,
            const uint32_t* pDynamicOffsets);
        void CmdCopyBuffer(
            VkBuffer srcBuffer,
            VkBuffer dstBuffer,
            uint32_t regionCount,
            const VkBufferCopy* pRegions);
        void CmdCopyImage(
            VkImage srcImage,
            VkImageLayout srcImageLayout,
            VkImage dstImage,
            VkImageLayout dstImageLayout,
            uint32_t regionCount,
            const VkImageCopy* pRegions);
        void CmdCopyBufferToImage(
            VkBuffer srcBuffer,
            VkImage dstImage,
            VkImageLayout dstImageLayout,
            uint32_t regionCount,
            const VkBufferImageCopy* pRegions);
        void CmdNextSubpass(VkSubpassContents);
        void CmdPipelineBarrier(
            VkPipelineStageFlags            srcStageMask,
            VkPipelineStageFlags            dstStageMask,
            VkDependencyFlags               dependencyFlags,
            uint32_t                        memoryBarrierCount,
            const VkMemoryBarrier*          pMemoryBarriers,
            uint32_t                        bufferMemoryBarrierCount,
            const VkBufferMemoryBarrier*    pBufferMemoryBarriers,
            uint32_t                        imageMemoryBarrierCount,
            const VkImageMemoryBarrier*     pImageMemoryBarriers);
        void CmdPushConstants(
            VkPipelineLayout layout,
            VkShaderStageFlags stageFlags,
            uint32_t offset,
            uint32_t size,
            const void* pValues);
		void CmdWriteTimestamp(
			VkPipelineStageFlagBits pipelineStage, 
			VkQueryPool queryPool, uint32_t query);
		void CmdResetQueryPool(
			VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount);
		void CmdSetEvent(VkEvent evnt, VkPipelineStageFlags stageMask);
		void CmdResetEvent(VkEvent evnt, VkPipelineStageFlags stageMask);
		void CmdBindVertexBuffers(
			uint32_t            firstBinding,
			uint32_t            bindingCount,
			const VkBuffer*     pBuffers,
			const VkDeviceSize*	pOffsets);

        DeviceContext(
            ObjectFactory& factory, 
            GlobalPools& globalPools,
            PipelineLayout& globalPipelineLayout,
            PipelineLayout& computePipelineLayout,
			CommandPool& cmdPool, 
			CommandBufferType cmdBufferType,
			TemporaryBufferSpace& tempBufferSpace);
		DeviceContext(const DeviceContext&) = delete;
		DeviceContext& operator=(const DeviceContext&) = delete;

    private:
        VulkanSharedPtr<VkCommandBuffer>    _commandList;
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

        VectorPattern<unsigned,2>           _presentationTargetDims;

        bool BindGraphicsPipeline();
        bool BindComputePipeline();
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<int Count> 
        void DeviceContext::Bind(
            const ResourceList<RenderTargetView, Count>& renderTargets, const DepthStencilView* depthStencil) 
        {
			assert(0);
        }

    template<int Count> 
        void    DeviceContext::BindVS(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
			auto r = MakeIteratorRange(shaderResources._buffers);
            _graphicsDescriptors._dynamicBindings.BindSRV(
                DescriptorSetBuilder::Stage::Vertex, 
                shaderResources._startingPoint,
                MakeIteratorRange((const TextureView*const*)r.begin(), (const TextureView*const*)r.end()));
        }

    template<int Count> 
        void    DeviceContext::BindPS(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
			auto r = MakeIteratorRange(shaderResources._buffers);
            _graphicsDescriptors._dynamicBindings.BindSRV(
                DescriptorSetBuilder::Stage::Pixel, 
                shaderResources._startingPoint,
                MakeIteratorRange((const TextureView*const*)r.begin(), (const TextureView*const*)r.end()));
        }

    template<int Count> 
        void    DeviceContext::BindCS(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
			auto r = MakeIteratorRange(shaderResources._buffers);
            _computeDescriptors._dynamicBindings.BindSRV(
                DescriptorSetBuilder::Stage::Compute, 
                shaderResources._startingPoint,
                MakeIteratorRange((const TextureView*const*)r.begin(), (const TextureView*const*)r.end()));
        }

    template<int Count> 
        void    DeviceContext::BindGS(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
			auto r = MakeIteratorRange(shaderResources._buffers);
            _graphicsDescriptors._dynamicBindings.BindSRV(
                DescriptorSetBuilder::Stage::Geometry, 
                shaderResources._startingPoint,
                MakeIteratorRange((const TextureView*const*)r.begin(), (const TextureView*const*)r.end()));
        }

    template<int Count> 
        void    DeviceContext::BindHS(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
			auto r = MakeIteratorRange(shaderResources._buffers);
            _graphicsDescriptors._dynamicBindings.BindSRV(
                DescriptorSetBuilder::Stage::Hull, 
                shaderResources._startingPoint,
                MakeIteratorRange((const TextureView*const*)r.begin(), (const TextureView*const*)r.end()));
        }

    template<int Count> 
        void    DeviceContext::BindDS(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
			auto r = MakeIteratorRange(shaderResources._buffers);
            _graphicsDescriptors._dynamicBindings.BindSRV(
                DescriptorSetBuilder::Stage::Domain, 
                shaderResources._startingPoint,
                MakeIteratorRange((const TextureView*const*)r.begin(), (const TextureView*const*)r.end()));
        }

    template<int Count> void    DeviceContext::BindVS(const ResourceList<SamplerState, Count>& samplerStates) 
        {
			VkSampler samplers[Count];
			for (unsigned c=0; c<Count; ++c)
				samplers[c] = samplerStates._buffers[c]->GetUnderlying();
            _graphicsDescriptors._dynamicBindings.Bind(
                DescriptorSetBuilder::Stage::Vertex, 
                samplerStates._startingPoint,
                MakeIteratorRange(samplers));
        }

    template<int Count> void    DeviceContext::BindPS(const ResourceList<SamplerState, Count>& samplerStates)
        {
			VkSampler samplers[Count];
			for (unsigned c=0; c<Count; ++c)
				samplers[c] = samplerStates._buffers[c]->GetUnderlying();
            _graphicsDescriptors._dynamicBindings.Bind(
                DescriptorSetBuilder::Stage::Pixel, 
                samplerStates._startingPoint,
                MakeIteratorRange(samplers));
        }

    template<int Count> void    DeviceContext::BindGS(const ResourceList<SamplerState, Count>& samplerStates)
        {
			VkSampler samplers[Count];
			for (unsigned c=0; c<Count; ++c)
				samplers[c] = samplerStates._buffers[c]->GetUnderlying();
            _graphicsDescriptors._dynamicBindings.Bind(
                DescriptorSetBuilder::Stage::Geometry, 
                samplerStates._startingPoint,
                MakeIteratorRange(samplers));
        }

    template<int Count> void    DeviceContext::BindCS(const ResourceList<SamplerState, Count>& samplerStates)
        {
			VkSampler samplers[Count];
			for (unsigned c=0; c<Count; ++c)
				samplers[c] = samplerStates._buffers[c]->GetUnderlying();
            _computeDescriptors._dynamicBindings.Bind(
                DescriptorSetBuilder::Stage::Compute, 
                samplerStates._startingPoint,
                MakeIteratorRange(samplers));
        }

    template<int Count> void    DeviceContext::BindHS(const ResourceList<SamplerState, Count>& samplerStates)
        {
			VkSampler samplers[Count];
			for (unsigned c=0; c<Count; ++c)
				samplers[c] = samplerStates._buffers[c]->GetUnderlying();
            _graphicsDescriptors._dynamicBindings.Bind(
                DescriptorSetBuilder::Stage::Hull, 
                samplerStates._startingPoint,
                MakeIteratorRange(samplers));
        }

    template<int Count> void    DeviceContext::BindDS(const ResourceList<SamplerState, Count>& samplerStates)
        {
			VkSampler samplers[Count];
			for (unsigned c=0; c<Count; ++c)
				samplers[c] = samplerStates._buffers[c]->GetUnderlying();
            _graphicsDescriptors._dynamicBindings.Bind(
                DescriptorSetBuilder::Stage::Domain, 
                samplerStates._startingPoint,
                MakeIteratorRange(samplers));
        }

    template<int Count> 
        void    DeviceContext::BindVS(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
			VkBuffer buffers[Count];
			for (unsigned c=0; c<Count; ++c)
				buffers[c] = constantBuffers._buffers[c]->GetBuffer();
            _graphicsDescriptors._dynamicBindings.Bind(
                DescriptorSetBuilder::Stage::Vertex, 
                constantBuffers._startingPoint,
                MakeIteratorRange(buffers));
        }

    template<int Count> 
        void    DeviceContext::BindPS(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
			VkBuffer buffers[Count];
			for (unsigned c=0; c<Count; ++c)
				buffers[c] = constantBuffers._buffers[c]->GetBuffer();
            _graphicsDescriptors._dynamicBindings.Bind(
                DescriptorSetBuilder::Stage::Pixel, 
                constantBuffers._startingPoint,
                MakeIteratorRange(buffers));
        }

    template<int Count> 
        void    DeviceContext::BindCS(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
			VkBuffer buffers[Count];
			for (unsigned c=0; c<Count; ++c)
				buffers[c] = constantBuffers._buffers[c]->GetBuffer();
            _computeDescriptors._dynamicBindings.Bind(
                DescriptorSetBuilder::Stage::Compute, 
                constantBuffers._startingPoint,
                MakeIteratorRange(buffers));
        }

    template<int Count> 
        void    DeviceContext::BindGS(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
			VkBuffer buffers[Count];
			for (unsigned c=0; c<Count; ++c)
				buffers[c] = constantBuffers._buffers[c]->GetBuffer();
            _graphicsDescriptors._dynamicBindings.Bind(
                DescriptorSetBuilder::Stage::Geometry, 
                constantBuffers._startingPoint,
                MakeIteratorRange(buffers));
        }

    template<int Count> 
        void    DeviceContext::BindHS(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
			VkBuffer buffers[Count];
			for (unsigned c=0; c<Count; ++c)
				buffers[c] = constantBuffers._buffers[c]->GetBuffer();
            _graphicsDescriptors._dynamicBindings.Bind(
                DescriptorSetBuilder::Stage::Hull, 
                constantBuffers._startingPoint,
                MakeIteratorRange(buffers));
        }

    template<int Count> 
        void    DeviceContext::BindDS(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
			VkBuffer buffers[Count];
			for (unsigned c=0; c<Count; ++c)
				buffers[c] = constantBuffers._buffers[c]->GetBuffer();
            _graphicsDescriptors._dynamicBindings.Bind(
                DescriptorSetBuilder::Stage::Domain, 
                constantBuffers._startingPoint,
                MakeIteratorRange(buffers));
        }


///////////////////////////////////////////////////////////////////////////////////////////////////

    template<int Count> 
        void    DeviceContext::BindVS_G(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
			auto r = MakeIteratorRange(shaderResources._buffers);
            _graphicsDescriptors._globalBindings.BindSRV(
                DescriptorSetBuilder::Stage::Vertex, 
                shaderResources._startingPoint,
                MakeIteratorRange((const TextureView*const*)r.begin(), (const TextureView*const*)r.end()));
        }

    template<int Count> 
        void    DeviceContext::BindPS_G(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
			auto r = MakeIteratorRange(shaderResources._buffers);
            _graphicsDescriptors._globalBindings.BindSRV(
                DescriptorSetBuilder::Stage::Pixel, 
                shaderResources._startingPoint,
                MakeIteratorRange((const TextureView*const*)r.begin(), (const TextureView*const*)r.end()));
        }

    template<int Count> 
        void    DeviceContext::BindCS_G(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
			auto r = MakeIteratorRange(shaderResources._buffers);
            _computeDescriptors._globalBindings.BindSRV(
                DescriptorSetBuilder::Stage::Compute, 
                shaderResources._startingPoint,
                MakeIteratorRange((const TextureView*const*)r.begin(), (const TextureView*const*)r.end()));
        }

    template<int Count> 
        void    DeviceContext::BindGS_G(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
			auto r = MakeIteratorRange(shaderResources._buffers);
            _graphicsDescriptors._globalBindings.BindSRV(
                DescriptorSetBuilder::Stage::Geometry, 
                shaderResources._startingPoint,
                MakeIteratorRange((const TextureView*const*)r.begin(), (const TextureView*const*)r.end()));
        }

    template<int Count> 
        void    DeviceContext::BindHS_G(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
			auto r = MakeIteratorRange(shaderResources._buffers);
            _graphicsDescriptors._globalBindings.BindSRV(
                DescriptorSetBuilder::Stage::Hull, 
                shaderResources._startingPoint,
                MakeIteratorRange((const TextureView*const*)r.begin(), (const TextureView*const*)r.end()));
        }

    template<int Count> 
        void    DeviceContext::BindDS_G(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
			auto r = MakeIteratorRange(shaderResources._buffers);
            _graphicsDescriptors._globalBindings.BindSRV(
                DescriptorSetBuilder::Stage::Domain, 
                shaderResources._startingPoint,
                MakeIteratorRange((const TextureView*const*)r.begin(), (const TextureView*const*)r.end()));
        }

    template<int Count> void    DeviceContext::BindVS_G(const ResourceList<SamplerState, Count>& samplerStates) 
        {
			VkSampler samplers[Count];
			for (unsigned c=0; c<Count; ++c)
				samplers[c] = samplerStates._buffers[c]->GetUnderlying();
            _graphicsDescriptors._globalBindings.Bind(
                DescriptorSetBuilder::Stage::Vertex, 
                samplerStates._startingPoint,
                MakeIteratorRange(samplers));
        }

    template<int Count> void    DeviceContext::BindPS_G(const ResourceList<SamplerState, Count>& samplerStates)
        {
			VkSampler samplers[Count];
			for (unsigned c=0; c<Count; ++c)
				samplers[c] = samplerStates._buffers[c]->GetUnderlying();
            _graphicsDescriptors._globalBindings.Bind(
                DescriptorSetBuilder::Stage::Pixel, 
                samplerStates._startingPoint,
                MakeIteratorRange(samplers));
        }

    template<int Count> void    DeviceContext::BindGS_G(const ResourceList<SamplerState, Count>& samplerStates)
        {
            VkSampler samplers[Count];
			for (unsigned c=0; c<Count; ++c)
				samplers[c] = samplerStates._buffers[c]->GetUnderlying();
			_graphicsDescriptors._globalBindings.Bind(
                DescriptorSetBuilder::Stage::Geometry, 
                samplerStates._startingPoint,
                MakeIteratorRange(samplers));
        }

    template<int Count> void    DeviceContext::BindCS_G(const ResourceList<SamplerState, Count>& samplerStates)
        {
            VkSampler samplers[Count];
			for (unsigned c=0; c<Count; ++c)
				samplers[c] = samplerStates._buffers[c]->GetUnderlying();
			_computeDescriptors._globalBindings.Bind(
                DescriptorSetBuilder::Stage::Compute, 
                samplerStates._startingPoint,
                MakeIteratorRange(samplers));
        }

    template<int Count> void    DeviceContext::BindHS_G(const ResourceList<SamplerState, Count>& samplerStates)
        {
            VkSampler samplers[Count];
			for (unsigned c=0; c<Count; ++c)
				samplers[c] = samplerStates._buffers[c]->GetUnderlying();
			_graphicsDescriptors._globalBindings.Bind(
                DescriptorSetBuilder::Stage::Hull, 
                samplerStates._startingPoint,
                MakeIteratorRange(samplers));
        }

    template<int Count> void    DeviceContext::BindDS_G(const ResourceList<SamplerState, Count>& samplerStates)
        {
            VkSampler samplers[Count];
			for (unsigned c=0; c<Count; ++c)
				samplers[c] = samplerStates._buffers[c]->GetUnderlying();
			_graphicsDescriptors._globalBindings.Bind(
                DescriptorSetBuilder::Stage::Domain, 
                samplerStates._startingPoint,
                MakeIteratorRange(samplers));
        }

    template<int Count> 
        void    DeviceContext::BindVS_G(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
            VkBuffer buffers[Count];
			for (unsigned c=0; c<Count; ++c)
				buffers[c] = constantBuffers._buffers[c]->GetBuffer();
			_graphicsDescriptors._globalBindings.Bind(
                DescriptorSetBuilder::Stage::Vertex, 
                constantBuffers._startingPoint,
                MakeIteratorRange(buffers));
        }

    template<int Count> 
        void    DeviceContext::BindPS_G(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
            VkBuffer buffers[Count];
			for (unsigned c=0; c<Count; ++c)
				buffers[c] = constantBuffers._buffers[c]->GetBuffer();
			_graphicsDescriptors._globalBindings.Bind(
                DescriptorSetBuilder::Stage::Pixel, 
                constantBuffers._startingPoint,
                MakeIteratorRange(buffers));
        }

    template<int Count> 
        void    DeviceContext::BindCS_G(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
            VkBuffer buffers[Count];
			for (unsigned c=0; c<Count; ++c)
				buffers[c] = constantBuffers._buffers[c]->GetBuffer();
			_computeDescriptors._globalBindings.Bind(
                DescriptorSetBuilder::Stage::Compute, 
                constantBuffers._startingPoint,
                MakeIteratorRange(buffers));
        }

    template<int Count> 
        void    DeviceContext::BindGS_G(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
            VkBuffer buffers[Count];
			for (unsigned c=0; c<Count; ++c)
				buffers[c] = constantBuffers._buffers[c]->GetBuffer();
			_graphicsDescriptors._globalBindings.Bind(
                DescriptorSetBuilder::Stage::Geometry, 
                constantBuffers._startingPoint,
                MakeIteratorRange(buffers));
        }

    template<int Count> 
        void    DeviceContext::BindHS_G(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
            VkBuffer buffers[Count];
			for (unsigned c=0; c<Count; ++c)
				buffers[c] = constantBuffers._buffers[c]->GetBuffer();
			_graphicsDescriptors._globalBindings.Bind(
                DescriptorSetBuilder::Stage::Hull, 
                constantBuffers._startingPoint,
                MakeIteratorRange(buffers));
        }

    template<int Count> 
        void    DeviceContext::BindDS_G(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
            VkBuffer buffers[Count];
			for (unsigned c=0; c<Count; ++c)
				buffers[c] = constantBuffers._buffers[c]->GetBuffer();
			_graphicsDescriptors._globalBindings.Bind(
                DescriptorSetBuilder::Stage::Domain, 
                constantBuffers._startingPoint,
                MakeIteratorRange(buffers));
        }

    template<int Count> 
        void    DeviceContext::BindCS(const ResourceList<UnorderedAccessView, Count>& unorderedAccess)
        {
            _computeDescriptors._dynamicBindings.BindUAV(
                DescriptorSetBuilder::Stage::Compute, 
                unorderedAccess._startingPoint,
                MakeIteratorRange(unorderedAccess._buffers));
        }


}}

