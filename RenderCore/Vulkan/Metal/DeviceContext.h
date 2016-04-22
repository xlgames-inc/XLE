// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "State.h"
#include "Forward.h"
#include "Pools.h"
#include "FrameBuffer.h"        // for NamedResources
#include "VulkanCore.h"
#include "IncludeVulkan.h"
#include "../../ResourceList.h"
#include "../../IDevice_Forward.h"
#include "../../IThreadContext_Forward.h"
#include "../../Utility/IteratorUtils.h"
#include <memory>
#include <sstream>

namespace RenderCore { enum class Format; }

namespace RenderCore { namespace Metal_Vulkan
{
    static const unsigned s_maxBoundVBs = 4;

    class GlobalPools;
    class FrameBuffer;

    /// Container for Topology::Enum
    namespace Topology
    {
        enum Enum
        {
            PointList       = 1,    // D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
            LineList        = 2,    // D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
            LineStrip       = 3,    // D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
            TriangleList    = 4,    // D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
            TriangleStrip   = 5,    // D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
            LineListAdj     = 10,   // D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ


            PatchList1 = 33,        // D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST	= 33,
	        PatchList2 = 34,        // D3D11_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST	= 34,
	        PatchList3 = 35,        // D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST	= 35,
	        PatchList4 = 36,        // D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST	= 36,
	        PatchList5 = 37,        // D3D11_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST	= 37,
	        PatchList6 = 38,        // D3D11_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST	= 38,
	        PatchList7 = 39,        // D3D11_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST	= 39,
	        PatchList8 = 40,        // D3D11_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST	= 40,
	        PatchList9 = 41,        // D3D11_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST	= 41,
	        PatchList10 = 42,       // D3D11_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST	= 42,
	        PatchList11 = 43,       // D3D11_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST	= 43,
	        PatchList12 = 44,       // D3D11_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST	= 44,
	        PatchList13 = 45,       // D3D11_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST	= 45,
	        PatchList14 = 46,       // D3D11_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST	= 46,
	        PatchList15 = 47,       // D3D11_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST	= 47,
	        PatchList16 = 48        // D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST	= 48
        };
    }

    class PipelineBuilder
    {
    public:
        void        Bind(const RasterizerState& rasterizer);
        void        Bind(const BlendState& blendState);
        void        Bind(const DepthStencilState& depthStencilState, unsigned stencilRef = 0x0);
        
        void        Bind(const BoundInputLayout& inputLayout);
        void        Bind(const ShaderProgram& shaderProgram);

        void        Bind(Topology::Enum topology);
        void        SetVertexStrides(unsigned first, std::initializer_list<unsigned> vertexStrides);

        VulkanUniquePtr<VkPipeline> CreatePipeline(VkRenderPass renderPass, unsigned subpass, TextureSamples samples);

        void                SetPipelineLayout(const VulkanSharedPtr<VkPipelineLayout>& layout);
        VkPipelineLayout    GetPipelineLayout();

        PipelineBuilder(const ObjectFactory& factory, GlobalPools& globalPools);
        ~PipelineBuilder();

        PipelineBuilder(const PipelineBuilder&) = delete;
        PipelineBuilder& operator=(const PipelineBuilder&) = delete;
    protected:
        RasterizerState         _rasterizerState;
        BlendState              _blendState;
        DepthStencilState       _depthStencilState;
        VkPrimitiveTopology     _topology;

        const BoundInputLayout* _inputLayout;       // note -- unprotected pointer
        const ShaderProgram*    _shaderProgram;

        VulkanSharedPtr<VkPipelineLayout> _pipelineLayout;

        const ObjectFactory*    _factory;
        GlobalPools*            _globalPools;
        unsigned                _vertexStrides[s_maxBoundVBs];

        bool            _pipelineStale;
    };

    class DescriptorSetBuilder
    {
    public:
        enum class Stage { Vertex, Pixel, Geometry, Compute, Hull, Domain, Max };
        void    Bind(Stage stage, unsigned startingPoint, IteratorRange<const VkImageView*> images);
        void    Bind(Stage stage, unsigned startingPoint, IteratorRange<const VkBuffer*> uniformBuffers);

        VkPipelineLayout                            GetPipelineLayout();
        const VulkanSharedPtr<VkPipelineLayout>&    SharePipelineLayout();

        void    GetDescriptorSets(IteratorRange<VkDescriptorSet*> dst);
        bool    HasChanges() const;
        void    Reset();

        DescriptorSetBuilder(
            const ObjectFactory& factory, DescriptorPool& descPool, 
            DummyResources& dummyResources);
        ~DescriptorSetBuilder();

