// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TextureView.h"
#include "FrameBuffer.h"        // for NamedResources
#include "DX11.h"
#include "../../IDevice_Forward.h"
#include "../../IThreadContext_Forward.h"
#include "../../ResourceList.h"
#include "../../../Utility/Mixins.h"
#include "../../../Utility/Threading/ThreadingUtils.h"
#include "../../../Utility/IntrusivePtr.h"

namespace RenderCore { namespace Metal_DX11
{
    class VertexBuffer;
    class IndexBuffer;
    class ShaderResourceView;
    class SamplerState;
    class ConstantBuffer;
    class BoundInputLayout;
    class VertexShader;
    class GeometryShader;
    class PixelShader;
    class ComputeShader;
    class DomainShader;
    class HullShader;
    class ShaderProgram;
    class DeepShaderProgram;
    class RasterizerState;
    class BlendState;
    class DepthStencilState;
    class DepthStencilView;
    class RenderTargetView;
    class ViewportDesc;
    class BoundClassInterfaces;
	class ObjectFactory;

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

        //  todo ---    DeviceContext, ObjectFactory & CommandList -- maybe these
        //              should go into RenderCore (because it's impossible to do anything without them)

    class CommandList : public RefCountedObject, noncopyable
    {
    public:
        CommandList(ID3D::CommandList* underlying);
        CommandList(intrusive_ptr<ID3D::CommandList>&& underlying);
        ID3D::CommandList* GetUnderlying() { return _underlying.get(); }
    private:
        intrusive_ptr<ID3D::CommandList> _underlying;
    };

    using CommandListPtr = intrusive_ptr<CommandList>;

    class DeviceContext : noncopyable
    {
    public:
        template<int Count> void    Bind(const ResourceList<VertexBuffer, Count>& VBs, unsigned stride, unsigned offset=0);

        ///////////////////////////////////////////////////////////////////////////////////////////
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
        ///////////////////////////////////////////////////////////////////////////////////////////
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
        ///////////////////////////////////////////////////////////////////////////////////////////

        template<int Count> void    Bind(const ResourceList<RenderTargetView, Count>& renderTargets, const DepthStencilView* depthStencil);
        template<int Count> void    BindCS(const ResourceList<UnorderedAccessView, Count>& unorderedAccess);

        template<int Count> void    BindSO(const ResourceList<VertexBuffer, Count>& buffers, unsigned offset=0);

        template<int Count1, int Count2> void    Bind(const ResourceList<RenderTargetView, Count1>& renderTargets, const DepthStencilView* depthStencil, const ResourceList<UnorderedAccessView, Count2>& unorderedAccess);

        void        Bind(unsigned startSlot, unsigned bufferCount, const VertexBuffer* VBs[], const unsigned strides[], const unsigned offsets[]);
        void        Bind(const IndexBuffer& ib, Format indexFormat, unsigned offset=0);
        void        Bind(const BoundInputLayout& inputLayout);
        void        Bind(Topology::Enum topology);
        void        Bind(const VertexShader& vertexShader);
        void        Bind(const GeometryShader& geometryShader);
        void        Bind(const PixelShader& pixelShader);
        void        Bind(const ComputeShader& computeShader);
        void        Bind(const DomainShader& domainShader);
        void        Bind(const HullShader& hullShader);
        void        Bind(const ShaderProgram& shaderProgram);
        void        Bind(const DeepShaderProgram& deepShaderProgram);
        void        Bind(const RasterizerState& rasterizer);
        void        Bind(const BlendState& blender);
        void        Bind(const DepthStencilState& depthStencilState, unsigned stencilRef = 0x0);
        void        Bind(const ViewportDesc& viewport);

        void        Bind(const ShaderProgram& shaderProgram, const BoundClassInterfaces& dynLinkage);
        void        Bind(const DeepShaderProgram& deepShaderProgram, const BoundClassInterfaces& dynLinkage);

        T1(Type) void   UnbindVS(unsigned startSlot, unsigned count);
        T1(Type) void   UnbindGS(unsigned startSlot, unsigned count);
        T1(Type) void   UnbindPS(unsigned startSlot, unsigned count);
        T1(Type) void   UnbindCS(unsigned startSlot, unsigned count);
		T1(Type) void   UnbindDS(unsigned startSlot, unsigned count);
        T1(Type) void   Unbind();
        void            UnbindSO();

        void        Draw(unsigned vertexCount, unsigned startVertexLocation=0);
        void        DrawIndexed(unsigned indexCount, unsigned startIndexLocation=0, unsigned baseVertexLocation=0);
        void        DrawAuto();
        void        Dispatch(unsigned countX, unsigned countY=1, unsigned countZ=1);

        void        Clear(const RenderTargetView& renderTargets, const VectorPattern<float,4>& values);
        struct ClearFilter { enum Enum { Depth = 1<<0, Stencil = 1<<1 }; using BitField = unsigned; };
        void        Clear(const DepthStencilView& depthStencil, ClearFilter::BitField clearFilter, float depth, unsigned stencil);
        void        ClearUInt(const UnorderedAccessView& unorderedAccess, const VectorPattern<unsigned,4>& values);
        void        ClearFloat(const UnorderedAccessView& unorderedAccess, const VectorPattern<float,4>& values);

        void        BeginCommandList();
        auto        ResolveCommandList() -> CommandListPtr;
        void        CommitCommandList(CommandList& commandList, bool preserveRenderState);

        static std::shared_ptr<DeviceContext> Get(IThreadContext& threadContext);
        static void PrepareForDestruction(IDevice* device, IPresentationChain* presentationChain);

        ID3D::DeviceContext*            GetUnderlying() const           { return _underlying.get(); }
        ID3D::UserDefinedAnnotation*    GetAnnotationInterface() const  { return _annotations.get(); }
        bool                            IsImmediate() const;

        void                        SetPresentationTarget(RenderTargetView* presentationTarget, const VectorPattern<unsigned,2>& dims);
        VectorPattern<unsigned,2>   GetPresentationTargetDims();

        void        InvalidateCachedState();

        ID3D::Buffer*               _currentCBs[6][14];
        ID3D::ShaderResourceView*   _currentSRVs[6][32];

		ObjectFactory&	GetFactory() { return *_factory; }

        DeviceContext(ID3D::DeviceContext* context);
        DeviceContext(intrusive_ptr<ID3D::DeviceContext>&& context);
        ~DeviceContext();
    private:
        intrusive_ptr<ID3D::DeviceContext> _underlying;
        intrusive_ptr<ID3D::UserDefinedAnnotation> _annotations;
		ObjectFactory* _factory;

        // NamedResources _namedResources;
        VectorPattern<unsigned,2> _presentationTargetDims;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    extern template void DeviceContext::Unbind<BoundInputLayout>();
    extern template void DeviceContext::Unbind<VertexBuffer>();
    extern template void DeviceContext::Unbind<RenderTargetView>();
    extern template void DeviceContext::Unbind<VertexShader>();
    extern template void DeviceContext::Unbind<PixelShader>();
    extern template void DeviceContext::Unbind<GeometryShader>();

}}
