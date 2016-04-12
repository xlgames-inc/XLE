// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DX11.h"
#include "Resource.h"
#include "../../../Utility/IntrusivePtr.h"

namespace RenderCore { namespace Metal_DX11
{
    class ObjectFactory;
    class DeviceContext;

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class VertexBuffer
    {
    public:
        VertexBuffer();
        VertexBuffer(const void* data, size_t byteCount);
        VertexBuffer(const ObjectFactory& factory, const void* data, size_t byteCount);
        ~VertexBuffer();

        VertexBuffer(const VertexBuffer& cloneFrom);
        VertexBuffer(VertexBuffer&& moveFrom) never_throws;
        VertexBuffer& operator=(const VertexBuffer& cloneFrom);
        VertexBuffer& operator=(VertexBuffer&& moveFrom) never_throws;
        VertexBuffer(UnderlyingResourcePtr cloneFrom);

        typedef ID3D::Buffer*       UnderlyingType;
        UnderlyingType              GetUnderlying() const { return _underlying.get(); }
        bool                        IsGood() const { return _underlying.get() != nullptr; }
    private:
        intrusive_ptr<ID3D::Buffer>    _underlying;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class IndexBuffer
    {
    public:
        IndexBuffer();
        IndexBuffer(const void* data, size_t byteCount);
        IndexBuffer(const ObjectFactory& factory, const void* data, size_t byteCount);
        ~IndexBuffer();

        IndexBuffer(const IndexBuffer& cloneFrom);
        IndexBuffer(IndexBuffer&& moveFrom) never_throws;
        IndexBuffer& operator=(const IndexBuffer& cloneFrom);
        IndexBuffer& operator=(IndexBuffer&& moveFrom) never_throws;
        explicit IndexBuffer(DeviceContext& context);

        typedef ID3D::Buffer*       UnderlyingType;
        UnderlyingType              GetUnderlying() const { return _underlying.get(); }
        bool                        IsGood() const { return _underlying.get() != nullptr; }
    private:
        intrusive_ptr<ID3D::Buffer>    _underlying;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class DeviceContext;

    class ConstantBuffer
    {
    public:
        ConstantBuffer(const void* data, size_t byteCount, bool immutable=true);
        ConstantBuffer(
            const ObjectFactory& factory,
            const void* data, size_t byteCount, bool immutable=true);
        ConstantBuffer();
        ~ConstantBuffer();

        void    Update(DeviceContext& context, const void* data, size_t byteCount);

        ConstantBuffer(const ConstantBuffer& cloneFrom);
        ConstantBuffer(ConstantBuffer&& moveFrom) never_throws;
        ConstantBuffer& operator=(const ConstantBuffer& cloneFrom);
        ConstantBuffer& operator=(ConstantBuffer&& moveFrom) never_throws;

        ConstantBuffer(intrusive_ptr<ID3D::Buffer> underlyingBuffer);

        typedef ID3D::Buffer*       UnderlyingType;
        UnderlyingType              GetUnderlying() const { return _underlying.get(); }
        bool                        IsGood() const { return _underlying.get() != nullptr; }
    private:
        intrusive_ptr<ID3D::Buffer>    _underlying;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////
    
}}