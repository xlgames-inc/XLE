// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "State.h"
#include "Forward.h"
#include "VulkanCore.h"
#include "../../Resource.h"
#include "../../IDevice_Forward.h"
#include "../../IThreadContext_Forward.h"
#include "../../Math/Vector.h"  // for Float4 in Clear()
#include <memory>

namespace RenderCore { namespace Metal_Vulkan
{
    static const unsigned s_maxBoundVBs = 4;

    class GlobalPools;

    /// Container for Topology::Enum
    namespace Topology
    {
        enum Enum
        {
            PointList       = 0,    // VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
            LineList        = 1,    // VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
            LineStrip       = 2,    // VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
            TriangleList    = 3,    // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            TriangleStrip   = 4,    // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
            LineListAdj     = 6,    // VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY


            PatchList1      = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList2      = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList3      = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList4      = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList5      = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList6      = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList7      = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList8      = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList9      = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList10     = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList11     = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList12     = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList13     = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList14     = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList15     = 10,   // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	        PatchList16     = 10    // VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
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
        void        SetVertexStrides(std::initializer_list<unsigned> vertexStrides);

        VulkanUniquePtr<VkPipeline> CreatePipeline(VkRenderPass renderPass, unsigned subpass = 0);

		void				SetPipelineLayout(VulkanSharedPtr<VkPipelineLayout> layout) { _pipelineLayout = std::move(layout); }
        VkPipelineLayout    GetPipelineLayout() { return _pipelineLayout.get(); }

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
    };

    using CommandList = VkCommandBuffer;
    using CommandListPtr = VulkanSharedPtr<VkCommandBuffer>;

	class DeviceContext : public PipelineBuilder
    {
    public:
		template<int Count> void    Bind(const ResourceList<VertexBuffer, Count>& VBs, unsigned stride, unsigned offset=0);

        template<int Count> void    BindVS(const ResourceList<ShaderResourceView, Count>& shaderResources) {}
        template<int Count> void    BindPS(const ResourceList<ShaderResourceView, Count>& shaderResources) {}
        template<int Count> void    BindCS(const ResourceList<ShaderResourceView, Count>& shaderResources) {}
        template<int Count> void    BindGS(const ResourceList<ShaderResourceView, Count>& shaderResources) {}
        template<int Count> void    BindHS(const ResourceList<ShaderResourceView, Count>& shaderResources) {}
        template<int Count> void    BindDS(const ResourceList<ShaderResourceView, Count>& shaderResources) {}

        template<int Count> void    BindVS(const ResourceList<SamplerState, Count>& samplerStates) {}
        template<int Count> void    BindPS(const ResourceList<SamplerState, Count>& samplerStates) {}
        template<int Count> void    BindGS(const ResourceList<SamplerState, Count>& samplerStates) {}
        template<int Count> void    BindCS(const ResourceList<SamplerState, Count>& samplerStates) {}
        template<int Count> void    BindHS(const ResourceList<SamplerState, Count>& samplerStates) {}
        template<int Count> void    BindDS(const ResourceList<SamplerState, Count>& samplerStates) {}

        template<int Count> void    BindVS(const ResourceList<ConstantBuffer, Count>& constantBuffers) {}
        template<int Count> void    BindPS(const ResourceList<ConstantBuffer, Count>& constantBuffers) {}
        template<int Count> void    BindCS(const ResourceList<ConstantBuffer, Count>& constantBuffers) {}
        template<int Count> void    BindGS(const ResourceList<ConstantBuffer, Count>& constantBuffers) {}
        template<int Count> void    BindHS(const ResourceList<ConstantBuffer, Count>& constantBuffers) {}
        template<int Count> void    BindDS(const ResourceList<ConstantBuffer, Count>& constantBuffers) {}

		template<int Count> void    Bind(const ResourceList<RenderTargetView, Count>& renderTargets, const DepthStencilView* depthStencil) {}
		template<int Count> void    BindCS(const ResourceList<UnorderedAccessView, Count>& unorderedAccess) {}

		template<int Count> void    BindSO(const ResourceList<VertexBuffer, Count>& buffers, unsigned offset=0) {}

		template<int Count1, int Count2> void    Bind(const ResourceList<RenderTargetView, Count1>& renderTargets, const DepthStencilView* depthStencil, const ResourceList<UnorderedAccessView, Count2>& unorderedAccess) {}

		void        Bind(unsigned startSlot, unsigned bufferCount, const VertexBuffer* VBs[], const unsigned strides[], const unsigned offsets[]) {}
        void        Bind(const IndexBuffer& ib, NativeFormat::Enum indexFormat, unsigned offset=0) {}
        void        Bind(const VertexShader& vertexShader) {}
        void        Bind(const GeometryShader& geometryShader) {}
        void        Bind(const PixelShader& pixelShader) {}
        void        Bind(const ComputeShader& computeShader) {}
        void        Bind(const DomainShader& domainShader) {}
        void        Bind(const HullShader& hullShader) {}
        void        Bind(const DeepShaderProgram& deepShaderProgram) {}
        void        Bind(const ViewportDesc& viewport);

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

        void        Clear(const RenderTargetView& renderTargets, const Float4& clearColour) {}
        void        Clear(const DepthStencilView& depthStencil, float depth, unsigned stencil) {}
        void        Clear(const UnorderedAccessView& unorderedAccess, unsigned values[4]) {}
        void        Clear(const UnorderedAccessView& unorderedAccess, float values[4]) {}
        void        ClearStencil(const DepthStencilView& depthStencil, unsigned stencil) {}

        static std::shared_ptr<DeviceContext> Get(IThreadContext& threadContext);

		void		BeginCommandList();
		void		CommitCommandList(VkCommandBuffer_T&, bool);
		auto        ResolveCommandList() -> CommandListPtr;

		VkCommandBuffer			GetCommandList() { assert(_commandList); return _commandList.get(); }
        const CommandListPtr&   ShareCommandList() { assert(_commandList); return _commandList; }

        void        Bind(VulkanSharedPtr<VkRenderPass> renderPass);
        void        BindPipeline();

        GlobalPools&    GetGlobalPools();
        VkDevice        GetUnderlyingDevice();
		const ObjectFactory& GetFactory() const { return *_factory; }

        DeviceContext(
            const ObjectFactory& factory, 
            GlobalPools& globalPools,
			VulkanSharedPtr<VkCommandBuffer> cmdList = nullptr);
		DeviceContext(const DeviceContext&) = delete;
		DeviceContext& operator=(const DeviceContext&) = delete;

    private:
        VulkanSharedPtr<VkCommandBuffer> _commandList;
        VulkanSharedPtr<VkRenderPass> _renderPass;
    };
}}

