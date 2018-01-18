// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderResource.h"
#include "../../RenderUtils.h"
#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    ShaderResourceView::ShaderResourceView() {}
    ShaderResourceView::ShaderResourceView(const intrusive_ptr<OpenGL::Texture>& underlyingTexture, bool hasMipMaps)
    : _hasMipMaps(hasMipMaps)
    {
        if (!glIsTexture(underlyingTexture->AsRawGLHandle()))
            Throw(Exceptions::GenericFailure("Binding non-texture to shader resource view"));

        _underlyingTexture = underlyingTexture;
    }
}}
