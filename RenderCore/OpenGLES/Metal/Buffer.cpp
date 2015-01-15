// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Buffer.h"
#include "../../../Utility/PtrUtils.h"
#include <GLES2/gl2.h>
#include <assert.h>

namespace RenderCore { namespace Metal_OpenGLES
{
    VertexBuffer::VertexBuffer(const void* data, size_t byteCount)
    {
        _underlying = CreateBuffer();
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)_underlying.get());
        glBufferData(GL_ARRAY_BUFFER, byteCount, data, GL_STATIC_DRAW);
    }

    VertexBuffer::~VertexBuffer() {}

    IndexBuffer::IndexBuffer(const void* data, size_t byteCount)
    {
        _underlying = CreateBuffer();
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)_underlying.get());
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, byteCount, data, GL_STATIC_DRAW);
    }

    IndexBuffer::~IndexBuffer() {}

    ConstantBuffer::ConstantBuffer(const void* data, size_t byteCount)
    {
        _underlying = std::make_shared<std::vector<uint8>>(
            (const uint8*)data, 
            (const uint8*)PtrAdd(data, byteCount));
    }

    ConstantBuffer::~ConstantBuffer() {}
}}

