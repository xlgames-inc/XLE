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

        std::vector<uint8_t> Readback();

        Buffer();
        Buffer( ObjectFactory& factory, const ResourceDesc& desc,
                const void* initData = nullptr, size_t initDataSize = 0);
        Buffer(const intrusive_ptr<OpenGL::Buffer>& underlying);
        ~Buffer();
    };

////////////////////////////////////////////////////////////////////////////////////////////////

    class VertexBuffer : public Buffer
    {
    public:
        VertexBuffer(const void* data, size_t byteCount);
        VertexBuffer(ObjectFactory& factory, const void* data, size_t byteCount);
        VertexBuffer(ObjectFactory& factory, const ResourceDesc& desc);
        VertexBuffer(const intrusive_ptr<OpenGL::Buffer>& underlying);
        VertexBuffer();
        ~VertexBuffer();
    };

////////////////////////////////////////////////////////////////////////////////////////////////

    class IndexBuffer : public Buffer
    {
    public:
        IndexBuffer(const void* data, size_t byteCount);
        IndexBuffer(ObjectFactory& factory, const void* data, size_t byteCount);
        IndexBuffer(ObjectFactory& factory, const ResourceDesc& desc);
        IndexBuffer(const intrusive_ptr<OpenGL::Buffer>& underlying);
        IndexBuffer();
        ~IndexBuffer();
    };

////////////////////////////////////////////////////////////////////////////////////////////////

    class ConstantBuffer
    {
    public:
        typedef std::shared_ptr<std::vector<uint8>>     UnderlyingType;
        UnderlyingType                                  GetUnderlying() const { return _underlying; }

        ConstantBuffer(const void* data, size_t byteCount);
        ConstantBuffer(ObjectFactory& factory, const void* data, size_t byteCount);
        ConstantBuffer(const intrusive_ptr<OpenGL::Buffer>& underlying);
        ~ConstantBuffer();
    private:
        std::shared_ptr<std::vector<uint8>>    _underlying;
    };

}}