        DescriptorSetBuilder(const DescriptorSetBuilder&) = delete;
        DescriptorSetBuilder& operator=(const DescriptorSetBuilder&) = delete;
    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    using CommandList = VkCommandBuffer;
    using CommandListPtr = VulkanSharedPtr<VkCommandBuffer>;

	class DeviceContext : public PipelineBuilder
    {
    public:
		template<int Count> void    Bind(const ResourceList<VertexBuffer, Count>& VBs, unsigned stride, unsigned offset=0);

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

		template<int Count> void    BindCS(const ResourceList<UnorderedAccessView, Count>& unorderedAccess) {}

		template<int Count> void    BindSO(const ResourceList<VertexBuffer, Count>& buffers, unsigned offset=0) {}

		template<int Count> void    Bind(const ResourceList<RenderTargetView, Count>& renderTargets, const DepthStencilView* depthStencil);
        template<int Count1, int Count2> void    Bind(const ResourceList<RenderTargetView, Count1>& renderTargets, const DepthStencilView* depthStencil, const ResourceList<UnorderedAccessView, Count2>& unorderedAccess) {}

		void        Bind(unsigned startSlot, unsigned bufferCount, const VertexBuffer* VBs[], const unsigned strides[], const unsigned offsets[]);
        void        Bind(const IndexBuffer& ib, Format indexFormat, unsigned offset=0);
        void        Bind(const ViewportDesc& viewport);

        void        Bind(const ComputeShader& computeShader) {}

        void        Bind(const DeepShaderProgram& deepShaderProgram) {}
        void        Bind(const ShaderProgram& shaderProgram, const BoundClassInterfaces& dynLinkage) {}
        void        Bind(const DeepShaderProgram& deepShaderProgram, const BoundClassInterfaces& dynLinkage) {}

        using PipelineBuilder::Bind;        // we need to expose the "Bind" functions in the base class, as well

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
        void        Clear(const DepthStencilView& depthStencil, float depth, unsigned stencil) {}
        void        ClearUInt(const UnorderedAccessView& unorderedAccess, const VectorPattern<unsigned,4>& clearColour) {}
        void        ClearFloat(const UnorderedAccessView& unorderedAccess, const VectorPattern<float,4>& clearColour) {}
        void        ClearStencil(const DepthStencilView& depthStencil, unsigned stencil) {}

        static std::shared_ptr<DeviceContext> Get(IThreadContext& threadContext);

		void		BeginCommandList();
		void		CommitCommandList(VkCommandBuffer_T&, bool);
		auto        ResolveCommandList() -> CommandListPtr;
		bool		IsImmediate() { return false; }
		
		void		InvalidateCachedState() {}
		static void PrepareForDestruction(IDevice*, IPresentationChain*);
        void        SetHideDescriptorSetBuilder() { _hideDescriptorSetBuilder = true; }

        GlobalPools&    GetGlobalPools();
        VkDevice        GetUnderlyingDevice();
		const ObjectFactory& GetFactory() const { return *_factory; }

        bool        BindPipeline();

        void                        SetPresentationTarget(RenderTargetView* presentationTarget, const VectorPattern<unsigned,2>& dims);
        VectorPattern<unsigned,2>   GetPresentationTargetDims();

        NamedResources& GetNamedResources() { return _namedResources; }

        void BeginRenderPass(
            VkRenderPass fbLayout, const FrameBuffer& fb,
            TextureSamples samples,
            VectorPattern<int, 2> offset, VectorPattern<unsigned, 2> extent,
            IteratorRange<const ClearValue*> clearValues);
        void EndRenderPass();
        bool IsInRenderPass() const;
        void SetImageLayout(
		    VkImage image,
		    VkImageAspectFlags aspectMask,
		    VkImageLayout oldImageLayout,
		    VkImageLayout newImageLayout,
            unsigned mipCount,
            unsigned layerCount);

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

        DeviceContext(
            const ObjectFactory& factory, 
            GlobalPools& globalPools,
			CommandPool& cmdPool, 
            CommandPool::BufferType cmdBufferType);
		DeviceContext(const DeviceContext&) = delete;
		DeviceContext& operator=(const DeviceContext&) = delete;

    private:
        VulkanSharedPtr<VkCommandBuffer>    _commandList;

        VkRenderPass                        _renderPass;
        TextureSamples                      _renderPassSamples;
        unsigned                            _renderPassSubpass;

        CommandPool*                        _cmdPool;
        CommandPool::BufferType             _cmdBufferType;
        DescriptorSetBuilder                _descriptorSetBuilder;
        bool                                _hideDescriptorSetBuilder;

        NamedResources                      _namedResources;
        VectorPattern<unsigned,2>           _presentationTargetDims;
    };

    void SetImageLayout(
        VkCommandBuffer commandBuffer,
		VkImage image,
		VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
        unsigned mipCount,
        unsigned layerCount);


