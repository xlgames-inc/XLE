// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeviceContext.h"
#include "IncludeDX11.h"     // (we need <DX11.h> because there is some inlined implementation in this file)

namespace RenderCore { namespace Metal_DX11
{
    template<int Count> void DeviceContext::Bind(const ResourceList<VertexBuffer, Count>& VBs, unsigned stride, unsigned offset)
    {
        UINT strides[Count], offsets[Count];
        std::fill(strides, &strides[dimof(strides)], stride);
        std::fill(offsets, &offsets[dimof(offsets)], offset);
        _underlying->IASetVertexBuffers(VBs._startingPoint, Count, VBs._buffers, strides, offsets);
    }

    template<int Count> void DeviceContext::BindVS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[0][shaderResources._startingPoint+c] = shaderResources._buffers[c];
        _underlying->VSSetShaderResources(shaderResources._startingPoint, Count, shaderResources._buffers);
    }
    
    template<int Count> void DeviceContext::BindPS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[1][shaderResources._startingPoint+c] = shaderResources._buffers[c];
        _underlying->PSSetShaderResources(shaderResources._startingPoint, Count, shaderResources._buffers);
    }

    template<int Count> void DeviceContext::BindCS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[5][shaderResources._startingPoint+c] = shaderResources._buffers[c];
        _underlying->CSSetShaderResources(shaderResources._startingPoint, Count, shaderResources._buffers);
    }

    template<int Count> void DeviceContext::BindGS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[2][shaderResources._startingPoint+c] = shaderResources._buffers[c];
        _underlying->GSSetShaderResources(shaderResources._startingPoint, Count, shaderResources._buffers);
    }

    template<int Count> void DeviceContext::BindHS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[3][shaderResources._startingPoint+c] = shaderResources._buffers[c];
        _underlying->HSSetShaderResources(shaderResources._startingPoint, Count, shaderResources._buffers);
    }

    template<int Count> void DeviceContext::BindDS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[4][shaderResources._startingPoint+c] = shaderResources._buffers[c];
        _underlying->DSSetShaderResources(shaderResources._startingPoint, Count, shaderResources._buffers);
    }

    template<int Count> void DeviceContext::BindVS(const ResourceList<SamplerState, Count>& samplerStates)
    {
        _underlying->VSSetSamplers(samplerStates._startingPoint, Count, samplerStates._buffers);
    }

    template<int Count> void DeviceContext::BindPS(const ResourceList<SamplerState, Count>& samplerStates)
    {
        _underlying->PSSetSamplers(samplerStates._startingPoint, Count, samplerStates._buffers);
    }

    template<int Count> void DeviceContext::BindGS(const ResourceList<SamplerState, Count>& samplerStates)
    {
        _underlying->GSSetSamplers(samplerStates._startingPoint, Count, samplerStates._buffers);
    }

    template<int Count> void DeviceContext::BindCS(const ResourceList<SamplerState, Count>& samplerStates)
    {
        _underlying->CSSetSamplers(samplerStates._startingPoint, Count, samplerStates._buffers);
    }

    template<int Count> void DeviceContext::BindHS(const ResourceList<SamplerState, Count>& samplerStates)
    {
        _underlying->HSSetSamplers(samplerStates._startingPoint, Count, samplerStates._buffers);
    }

    template<int Count> void DeviceContext::BindDS(const ResourceList<SamplerState, Count>& samplerStates)
    {
        _underlying->DSSetSamplers(samplerStates._startingPoint, Count, samplerStates._buffers);
    }

    template<int Count> void DeviceContext::BindVS(const ResourceList<ConstantBuffer, Count>& constantBuffers)
    {
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[0][constantBuffers._startingPoint+c] = constantBuffers._buffers[c];
        _underlying->VSSetConstantBuffers(constantBuffers._startingPoint, Count, constantBuffers._buffers);
    }

    template<int Count> void DeviceContext::BindPS(const ResourceList<ConstantBuffer, Count>& constantBuffers)
    {
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[1][constantBuffers._startingPoint+c] = constantBuffers._buffers[c];
        _underlying->PSSetConstantBuffers(constantBuffers._startingPoint, Count, constantBuffers._buffers);
    }

    template<int Count> void DeviceContext::BindCS(const ResourceList<ConstantBuffer, Count>& constantBuffers)
    {
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[5][constantBuffers._startingPoint+c] = constantBuffers._buffers[c];
        _underlying->CSSetConstantBuffers(constantBuffers._startingPoint, Count, constantBuffers._buffers);
    }

    template<int Count> void DeviceContext::BindGS(const ResourceList<ConstantBuffer, Count>& constantBuffers)
    {
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[2][constantBuffers._startingPoint+c] = constantBuffers._buffers[c];
        _underlying->GSSetConstantBuffers(constantBuffers._startingPoint, Count, constantBuffers._buffers);
    }

    template<int Count> void DeviceContext::BindHS(const ResourceList<ConstantBuffer, Count>& constantBuffers)
    {
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[3][constantBuffers._startingPoint+c] = constantBuffers._buffers[c];
        _underlying->HSSetConstantBuffers(constantBuffers._startingPoint, Count, constantBuffers._buffers);
    }

    template<int Count> void DeviceContext::BindDS(const ResourceList<ConstantBuffer, Count>& constantBuffers)
    {
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[4][constantBuffers._startingPoint+c] = constantBuffers._buffers[c];
        _underlying->DSSetConstantBuffers(constantBuffers._startingPoint, Count, constantBuffers._buffers);
    }

    template<int Count> void DeviceContext::Bind(const ResourceList<RenderTargetView, Count>& renderTargets, const DepthStencilView* depthStencil)
    {
        assert(renderTargets._startingPoint == 0);
        _underlying->OMSetRenderTargets(Count, renderTargets._buffers, depthStencil?depthStencil->GetUnderlying():nullptr);
    }

    template<int Count> void DeviceContext::BindCS(const ResourceList<UnorderedAccessView, Count>& unorderedAccess)
    {
        const UINT initialCounts[16] = { UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1) };
        assert(Count <= dimof(initialCounts));
        _underlying->CSSetUnorderedAccessViews(unorderedAccess._startingPoint, Count, unorderedAccess._buffers, initialCounts);
    }

    template<int Count1, int Count2> void    DeviceContext::Bind(const ResourceList<RenderTargetView, Count1>& renderTargets, const DepthStencilView* depthStencil, const ResourceList<UnorderedAccessView, Count2>& unorderedAccess)
    {
        const UINT initialCounts[16] = { UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1) };
        assert(Count2 <= dimof(initialCounts));
        _underlying->OMSetRenderTargetsAndUnorderedAccessViews(
            Count1, renderTargets._buffers, depthStencil?depthStencil->GetUnderlying():nullptr,
            Count1 + unorderedAccess._startingPoint, Count2, unorderedAccess._buffers, initialCounts);
    }

    template<> inline void DeviceContext::Unbind<ComputeShader>()   { _underlying->CSSetShader(nullptr, nullptr, 0); }
    template<> inline void DeviceContext::Unbind<HullShader>()      { _underlying->HSSetShader(nullptr, nullptr, 0); }
    template<> inline void DeviceContext::Unbind<DomainShader>()    { _underlying->DSSetShader(nullptr, nullptr, 0); }

}}

