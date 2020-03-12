// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TextureView.h"
#include "ObjectFactory.h"
#include "DeviceContext.h"
#include "Format.h"
#include "Resource.h"
#include "../../RenderUtils.h"
#include "../../Format.h"
#include "DX11Utils.h"

namespace RenderCore { namespace Metal_DX11
{
	static const bool s_allowDefaultViews = true;

    static bool IsDefault(const TextureViewDesc& window)
    {
        return window._format._aspect == TextureViewDesc::UndefinedAspect
            && window._format._explicitFormat == Format(0)
            && window._mipRange._min == TextureViewDesc::All._min && window._mipRange._count == TextureViewDesc::All._count
            && window._arrayLayerRange._min == TextureViewDesc::All._min && window._arrayLayerRange._count == TextureViewDesc::All._count
            && window._dimensionality == TextureDesc::Dimensionality::Undefined
            && window._flags == 0;
    }

    static DXGI_FORMAT ResolveFormat(DXGI_FORMAT baseFormat, TextureViewDesc::FormatFilter filter, FormatUsage usage)
    {
        if (filter._explicitFormat != Format(0))
            return AsDXGIFormat(filter._explicitFormat);

        // common case is everything set to defaults --
        if (filter._aspect == TextureViewDesc::UndefinedAspect)
            return baseFormat;      // (or just use DXGI_FORMAT_UNKNOWN)

        return AsDXGIFormat(ResolveFormat(AsFormat(baseFormat), filter, usage));
    }

    RenderTargetView::RenderTargetView(
		ObjectFactory& factory,
		const std::shared_ptr<IResource>& iresource,
		const TextureViewDesc& window)
    {
		auto* resource = AsID3DResource(*iresource);
        if (!resource) {
            Throw(::Exceptions::BasicLabel("NULL resource passed to RenderTargetView constructor"));
        }

		assert(&factory == &GetObjectFactory(*iresource));

        if (s_allowDefaultViews && IsDefault(window)) {
            _underlying = factory.CreateRenderTargetView(resource);
        } else {
            // Build an D3D11_RENDER_TARGET_VIEW_DESC based on the properties of
            // the resource and the view window.
            D3D11_RENDER_TARGET_VIEW_DESC viewDesc;

            // Note --  here we're exploiting the fact that the Texture1DArray/Texture2DArray/Texture2DMSArray members are overlapping 
            //          supersets of their associated non array forms.
            D3D11_RESOURCE_DIMENSION resType;
            resource->GetType(&resType);
            switch (resType) {
            case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                {
                    TextureDesc1D textureDesc(resource);
                    if (window._arrayLayerRange._min >= textureDesc.ArraySize)
						Throw(std::runtime_error("Array layer range minimum is larger than the total texture array size"));
                    auto arraySize = std::min(textureDesc.ArraySize - window._arrayLayerRange._min, window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._count != 0) || (window._flags & TextureViewDesc::Flags::ForceArray);
                    viewDesc.ViewDimension = selectArrayForm ? D3D11_RTV_DIMENSION_TEXTURE1DARRAY : D3D11_RTV_DIMENSION_TEXTURE1D;
                    viewDesc.Texture1DArray.MipSlice = window._mipRange._min;
                    viewDesc.Texture1DArray.FirstArraySlice = window._arrayLayerRange._min;
                    viewDesc.Texture1DArray.ArraySize = arraySize;
                    viewDesc.Format = ResolveFormat(textureDesc.Format, window._format, FormatUsage::RTV);
                    break;
                }

            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                {
                    TextureDesc2D textureDesc(resource);
                    if (window._arrayLayerRange._min >= textureDesc.ArraySize)
						Throw(std::runtime_error("Array layer range minimum is larger than the total texture array size"));
                    auto arraySize = std::min(textureDesc.ArraySize - window._arrayLayerRange._min, window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._count != 0) || (window._flags & TextureViewDesc::Flags::ForceArray);
                    if (textureDesc.SampleDesc.Count > 1 && !(window._flags & TextureViewDesc::Flags::ForceSingleSample)) {
                        viewDesc.ViewDimension = selectArrayForm ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DMS;
                        viewDesc.Texture2DMSArray.FirstArraySlice = window._arrayLayerRange._min;
                        viewDesc.Texture2DMSArray.ArraySize = arraySize;
                    } else {
                        viewDesc.ViewDimension = selectArrayForm ? D3D11_RTV_DIMENSION_TEXTURE2DARRAY : D3D11_RTV_DIMENSION_TEXTURE2D;
                        viewDesc.Texture2DArray.MipSlice = window._mipRange._min;
                        viewDesc.Texture2DArray.FirstArraySlice = window._arrayLayerRange._min;
                        viewDesc.Texture2DArray.ArraySize = arraySize;
                    }
                    viewDesc.Format = ResolveFormat(textureDesc.Format, window._format, FormatUsage::RTV);
                }
                break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
                {
                    TextureDesc3D textureDesc(resource);
                    viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
                    viewDesc.Texture3D.MipSlice = window._mipRange._min;
                    viewDesc.Texture3D.FirstWSlice = 0;
                    viewDesc.Texture3D.WSize = ~0u;
                    viewDesc.Format = ResolveFormat(textureDesc.Format, window._format, FormatUsage::RTV);
                }
                break;

            case D3D11_RESOURCE_DIMENSION_BUFFER:
                // oddly, we can render to a buffer?
                viewDesc.ViewDimension = D3D11_RTV_DIMENSION_BUFFER;
                viewDesc.Buffer.ElementOffset = 0;
                viewDesc.Buffer.ElementWidth = BitsPerPixel(window._format._explicitFormat) / 8;
                viewDesc.Format = AsDXGIFormat(window._format._explicitFormat);
                break;

            default:
                Throw(::Exceptions::BasicLabel("Invalid resource type used with RenderTargetView"));
            }
            
            _underlying = factory.CreateRenderTargetView(resource, &viewDesc);
        }
    }

