// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeviceContext.h"
#include "DeviceContextImpl.h"
#include "InputLayout.h"
#include "Shader.h"
#include "State.h"
#include "Buffer.h"
#include "Resource.h"
#include "ObjectFactory.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/Threading/Mutex.h"
#include "../../Utility/MemoryUtils.h"

#include "../IDeviceDX11.h"
#include "IncludeDX11.h"
#include "DX11Utils.h"

namespace RenderCore { namespace Metal_DX11
{
    static_assert(  (unsigned)Topology::PointList      == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST
                &&  (unsigned)Topology::LineList       == D3D11_PRIMITIVE_TOPOLOGY_LINELIST
                &&  (unsigned)Topology::LineStrip      == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP
                &&  (unsigned)Topology::TriangleList   == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
                &&  (unsigned)Topology::TriangleStrip  == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
                "Toplogy flags are out-of-sync");

    void DeviceContext::Bind(unsigned startSlot, unsigned bufferCount, const VertexBuffer* VBs[], const unsigned strides[], const unsigned offsets[])
    {
        ID3D::Buffer* buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
        for (unsigned c=0; c<bufferCount; ++c) buffers[c] = VBs[c]->GetUnderlying();
        _underlying->IASetVertexBuffers(startSlot, bufferCount, buffers, strides, offsets);
    }

    void DeviceContext::Bind(const BoundInputLayout& inputLayout)
    {
        _underlying->IASetInputLayout(inputLayout.GetUnderlying());
    }

    void DeviceContext::Bind(Topology topology)
    {
        _underlying->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY(topology));
    }

