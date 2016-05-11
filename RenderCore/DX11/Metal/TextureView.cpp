// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TextureView.h"
#include "ObjectFactory.h"
#include "DeviceContext.h"
#include "Format.h"
#include "../../RenderUtils.h"
#include "../../Format.h"
#include "DX11Utils.h"

namespace RenderCore { namespace Metal_DX11
{
    static bool IsDefault(const TextureViewWindow& window)
    {
        return window._format._aspect == TextureViewWindow::UndefinedAspect
            && window._format._explicitFormat == Format(0)
            && window._mipRange._min == TextureViewWindow::All._min && window._mipRange._count == TextureViewWindow::All._count
            && window._arrayLayerRange._min == TextureViewWindow::All._min && window._arrayLayerRange._count == TextureViewWindow::All._count
            && window._dimensionality == TextureDesc::Dimensionality::Undefined
            && window._flags == 0;
    }

    static DXGI_FORMAT SpecializeFormat(DXGI_FORMAT baseFormat, TextureViewWindow::FormatFilter filter, bool isDSV = false)
    {
        if (filter._explicitFormat != Format(0))
            return AsDXGIFormat(filter._explicitFormat);

        // common case is everything set to defaults --
        if (filter._aspect == TextureViewWindow::UndefinedAspect)
            return baseFormat;      // (or just use DXGI_FORMAT_UNKNOWN)

        // We need to filter the format by aspect and color space.
        // First, check if there are linear & SRGB versions of the format. If there are, we can ignore the "aspect" filter,
        // because these formats only have color aspects
        Format fmt = AsFormat(baseFormat);
        switch (filter._aspect) {
        default:
            return baseFormat;

        case TextureViewWindow::DepthStencil:
        case TextureViewWindow::Depth:
            if (isDSV) return AsDXGIFormat(AsDepthStencilFormat(fmt));
            return AsDXGIFormat(AsDepthAspectSRVFormat(fmt));

        case TextureViewWindow::Stencil:
            if (isDSV) return AsDXGIFormat(AsDepthStencilFormat(fmt));
            return AsDXGIFormat(AsStencilAspectSRVFormat(fmt));

        case TextureViewWindow::ColorLinear:
            return AsDXGIFormat(AsLinearFormat(fmt));

        case TextureViewWindow::ColorSRGB:
            return AsDXGIFormat(AsSRGBFormat(fmt));
        }
    }