	IResourcePtr RenderTargetView::GetResource() const
    {
        return AsResourcePtr(ExtractResource<ID3D::Resource>(_underlying.get()));
    }

    RenderTargetView::RenderTargetView(DeviceContext& context)
    {
        ID3D::RenderTargetView* rawPtr = nullptr; 
        context.GetUnderlying()->OMGetRenderTargets(1, &rawPtr, nullptr);
        _underlying = moveptr(rawPtr);
    }

    DepthStencilView::DepthStencilView(
		ObjectFactory& factory,
		const std::shared_ptr<IResource>& iresource,
		const TextureViewDesc& window)
    {
		auto* resource = AsID3DResource(*iresource);
        if (!resource) {
            Throw(::Exceptions::BasicLabel("NULL resource passed to DepthStencilView constructor"));
        }

		assert(&factory == &GetObjectFactory(*iresource));

        if (s_allowDefaultViews && IsDefault(window)) {
            _underlying = factory.CreateDepthStencilView(resource);
        } else {
            D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc;
            viewDesc.Flags = 0;
            if (window._flags & TextureViewDesc::Flags::JustDepth) viewDesc.Flags |= D3D11_DSV_READ_ONLY_STENCIL;
            if (window._flags & TextureViewDesc::Flags::JustStencil) viewDesc.Flags |= D3D11_DSV_READ_ONLY_DEPTH;

            D3D11_RESOURCE_DIMENSION resType;
            resource->GetType(&resType);
            switch (resType) {
            case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                {
                    TextureDesc1D textureDesc(resource);
                    if (window._arrayLayerRange._min >= textureDesc.ArraySize)
						Throw(std::runtime_error("Array layer range minimum is larger than the total texture array size"));
                    auto arraySize = std::min(textureDesc.ArraySize - window._arrayLayerRange._min, window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._count != 0) || (window._flags & TextureViewDesc::Flags::ForceArray);
                    viewDesc.ViewDimension = selectArrayForm ? D3D11_DSV_DIMENSION_TEXTURE1DARRAY : D3D11_DSV_DIMENSION_TEXTURE1D;
                    viewDesc.Texture1DArray.MipSlice = window._mipRange._min;
                    viewDesc.Texture1DArray.FirstArraySlice = window._arrayLayerRange._min;
                    viewDesc.Texture1DArray.ArraySize = arraySize;
                    viewDesc.Format = ResolveFormat(textureDesc.Format, window._format, FormatUsage::DSV);
                }
                break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                {
                    TextureDesc2D textureDesc(resource);
					if (window._arrayLayerRange._min >= textureDesc.ArraySize)
						Throw(std::runtime_error("Array layer range minimum is larger than the total texture array size"));
                    auto arraySize = std::min(textureDesc.ArraySize - window._arrayLayerRange._min, window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._count != 0) || (window._flags & TextureViewDesc::Flags::ForceArray);
                    if (textureDesc.SampleDesc.Count > 1 && !(window._flags & TextureViewDesc::Flags::ForceSingleSample)) {
                        viewDesc.ViewDimension = selectArrayForm ? D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D11_DSV_DIMENSION_TEXTURE2DMS;
                        viewDesc.Texture2DMSArray.FirstArraySlice = window._arrayLayerRange._min;
                        viewDesc.Texture2DMSArray.ArraySize = arraySize;
                    } else {
                        viewDesc.ViewDimension = selectArrayForm ? D3D11_DSV_DIMENSION_TEXTURE2DARRAY : D3D11_DSV_DIMENSION_TEXTURE2D;
                        viewDesc.Texture2DArray.MipSlice = window._mipRange._min;
                        viewDesc.Texture2DArray.FirstArraySlice = window._arrayLayerRange._min;
                        viewDesc.Texture2DArray.ArraySize = arraySize;
                    }
                    viewDesc.Format = ResolveFormat(textureDesc.Format, window._format, FormatUsage::DSV);
                }
                break;

            default:
                Throw(::Exceptions::BasicLabel("Invalid resource type used with DepthStencilView"));
            }

            _underlying = factory.CreateDepthStencilView(resource, &viewDesc);
        }
    }

