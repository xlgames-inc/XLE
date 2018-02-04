// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TextureView.h"
#include "../../RenderUtils.h"
#include "IncludeGLES.h"

namespace RenderCore { namespace Metal_OpenGLES
{
    static Resource& AsResource(IResource& resource)
    {
        auto* result = (Resource*)resource.QueryInterface(typeid(Resource).hash_code());
        if (!result)
            Throw(::Exceptions::BasicLabel("Unexpected resource type passed to texture view"));
        return *result;
    }

    ShaderResourceView::ShaderResourceView() {}
    ShaderResourceView::ShaderResourceView(const intrusive_ptr<OpenGL::Texture>& underlyingTexture, bool hasMipMaps)
    : _hasMipMaps(hasMipMaps)
    {
        if (!glIsTexture(underlyingTexture->AsRawGLHandle()))
            Throw(Exceptions::GenericFailure("Binding non-texture to shader resource view"));

        _underlyingTexture = underlyingTexture;
    }

    ShaderResourceView::ShaderResourceView(const ObjectFactory& factory, const std::shared_ptr<IResource>& resource, const TextureViewDesc& window)
    : Resource(AsResource(*resource))
    {
        // todo -- handle "view" transformation
    }

    ShaderResourceView::ShaderResourceView(const std::shared_ptr<IResource>& resource, const TextureViewDesc& window)
    : ShaderResourceView(GetObjectFactory(*resource.get()), resource, window)
    {
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    RenderTargetView::RenderTargetView() {}
    RenderTargetView::RenderTargetView(const intrusive_ptr<OpenGL::Texture>& underlyingTexture)
    {
        if (!glIsTexture(underlyingTexture->AsRawGLHandle()))
            Throw(Exceptions::GenericFailure("Binding non-texture to render target view"));

        _underlyingTexture = underlyingTexture;
    }

    RenderTargetView::RenderTargetView(const intrusive_ptr<OpenGL::RenderBuffer>& underlyingRenderbuffer)
    {
        if (!glIsRenderbuffer(underlyingRenderbuffer->AsRawGLHandle()))
            Throw(Exceptions::GenericFailure("Binding non-render buffer to render target view"));

        _underlyingRenderBuffer = underlyingRenderbuffer;
    }

    RenderTargetView::RenderTargetView(const ObjectFactory& factory, const std::shared_ptr<IResource>& resource, const TextureViewDesc& window)
    : Resource(AsResource(*resource))
    , _window(window)
    {
        // todo -- handle "view" transformation
    }

    RenderTargetView::RenderTargetView(const std::shared_ptr<IResource>& resource, const TextureViewDesc& window)
    : RenderTargetView(GetObjectFactory(*resource.get()), resource, window)
    {
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

    DepthStencilView::DepthStencilView() {}
    DepthStencilView::DepthStencilView(const intrusive_ptr<OpenGL::Texture>& underlyingTexture)
    {
        if (!glIsTexture(underlyingTexture->AsRawGLHandle()))
            Throw(Exceptions::GenericFailure("Binding non-texture to depth stencil target view"));

        _underlyingTexture = underlyingTexture;
    }
    DepthStencilView::DepthStencilView(const intrusive_ptr<OpenGL::RenderBuffer>& underlyingRenderbuffer)
    {
        if (!glIsRenderbuffer(underlyingRenderbuffer->AsRawGLHandle()))
            Throw(Exceptions::GenericFailure("Binding non-render buffer to depth stencil target view"));

        _underlyingRenderBuffer = underlyingRenderbuffer;
    }

    DepthStencilView::DepthStencilView(const ObjectFactory& factory, const std::shared_ptr<IResource>& resource, const TextureViewDesc& window)
    : Resource(AsResource(*resource))
    , _window(window)
    {
        // todo -- handle "view" transformation
    }

    DepthStencilView::DepthStencilView(const std::shared_ptr<IResource>& resource, const TextureViewDesc& window)
    : DepthStencilView(GetObjectFactory(*resource.get()), resource, window)
    {
    }
}}
