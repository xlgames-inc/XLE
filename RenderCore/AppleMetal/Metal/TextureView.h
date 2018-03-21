// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Resource.h"
#include "ObjectFactory.h"
#include "../../../Externals/Misc/OCPtr.h"

namespace RenderCore { namespace Metal_AppleMetal
{
    class ShaderResourceView
    {
    public:
        TBC::OCPtr<id>              GetUnderlying() const { return _resource->GetTexture(); }
        bool                        IsGood() const { return _resource != nullptr && _resource->GetTexture().get() != nullptr; }
        bool                        HasMipMaps() const { return _hasMipMaps; }
        const std::shared_ptr<Resource>& GetResource() const { return _resource; }

        ShaderResourceView(const ObjectFactory& factory, const IResourcePtr& resource, const TextureViewDesc& window = TextureViewDesc());
        explicit ShaderResourceView(const IResourcePtr& resource, const TextureViewDesc& window = TextureViewDesc());
        ShaderResourceView();

        TextureViewDesc _window;

    private:
        bool _hasMipMaps;
        std::shared_ptr<Resource> _resource;
    };

    /* KenD -- not necessary for Metal implementation at this point */
#if 0
    class RenderTargetView
    {
    public:
        bool IsGood() const { return _resource && (_resource->IsBackBuffer() || _resource->GetTexture().get() != nullptr || _resource->GetRenderBuffer().get() != nullptr); }
        const std::shared_ptr<Resource>& GetResource() { return _resource; }

        RenderTargetView(const ObjectFactory& factory, const IResourcePtr& resource, const TextureViewDesc& window = TextureViewDesc());
        explicit RenderTargetView(const IResourcePtr& resource, const TextureViewDesc& window = TextureViewDesc());
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

        DepthStencilView(const ObjectFactory& factory, const IResourcePtr& resource, const TextureViewDesc& window = TextureViewDesc());
        explicit DepthStencilView(const IResourcePtr& resource, const TextureViewDesc& window = TextureViewDesc());
        DepthStencilView();

        TextureViewDesc _window;

    private:
        std::shared_ptr<Resource> _resource;
    };
#endif
}}