    IResourcePtr DepthStencilView::GetResource() const
    {
        return AsResourcePtr(ExtractResource<ID3D::Resource>(_underlying.get()));
    }

    DepthStencilView::DepthStencilView(DeviceContext& context)
    {
        // get the currently bound depth stencil view from the context
        ID3D::DepthStencilView* rawPtr = nullptr; 
        context.GetUnderlying()->OMGetRenderTargets(0, nullptr, &rawPtr);
        _underlying = moveptr(rawPtr);
    }

    UnorderedAccessView::UnorderedAccessView(
		ObjectFactory& factory,
		const std::shared_ptr<IResource>& iresource,
		const TextureViewDesc& window)
    {
		auto* resource = AsID3DResource(*iresource);
        if (!resource) {
            Throw(::Exceptions::BasicLabel("NULL resource passed to UnorderedAccessView constructor"));
        }

		assert(&factory == &GetObjectFactory(*iresource));

        if (s_allowDefaultViews && IsDefault(window)) {
            _underlying = factory.CreateUnorderedAccessView(resource);
        } else {
            D3D11_UNORDERED_ACCESS_VIEW_DESC viewDesc;

            D3D11_RESOURCE_DIMENSION resType;
            resource->GetType(&resType);
            switch (resType) {
            case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                {
                    TextureDesc1D textureDesc(resource);
                    if (window._arrayLayerRange._min >= textureDesc.ArraySize)
						Throw(std::runtime_error("Array layer range minimum is larger than the total texture array size"));
                    auto arraySize = std::min(textureDesc.ArraySize - window._arrayLayerRange._min, window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._count != 0) || (window._flags & TextureViewDesc::Flags::ForceArray);
                    viewDesc.ViewDimension = selectArrayForm ? D3D11_UAV_DIMENSION_TEXTURE1DARRAY : D3D11_UAV_DIMENSION_TEXTURE1D;
                    viewDesc.Texture1DArray.MipSlice = window._mipRange._min;
                    viewDesc.Texture1DArray.FirstArraySlice = window._arrayLayerRange._min;
                    viewDesc.Texture1DArray.ArraySize = arraySize;
                    viewDesc.Format = ResolveFormat(textureDesc.Format, window._format, FormatUsage::UAV);
                    break;
                }

            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                {
                    TextureDesc2D textureDesc(resource);
                    if (window._arrayLayerRange._min >= textureDesc.ArraySize)
						Throw(std::runtime_error("Array layer range minimum is larger than the total texture array size"));
                    auto arraySize = std::min(textureDesc.ArraySize - window._arrayLayerRange._min, window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._count != 0) || (window._flags & TextureViewDesc::Flags::ForceArray);
                    viewDesc.ViewDimension = selectArrayForm ? D3D11_UAV_DIMENSION_TEXTURE2DARRAY : D3D11_UAV_DIMENSION_TEXTURE2D;
                    viewDesc.Texture2DArray.MipSlice = window._mipRange._min;
                    viewDesc.Texture2DArray.FirstArraySlice = window._arrayLayerRange._min;
                    viewDesc.Texture2DArray.ArraySize = arraySize;
                    viewDesc.Format = ResolveFormat(textureDesc.Format, window._format, FormatUsage::UAV);
                }
                break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
                {
                    TextureDesc3D textureDesc(resource);
                    viewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
                    viewDesc.Texture3D.MipSlice = window._mipRange._min;
                    viewDesc.Texture3D.FirstWSlice = 0;
                    viewDesc.Texture3D.WSize = ~0u;
                    viewDesc.Format = ResolveFormat(textureDesc.Format, window._format, FormatUsage::UAV);
                }
                break;

            case D3D11_RESOURCE_DIMENSION_BUFFER:
                {
                    D3DBufferDesc bufferDesc(resource);
                    viewDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
                    viewDesc.Buffer.FirstElement = 0;
                    viewDesc.Buffer.NumElements = bufferDesc.StructureByteStride ? (bufferDesc.ByteWidth/bufferDesc.StructureByteStride) : bufferDesc.ByteWidth;
                    viewDesc.Buffer.Flags = 0;
                    if (window._flags & TextureViewDesc::Flags::AppendBuffer) viewDesc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_APPEND;
                    if (window._flags & TextureViewDesc::Flags::AttachedCounter) viewDesc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_COUNTER;
                    viewDesc.Format = AsDXGIFormat(window._format._explicitFormat);
                }
                break;

            default:
                Throw(::Exceptions::BasicLabel("Invalid resource type used with RenderTargetView"));
            }
            _underlying = GetObjectFactory(*resource).CreateUnorderedAccessView(resource, &viewDesc);
        }
    }

