// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Resource.h"

namespace RenderCore { namespace Metal_DX11
{
    class ObjectFactory;
    class DeviceContext;

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class Buffer : public Resource
    {
    public:
        Buffer();
        Buffer( ObjectFactory& factory, const ResourceDesc& desc,
				IteratorRange<const void*> initData = {});
        ~Buffer();

		void    Update(DeviceContext& context, const void* data, size_t byteCount);

        Buffer(const Buffer& cloneFrom) = default;
        Buffer(Buffer&& moveFrom) never_throws = default;
        Buffer& operator=(const Buffer& cloneFrom) = default;
        Buffer& operator=(Buffer&& moveFrom) never_throws = default;
        Buffer(UnderlyingResourcePtr cloneFrom);

        typedef ID3D::Buffer*       UnderlyingType;
        UnderlyingType              GetUnderlying() const { return (UnderlyingType)_underlying.get(); }
        bool                        IsGood() const { return _underlying.get() != nullptr; }
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

	Buffer MakeVertexBuffer(ObjectFactory& factory, IteratorRange<const void*>);
    Buffer MakeIndexBuffer(ObjectFactory& factory, IteratorRange<const void*>);
    Buffer MakeConstantBuffer(ObjectFactory& factory, IteratorRange<const void*>, bool immutable = true);
	Buffer MakeConstantBuffer(ObjectFactory& factory, size_t size);

	using ConstantBuffer = Buffer;
    
}}