    template<int Count> 
        void DeviceContext::Bind(
            const ResourceList<RenderTargetView, Count>& renderTargets, const DepthStencilView* depthStencil) 
        {
        }

    template<int Count> 
        void    DeviceContext::BindVS(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
            _hideDescriptorSetBuilder = false;
            _descriptorSetBuilder.Bind(
                DescriptorSetBuilder::Stage::Vertex, 
                shaderResources._startingPoint,
                MakeIteratorRange(shaderResources._buffers));
        }

    template<int Count> 
        void    DeviceContext::BindPS(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
            _hideDescriptorSetBuilder = false;
            _descriptorSetBuilder.Bind(
                DescriptorSetBuilder::Stage::Pixel, 
                shaderResources._startingPoint,
                MakeIteratorRange(shaderResources._buffers));
        }

    template<int Count> 
        void    DeviceContext::BindCS(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
            _hideDescriptorSetBuilder = false;
            _descriptorSetBuilder.Bind(
                DescriptorSetBuilder::Stage::Compute, 
                shaderResources._startingPoint,
                MakeIteratorRange(shaderResources._buffers));
        }

    template<int Count> 
        void    DeviceContext::BindGS(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
            _hideDescriptorSetBuilder = false;
            _descriptorSetBuilder.Bind(
                DescriptorSetBuilder::Stage::Geometry, 
                shaderResources._startingPoint,
                MakeIteratorRange(shaderResources._buffers));
        }

    template<int Count> 
        void    DeviceContext::BindHS(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
            _hideDescriptorSetBuilder = false;
            _descriptorSetBuilder.Bind(
                DescriptorSetBuilder::Stage::Hull, 
                shaderResources._startingPoint,
                MakeIteratorRange(shaderResources._buffers));
        }

    template<int Count> 
        void    DeviceContext::BindDS(const ResourceList<ShaderResourceView, Count>& shaderResources) 
        {
            _hideDescriptorSetBuilder = false;
            _descriptorSetBuilder.Bind(
                DescriptorSetBuilder::Stage::Domain, 
                shaderResources._startingPoint,
                MakeIteratorRange(shaderResources._buffers));
        }

    template<int Count> void    DeviceContext::BindVS(const ResourceList<SamplerState, Count>& samplerStates) {}
    template<int Count> void    DeviceContext::BindPS(const ResourceList<SamplerState, Count>& samplerStates) {}
    template<int Count> void    DeviceContext::BindGS(const ResourceList<SamplerState, Count>& samplerStates) {}
    template<int Count> void    DeviceContext::BindCS(const ResourceList<SamplerState, Count>& samplerStates) {}
    template<int Count> void    DeviceContext::BindHS(const ResourceList<SamplerState, Count>& samplerStates) {}
    template<int Count> void    DeviceContext::BindDS(const ResourceList<SamplerState, Count>& samplerStates) {}

    template<int Count> 
        void    DeviceContext::BindVS(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
            _hideDescriptorSetBuilder = false;
            _descriptorSetBuilder.Bind(
                DescriptorSetBuilder::Stage::Vertex, 
                constantBuffers._startingPoint,
                MakeIteratorRange(constantBuffers._buffers));
        }

    template<int Count> 
        void    DeviceContext::BindPS(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
            _hideDescriptorSetBuilder = false;
            _descriptorSetBuilder.Bind(
                DescriptorSetBuilder::Stage::Pixel, 
                constantBuffers._startingPoint,
                MakeIteratorRange(constantBuffers._buffers));
        }

    template<int Count> 
        void    DeviceContext::BindCS(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
            _hideDescriptorSetBuilder = false;
            _descriptorSetBuilder.Bind(
                DescriptorSetBuilder::Stage::Compute, 
                constantBuffers._startingPoint,
                MakeIteratorRange(constantBuffers._buffers));
        }

    template<int Count> 
        void    DeviceContext::BindGS(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
            _hideDescriptorSetBuilder = false;
            _descriptorSetBuilder.Bind(
                DescriptorSetBuilder::Stage::Geometry, 
                constantBuffers._startingPoint,
                MakeIteratorRange(constantBuffers._buffers));
        }

    template<int Count> 
        void    DeviceContext::BindHS(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
            _hideDescriptorSetBuilder = false;
            _descriptorSetBuilder.Bind(
                DescriptorSetBuilder::Stage::Hull, 
                constantBuffers._startingPoint,
                MakeIteratorRange(constantBuffers._buffers));
        }

    template<int Count> 
        void    DeviceContext::BindDS(const ResourceList<ConstantBuffer, Count>& constantBuffers) 
        {
            _hideDescriptorSetBuilder = false;
            _descriptorSetBuilder.Bind(
                DescriptorSetBuilder::Stage::Domain, 
                constantBuffers._startingPoint,
                MakeIteratorRange(constantBuffers._buffers));
        }

}}