    IResourcePtr UnorderedAccessView::GetResource() const
    {
        return AsResourcePtr(ExtractResource<ID3D::Resource>(_underlying.get()));
    }

	ShaderResourceView::ShaderResourceView(
		ObjectFactory& factory,
		const std::shared_ptr<IResource>& iresource,
		const TextureViewDesc& window)
	{
		// note --	for Vulkan compatibility, this should change so that array resources
		//			get array views by default (unless it's disabled somehow via the window)

		auto* resource = AsID3DResource(*iresource); 
		if (!resource)
			Throw(::Exceptions::BasicLabel("Null resource passed to ShaderResourceView constructor"));

		assert(&factory == &GetObjectFactory(*iresource));

        if (s_allowDefaultViews && IsDefault(window)) {
            _underlying = factory.CreateShaderResourceView(resource);
        } else {
            D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;

            D3D11_RESOURCE_DIMENSION resType;
            resource->GetType(&resType);
            switch (resType) {
            case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                {
                    TextureDesc1D textureDesc(resource);
                    if (window._arrayLayerRange._min >= textureDesc.ArraySize)
						Throw(std::runtime_error("Array layer range minimum is larger than the total texture array size"));
                    auto arraySize = std::min(textureDesc.ArraySize - window._arrayLayerRange._min, window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._count != 0) || (window._flags & TextureViewDesc::Flags::ForceArray);
                    viewDesc.ViewDimension = selectArrayForm ? D3D11_SRV_DIMENSION_TEXTURE1DARRAY : D3D11_SRV_DIMENSION_TEXTURE1D;
                    viewDesc.Texture1DArray.MostDetailedMip = window._mipRange._min;
                    viewDesc.Texture1DArray.MipLevels = window._mipRange._count;
                    viewDesc.Texture1DArray.FirstArraySlice = window._arrayLayerRange._min;
                    viewDesc.Texture1DArray.ArraySize = arraySize;
                    viewDesc.Format = ResolveFormat(textureDesc.Format, window._format, FormatUsage::SRV);
                    break;
                }

            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                {
                    TextureDesc2D textureDesc(resource);
                    if (window._arrayLayerRange._min >= textureDesc.ArraySize)
						Throw(std::runtime_error("Array layer range minimum is larger than the total texture array size"));
                    auto arraySize = std::min(textureDesc.ArraySize - window._arrayLayerRange._min, window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._count != 0) || (window._flags & TextureViewDesc::Flags::ForceArray);
                    if (textureDesc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) {
                        viewDesc.ViewDimension = selectArrayForm ? D3D11_SRV_DIMENSION_TEXTURECUBEARRAY : D3D11_SRV_DIMENSION_TEXTURECUBE;
                        viewDesc.TextureCubeArray.MostDetailedMip = window._mipRange._min;
                        viewDesc.TextureCubeArray.MipLevels = window._mipRange._count;
                        viewDesc.TextureCubeArray.First2DArrayFace = 0;
                        viewDesc.TextureCubeArray.NumCubes = std::max(1u, arraySize/6u); // ?
                    } else {
                        if (textureDesc.SampleDesc.Count > 1 && !(window._flags & TextureViewDesc::Flags::ForceSingleSample)) {
                            viewDesc.ViewDimension = selectArrayForm ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DMS;
                            viewDesc.Texture2DMSArray.FirstArraySlice = window._arrayLayerRange._min;
                            viewDesc.Texture2DMSArray.ArraySize = arraySize;
                        } else {
                            viewDesc.ViewDimension = selectArrayForm ? D3D11_SRV_DIMENSION_TEXTURE2DARRAY : D3D11_SRV_DIMENSION_TEXTURE2D;
                            viewDesc.Texture2DArray.MostDetailedMip = window._mipRange._min;
                            viewDesc.Texture2DArray.MipLevels = window._mipRange._count;
                            viewDesc.Texture2DArray.FirstArraySlice = window._arrayLayerRange._min;
                            viewDesc.Texture2DArray.ArraySize = arraySize;
                        }
                    }
                    viewDesc.Format = ResolveFormat(textureDesc.Format, window._format, FormatUsage::SRV);
                }
                break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
                {
                    TextureDesc3D textureDesc(resource);
                    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                    viewDesc.Texture3D.MostDetailedMip = window._mipRange._min;
                    viewDesc.Texture3D.MipLevels = window._mipRange._count;
                    viewDesc.Format = ResolveFormat(textureDesc.Format, window._format, FormatUsage::SRV);
                }
                break;

            case D3D11_RESOURCE_DIMENSION_BUFFER:
                {
                    D3DBufferDesc bufferDesc(resource);
                    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
                    viewDesc.BufferEx.FirstElement = 0;
                    viewDesc.BufferEx.NumElements = bufferDesc.StructureByteStride ? (bufferDesc.ByteWidth / bufferDesc.StructureByteStride) : (bufferDesc.ByteWidth/4);
                    viewDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;      // (always raw currently?)
                    viewDesc.Format = AsDXGIFormat(window._format._explicitFormat);
                }
                break;

            default:
                Throw(::Exceptions::BasicLabel("Invalid resource type used with RenderTargetView"));
            }

            _underlying = factory.CreateShaderResourceView(resource, &viewDesc);
        }
    }

