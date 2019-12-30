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

    void DeviceContext::Bind(const Viewport& viewport)
    {
            // ("ViewportDesc" is equivalent to D3D11_VIEWPORT)
            //      --  we could do static_asserts to check the offsets of the members
            //          to make sure.
        _underlying->RSSetViewports(1, (D3D11_VIEWPORT*)&viewport);
    }

	void DeviceContext::SetViewportAndScissorRects(IteratorRange<const Viewport*> viewports, IteratorRange<const ScissorRect*> scissorRects)
	{
		// "Viewport" is compatible with D3D11_VIEWPORT, but not the same size
		D3D11_VIEWPORT d3dViewports[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];
		auto cnt = std::min(viewports.size(), dimof(d3dViewports));
		for (unsigned c=0; c<cnt; ++c) {
			assert(viewports[c].OriginIsUpperLeft);
			d3dViewports[c] = *(const D3D11_VIEWPORT*)(viewports.begin() + c);
		}
		_underlying->RSSetViewports((UINT)cnt, d3dViewports);

		D3D11_RECT rects[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];
		cnt = std::min(scissorRects.size(), dimof(rects));
		for (unsigned c=0; c<cnt; ++c) {
			assert(scissorRects[c].OriginIsUpperLeft);
			rects[c].left = scissorRects[c].X;
			rects[c].top = scissorRects[c].Y;
			rects[c].right = scissorRects[c].X + scissorRects[c].Width;
			rects[c].bottom = scissorRects[c].Y + scissorRects[c].Height;
		}
		_underlying->RSSetScissorRects((UINT)cnt, rects);
	}

	static_assert(  offsetof(ViewportDesc, X) == offsetof(D3D11_VIEWPORT, TopLeftX)
                &&  offsetof(ViewportDesc, Y) == offsetof(D3D11_VIEWPORT, TopLeftY)
                &&  offsetof(ViewportDesc, Width) == offsetof(D3D11_VIEWPORT, Width)
                &&  offsetof(ViewportDesc, Height) == offsetof(D3D11_VIEWPORT, Height)
                &&  offsetof(ViewportDesc, MinDepth) == offsetof(D3D11_VIEWPORT, MinDepth)
                &&  offsetof(ViewportDesc, MaxDepth) == offsetof(D3D11_VIEWPORT, MaxDepth),
                "ViewportDesc is no longer compatible with D3D11_VIEWPORT");

	Viewport DeviceContext::GetBoundViewport()
	{
		Viewport result;
		UINT viewportsToGet = 1;
        GetUnderlying()->RSGetViewports(&viewportsToGet, (D3D11_VIEWPORT*)&result);
		result.OriginIsUpperLeft = true;
		return result;
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

    void DeviceContext::UnbindInputLayout()
    {
        _underlying->IASetInputLayout(nullptr);

		ID3D::Buffer* vb = nullptr;
        UINT strides = 0, offsets = 0;
        _underlying->IASetVertexBuffers(0, 1, &vb, &strides, &offsets);
    }

    void DeviceContext::UnbindVS() { _underlying->VSSetShader(nullptr, nullptr, 0); }
    void DeviceContext::UnbindPS() { _underlying->PSSetShader(nullptr, nullptr, 0); }
    void DeviceContext::UnbindGS() { _underlying->GSSetShader(nullptr, nullptr, 0); }
	void DeviceContext::UnbindHS() { _underlying->HSSetShader(nullptr, nullptr, 0); }
	void DeviceContext::UnbindDS() { _underlying->DSSetShader(nullptr, nullptr, 0); }
	void DeviceContext::UnbindCS() { _underlying->CSSetShader(nullptr, nullptr, 0); }

    void DeviceContext::Dispatch(unsigned countX, unsigned countY, unsigned countZ)
    {
        _underlying->Dispatch(countX, countY, countZ);
    }

    void        DeviceContext::InvalidateCachedState()
    {
        XlZeroMemory(_currentCBs);
        XlZeroMemory(_currentSRVs);
		XlZeroMemory(_currentSSs);
    }

	NumericUniformsInterface& DeviceContext::GetNumericUniforms(ShaderStage stage)
	{
		assert(unsigned(stage) < _numericUniforms.size());
		return _numericUniforms[unsigned(stage)];
	}

	void DeviceContext::BeginRenderPass()
    {
        assert(!_inRenderPass);
        _inRenderPass = true;
    }

    void DeviceContext::BeginSubpass(unsigned renderTargetWidth, unsigned renderTargetHeight)
	{
        _renderTargetWidth = renderTargetWidth;
        _renderTargetHeight = renderTargetHeight;
    }

    void DeviceContext::EndSubpass()
	{
        _renderTargetWidth = 0;
        _renderTargetHeight = 0;
    }

    void DeviceContext::EndRenderPass()
    {
        assert(_inRenderPass);
        _inRenderPass = false;
        _renderTargetWidth = 0;
        _renderTargetHeight = 0;

        for (auto fn: _onEndRenderPassFunctions) { fn(); }
        _onEndRenderPassFunctions.clear();
    }

    bool DeviceContext::InRenderPass()
    {
        return _inRenderPass;
    }

    void DeviceContext::OnEndRenderPass(std::function<void ()> fn)
    {
        if (!_inRenderPass) {
            _onEndRenderPassFunctions.emplace_back(std::move(fn));
        } else {
            fn();
        }
    }

    DeviceContext::DeviceContext(ID3D::DeviceContext* context)
	: DeviceContext(intrusive_ptr<ID3D::DeviceContext>(context)) {}

    DeviceContext::DeviceContext(intrusive_ptr<ID3D::DeviceContext>&& context)
    : _underlying(std::forward<intrusive_ptr<ID3D::DeviceContext>>(context))
	, _factory(nullptr)
    {
        XlZeroMemory(_currentCBs);
        XlZeroMemory(_currentSRVs);
		XlZeroMemory(_currentSSs);
        _annotations = QueryInterfaceCast<ID3D::UserDefinedAnnotation>(_underlying);

		ID3D::Device* devRaw = nullptr;
		_underlying->GetDevice(&devRaw);
        intrusive_ptr<ID3D::Device> dev = moveptr(devRaw);
		if (dev) _factory = &GetObjectFactory(*dev);

		_numericUniforms.resize(6);
		for (unsigned c=0; c<_numericUniforms.size(); ++c)
			_numericUniforms[c] = NumericUniformsInterface{*this, (ShaderStage)c};

		_inRenderPass = false;
        _renderTargetWidth = 0;
        _renderTargetHeight = 0;
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

    void    DeviceContext::ExecuteCommandList(CommandList& commandList, bool preserveRenderState)
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
}}
