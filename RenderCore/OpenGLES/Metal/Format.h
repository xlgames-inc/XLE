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
    Format VertexAttributePointerAsFormat(GLint size, GLenum type, bool normalized);

    namespace FeatureSet
    {
        enum Flags
        {
            GLES200     = (1<<0),
            GLES300     = (1<<1),
            PVRTC       = (1<<2),
            ETC1TC      = (1<<3),
            ETC2TC      = (1<<4)
        };
        using BitField = unsigned;
    };

    struct glPixelFormat
    {
        GLenum _format;
        GLenum _type;
        GLenum _internalFormat;     // (the sized/compressed format)

        FeatureSet::BitField _textureFeatureSet;
        FeatureSet::BitField _renderbufferFeatureSet;
    };

    glPixelFormat AsTexelFormatType(Format fmt);

    ImpliedTyping::TypeDesc GLUniformTypeAsTypeDesc(GLenum glType);
    Format SizedInternalFormatAsRenderCoreFormat(GLenum sizedInternalFormat);
    GLenum AsGLenum(Topology topology);
    GLenum AsGLenum(Blend input);
    GLenum AsGLenum(CullMode cullMode);
    GLenum AsGLenum(FaceWinding faceWinding);
    GLenum AsGLenum(CompareOp compare);
    GLenum AsGLenum(StencilOp stencilOp);
}}

