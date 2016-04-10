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
#if 0
	void Resource::SetImageLayout(
		DeviceContext& context, ImageLayout oldLayout, ImageLayout newLayout) {}
	Resource::Resource(
		const ObjectFactory& factory, const Desc& desc,
		const void* initData, size_t initDataSize)
	{}
	Resource::Resource() {}
	Resource::~Resource() {}
#endif

	void Copy(
		DeviceContext& context, 
		UnderlyingResourcePtr dst, UnderlyingResourcePtr src,
		ImageLayout dstLayout, ImageLayout srcLayout)
	{
		context.GetUnderlying()->CopyResource(dst.get(), src.get());
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

	void SetImageLayout(
		DeviceContext& context, UnderlyingResourcePtr res,
		ImageLayout oldLayout, ImageLayout newLayout)
	{

	}

	ResourceDesc ExtractDesc(UnderlyingResourcePtr res)
	{
		return ResourceDesc();
	}

	ID3D::Resource* AsID3DResource(UnderlyingResourcePtr res)
	{
		return res.get();
	}

	RenderCore::ResourcePtr AsResourcePtr(ID3D::Resource* res)
	{
		res->AddRef();	// balanced by the release on destroying the new pointer
		return ResourcePtr(
			(Resource*)res,
			[](Resource* r) { ((ID3D::Resource*)r)->Release(); });
	}

	RenderCore::ResourcePtr AsResourcePtr(intrusive_ptr<ID3D::Resource>&& ptr)
	{
		// moving the reference count out of the given ptr (ie, no AddRef required)
		return ResourcePtr(
			(Resource*)ReleaseOwnership(ptr),
			[](Resource* r) { ((ID3D::Resource*)r)->Release(); });
	}
}}
