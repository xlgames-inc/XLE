// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RenderTargetView.h"
#include "ObjectFactory.h"
#include "DeviceContext.h"
#include "../../RenderUtils.h"
#include "DX11Utils.h"

namespace RenderCore { namespace Metal_DX11
{

    RenderTargetView::RenderTargetView(
		UnderlyingResourcePtr resource,
        NativeFormat::Enum format, const SubResourceSlice& arraySlice)
    {
        if (!resource.get()) {
            Throw(::Exceptions::BasicLabel("NULL resource passed to RenderTargetView constructor"));
        }

        intrusive_ptr<ID3D::RenderTargetView> rtv;
        if (format == NativeFormat::Unknown) {
            rtv = GetObjectFactory(*resource.get()).CreateRenderTargetView(resource.get());
        } else {
            TextureDesc2D textureDesc(resource.get());

            D3D11_RENDER_TARGET_VIEW_DESC viewDesc;
            viewDesc.Format = AsDXGIFormat(format);
            if (arraySlice._arraySize == 0) {
                if (textureDesc.SampleDesc.Count > 1) {
                    viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
                } else {
                    viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                    viewDesc.Texture2D.MipSlice = 0;
                }
            } else {
                if (textureDesc.SampleDesc.Count > 1) {
                    viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
                    viewDesc.Texture2DMSArray.ArraySize = arraySlice._arraySize;
                    viewDesc.Texture2DMSArray.FirstArraySlice = arraySlice._firstArraySlice;
                } else {
                    viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                    viewDesc.Texture2DArray.ArraySize = arraySlice._arraySize;
                    viewDesc.Texture2DArray.FirstArraySlice = arraySlice._firstArraySlice;
                    viewDesc.Texture2DArray.MipSlice = arraySlice._mipMapIndex;
                }
            }
            rtv = GetObjectFactory(*resource.get()).CreateRenderTargetView(resource.get(), &viewDesc);
        }
        _underlying = std::move(rtv);
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
		UnderlyingResourcePtr resource,
        NativeFormat::Enum format, const SubResourceSlice& arraySlice)
    {
        if (!resource.get()) {
            Throw(::Exceptions::BasicLabel("NULL resource passed to DepthStencilView constructor"));
        }

        intrusive_ptr<ID3D::DepthStencilView> view;
        if (format == NativeFormat::Unknown) {
            view = GetObjectFactory(*resource.get()).CreateDepthStencilView(resource.get());
        } else {
            TextureDesc2D textureDesc(resource.get());

            D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc;
            viewDesc.Format = AsDXGIFormat(format);
            viewDesc.Flags = 0;
            if (arraySlice._arraySize == 0) {
                if (textureDesc.SampleDesc.Count > 1) {
                    viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
                } else {
                    viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                    viewDesc.Texture2D.MipSlice = 0;
                }
            } else {
                if (textureDesc.SampleDesc.Count > 1) {
                    viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
                    viewDesc.Texture2DMSArray.ArraySize = arraySlice._arraySize;
                    viewDesc.Texture2DMSArray.FirstArraySlice = arraySlice._firstArraySlice;
                } else {
                    viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                    viewDesc.Texture2DArray.ArraySize = arraySlice._arraySize;
                    viewDesc.Texture2DArray.FirstArraySlice = arraySlice._firstArraySlice;
                    viewDesc.Texture2DArray.MipSlice = 0;
                }
            }
            view = GetObjectFactory(*resource.get()).CreateDepthStencilView(resource.get(), &viewDesc);
        }
        _underlying = std::move(view);
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



    UnorderedAccessView::UnorderedAccessView(UnderlyingResourcePtr resource, NativeFormat::Enum format, unsigned mipSlice, bool appendBuffer, bool forceArray)
    {
        if (!resource.get()) {
            Throw(::Exceptions::BasicLabel("NULL resource passed to UnorderedAccessView constructor"));
        }

        intrusive_ptr<ID3D::UnorderedAccessView> view = nullptr;
        if (format == NativeFormat::Unknown && mipSlice == 0 && !appendBuffer && !forceArray) {
            view = GetObjectFactory(*resource.get()).CreateUnorderedAccessView(resource.get());
        } else {
            D3D11_UNORDERED_ACCESS_VIEW_DESC viewDesc;
            viewDesc.Format = AsDXGIFormat(format);

            TextureDesc2D textureDesc(resource.get());
            if (textureDesc.Width > 0) {
                if (textureDesc.ArraySize > 1 || forceArray) {
                    viewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
                    viewDesc.Texture2DArray.MipSlice = mipSlice;
                    viewDesc.Texture2DArray.FirstArraySlice = 0;
                    viewDesc.Texture2DArray.ArraySize = textureDesc.ArraySize;
                } else {
                    viewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
                    viewDesc.Texture2D.MipSlice = mipSlice;
                }
            } else {
                TextureDesc3D t3dDesc(resource.get());
                if (t3dDesc.Width > 0) {
                    viewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
                    viewDesc.Texture3D.MipSlice = mipSlice;
                    viewDesc.Texture3D.FirstWSlice = 0;
                    viewDesc.Texture3D.WSize = (UINT)-1;
                } else {
                    TextureDesc1D t1dDesc(resource.get());
                    if (t1dDesc.Width > 0) {
                        viewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
                        viewDesc.Texture1D.MipSlice = mipSlice;
                    } else {
                        D3DBufferDesc bufferDesc(resource.get());
                        viewDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
                        viewDesc.Buffer.FirstElement = 0;
                        viewDesc.Buffer.NumElements = bufferDesc.StructureByteStride ? (bufferDesc.ByteWidth/bufferDesc.StructureByteStride) : bufferDesc.ByteWidth;
                        viewDesc.Buffer.Flags = appendBuffer ? D3D11_BUFFER_UAV_FLAG_APPEND : 0;
                    }
                }
            }

            view = GetObjectFactory(*resource.get()).CreateUnorderedAccessView(resource.get(), &viewDesc);
        }
        
        _underlying = std::move(view);
    }

    UnorderedAccessView::UnorderedAccessView(UnderlyingResourcePtr resource, Flags::BitField field)
    {
        if (!resource.get()) {
            Throw(::Exceptions::BasicLabel("NULL resource passed to DepthStencilView constructor"));
        }

        intrusive_ptr<ID3D::UnorderedAccessView> view;
        D3DBufferDesc bufferDesc(resource.get());
        auto buffer = QueryInterfaceCast<ID3D::Buffer>(resource.get());
        if (buffer) {
            D3D11_UNORDERED_ACCESS_VIEW_DESC viewDesc;
            viewDesc.Format = DXGI_FORMAT_UNKNOWN;
            viewDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            viewDesc.Buffer.FirstElement = 0;
            viewDesc.Buffer.NumElements = bufferDesc.ByteWidth / bufferDesc.StructureByteStride;
            viewDesc.Buffer.Flags = 0;
            if (field & Flags::AttachedCounter) {
                viewDesc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_COUNTER;
            }
            view = GetObjectFactory(*resource.get()).CreateUnorderedAccessView(resource.get(), &viewDesc);
        } else {
            view = GetObjectFactory(*resource.get()).CreateUnorderedAccessView(resource.get());
        }

        _underlying = std::move(view);
    }

    UnorderedAccessView::UnorderedAccessView() {}
    UnorderedAccessView::~UnorderedAccessView() {}

    UnorderedAccessView::UnorderedAccessView(const UnorderedAccessView& cloneFrom) : _underlying(cloneFrom._underlying) {}
    UnorderedAccessView& UnorderedAccessView::operator=(const UnorderedAccessView& cloneFrom) { _underlying = cloneFrom._underlying; return *this; }

    UnorderedAccessView::UnorderedAccessView(UnorderedAccessView&& moveFrom) never_throws : _underlying(std::move(moveFrom._underlying)) {}
    UnorderedAccessView& UnorderedAccessView::operator=(UnorderedAccessView&& moveFrom) never_throws { _underlying = std::move(moveFrom._underlying); return *this; }


}}

