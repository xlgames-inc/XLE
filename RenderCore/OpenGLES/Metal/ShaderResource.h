// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Resource.h"
#include "IndexedGLType.h"
#include "../../../Core/Exceptions.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    class ShaderResourceView : public Resource
    {
    public:
        using UnderlyingType = intrusive_ptr<OpenGL::Texture>;
        const UnderlyingType &      GetUnderlying() const { return GetTexture(); }
        bool                        IsGood() const { return _underlyingTexture.get() != nullptr; }

        ShaderResourceView();
        ShaderResourceView(const intrusive_ptr<OpenGL::Texture>& underlyingTexture);
    };

}}
