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
		struct UpdateFlags
        {
            enum Enum {
                UnsynchronizedWrite = 1 << 0
            };
            using BitField = unsigned;
        };
        void Update(DeviceContext& context, const void* data, size_t byteCount, size_t writeOffset = 0u, UpdateFlags::BitField flags = 0u);

        Buffer();
        Buffer( ObjectFactory& factory, const ResourceDesc& desc,
				IteratorRange<const void*> initData = {});
        ~Buffer();

        Buffer(const Buffer& cloneFrom) = default;
        Buffer(Buffer&& moveFrom) never_throws = default;
        Buffer& operator=(const Buffer& cloneFrom) = default;
        Buffer& operator=(Buffer&& moveFrom) never_throws = default;
        Buffer(UnderlyingResourcePtr cloneFrom);

        typedef ID3D::Buffer*       UnderlyingType;
        UnderlyingType              GetUnderlying() const { return (UnderlyingType)_underlying.get(); }
        bool                        IsGood() const { return _underlying.get() != nullptr; }

		virtual void*			QueryInterface(size_t guid);
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

	Buffer MakeVertexBuffer(ObjectFactory& factory, IteratorRange<const void*>);
    Buffer MakeIndexBuffer(ObjectFactory& factory, IteratorRange<const void*>);
    Buffer MakeConstantBuffer(ObjectFactory& factory, IteratorRange<const void*>, bool immutable = true);
	Buffer MakeConstantBuffer(ObjectFactory& factory, size_t size);

	using ConstantBuffer = Buffer;
    
}}