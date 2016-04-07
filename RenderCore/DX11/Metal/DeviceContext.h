// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DX11.h"
#include "../../IDevice_Forward.h"
#include "../../IThreadContext_Forward.h"
#include "../../Resource.h"
#include "../../../Utility/Mixins.h"
#include "../../../Utility/Threading/ThreadingUtils.h"
#include "../../../Utility/IntrusivePtr.h"
#include "../../../Math/Vector.h"
#include "RenderTargetView.h"

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

    namespace NativeFormat { enum Enum; }

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

    class ObjectFactory
    {
    public:
        ID3D::Device*   GetUnderlying() { return _device.get(); }

            //  Wrappers for ID3D::Device creation functions. Using a single point
            //  of all for D3D creation methods allows us a lot of useful debugging
            //  and profiling tools (eg, for attaching names and leak detection)

        /// @{
        /// Basic states
        intrusive_ptr<ID3D::BlendState> CreateBlendState(const D3D11_BLEND_DESC*) const;
        intrusive_ptr<ID3D::DepthStencilState> CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC*) const;
        intrusive_ptr<ID3D::RasterizerState> CreateRasterizerState(const D3D11_RASTERIZER_DESC*) const;
        intrusive_ptr<ID3D::SamplerState> CreateSamplerState(const D3D11_SAMPLER_DESC*) const;
        /// @}

        /// @{
        /// Resources
        intrusive_ptr<ID3D::Buffer> CreateBuffer(const D3D11_BUFFER_DESC*, const D3D11_SUBRESOURCE_DATA* = nullptr, const char name[] = nullptr) const;
        intrusive_ptr<ID3D::Texture1D> CreateTexture1D(const D3D11_TEXTURE1D_DESC*, const D3D11_SUBRESOURCE_DATA* = nullptr, const char name[] = nullptr) const;
        intrusive_ptr<ID3D::Texture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC*, const D3D11_SUBRESOURCE_DATA* = nullptr, const char name[] = nullptr) const;
        intrusive_ptr<ID3D::Texture3D> CreateTexture3D(const D3D11_TEXTURE3D_DESC*, const D3D11_SUBRESOURCE_DATA* = nullptr, const char name[] = nullptr) const;
        /// @}

        /// @{
        /// Resource views
        intrusive_ptr<ID3D::RenderTargetView> CreateRenderTargetView(ID3D::Resource *, const D3D11_RENDER_TARGET_VIEW_DESC* = nullptr) const;
        intrusive_ptr<ID3D::ShaderResourceView> CreateShaderResourceView(ID3D::Resource*, const D3D11_SHADER_RESOURCE_VIEW_DESC* = nullptr) const;
        intrusive_ptr<ID3D::UnorderedAccessView> CreateUnorderedAccessView(ID3D11Resource*, const D3D11_UNORDERED_ACCESS_VIEW_DESC* = nullptr) const;
        intrusive_ptr<ID3D::DepthStencilView> CreateDepthStencilView(ID3D::Resource*, const D3D11_DEPTH_STENCIL_VIEW_DESC* = nullptr) const;
        /// @}

        /// @{
        /// Shaders
        intrusive_ptr<ID3D::VertexShader> CreateVertexShader(const void*, size_t, ID3D11ClassLinkage * = nullptr) const;
        intrusive_ptr<ID3D::PixelShader> CreatePixelShader(const void*, size_t, ID3D11ClassLinkage * = nullptr) const;
        intrusive_ptr<ID3D::ComputeShader> CreateComputeShader(const void*, size_t, ID3D::ClassLinkage* = nullptr) const;
        intrusive_ptr<ID3D::GeometryShader> CreateGeometryShader(const void*, size_t, ID3D::ClassLinkage* = nullptr) const;
        intrusive_ptr<ID3D::GeometryShader> CreateGeometryShaderWithStreamOutput(
            const void*, size_t,
            const D3D11_SO_DECLARATION_ENTRY* declEntries,
            unsigned declEntryCount, const unsigned bufferStrides[], unsigned stridesCount,
            unsigned rasterizedStreamIndex, ID3D::ClassLinkage* = nullptr) const;
        intrusive_ptr<ID3D::DomainShader> CreateDomainShader(const void*, size_t, ID3D::ClassLinkage* = nullptr) const;
        intrusive_ptr<ID3D::HullShader> CreateHullShader(const void*, size_t, ID3D::ClassLinkage* = nullptr) const;
        /// @}

        /// @{
        /// Shader dynamic linking
        intrusive_ptr<ID3D::ClassLinkage> ObjectFactory::CreateClassLinkage() const;
        /// @}

        /// @{
        /// Misc
        intrusive_ptr<ID3D::DeviceContext> CreateDeferredContext() const;
        intrusive_ptr<ID3D::InputLayout> CreateInputLayout(
            const D3D11_INPUT_ELEMENT_DESC inputElements[], unsigned inputElementsCount,
            const void *, size_t) const;
        intrusive_ptr<ID3D::Query> CreateQuery(const D3D11_QUERY_DESC*) const;
        /// @}

        static void PrepareDevice(ID3D::Device&);
        static void ReleaseDevice(ID3D::Device&);

        void AttachCurrentModule();
        void DetachCurrentModule();

        ObjectFactory(IDevice* device);
        ObjectFactory(ID3D::Device& device);
        ObjectFactory(ID3D::Resource& resource);
        ObjectFactory();
        ~ObjectFactory();

        ObjectFactory(const ObjectFactory& cloneFrom);
        ObjectFactory(ObjectFactory&& moveFrom) never_throws;
        ObjectFactory& operator=(const ObjectFactory& cloneFrom);
        ObjectFactory& operator=(ObjectFactory&& moveFrom) never_throws;
    private:
        intrusive_ptr<ID3D::Device> _device;
        class AttachedData;
        intrusive_ptr<AttachedData> _attachedData;
        static intrusive_ptr<AttachedData> InitAttachedData(ID3D::Device* device);
    };



    class DeviceContext : noncopyable
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

        template<int Count> void    Bind(const ResourceList<RenderTargetView, Count>& renderTargets, const DepthStencilView* depthStencil);
        template<int Count> void    BindCS(const ResourceList<UnorderedAccessView, Count>& unorderedAccess);

        template<int Count> void    BindSO(const ResourceList<VertexBuffer, Count>& buffers, unsigned offset=0);

        template<int Count1, int Count2> void    Bind(const ResourceList<RenderTargetView, Count1>& renderTargets, const DepthStencilView* depthStencil, const ResourceList<UnorderedAccessView, Count2>& unorderedAccess);

        void        Bind(unsigned startSlot, unsigned bufferCount, const VertexBuffer* VBs[], const unsigned strides[], const unsigned offsets[]);
        void        Bind(const IndexBuffer& ib, NativeFormat::Enum indexFormat, unsigned offset=0);
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

        void        Clear(const RenderTargetView& renderTargets, const Float4& clearColour);
        void        Clear(const DepthStencilView& depthStencil, float depth, unsigned stencil);
        void        Clear(const UnorderedAccessView& unorderedAccess, unsigned values[4]);
        void        Clear(const UnorderedAccessView& unorderedAccess, float values[4]);
        void        ClearStencil(const DepthStencilView& depthStencil, unsigned stencil);

        void        BeginCommandList();
        auto        ResolveCommandList() -> CommandListPtr;
        void        CommitCommandList(CommandList& commandList, bool preserveRenderState);

        static std::shared_ptr<DeviceContext> Get(IThreadContext& threadContext);
        static void PrepareForDestruction(IDevice* device, IPresentationChain* presentationChain);

        ID3D::DeviceContext*            GetUnderlying() const           { return _underlying.get(); }
        ID3D::UserDefinedAnnotation*    GetAnnotationInterface() const  { return _annotations.get(); }
        bool                            IsImmediate() const;

        void        InvalidateCachedState();

        ID3D::Buffer*               _currentCBs[6][14];
        ID3D::ShaderResourceView*   _currentSRVs[6][32];

        DeviceContext(ID3D::DeviceContext* context);
        DeviceContext(intrusive_ptr<ID3D::DeviceContext>&& context);
        ~DeviceContext();
    private:
        intrusive_ptr<ID3D::DeviceContext> _underlying;
        intrusive_ptr<ID3D::UserDefinedAnnotation> _annotations;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    extern template void DeviceContext::Unbind<BoundInputLayout>();
    extern template void DeviceContext::Unbind<VertexBuffer>();
    extern template void DeviceContext::Unbind<RenderTargetView>();
    extern template void DeviceContext::Unbind<VertexShader>();
    extern template void DeviceContext::Unbind<PixelShader>();
    extern template void DeviceContext::Unbind<GeometryShader>();

}}