    RenderTargetView::RenderTargetView(
        const ObjectFactory& factory,
		UnderlyingResourcePtr resource,
		const TextureViewWindow& window)
    {
        if (!resource.get()) {
            Throw(::Exceptions::BasicLabel("NULL resource passed to RenderTargetView constructor"));
        }

        if (IsDefault(window)) {
            _underlying = factory.CreateRenderTargetView(resource.get());
        } else {
            // Build an D3D11_RENDER_TARGET_VIEW_DESC based on the properties of
            // the resource and the view window.
            D3D11_RENDER_TARGET_VIEW_DESC viewDesc;

            // Note --  here we're exploiting the fact that the Texture1DArray/Texture2DArray/Texture2DMSArray members are overlapping 
            //          supersets of their associated non array forms.
            D3D11_RESOURCE_DIMENSION resType;
            resource.get()->GetType(&resType);
            switch (resType) {
            case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                {
                    TextureDesc1D textureDesc(resource.get());
                    auto arraySize = std::min((int)textureDesc.ArraySize - (int)window._arrayLayerRange._min, (int)window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._min == 0) || (window._flags & TextureViewWindow::Flags::ForceArray);
                    viewDesc.ViewDimension = selectArrayForm ? D3D11_RTV_DIMENSION_TEXTURE1DARRAY : D3D11_RTV_DIMENSION_TEXTURE1D;
                    viewDesc.Texture1DArray.MipSlice = window._mipRange._min;
                    viewDesc.Texture1DArray.FirstArraySlice = window._arrayLayerRange._min;
                    viewDesc.Texture1DArray.ArraySize = arraySize;
                    viewDesc.Format = SpecializeFormat(textureDesc.Format, window._format);
                    break;
                }

            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                {
                    TextureDesc2D textureDesc(resource.get());
                    auto arraySize = std::min((int)textureDesc.ArraySize - (int)window._arrayLayerRange._min, (int)window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._min == 0) || (window._flags & TextureViewWindow::Flags::ForceArray);
                    if (textureDesc.SampleDesc.Count > 1 && !(window._flags & TextureViewWindow::Flags::ForceSingleSample)) {
                        viewDesc.ViewDimension = selectArrayForm ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DMS;
                        viewDesc.Texture2DMSArray.FirstArraySlice = window._arrayLayerRange._min;
                        viewDesc.Texture2DMSArray.ArraySize = arraySize;
                    } else {
                        viewDesc.ViewDimension = selectArrayForm ? D3D11_RTV_DIMENSION_TEXTURE2DARRAY : D3D11_RTV_DIMENSION_TEXTURE2D;
                        viewDesc.Texture2DArray.MipSlice = window._mipRange._min;
                        viewDesc.Texture2DArray.FirstArraySlice = window._arrayLayerRange._min;
                        viewDesc.Texture2DArray.ArraySize = arraySize;
                    }
                    viewDesc.Format = SpecializeFormat(textureDesc.Format, window._format);
                }
                break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
                {
                    TextureDesc3D textureDesc(resource.get());
                    viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
                    viewDesc.Texture3D.MipSlice = window._mipRange._min;
                    viewDesc.Texture3D.FirstWSlice = 0;
                    viewDesc.Texture3D.WSize = ~0u;
                    viewDesc.Format = SpecializeFormat(textureDesc.Format, window._format);
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
            
            _underlying = factory.CreateRenderTargetView(resource.get(), &viewDesc);
        }
    }

    RenderTargetView::RenderTargetView(UnderlyingResourcePtr resource, const TextureViewWindow& window)
    : RenderTargetView(GetObjectFactory(*resource.get()), resource, window)
    {}

    auto RenderTargetView::GetResource() const -> intrusive_ptr<ID3D::Resource>
    {
        return ExtractResource<ID3D::Resource>(_underlying.get());
    }

    ResourcePtr RenderTargetView::ShareResource() const
    {
        return AsResourcePtr(GetResource());
    }

    RenderTargetView::RenderTargetView(ID3D::RenderTargetView* resource)
    : _underlying(resource)
    {
    }

    RenderTargetView::RenderTargetView(MovePTRHelper<ID3D::RenderTargetView> resource)
    : _underlying(resource)
    {
    }

    RenderTargetView::RenderTargetView(DeviceContext& context)
    {
        ID3D::RenderTargetView* rawPtr = nullptr; 
        context.GetUnderlying()->OMGetRenderTargets(1, &rawPtr, nullptr);
        _underlying = moveptr(rawPtr);
    }

    RenderTargetView::RenderTargetView() {}

    RenderTargetView::~RenderTargetView() {}

    RenderTargetView::RenderTargetView(const RenderTargetView& cloneFrom) : _underlying(cloneFrom._underlying) {}
    RenderTargetView& RenderTargetView::operator=(const RenderTargetView& cloneFrom) { _underlying = cloneFrom._underlying; return *this; }

    RenderTargetView::RenderTargetView(RenderTargetView&& moveFrom) never_throws : _underlying(std::move(moveFrom._underlying)) {}
    RenderTargetView& RenderTargetView::operator=(RenderTargetView&& moveFrom) never_throws { _underlying = std::move(moveFrom._underlying); return *this; }


    DepthStencilView::DepthStencilView(
        const ObjectFactory& factory,
		UnderlyingResourcePtr resource,
		const TextureViewWindow& window)
    {
        if (!resource.get()) {
            Throw(::Exceptions::BasicLabel("NULL resource passed to DepthStencilView constructor"));
        }

        if (IsDefault(window)) {
            _underlying = factory.CreateDepthStencilView(resource.get());
        } else {
            D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc;
            viewDesc.Flags = 0;
            if (window._flags & TextureViewWindow::Flags::JustDepth) viewDesc.Flags |= D3D11_DSV_READ_ONLY_STENCIL;
            if (window._flags & TextureViewWindow::Flags::JustStencil) viewDesc.Flags |= D3D11_DSV_READ_ONLY_DEPTH;

            D3D11_RESOURCE_DIMENSION resType;
            resource.get()->GetType(&resType);
            switch (resType) {
            case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                {
                    TextureDesc1D textureDesc(resource.get());
                    auto arraySize = std::min((int)textureDesc.ArraySize - (int)window._arrayLayerRange._min, (int)window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._min == 0) || (window._flags & TextureViewWindow::Flags::ForceArray);
                    viewDesc.ViewDimension = selectArrayForm ? D3D11_DSV_DIMENSION_TEXTURE1DARRAY : D3D11_DSV_DIMENSION_TEXTURE1D;
                    viewDesc.Texture1DArray.MipSlice = window._mipRange._min;
                    viewDesc.Texture1DArray.FirstArraySlice = window._arrayLayerRange._min;
                    viewDesc.Texture1DArray.ArraySize = arraySize;
                    viewDesc.Format = SpecializeFormat(textureDesc.Format, window._format, true);
                }
                break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                {
                    TextureDesc2D textureDesc(resource.get());
                    auto arraySize = std::min((int)textureDesc.ArraySize - (int)window._arrayLayerRange._min, (int)window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._min == 0) || (window._flags & TextureViewWindow::Flags::ForceArray);
                    if (textureDesc.SampleDesc.Count > 1 && !(window._flags & TextureViewWindow::Flags::ForceSingleSample)) {
                        viewDesc.ViewDimension = selectArrayForm ? D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D11_DSV_DIMENSION_TEXTURE2DMS;
                        viewDesc.Texture2DMSArray.FirstArraySlice = window._arrayLayerRange._min;
                        viewDesc.Texture2DMSArray.ArraySize = arraySize;
                    } else {
                        viewDesc.ViewDimension = selectArrayForm ? D3D11_DSV_DIMENSION_TEXTURE2DARRAY : D3D11_DSV_DIMENSION_TEXTURE2D;
                        viewDesc.Texture2DArray.MipSlice = window._mipRange._min;
                        viewDesc.Texture2DArray.FirstArraySlice = window._arrayLayerRange._min;
                        viewDesc.Texture2DArray.ArraySize = arraySize;
                    }
                    viewDesc.Format = SpecializeFormat(textureDesc.Format, window._format, true);
                }
                break;

            default:
                Throw(::Exceptions::BasicLabel("Invalid resource type used with DepthStencilView"));
            }

            _underlying = factory.CreateDepthStencilView(resource.get(), &viewDesc);
        }
    }

    DepthStencilView::DepthStencilView(UnderlyingResourcePtr resource, const TextureViewWindow& window)
    : DepthStencilView(GetObjectFactory(*resource.get()), resource, window)
    {}

    auto DepthStencilView::GetResource() const -> intrusive_ptr<ID3D::Resource>
    {
        return ExtractResource<ID3D::Resource>(_underlying.get());
    }

    ResourcePtr DepthStencilView::ShareResource() const
    {
        return AsResourcePtr(GetResource());
    }

    DepthStencilView::DepthStencilView(DeviceContext& context)
    {
        // get the currently bound depth stencil view from the context
        ID3D::DepthStencilView* rawPtr = nullptr; 
        context.GetUnderlying()->OMGetRenderTargets(0, nullptr, &rawPtr);
        _underlying = moveptr(rawPtr);
    }

    DepthStencilView::DepthStencilView(ID3D::DepthStencilView* resource)
    : _underlying(resource)
    {
    }

    DepthStencilView::DepthStencilView(MovePTRHelper<ID3D::DepthStencilView> resource)
    : _underlying(resource)
    {
    }

    DepthStencilView::DepthStencilView() {}

    DepthStencilView::~DepthStencilView() {}

    DepthStencilView::DepthStencilView(const DepthStencilView& cloneFrom) : _underlying(cloneFrom._underlying) {}
    DepthStencilView& DepthStencilView::operator=(const DepthStencilView& cloneFrom) { _underlying = cloneFrom._underlying; return *this; }

    DepthStencilView::DepthStencilView(DepthStencilView&& moveFrom) never_throws : _underlying(std::move(moveFrom._underlying)) {}
    DepthStencilView& DepthStencilView::operator=(DepthStencilView&& moveFrom) never_throws { _underlying = std::move(moveFrom._underlying); return *this; }



    UnorderedAccessView::UnorderedAccessView(const ObjectFactory& factory, UnderlyingResourcePtr resource, const TextureViewWindow& window)
    {
        if (!resource.get()) {
            Throw(::Exceptions::BasicLabel("NULL resource passed to UnorderedAccessView constructor"));
        }

        if (IsDefault(window)) {
            _underlying = factory.CreateUnorderedAccessView(resource.get());
        } else {
            D3D11_UNORDERED_ACCESS_VIEW_DESC viewDesc;

            D3D11_RESOURCE_DIMENSION resType;
            resource.get()->GetType(&resType);
            switch (resType) {
            case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                {
                    TextureDesc1D textureDesc(resource.get());
                    auto arraySize = std::min((int)textureDesc.ArraySize - (int)window._arrayLayerRange._min, (int)window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._min == 0) || (window._flags & TextureViewWindow::Flags::ForceArray);
                    viewDesc.ViewDimension = selectArrayForm ? D3D11_UAV_DIMENSION_TEXTURE1DARRAY : D3D11_UAV_DIMENSION_TEXTURE1D;
                    viewDesc.Texture1DArray.MipSlice = window._mipRange._min;
                    viewDesc.Texture1DArray.FirstArraySlice = window._arrayLayerRange._min;
                    viewDesc.Texture1DArray.ArraySize = arraySize;
                    viewDesc.Format = SpecializeFormat(textureDesc.Format, window._format);
                    break;
                }

            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                {
                    TextureDesc2D textureDesc(resource.get());
                    auto arraySize = std::min((int)textureDesc.ArraySize - (int)window._arrayLayerRange._min, (int)window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._min == 0) || (window._flags & TextureViewWindow::Flags::ForceArray);
                    viewDesc.ViewDimension = selectArrayForm ? D3D11_UAV_DIMENSION_TEXTURE2DARRAY : D3D11_UAV_DIMENSION_TEXTURE2D;
                    viewDesc.Texture2DArray.MipSlice = window._mipRange._min;
                    viewDesc.Texture2DArray.FirstArraySlice = window._arrayLayerRange._min;
                    viewDesc.Texture2DArray.ArraySize = arraySize;
                    viewDesc.Format = SpecializeFormat(textureDesc.Format, window._format);
                }
                break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
                {
                    TextureDesc3D textureDesc(resource.get());
                    viewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
                    viewDesc.Texture3D.MipSlice = window._mipRange._min;
                    viewDesc.Texture3D.FirstWSlice = 0;
                    viewDesc.Texture3D.WSize = ~0u;
                    viewDesc.Format = SpecializeFormat(textureDesc.Format, window._format);
                }
                break;

            case D3D11_RESOURCE_DIMENSION_BUFFER:
                {
                    D3DBufferDesc bufferDesc(resource.get());
                    viewDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
                    viewDesc.Buffer.FirstElement = 0;
                    viewDesc.Buffer.NumElements = bufferDesc.StructureByteStride ? (bufferDesc.ByteWidth/bufferDesc.StructureByteStride) : bufferDesc.ByteWidth;
                    viewDesc.Buffer.Flags = 0;
                    if (window._flags & TextureViewWindow::Flags::AppendBuffer) viewDesc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_APPEND;
                    if (window._flags & TextureViewWindow::Flags::AttachedCounter) viewDesc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_COUNTER;
                    viewDesc.Format = AsDXGIFormat(window._format._explicitFormat);
                }
                break;

            default:
                Throw(::Exceptions::BasicLabel("Invalid resource type used with RenderTargetView"));
            }
            _underlying = GetObjectFactory(*resource.get()).CreateUnorderedAccessView(resource.get(), &viewDesc);
        }
    }

    UnorderedAccessView::UnorderedAccessView(UnderlyingResourcePtr resource, const TextureViewWindow& window)
    : UnorderedAccessView(GetObjectFactory(*resource.get()), resource, window)
    {}

    auto UnorderedAccessView::GetResource() const -> intrusive_ptr<ID3D::Resource>
    {
        return ExtractResource<ID3D::Resource>(_underlying.get());
    }

    ResourcePtr UnorderedAccessView::ShareResource() const
    {
        return AsResourcePtr(GetResource());
    }

    UnorderedAccessView::UnorderedAccessView() {}
    UnorderedAccessView::~UnorderedAccessView() {}

    UnorderedAccessView::UnorderedAccessView(const UnorderedAccessView& cloneFrom) : _underlying(cloneFrom._underlying) {}
    UnorderedAccessView& UnorderedAccessView::operator=(const UnorderedAccessView& cloneFrom) { _underlying = cloneFrom._underlying; return *this; }

    UnorderedAccessView::UnorderedAccessView(UnorderedAccessView&& moveFrom) never_throws : _underlying(std::move(moveFrom._underlying)) {}
    UnorderedAccessView& UnorderedAccessView::operator=(UnorderedAccessView&& moveFrom) never_throws { _underlying = std::move(moveFrom._underlying); return *this; }

	ShaderResourceView::ShaderResourceView(const ObjectFactory& factory, UnderlyingResourcePtr resource, const TextureViewWindow& window)
	{
		// note --	for Vulkan compatibility, this should change so that array resources
		//			get array views by default (unless it's disabled somehow via the window)

        if (!resource.get())
			Throw(::Exceptions::BasicLabel("Null resource passed to ShaderResourceView constructor"));

        if (IsDefault(window)) {
            _underlying = factory.CreateShaderResourceView(resource.get());
        } else {
            D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;

            D3D11_RESOURCE_DIMENSION resType;
            resource.get()->GetType(&resType);
            switch (resType) {
            case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                {
                    TextureDesc1D textureDesc(resource.get());
                    auto arraySize = std::min((int)textureDesc.ArraySize - (int)window._arrayLayerRange._min, (int)window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._min == 0) || (window._flags & TextureViewWindow::Flags::ForceArray);
                    viewDesc.ViewDimension = selectArrayForm ? D3D11_SRV_DIMENSION_TEXTURE1DARRAY : D3D11_SRV_DIMENSION_TEXTURE1D;
                    viewDesc.Texture1DArray.MostDetailedMip = window._mipRange._min;
                    viewDesc.Texture1DArray.MipLevels = window._mipRange._count;
                    viewDesc.Texture1DArray.FirstArraySlice = window._arrayLayerRange._min;
                    viewDesc.Texture1DArray.ArraySize = arraySize;
                    viewDesc.Format = SpecializeFormat(textureDesc.Format, window._format);
                    break;
                }

            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                {
                    TextureDesc2D textureDesc(resource.get());
                    auto arraySize = std::min((int)textureDesc.ArraySize - (int)window._arrayLayerRange._min, (int)window._arrayLayerRange._count);
                    bool selectArrayForm = ((arraySize>0) && window._arrayLayerRange._min == 0) || (window._flags & TextureViewWindow::Flags::ForceArray);
                    if (textureDesc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) {
                        viewDesc.ViewDimension = selectArrayForm ? D3D11_SRV_DIMENSION_TEXTURECUBEARRAY : D3D11_SRV_DIMENSION_TEXTURECUBE;
                        viewDesc.TextureCubeArray.MostDetailedMip = window._mipRange._min;
                        viewDesc.TextureCubeArray.MipLevels = window._mipRange._count;
                        viewDesc.TextureCubeArray.First2DArrayFace = 0;
                        viewDesc.TextureCubeArray.NumCubes = std::max(1u, arraySize/6u); // ?
                    } else {
                        if (textureDesc.SampleDesc.Count > 1 && !(window._flags & TextureViewWindow::Flags::ForceSingleSample)) {
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
                    viewDesc.Format = SpecializeFormat(textureDesc.Format, window._format);
                }
                break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
                {
                    TextureDesc3D textureDesc(resource.get());
                    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                    viewDesc.Texture3D.MostDetailedMip = window._mipRange._min;
                    viewDesc.Texture3D.MipLevels = window._mipRange._count;
                    viewDesc.Format = SpecializeFormat(textureDesc.Format, window._format);
                }
                break;

            case D3D11_RESOURCE_DIMENSION_BUFFER:
                {
                    D3DBufferDesc bufferDesc(resource.get());
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

            _underlying = factory.CreateShaderResourceView(resource.get(), &viewDesc);
        }
    }

    ShaderResourceView::ShaderResourceView(UnderlyingResourcePtr resource, const TextureViewWindow& window)
    : ShaderResourceView(GetObjectFactory(*resource.get()), resource, window)
    {}

    ShaderResourceView ShaderResourceView::RawBuffer(UnderlyingResourcePtr res, unsigned sizeBytes, unsigned offsetBytes)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        srvDesc.BufferEx.FirstElement = offsetBytes / 4;
        srvDesc.BufferEx.NumElements = sizeBytes / 4;
        srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
        auto r = res.get();
        return ShaderResourceView(
            GetObjectFactory(*r).CreateShaderResourceView(r, &srvDesc));
    }

    auto ShaderResourceView::GetResource() const -> intrusive_ptr<ID3D::Resource>
    {
        return ExtractResource<ID3D::Resource>(_underlying.get());
    }

    ResourcePtr ShaderResourceView::ShareResource() const
    {
        return AsResourcePtr(GetResource());
    }

    ShaderResourceView::ShaderResourceView(intrusive_ptr<ID3D::ShaderResourceView>&& resource)
    : _underlying(resource)
    {
    }

    ShaderResourceView::ShaderResourceView(MovePTRHelper<ID3D::ShaderResourceView> resource)
    : _underlying(resource)
    {
    }

    ShaderResourceView::ShaderResourceView() {}

    ShaderResourceView::~ShaderResourceView() {}

    ShaderResourceView::ShaderResourceView(const ShaderResourceView& cloneFrom) : _underlying(cloneFrom._underlying) {}
    ShaderResourceView& ShaderResourceView::operator=(const ShaderResourceView& cloneFrom) { _underlying = cloneFrom._underlying; return *this; }

    ShaderResourceView::ShaderResourceView(ShaderResourceView&& moveFrom) never_throws : _underlying(std::move(moveFrom._underlying)) {}
    ShaderResourceView& ShaderResourceView::operator=(ShaderResourceView&& moveFrom) never_throws { _underlying = std::move(moveFrom._underlying); return *this; }


}}

