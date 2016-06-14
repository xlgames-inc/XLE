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
#include "../../Types_Forward.h"
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
        void        Bind(Topology topology);
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

        std::shared_ptr<DeviceContext> Fork();

        DeviceContext(ID3D::DeviceContext* context);
        DeviceContext(intrusive_ptr<ID3D::DeviceContext>&& context);
        ~DeviceContext();

        #if COMPILER_DEFAULT_IMPLICIT_OPERATORS == 1
            DeviceContext(const DeviceContext&) = delete;
            DeviceContext& operator=(const DeviceContext&) = delete;
            DeviceContext(DeviceContext&&) = default;
            DeviceContext& operator=(DeviceContext&&) = default;
        #endif
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

    ObjectFactory& GetObjectFactory(IDevice& device);
	ObjectFactory& GetObjectFactory(DeviceContext&);
	ObjectFactory& GetObjectFactory();
    ObjectFactory& GetObjectFactory(ID3D::Device& device);
	ObjectFactory& GetObjectFactory(ID3D::Resource& resource);

}}