    void DeviceContext::Bind(const VertexShader& vertexShader)
    {
        _underlying->VSSetShader(vertexShader.GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const GeometryShader& geometryShader)
    {
        _underlying->GSSetShader(geometryShader.GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const PixelShader& pixelShader)
    {
        _underlying->PSSetShader(pixelShader.GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const ComputeShader& computeShader)
    {
        _underlying->CSSetShader(computeShader.GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const DomainShader& domainShader)
    {
        _underlying->DSSetShader(domainShader.GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const HullShader& hullShader)
    {
        _underlying->HSSetShader(hullShader.GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const ShaderProgram& shaderProgram)
    {
        _underlying->VSSetShader(shaderProgram.GetVertexShader().GetUnderlying(), nullptr, 0);
        _underlying->GSSetShader(shaderProgram.GetGeometryShader().GetUnderlying(), nullptr, 0);
        _underlying->PSSetShader(shaderProgram.GetPixelShader().GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const ShaderProgram& shaderProgram, const BoundClassInterfaces& dynLinkage)
    {
        auto& vsDyn = dynLinkage.GetClassInstances(ShaderStage::Vertex);
        _underlying->VSSetShader(shaderProgram.GetVertexShader().GetUnderlying(), 
            (ID3D::ClassInstance*const*)AsPointer(vsDyn.cbegin()), (unsigned)vsDyn.size());

        auto& psDyn = dynLinkage.GetClassInstances(ShaderStage::Pixel);
        _underlying->PSSetShader(shaderProgram.GetPixelShader().GetUnderlying(), 
            (ID3D::ClassInstance*const*)AsPointer(psDyn.cbegin()), (unsigned)psDyn.size());

        _underlying->GSSetShader(shaderProgram.GetGeometryShader().GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const DeepShaderProgram& shaderProgram)
    {
        _underlying->VSSetShader(shaderProgram.GetVertexShader().GetUnderlying(), nullptr, 0);
        _underlying->GSSetShader(shaderProgram.GetGeometryShader().GetUnderlying(), nullptr, 0);
        _underlying->PSSetShader(shaderProgram.GetPixelShader().GetUnderlying(), nullptr, 0);
        _underlying->HSSetShader(shaderProgram.GetHullShader().GetUnderlying(), nullptr, 0);
        _underlying->DSSetShader(shaderProgram.GetDomainShader().GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const DeepShaderProgram& shaderProgram, const BoundClassInterfaces& dynLinkage)
    {
        auto& vsDyn = dynLinkage.GetClassInstances(ShaderStage::Vertex);
        _underlying->VSSetShader(shaderProgram.GetVertexShader().GetUnderlying(), 
            (ID3D::ClassInstance*const*)AsPointer(vsDyn.cbegin()), (unsigned)vsDyn.size());

        auto& psDyn = dynLinkage.GetClassInstances(ShaderStage::Pixel);
        _underlying->PSSetShader(shaderProgram.GetPixelShader().GetUnderlying(), 
            (ID3D::ClassInstance*const*)AsPointer(psDyn.cbegin()), (unsigned)psDyn.size());

        _underlying->GSSetShader(shaderProgram.GetGeometryShader().GetUnderlying(), nullptr, 0);
        _underlying->HSSetShader(shaderProgram.GetHullShader().GetUnderlying(), nullptr, 0);
        _underlying->DSSetShader(shaderProgram.GetDomainShader().GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const RasterizerState& rasterizer)
    {
        _underlying->RSSetState(rasterizer.GetUnderlying());
    }

    void DeviceContext::Bind(const BlendState& blender)
    {
        const FLOAT blendFactors[] = {1.f, 1.f, 1.f, 1.f};
        _underlying->OMSetBlendState(blender.GetUnderlying(), blendFactors, 0xffffffff);
    }

    void DeviceContext::Bind(const DepthStencilState& depthStencil, unsigned stencilRef)
    {
        _underlying->OMSetDepthStencilState(depthStencil.GetUnderlying(), stencilRef);
    }

    void DeviceContext::Bind(const IndexBuffer& ib, Format indexFormat, unsigned offset)
    {
        _underlying->IASetIndexBuffer(ib.GetUnderlying(), AsDXGIFormat(indexFormat), offset);
    }

    void DeviceContext::Bind(const ViewportDesc& viewport)
    {
            // ("ViewportDesc" is equivalent to D3D11_VIEWPORT)
            //      --  we could do static_asserts to check the offsets of the members
            //          to make sure.
        _underlying->RSSetViewports(1, (D3D11_VIEWPORT*)&viewport);
    }

    void DeviceContext::Draw(unsigned vertexCount, unsigned startVertexLocation)
    {
        _underlying->Draw(vertexCount, startVertexLocation);
    }

    void DeviceContext::DrawIndexed(unsigned indexCount, unsigned startIndexLocation, unsigned baseVertexLocation)
    {
        _underlying->DrawIndexed(indexCount, startIndexLocation, baseVertexLocation);
    }

    void DeviceContext::DrawAuto()
    {
        _underlying->DrawAuto();
    }

    void DeviceContext::Clear(const RenderTargetView& renderTargets, const VectorPattern<float,4>& values)
    {
        _underlying->ClearRenderTargetView(renderTargets.GetUnderlying(), values._values);
    }

    void DeviceContext::Clear(const DepthStencilView& depthStencil, ClearFilter::BitField clearFilter, float depth, unsigned stencil)
    {
        unsigned underlyingFilter = 0u;
        if (clearFilter & ClearFilter::Depth) underlyingFilter |= D3D11_CLEAR_DEPTH;
        if (clearFilter & ClearFilter::Stencil) underlyingFilter |= D3D11_CLEAR_STENCIL;
        _underlying->ClearDepthStencilView(depthStencil.GetUnderlying(), underlyingFilter, depth, (UINT8)stencil);
    }

    void DeviceContext::ClearUInt(const UnorderedAccessView& unorderedAccess, const VectorPattern<unsigned,4>& values)
    {
        _underlying->ClearUnorderedAccessViewUint(unorderedAccess.GetUnderlying(), values._values);
    }

    void DeviceContext::ClearFloat(const UnorderedAccessView& unorderedAccess, const VectorPattern<float,4>& values)
    {
        _underlying->ClearUnorderedAccessViewFloat(unorderedAccess.GetUnderlying(), values._values);
    }

    template<>
        void  DeviceContext::UnbindVS<ShaderResourceView>(unsigned startSlot, unsigned count)
    {
            // note --  D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT is 128 for D3D11
            //          It's a little too big to declared on the stack. So let's limit
            //          the maximum size
        ID3D::ShaderResourceView* srv[16];
        assert(count <= dimof(srv));
        count = std::min(count, (unsigned)dimof(srv));
        std::fill(srv, &srv[count], nullptr);
        _underlying->VSSetShaderResources(startSlot, count, srv);
    }

    template<>
        void  DeviceContext::UnbindGS<ShaderResourceView>(unsigned startSlot, unsigned count)
    {
        ID3D::ShaderResourceView* srv[16];
        assert(count <= dimof(srv));
        count = std::min(count, (unsigned)dimof(srv));
        std::fill(srv, &srv[count], nullptr);
        _underlying->GSSetShaderResources(startSlot, count, srv);
    }

    template<>
        void  DeviceContext::UnbindPS<ShaderResourceView>(unsigned startSlot, unsigned count)
    {
        ID3D::ShaderResourceView* srv[16];
        assert(count <= dimof(srv));
        count = std::min(count, (unsigned)dimof(srv));
        std::fill(srv, &srv[count], nullptr);
        _underlying->PSSetShaderResources(startSlot, count, srv);
    }

    template<>
        void  DeviceContext::UnbindCS<ShaderResourceView>(unsigned startSlot, unsigned count)
    {
        ID3D::ShaderResourceView* srv[16];
        assert(count <= dimof(srv));
        count = std::min(count, (unsigned)dimof(srv));
        std::fill(srv, &srv[count], nullptr);
        _underlying->CSSetShaderResources(startSlot, count, srv);
    }

	template<>
		void  DeviceContext::UnbindDS<ShaderResourceView>(unsigned startSlot, unsigned count)
		{
			ID3D::ShaderResourceView* srv[16];
			assert(count <= dimof(srv));
			count = std::min(count, (unsigned)dimof(srv));
			std::fill(srv, &srv[count], nullptr);
			_underlying->DSSetShaderResources(startSlot, count, srv);
		}

    template<>
        void  DeviceContext::UnbindCS<UnorderedAccessView>(unsigned startSlot, unsigned count)
    {
        ID3D::UnorderedAccessView* uoavs[16];
        unsigned initialCounts[16] = {
            unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1),
            unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1), unsigned(-1)
        };
        assert(count <= dimof(uoavs));
        count = std::min(count, (unsigned)dimof(uoavs));
        std::fill(uoavs, &uoavs[count], nullptr);
        _underlying->CSSetUnorderedAccessViews(startSlot, count, uoavs, initialCounts);
    }

    template<> void DeviceContext::Unbind<ComputeShader>()   { _underlying->CSSetShader(nullptr, nullptr, 0); }
    template<> void DeviceContext::Unbind<HullShader>()      { _underlying->HSSetShader(nullptr, nullptr, 0); }
    template<> void DeviceContext::Unbind<DomainShader>()    { _underlying->DSSetShader(nullptr, nullptr, 0); }

    template<> void DeviceContext::Unbind<BoundInputLayout>()
    {
        _underlying->IASetInputLayout(nullptr);
    }

    template<> void DeviceContext::Unbind<VertexBuffer>()
    {
        ID3D::Buffer* vb = nullptr;
        UINT strides = 0, offsets = 0;
        _underlying->IASetVertexBuffers(0, 1, &vb, &strides, &offsets);
    }

    template<> void DeviceContext::Unbind<RenderTargetView>()
    {
        _underlying->OMSetRenderTargets(0, nullptr, nullptr);
    }

    template<> void DeviceContext::Unbind<VertexShader>()
    {
        _underlying->VSSetShader(nullptr, nullptr, 0);
    }

    template<> void DeviceContext::Unbind<PixelShader>()
    {
        _underlying->PSSetShader(nullptr, nullptr, 0);
    }

    template<> void DeviceContext::Unbind<GeometryShader>()
    {
        _underlying->GSSetShader(nullptr, nullptr, 0);
    }

    void DeviceContext::UnbindSO()
    {
        _underlying->SOSetTargets(0, nullptr, nullptr);
    }

    void DeviceContext::Dispatch(unsigned countX, unsigned countY, unsigned countZ)
    {
        _underlying->Dispatch(countX, countY, countZ);
    }

    void        DeviceContext::InvalidateCachedState()
    {
        XlZeroMemory(_currentCBs);
        XlZeroMemory(_currentSRVs);
    }

    void DeviceContext::SetPresentationTarget(RenderTargetView* presentationTarget, const VectorPattern<unsigned,2>& dims)
    {
        _presentationTargetDims = dims;
    }

    VectorPattern<unsigned,2> DeviceContext::GetPresentationTargetDims()
    {
        return _presentationTargetDims;
    }

    DeviceContext::DeviceContext(ID3D::DeviceContext* context)
	: DeviceContext(intrusive_ptr<ID3D::DeviceContext>(context)) {}

    DeviceContext::DeviceContext(intrusive_ptr<ID3D::DeviceContext>&& context)
    : _underlying(std::forward<intrusive_ptr<ID3D::DeviceContext>>(context))
	, _factory(nullptr)
    {
        XlZeroMemory(_currentCBs);
        XlZeroMemory(_currentSRVs);
        _annotations = QueryInterfaceCast<ID3D::UserDefinedAnnotation>(_underlying);

		ID3D::Device* devRaw = nullptr;
		_underlying->GetDevice(&devRaw);
        intrusive_ptr<ID3D::Device> dev = moveptr(devRaw);
		if (dev) _factory = &GetObjectFactory(*dev);
    }

    DeviceContext::~DeviceContext()
    {}


    void                        DeviceContext::BeginCommandList()
    {
            // (nothing required in D3D11)
    }

    intrusive_ptr<CommandList>     DeviceContext::ResolveCommandList()
    {
        ID3D::CommandList* commandListTemp = nullptr;
        HRESULT hresult = _underlying->FinishCommandList(FALSE, &commandListTemp);
        if (SUCCEEDED(hresult) && commandListTemp) {
            intrusive_ptr<ID3D::CommandList> underlyingCommandList = moveptr(commandListTemp);
            return make_intrusive<CommandList>(underlyingCommandList.get());
        }
        return intrusive_ptr<CommandList>();
    }

    bool    DeviceContext::IsImmediate() const
    {
        auto type = _underlying->GetType();
        return type == D3D11_DEVICE_CONTEXT_IMMEDIATE;
    }

    void    DeviceContext::CommitCommandList(CommandList& commandList, bool preserveRenderState)
    {
        // Note that if "preserveRenderState" isn't set, the device will be reset to it's default
        // state.
        _underlying->ExecuteCommandList(commandList.GetUnderlying(), preserveRenderState);
    }

    std::shared_ptr<DeviceContext>  DeviceContext::Get(IThreadContext& threadContext)
    {
        auto tc = (IThreadContextDX11*)threadContext.QueryInterface(__uuidof(IThreadContextDX11));
        if (tc) {
            return tc->GetUnderlying();
        }
        return nullptr;
    }

    void DeviceContext::PrepareForDestruction(IDevice* device, IPresentationChain* presentationChain)
    {
		auto immContext = device->GetImmediateContext();
        auto metalContext = Get(*immContext);
        if (metalContext) {
            metalContext->GetUnderlying()->ClearState();
            for (unsigned c=0; c<6; ++c) {
				immContext->BeginFrame(*presentationChain);
                metalContext->GetUnderlying()->Flush();
            }
        }
    }

    std::shared_ptr<DeviceContext> DeviceContext::Fork()
    {
        ID3D::Device* devRaw = nullptr;
		_underlying->GetDevice(&devRaw);
        intrusive_ptr<ID3D::Device> dev = moveptr(devRaw);

        ID3D::DeviceContext* rawContext = nullptr;
        auto hresult = dev->CreateDeferredContext(0, &rawContext);
        intrusive_ptr<ID3D::DeviceContext> context = moveptr(rawContext);
        if (!SUCCEEDED(hresult) || !context) 
            Throw(::Exceptions::BasicLabel("Failure while forking device context"));

        return std::make_shared<DeviceContext>(std::move(context));
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    CommandList::CommandList(ID3D::CommandList* underlying)
    :   _underlying(underlying)
    {
    }

    CommandList::CommandList(intrusive_ptr<ID3D::CommandList>&& underlying)
    :   _underlying(std::forward<intrusive_ptr<ID3D::CommandList>>(underlying))
    {}

}}

#include "TextureView.h"

namespace RenderCore { namespace Metal_DX11
{
    template void DeviceContext::Bind<1>(const ResourceList<VertexBuffer, 1>&, unsigned, unsigned);
    template void DeviceContext::Bind<2>(const ResourceList<VertexBuffer, 2>&, unsigned, unsigned);
    template void DeviceContext::Bind<3>(const ResourceList<VertexBuffer, 3>&, unsigned, unsigned);

    template void DeviceContext::Bind<0>(const ResourceList<RenderTargetView, 0>&, const DepthStencilView*);
    template void DeviceContext::Bind<1>(const ResourceList<RenderTargetView, 1>&, const DepthStencilView*);
    template void DeviceContext::Bind<2>(const ResourceList<RenderTargetView, 2>&, const DepthStencilView*);
    template void DeviceContext::Bind<3>(const ResourceList<RenderTargetView, 3>&, const DepthStencilView*);
    template void DeviceContext::Bind<4>(const ResourceList<RenderTargetView, 4>&, const DepthStencilView*);
    template void DeviceContext::Bind<1,1>(const ResourceList<RenderTargetView, 1>&, const DepthStencilView*, const ResourceList<UnorderedAccessView, 1>&);
    template void DeviceContext::Bind<1,2>(const ResourceList<RenderTargetView, 1>&, const DepthStencilView*, const ResourceList<UnorderedAccessView, 2>&);

    #define EXPAND(BINDABLE, FN)                                                          \
        template void DeviceContext::FN<1>(const ResourceList<BINDABLE, 1>&);             \
        template void DeviceContext::FN<2>(const ResourceList<BINDABLE, 2>&);             \
        template void DeviceContext::FN<3>(const ResourceList<BINDABLE, 3>&);             \
        template void DeviceContext::FN<4>(const ResourceList<BINDABLE, 4>&);             \
        template void DeviceContext::FN<5>(const ResourceList<BINDABLE, 5>&);             \
        template void DeviceContext::FN<6>(const ResourceList<BINDABLE, 6>&);             \
        template void DeviceContext::FN<7>(const ResourceList<BINDABLE, 7>&);             \
        template void DeviceContext::FN<8>(const ResourceList<BINDABLE, 8>&);             \
        template void DeviceContext::FN<9>(const ResourceList<BINDABLE, 9>&);             \
        /**/

    #define EXPANDSTAGES(BINDABLE)          \
        EXPAND(BINDABLE, BindVS)            \
        EXPAND(BINDABLE, BindPS)            \
        EXPAND(BINDABLE, BindGS)            \
        EXPAND(BINDABLE, BindHS)            \
        EXPAND(BINDABLE, BindDS)            \
        EXPAND(BINDABLE, BindCS)            \
        EXPAND(BINDABLE, BindVS_G)          \
        EXPAND(BINDABLE, BindPS_G)          \
        EXPAND(BINDABLE, BindGS_G)          \
        EXPAND(BINDABLE, BindHS_G)          \
        EXPAND(BINDABLE, BindDS_G)          \
        EXPAND(BINDABLE, BindCS_G)          \
        /**/

    EXPANDSTAGES(SamplerState)
    EXPANDSTAGES(ShaderResourceView)
    EXPANDSTAGES(ConstantBuffer)
    EXPAND(UnorderedAccessView, BindCS)

    template void DeviceContext::BindSO<1>(const ResourceList<VertexBuffer, 1>&, unsigned);
    template void DeviceContext::BindSO<2>(const ResourceList<VertexBuffer, 2>&, unsigned);
}}
