// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Format.h"
#include "../../Types.h"
#include <utility>
#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    unsigned    AsGLComponents(FormatComponents components);
    unsigned    AsGLCompressionType(FormatCompressionType compressionType);
    unsigned    AsGLComponentWidths(Format format);
    unsigned    AsGLVertexComponentType(Format format);

    std::tuple<GLint, GLenum, bool> AsVertexAttributePointer(Format fmt);
    RenderCore::Format VertexAttributePointerAsFormat(GLint size, GLenum type, bool normalized);
    std::pair<GLenum, GLenum> AsTexelFormatType(Format fmt);
    ImpliedTyping::TypeDesc GLUniformTypeAsTypeDesc(GLenum glType);
    RenderCore::Format SizedInternalFormatAsRenderCoreFormat(GLenum sizedInternalFormat);
    GLenum AsGLTopology(RenderCore::Topology topology);
    GLenum AsGLBlend(RenderCore::Blend input);
}}

