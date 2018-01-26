// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Resource.h"
#include "../../ResourceDesc.h"
#include "../../../Core/Types.h"
#include <memory>
#include <vector>

namespace RenderCore { namespace Metal_OpenGLES
{
    class DeviceContext;
    class ObjectFactory;

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
        void Update(DeviceContext& context, const void* data, size_t dataSize, size_t writeOffset = 0u, UpdateFlags::BitField flags = 0u);

        using UnderlyingType = intrusive_ptr<OpenGL::Buffer>;
        const UnderlyingType &      GetUnderlying() const { return GetBuffer(); }
        bool                        IsGood() const { return _underlyingBuffer.get() != nullptr; }

        std::vector<uint8_t>        Readback();

        Buffer();
        Buffer( ObjectFactory& factory, const ResourceDesc& desc,
                const void* initData = nullptr, size_t initDataSize = 0);
        Buffer(const intrusive_ptr<OpenGL::Buffer>& underlying);
        ~Buffer();
    };

    Buffer MakeVertexBuffer(ObjectFactory& factory, IteratorRange<const void*>);
    Resource MakeIndexBuffer(ObjectFactory& factory, IteratorRange<const void*>);
    Resource MakeConstantBuffer(ObjectFactory& factory, IteratorRange<const void*>);

}}

