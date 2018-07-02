// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DeviceContext.h"
#include "IncludeDX11.h"     // (we need <DX11.h> because there is some inlined implementation in this file)

namespace RenderCore { namespace Metal_DX11
{

    template<int Count> void DeviceContext::Bind(const ResourceList<RenderTargetView, Count>& renderTargets, const DepthStencilView* depthStencil)
    {
		ID3D::RenderTargetView* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, renderTargets);
        assert(renderTargets._startingPoint == 0);
        _underlying->OMSetRenderTargets(Count, underlyings, depthStencil?depthStencil->GetUnderlying():nullptr);
    }

	template<> inline void DeviceContext::Bind(const ResourceList<RenderTargetView, 0>& renderTargets, const DepthStencilView* depthStencil)
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

    /*template<int Count> void DeviceContext::BindSO(const ResourceList<Buffer, Count>& buffers, unsigned offset)
    {
		ID3D::Buffer* underlyings[Count];
		CopyArrayOfUnderlying(underlyings, buffers);
        UINT offsets[Count];
        std::fill(offsets, &offsets[dimof(offsets)], offset);
        assert(buffers._startingPoint==0);
        _underlying->SOSetTargets(Count, underlyings, offsets);
    }*/

}}

