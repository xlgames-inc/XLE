// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Resource.h"
#include "ObjectFactory.h"
#include "../../../Core/Exceptions.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    class ShaderResourceView
    {
    public:
        using UnderlyingType = intrusive_ptr<OpenGL::Texture>;
        const UnderlyingType &      GetUnderlying() const { return _resource->GetTexture(); }
        bool                        IsGood() const { return _resource->GetTexture().get() != nullptr; }
        bool                        HasMipMaps() const { return _hasMipMaps; }
        const std::shared_ptr<Resource>& GetResource() const { return _resource; }

        ShaderResourceView(const ObjectFactory& factory, const std::shared_ptr<IResource>& resource, const TextureViewDesc& window = TextureViewDesc());
        explicit ShaderResourceView(const std::shared_ptr<IResource>& resource, const TextureViewDesc& window = TextureViewDesc());
        ShaderResourceView();

        TextureViewDesc _window;

    private:
        bool _hasMipMaps;
        std::shared_ptr<Resource> _resource;
    };

    class RenderTargetView
    {
    public:
        bool IsGood() const { return _resource && (_resource->IsBackBuffer() || _resource->GetTexture().get() != nullptr || _resource->GetRenderBuffer().get() != nullptr); }
        const std::shared_ptr<Resource>& GetResource() { return _resource; }

        RenderTargetView(const ObjectFactory& factory, const std::shared_ptr<IResource>& resource, const TextureViewDesc& window = TextureViewDesc());
        explicit RenderTargetView(const std::shared_ptr<IResource>& resource, const TextureViewDesc& window = TextureViewDesc());
        RenderTargetView();

        TextureViewDesc _window;

    private:
        std::shared_ptr<Resource> _resource;
    };

    class DepthStencilView
    {
    public:
        bool IsGood() const { return _resource && (_resource->GetTexture().get() != nullptr || _resource->GetRenderBuffer().get() != nullptr); }
        const std::shared_ptr<Resource>& GetResource() { return _resource; }

        DepthStencilView(const ObjectFactory& factory, const std::shared_ptr<IResource>& resource, const TextureViewDesc& window = TextureViewDesc());
        explicit DepthStencilView(const std::shared_ptr<IResource>& resource, const TextureViewDesc& window = TextureViewDesc());
        DepthStencilView();

        TextureViewDesc _window;

    private:
        std::shared_ptr<Resource> _resource;
    };

}}
