// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeviceContext.h"
#include "IncludeDX11.h"     // (we need <DX11.h> because there is some inlined implementation in this file)

namespace RenderCore { namespace Metal_DX11
{
	template<int Count, typename Type, typename UnderlyingType>
		void CopyArrayOfUnderlying(UnderlyingType* (&output)[Count], const ResourceList<Type, Count>& input)
		{
			for (unsigned c=0; c<Count; ++c) {
				output[c] = input._buffers[c]->GetUnderlying();
			}
		}

    ///////////////////////////////////////////////////////////////////////////////////////////
    template<int Count> void DeviceContext::BindVS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
		ID3D::ShaderResourceView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, shaderResources);
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[0][shaderResources._startingPoint+c] = underlyings[c];
        _underlying->VSSetShaderResources(shaderResources._startingPoint, Count, underlyings);
    }
    
    template<int Count> void DeviceContext::BindPS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
		ID3D::ShaderResourceView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, shaderResources);
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[1][shaderResources._startingPoint+c] = underlyings[c];
        _underlying->PSSetShaderResources(shaderResources._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindCS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
		ID3D::ShaderResourceView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, shaderResources);
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[5][shaderResources._startingPoint+c] = underlyings[c];
        _underlying->CSSetShaderResources(shaderResources._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindGS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
		ID3D::ShaderResourceView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, shaderResources);
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[2][shaderResources._startingPoint+c] = underlyings[c];
        _underlying->GSSetShaderResources(shaderResources._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindHS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
		ID3D::ShaderResourceView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, shaderResources);
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[3][shaderResources._startingPoint+c] = underlyings[c];
        _underlying->HSSetShaderResources(shaderResources._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindDS(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
		ID3D::ShaderResourceView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, shaderResources);
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[4][shaderResources._startingPoint+c] = underlyings[c];
        _underlying->DSSetShaderResources(shaderResources._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindVS(const ResourceList<SamplerState, Count>& samplerStates)
    {
		ID3D::SamplerState* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, samplerStates);
        _underlying->VSSetSamplers(samplerStates._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindPS(const ResourceList<SamplerState, Count>& samplerStates)
    {
		ID3D::SamplerState* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, samplerStates);
        _underlying->PSSetSamplers(samplerStates._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindGS(const ResourceList<SamplerState, Count>& samplerStates)
    {
		ID3D::SamplerState* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, samplerStates);
        _underlying->GSSetSamplers(samplerStates._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindCS(const ResourceList<SamplerState, Count>& samplerStates)
    {
		ID3D::SamplerState* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, samplerStates);
        _underlying->CSSetSamplers(samplerStates._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindHS(const ResourceList<SamplerState, Count>& samplerStates)
    {
		ID3D::SamplerState* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, samplerStates);
        _underlying->HSSetSamplers(samplerStates._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindDS(const ResourceList<SamplerState, Count>& samplerStates)
    {
		ID3D::SamplerState* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, samplerStates);
        _underlying->DSSetSamplers(samplerStates._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindVS(const ResourceList<Buffer, Count>& constantBuffers)
    {
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, constantBuffers);
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[0][constantBuffers._startingPoint+c] = underlyings[c];
        _underlying->VSSetConstantBuffers(constantBuffers._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindPS(const ResourceList<Buffer, Count>& constantBuffers)
    {
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, constantBuffers);
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[1][constantBuffers._startingPoint+c] = underlyings[c];
        _underlying->PSSetConstantBuffers(constantBuffers._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindCS(const ResourceList<Buffer, Count>& constantBuffers)
    {
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, constantBuffers);
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[5][constantBuffers._startingPoint+c] = underlyings[c];
        _underlying->CSSetConstantBuffers(constantBuffers._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindGS(const ResourceList<Buffer, Count>& constantBuffers)
    {
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, constantBuffers);
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[2][constantBuffers._startingPoint+c] = underlyings[c];
        _underlying->GSSetConstantBuffers(constantBuffers._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindHS(const ResourceList<Buffer, Count>& constantBuffers)
    {
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, constantBuffers);
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[3][constantBuffers._startingPoint+c] = underlyings[c];
        _underlying->HSSetConstantBuffers(constantBuffers._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindDS(const ResourceList<Buffer, Count>& constantBuffers)
    {
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, constantBuffers);
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[4][constantBuffers._startingPoint+c] = underlyings[c];
        _underlying->DSSetConstantBuffers(constantBuffers._startingPoint, Count, underlyings);
    }
    ///////////////////////////////////////////////////////////////////////////////////////////
    template<int Count> void DeviceContext::BindVS_G(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
		ID3D::ShaderResourceView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, shaderResources);
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[0][shaderResources._startingPoint+c] = underlyings[c];
        _underlying->VSSetShaderResources(shaderResources._startingPoint, Count, underlyings);
    }
    
    template<int Count> void DeviceContext::BindPS_G(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
		ID3D::ShaderResourceView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, shaderResources);
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[1][shaderResources._startingPoint+c] = underlyings[c];
        _underlying->PSSetShaderResources(shaderResources._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindCS_G(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
		ID3D::ShaderResourceView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, shaderResources);
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[5][shaderResources._startingPoint+c] = underlyings[c];
        _underlying->CSSetShaderResources(shaderResources._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindGS_G(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
		ID3D::ShaderResourceView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, shaderResources);
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[2][shaderResources._startingPoint+c] = underlyings[c];
        _underlying->GSSetShaderResources(shaderResources._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindHS_G(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
		ID3D::ShaderResourceView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, shaderResources);
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[3][shaderResources._startingPoint+c] = underlyings[c];
        _underlying->HSSetShaderResources(shaderResources._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindDS_G(const ResourceList<ShaderResourceView, Count>& shaderResources)
    {
		ID3D::ShaderResourceView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, shaderResources);
        for (unsigned c=0; c<Count; ++c)
            _currentSRVs[4][shaderResources._startingPoint+c] = underlyings[c];
        _underlying->DSSetShaderResources(shaderResources._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindVS_G(const ResourceList<SamplerState, Count>& samplerStates)
    {
		ID3D::SamplerState* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, samplerStates);
        _underlying->VSSetSamplers(samplerStates._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindPS_G(const ResourceList<SamplerState, Count>& samplerStates)
    {
		ID3D::SamplerState* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, samplerStates);
        _underlying->PSSetSamplers(samplerStates._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindGS_G(const ResourceList<SamplerState, Count>& samplerStates)
    {
		ID3D::SamplerState* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, samplerStates);
        _underlying->GSSetSamplers(samplerStates._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindCS_G(const ResourceList<SamplerState, Count>& samplerStates)
    {
		ID3D::SamplerState* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, samplerStates);
        _underlying->CSSetSamplers(samplerStates._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindHS_G(const ResourceList<SamplerState, Count>& samplerStates)
    {
		ID3D::SamplerState* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, samplerStates);
        _underlying->HSSetSamplers(samplerStates._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindDS_G(const ResourceList<SamplerState, Count>& samplerStates)
    {
		ID3D::SamplerState* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, samplerStates);
        _underlying->DSSetSamplers(samplerStates._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindVS_G(const ResourceList<Buffer, Count>& constantBuffers)
    {
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, constantBuffers);
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[0][constantBuffers._startingPoint+c] = underlyings[c];
        _underlying->VSSetConstantBuffers(constantBuffers._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindPS_G(const ResourceList<Buffer, Count>& constantBuffers)
    {
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, constantBuffers);
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[1][constantBuffers._startingPoint+c] = underlyings[c];
        _underlying->PSSetConstantBuffers(constantBuffers._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindCS_G(const ResourceList<Buffer, Count>& constantBuffers)
    {
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, constantBuffers);
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[5][constantBuffers._startingPoint+c] = underlyings[c];
        _underlying->CSSetConstantBuffers(constantBuffers._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindGS_G(const ResourceList<Buffer, Count>& constantBuffers)
    {
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, constantBuffers);
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[2][constantBuffers._startingPoint+c] = underlyings[c];
        _underlying->GSSetConstantBuffers(constantBuffers._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindHS_G(const ResourceList<Buffer, Count>& constantBuffers)
    {
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, constantBuffers);
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[3][constantBuffers._startingPoint+c] = underlyings[c];
        _underlying->HSSetConstantBuffers(constantBuffers._startingPoint, Count, underlyings);
    }

    template<int Count> void DeviceContext::BindDS_G(const ResourceList<Buffer, Count>& constantBuffers)
    {
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, constantBuffers);
        for (unsigned c=0; c<Count; ++c)
            _currentCBs[4][constantBuffers._startingPoint+c] = underlyings[c];
        _underlying->DSSetConstantBuffers(constantBuffers._startingPoint, Count, underlyings);
    }
    ///////////////////////////////////////////////////////////////////////////////////////////

    template<int Count> void DeviceContext::Bind(const ResourceList<RenderTargetView, Count>& renderTargets, const DepthStencilView* depthStencil)
    {
		ID3D::RenderTargetView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, renderTargets);
        assert(renderTargets._startingPoint == 0);
        _underlying->OMSetRenderTargets(Count, underlyings, depthStencil?depthStencil->GetUnderlying():nullptr);
    }

	template<> void DeviceContext::Bind(const ResourceList<RenderTargetView, 0>& renderTargets, const DepthStencilView* depthStencil)
	{
		assert(renderTargets._startingPoint == 0);
		_underlying->OMSetRenderTargets(0, nullptr, depthStencil ? depthStencil->GetUnderlying() : nullptr);
	}

    template<int Count> void DeviceContext::BindCS(const ResourceList<UnorderedAccessView, Count>& unorderedAccess)
    {
		ID3D::UnorderedAccessView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, unorderedAccess);
        const UINT initialCounts[16] = { UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1) };
        assert(Count <= dimof(initialCounts));
        _underlying->CSSetUnorderedAccessViews(unorderedAccess._startingPoint, Count, underlyings, initialCounts);
    }

    template<int Count1, int Count2> void    DeviceContext::Bind(const ResourceList<RenderTargetView, Count1>& renderTargets, const DepthStencilView* depthStencil, const ResourceList<UnorderedAccessView, Count2>& unorderedAccess)
    {
		ID3D::RenderTargetView* underlyings[Count1];
		CopyArrayOfUnderlying(underlyings, renderTargets);
		ID3D::UnorderedAccessView* uavUnderlyings[Count2];
		CopyArrayOfUnderlying(uavUnderlyings, unorderedAccess);
        const UINT initialCounts[16] = { UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1) };
        assert(Count2 <= dimof(initialCounts));
        _underlying->OMSetRenderTargetsAndUnorderedAccessViews(
            Count1, underlyings, depthStencil?depthStencil->GetUnderlying():nullptr,
            Count1 + unorderedAccess._startingPoint, Count2, uavUnderlyings, initialCounts);
    }

    template<int Count> void DeviceContext::BindSO(const ResourceList<Buffer, Count>& buffers, unsigned offset)
    {
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, buffers);
        UINT offsets[Count];
        std::fill(offsets, &offsets[dimof(offsets)], offset);
        assert(buffers._startingPoint==0);
        _underlying->SOSetTargets(Count, underlyings, offsets);
    }


}}

