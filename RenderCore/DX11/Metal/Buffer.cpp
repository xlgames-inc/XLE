// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Buffer.h"
#include "DeviceContext.h"
#include "DX11Utils.h"
#include "../../../Core/Exceptions.h"

namespace RenderCore { namespace Metal_DX11
{
        
    VertexBuffer::VertexBuffer(const void* data, size_t byteCount)
    {
        if (byteCount!=0) {
            D3D11_BUFFER_DESC desc;
            desc.ByteWidth = (UINT)byteCount;
            desc.Usage = D3D11_USAGE_IMMUTABLE;
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            if (!data) {    // hack -- assume it's a stream output buffer for now
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags |= D3D11_BIND_STREAM_OUTPUT;
            }
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;
            desc.StructureByteStride = 0;
            D3D11_SUBRESOURCE_DATA subresData;
            subresData.pSysMem = data;
            subresData.SysMemPitch = subresData.SysMemSlicePitch = 0;
            _underlying = ObjectFactory().CreateBuffer(&desc, data?(&subresData):nullptr);
        }
    }

    VertexBuffer::VertexBuffer() {}

    VertexBuffer::~VertexBuffer() {}

    VertexBuffer::VertexBuffer(const VertexBuffer& cloneFrom) : _underlying(cloneFrom._underlying) {}
    VertexBuffer& VertexBuffer::operator=(const VertexBuffer& cloneFrom)            { _underlying = cloneFrom._underlying; return *this; }

    VertexBuffer::VertexBuffer(VertexBuffer&& moveFrom) never_throws : _underlying(std::move(moveFrom._underlying)) {}
    VertexBuffer& VertexBuffer::operator=(VertexBuffer&& moveFrom) never_throws     { _underlying = std::move(moveFrom._underlying); return *this; }

    VertexBuffer::VertexBuffer(intrusive_ptr<ID3D::Resource>&& cloneFrom)
    {
        _underlying = std::move(QueryInterfaceCast<ID3D::Buffer>(cloneFrom));
    }

        ////////////////////////////////////////////////////////////////////////////////////////////////

    IndexBuffer::IndexBuffer() {}
    
    IndexBuffer::IndexBuffer(const void* data, size_t byteCount)
    {
        if (byteCount!=0) {
            D3D11_BUFFER_DESC desc;
            desc.ByteWidth = (UINT)byteCount;
            desc.Usage = D3D11_USAGE_IMMUTABLE;
            desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;
            desc.StructureByteStride = 0;
            D3D11_SUBRESOURCE_DATA subresData;
            subresData.pSysMem = data;
            subresData.SysMemPitch = subresData.SysMemSlicePitch = 0;
            _underlying = ObjectFactory().CreateBuffer(&desc, &subresData);
        }
    }

    IndexBuffer::~IndexBuffer() {}

    IndexBuffer::IndexBuffer(const IndexBuffer& cloneFrom) : _underlying(cloneFrom._underlying) {}
    IndexBuffer& IndexBuffer::operator=(const IndexBuffer& cloneFrom)            { _underlying = cloneFrom._underlying; return *this; }

    IndexBuffer::IndexBuffer(IndexBuffer&& moveFrom) never_throws : _underlying(std::move(moveFrom._underlying)) {}
    IndexBuffer& IndexBuffer::operator=(IndexBuffer&& moveFrom) never_throws     { _underlying = std::move(moveFrom._underlying); return *this; }

        ////////////////////////////////////////////////////////////////////////////////////////////////

    ConstantBuffer::ConstantBuffer() {}
    ConstantBuffer::ConstantBuffer(const void* data, size_t byteCount, bool immutable)
    {
        if (byteCount!=0) {
            D3D11_BUFFER_DESC desc;
            desc.ByteWidth = (UINT)byteCount;
            if (data && immutable) {
                desc.Usage = D3D11_USAGE_IMMUTABLE;
                desc.CPUAccessFlags = 0;
            } else {
                desc.Usage = D3D11_USAGE_DYNAMIC;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            }
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            
            desc.MiscFlags = 0;
            desc.StructureByteStride = 0;
            D3D11_SUBRESOURCE_DATA subresData;
            subresData.pSysMem = data;
            subresData.SysMemPitch = subresData.SysMemSlicePitch = 0;
            _underlying = ObjectFactory().CreateBuffer(&desc, data?(&subresData):nullptr);
        }
    }

    ConstantBuffer::~ConstantBuffer() {}

    ConstantBuffer::ConstantBuffer(const ConstantBuffer& cloneFrom) : _underlying(cloneFrom._underlying) {}
    ConstantBuffer& ConstantBuffer::operator=(const ConstantBuffer& cloneFrom)            { _underlying = cloneFrom._underlying; return *this; }

    ConstantBuffer::ConstantBuffer(intrusive_ptr<ID3D::Buffer> underlyingBuffer) : _underlying(underlyingBuffer) {}

    void    ConstantBuffer::Update(DeviceContext& context, const void* data, size_t byteCount)
    {
        // context.GetUnderlying()->UpdateSubresource(
        //     _underlying, 0, nullptr, data, byteCount, byteCount);

        D3D11_MAPPED_SUBRESOURCE result;
        ID3D::DeviceContext* devContext = context.GetUnderlying();
        HRESULT hresult = devContext->Map(_underlying.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &result);
        if (SUCCEEDED(hresult) && result.pData) {
            XlCopyMemory(result.pData, data, byteCount);
            devContext->Unmap(_underlying.get(), 0);
        }
    }

    ConstantBuffer::ConstantBuffer(ConstantBuffer&& moveFrom) never_throws : _underlying(std::move(moveFrom._underlying)) {}
    ConstantBuffer& ConstantBuffer::operator=(ConstantBuffer&& moveFrom) never_throws     { _underlying = std::move(moveFrom._underlying); return *this; }

}}

