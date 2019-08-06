// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TextureView.h"

namespace RenderCore { namespace Metal_AppleMetal
{
    static std::shared_ptr<Resource> AsResource(const IResourcePtr& rp)
    {
        auto* res = (Resource*)rp->QueryInterface(typeid(Resource).hash_code());
        if (!res || res != rp.get())
            Throw(::Exceptions::BasicLabel("Unexpected resource type passed to texture view"));
        return std::static_pointer_cast<Resource>(rp);
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    ShaderResourceView::ShaderResourceView(const ObjectFactory& factory, const IResourcePtr& resource, const TextureViewDesc& window)
    : _resource(AsResource(resource)), _window(window)
    {
        _hasMipMaps = _resource->GetDesc()._textureDesc._mipCount > 1;
    }

    ShaderResourceView::ShaderResourceView(const IResourcePtr& resource, const TextureViewDesc& window)
    : ShaderResourceView(GetObjectFactory(), resource, window) {}

    ShaderResourceView::ShaderResourceView()
    : _hasMipMaps(false)
    {}
}}
