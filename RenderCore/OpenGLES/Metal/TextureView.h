// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Resource.h"
#include "ObjectFactory.h"
#include "../../../Core/Exceptions.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    class ShaderResourceView : public Resource
    {
    public:
        using UnderlyingType = intrusive_ptr<OpenGL::Texture>;
        const UnderlyingType &      GetUnderlying() const { return GetTexture(); }
        bool                        IsGood() const { return _underlyingTexture.get() != nullptr; }
        bool                        HasMipMaps() const { return _hasMipMaps; }

        ShaderResourceView(const ObjectFactory& factory, const std::shared_ptr<IResource>& resource, const TextureViewDesc& window = TextureViewDesc());
        explicit ShaderResourceView(const std::shared_ptr<IResource>& resource, const TextureViewDesc& window = TextureViewDesc());

        ShaderResourceView();
        ShaderResourceView(const intrusive_ptr<OpenGL::Texture>& underlyingTexture, bool hasMipMaps = true);

    private:
        bool _hasMipMaps;
    };

    class RenderTargetView : public Resource
    {
    public:
        bool                        IsGood() const { return _underlyingTexture.get() != nullptr || _underlyingRenderBuffer.get() != nullptr; }

        RenderTargetView(const ObjectFactory& factory, const std::shared_ptr<IResource>& resource, const TextureViewDesc& window = TextureViewDesc());
        explicit RenderTargetView(const std::shared_ptr<IResource>& resource, const TextureViewDesc& window = TextureViewDesc());

        RenderTargetView();
        RenderTargetView(const intrusive_ptr<OpenGL::Texture>& underlyingTexture);
        RenderTargetView(const intrusive_ptr<OpenGL::RenderBuffer>& underlyingRenderbuffer);

        TextureViewDesc _window;
    };

    class DepthStencilView : public Resource
    {
    public:
        bool                        IsGood() const { return _underlyingTexture.get() != nullptr || _underlyingRenderBuffer.get() != nullptr; }

        DepthStencilView(const ObjectFactory& factory, const std::shared_ptr<IResource>& resource, const TextureViewDesc& window = TextureViewDesc());
        explicit DepthStencilView(const std::shared_ptr<IResource>& resource, const TextureViewDesc& window = TextureViewDesc());

        DepthStencilView();
        DepthStencilView(const intrusive_ptr<OpenGL::Texture>& underlyingTexture);
        DepthStencilView(const intrusive_ptr<OpenGL::RenderBuffer>& underlyingRenderbuffer);

        TextureViewDesc _window;
    };

}}
