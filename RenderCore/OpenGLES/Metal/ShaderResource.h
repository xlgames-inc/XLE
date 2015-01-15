// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IndexedGLType.h"
#include "../../../Core/Exceptions.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    class ShaderResourceView
    {
    public:
        // ShaderResourceView(const Resource& resource);
        ShaderResourceView(OpenGL::Resource* underlyingTexture);
        ShaderResourceView(OpenGL::Texture* underlyingTexture);
        
        typedef OpenGL::Resource*     UnderlyingResource;
        typedef OpenGL::Texture*      UnderlyingType;
        OpenGL::Texture*              GetUnderlying() const { return _underlyingTexture.get(); }
    private:
        intrusive_ptr<OpenGL::Texture>   _underlyingTexture;
    };

}}

