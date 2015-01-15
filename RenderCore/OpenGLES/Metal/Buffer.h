// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IndexedGLType.h"
#include "../../../Core/Types.h"
#include <memory>
#include <vector>

namespace RenderCore { namespace Metal_OpenGLES
{
        ////////////////////////////////////////////////////////////////////////////////////////////////

    class VertexBuffer
    {
    public:
        VertexBuffer(const void* data, size_t byteCount);
        ~VertexBuffer();

        typedef OpenGL::Buffer*       UnderlyingType;
        UnderlyingType                GetUnderlying() const { return _underlying.get(); }
    protected:
        intrusive_ptr<OpenGL::Buffer>    _underlying;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class IndexBuffer
    {
    public:
        IndexBuffer(const void* data, size_t byteCount);
        ~IndexBuffer();

        typedef OpenGL::Buffer*       UnderlyingType;
        UnderlyingType                GetUnderlying() const { return _underlying.get(); }
    protected:
        intrusive_ptr<OpenGL::Buffer>    _underlying;
    };

        ////////////////////////////////////////////////////////////////////////////////////////////////

    class ConstantBuffer
    {
    public:
        ConstantBuffer(const void* data, size_t byteCount);
        ~ConstantBuffer();

        typedef std::shared_ptr<std::vector<uint8>>     UnderlyingType;
        UnderlyingType                                  GetUnderlying() const { return _underlying; }
    private:
        std::shared_ptr<std::vector<uint8>>    _underlying;
    };

}}

