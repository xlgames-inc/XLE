// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Resource.h"
#include "DeviceContext.h"
#include "DX11Utils.h"

namespace RenderCore { namespace Metal_DX11
{
	void Resource::SetImageLayout(
		DeviceContext& context, ImageLayout oldLayout, ImageLayout newLayout) {}

	Resource::Resource(
		const ObjectFactory& factory, const Desc& desc,
		const void* initData, size_t initDataSize)
	{}

	Resource::Resource() {}

	Resource::~Resource() {}

    void Copy(DeviceContext& context, ID3D::Resource* dst, ID3D::Resource* src)
    {
        context.GetUnderlying()->CopyResource(dst, src);
    }

	void Copy(DeviceContext& context, Resource& dst, Resource& src, ImageLayout dstLayout, ImageLayout srcLayout)
	{
		Copy(context, dst.GetImage(), src.GetImage());
	}

    void CopyPartial(
        DeviceContext& context, 
        const CopyPartial_Dest& dst, const CopyPartial_Src& src)
    {
        bool useSrcBox = false;
        D3D11_BOX srcBox;
        if (src._leftTopFront._x != ~0u || src._rightBottomBack._x != ~0u) {
            srcBox = D3D11_BOX { 
                src._leftTopFront._x, src._leftTopFront._y, src._leftTopFront._z,
                src._rightBottomBack._x, src._rightBottomBack._y, src._rightBottomBack._z
            };
            useSrcBox = true;
        }

        context.GetUnderlying()->CopySubresourceRegion(
            dst._resource, dst._subResource,
            dst._leftTopFront._x, dst._leftTopFront._y, dst._leftTopFront._z,
            src._resource, src._subResource,
            useSrcBox ? &srcBox : nullptr);
    }

    intrusive_ptr<ID3D::Resource> Duplicate(DeviceContext& context, ID3D::Resource* inputResource)
    {
        return DuplicateResource(context.GetUnderlying(), inputResource);
    }
}}