    ShaderResourceView ShaderResourceView::RawBuffer(
		const std::shared_ptr<IResource>& resource,
		unsigned sizeBytes, unsigned offsetBytes)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        srvDesc.BufferEx.FirstElement = offsetBytes / 4;
        srvDesc.BufferEx.NumElements = sizeBytes / 4;
        srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
        auto r = AsID3DResource(*resource);
        return ShaderResourceView(
            GetObjectFactory(*resource).CreateShaderResourceView(r, &srvDesc));
    }

	IResourcePtr ShaderResourceView::GetResource() const
    {
        return AsResourcePtr(ExtractResource<ID3D::Resource>(_underlying.get()));
    }

//////////////////////////////////////////////////////////////////////////////////////////////////////

	// Operators moved into here to avoid making D3D headers a dependency of TextureView.h

	RenderTargetView::RenderTargetView(intrusive_ptr<ID3D::RenderTargetView>&& resource) : _underlying(std::move(resource)) {}
	RenderTargetView::RenderTargetView(const intrusive_ptr<ID3D::RenderTargetView>& resource) : _underlying(resource) {}
	RenderTargetView::RenderTargetView() {}
	RenderTargetView::~RenderTargetView() {}
	RenderTargetView::RenderTargetView(const RenderTargetView& cloneFrom) : _underlying(cloneFrom._underlying) {}
	RenderTargetView::RenderTargetView(RenderTargetView&& moveFrom) never_throws : _underlying(std::move(moveFrom._underlying)) {}
	RenderTargetView::RenderTargetView(const std::shared_ptr<IResource>& resource, const TextureViewDesc& window)
	: RenderTargetView(GetObjectFactory(*resource), resource, window)
	{
	}
	RenderTargetView& RenderTargetView::operator=(const RenderTargetView& cloneFrom)
	{
		_underlying = cloneFrom._underlying;
		return *this;
	}
	RenderTargetView& RenderTargetView::operator=(RenderTargetView&& moveFrom) never_throws
	{
		_underlying = std::move(moveFrom._underlying);
		return *this;
	}

////////////////////////////////////////////////////////////////////////////////////////////

