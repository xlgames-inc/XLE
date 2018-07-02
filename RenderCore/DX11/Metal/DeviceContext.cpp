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

    void DeviceContext::Bind(Topology topology)
    {
        _underlying->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY(topology));
    }

    void DeviceContext::Bind(const ComputeShader& computeShader)
    {
        _underlying->CSSetShader(computeShader.GetUnderlying(), nullptr, 0);
    }

    void DeviceContext::Bind(const ShaderProgram& shaderProgram)
    {
		shaderProgram.Apply(*this);
    }

    void DeviceContext::Bind(const ShaderProgram& shaderProgram, const BoundClassInterfaces& dynLinkage)
    {
		shaderProgram.Apply(*this, dynLinkage);
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

    void DeviceContext::Bind(const Resource& ib, Format indexFormat, unsigned offset)
    {
		assert(QueryInterfaceCast<ID3D::Buffer>(ib._underlying.get()));
        _underlying->IASetIndexBuffer((ID3D::Buffer*)ib._underlying.get(), AsDXGIFormat(indexFormat), offset);
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

    template<> void DeviceContext::Unbind<ComputeShader>()	{ _underlying->CSSetShader(nullptr, nullptr, 0); }

    template<> void DeviceContext::Unbind<BoundInputLayout>()
    {
        _underlying->IASetInputLayout(nullptr);

		ID3D::Buffer* vb = nullptr;
        UINT strides = 0, offsets = 0;
        _underlying->IASetVertexBuffers(0, 1, &vb, &strides, &offsets);
    }

    template<> void DeviceContext::Unbind<RenderTargetView>()
    {
        _underlying->OMSetRenderTargets(0, nullptr, nullptr);
    }

    void DeviceContext::UnbindVS() { _underlying->VSSetShader(nullptr, nullptr, 0); }
    void DeviceContext::UnbindPS() { _underlying->PSSetShader(nullptr, nullptr, 0); }
    void DeviceContext::UnbindGS() { _underlying->GSSetShader(nullptr, nullptr, 0); }
	void DeviceContext::UnbindHS() { _underlying->HSSetShader(nullptr, nullptr, 0); }
	void DeviceContext::UnbindDS() { _underlying->DSSetShader(nullptr, nullptr, 0); }

    void DeviceContext::Dispatch(unsigned countX, unsigned countY, unsigned countZ)
    {
        _underlying->Dispatch(countX, countY, countZ);
    }

    void        DeviceContext::InvalidateCachedState()
    {
        XlZeroMemory(_currentCBs);
        XlZeroMemory(_currentSRVs);
    }

	NumericUniformsInterface& DeviceContext::GetNumericUniforms(ShaderStage stage)
	{
		assert(unsigned(stage) < _numericUniforms.size());
		return _numericUniforms[unsigned(stage)];
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

		_numericUniforms.resize(6);
		for (unsigned c=0; c<_numericUniforms.size(); ++c)
			_numericUniforms[c] = NumericUniformsInterface{*this, (ShaderStage)c};
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
        auto tc = (IThreadContextDX11*)threadContext.QueryInterface(typeid(IThreadContextDX11).hash_code());
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

namespace RenderCore { namespace Metal_DX11
{
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

    EXPAND(UnorderedAccessView, BindCS)

    template void DeviceContext::BindSO<1>(const ResourceList<Buffer, 1>&, unsigned);
    template void DeviceContext::BindSO<2>(const ResourceList<Buffer, 2>&, unsigned);
}}