	DepthStencilView::DepthStencilView(intrusive_ptr<ID3D::DepthStencilView>&& resource) : _underlying(std::move(resource)) {}
	DepthStencilView::DepthStencilView(const intrusive_ptr<ID3D::DepthStencilView>& resource) : _underlying(resource) {}
	DepthStencilView::DepthStencilView() {}
	DepthStencilView::~DepthStencilView() {}
	DepthStencilView::DepthStencilView(const DepthStencilView& cloneFrom) : _underlying(cloneFrom._underlying) {}
	DepthStencilView::DepthStencilView(DepthStencilView&& moveFrom) never_throws : _underlying(std::move(moveFrom._underlying)) {}
	DepthStencilView::DepthStencilView(const std::shared_ptr<IResource>& resource, const TextureViewDesc& window)
	: DepthStencilView(GetObjectFactory(*resource), resource, window)
	{
	}
	DepthStencilView& DepthStencilView::operator=(const DepthStencilView& cloneFrom)
	{
		_underlying = cloneFrom._underlying;
		return *this;
	}
	DepthStencilView& DepthStencilView::operator=(DepthStencilView&& moveFrom) never_throws
	{
		_underlying = std::move(moveFrom._underlying);
		return *this;
	}

////////////////////////////////////////////////////////////////////////////////////////////

	UnorderedAccessView::UnorderedAccessView(intrusive_ptr<ID3D::UnorderedAccessView>&& resource) : _underlying(std::move(resource)) {}
	UnorderedAccessView::UnorderedAccessView(const intrusive_ptr<ID3D::UnorderedAccessView>& resource) : _underlying(resource) {}
	UnorderedAccessView::UnorderedAccessView() {}
	UnorderedAccessView::~UnorderedAccessView() {}
	UnorderedAccessView::UnorderedAccessView(const UnorderedAccessView& cloneFrom) : _underlying(cloneFrom._underlying) {}
	UnorderedAccessView::UnorderedAccessView(UnorderedAccessView&& moveFrom) never_throws : _underlying(std::move(moveFrom._underlying)) {}
	UnorderedAccessView::UnorderedAccessView(const std::shared_ptr<IResource>& resource, const TextureViewDesc& window)
	: UnorderedAccessView(GetObjectFactory(*resource), resource, window)
	{
	}
	UnorderedAccessView& UnorderedAccessView::operator=(const UnorderedAccessView& cloneFrom)
	{
		_underlying = cloneFrom._underlying;
		return *this;
	}
	UnorderedAccessView& UnorderedAccessView::operator=(UnorderedAccessView&& moveFrom) never_throws
	{
		_underlying = std::move(moveFrom._underlying);
		return *this;
	}

////////////////////////////////////////////////////////////////////////////////////////////

	ShaderResourceView::ShaderResourceView(intrusive_ptr<ID3D::ShaderResourceView>&& resource) : _underlying(std::move(resource)) {}
	ShaderResourceView::ShaderResourceView(const intrusive_ptr<ID3D::ShaderResourceView>& resource) : _underlying(resource) {}
	ShaderResourceView::ShaderResourceView() {}
	ShaderResourceView::~ShaderResourceView() {}
	ShaderResourceView::ShaderResourceView(const ShaderResourceView& cloneFrom) : _underlying(cloneFrom._underlying) {}
	ShaderResourceView::ShaderResourceView(ShaderResourceView&& moveFrom) never_throws : _underlying(std::move(moveFrom._underlying)) {}
	ShaderResourceView::ShaderResourceView(const std::shared_ptr<IResource>& resource, const TextureViewDesc& window)
	: ShaderResourceView(GetObjectFactory(*resource), resource, window)
	{
	}
	ShaderResourceView& ShaderResourceView::operator=(const ShaderResourceView& cloneFrom)
	{
		_underlying = cloneFrom._underlying;
		return *this;
	}
	ShaderResourceView& ShaderResourceView::operator=(ShaderResourceView&& moveFrom) never_throws
	{
		_underlying = std::move(moveFrom._underlying);
		return *this;
	}


}}

