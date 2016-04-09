// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../Utility/IntrusivePtr.h"
#include "../Utility/ParameterPackUtils.h"
#include "../Core/Exceptions.h"
#include <type_traits>
#include <tuple>

namespace RenderCore { class TextureDesc; class LinearBufferDesc; }
namespace BufferUploads { class DataPacket; class ResourceLocator; }

namespace SceneEngine
{
    /// <summary>A GPU resources with one or more "views"</summary>
    /// Usually GPU resources are allocated by first reserving some memory,
    /// and then creating some view objects. The view objects determine how
    /// the memory will be used -- as a render target, or a shader resource (etc).
    ///
    /// This class is a utility to help automate the creation of the resource
    /// and views. It doesn't provide much more functionality; just enough for
    /// construction and some accessors.
    ///
    /// See the GestaltTypes namespace for some renames for common types.
    /// For example, "RTVSRV" might be the most common type. This is a resource
    /// that can be used as a render target and as a texture.
    ///
    /// <seealso cref="SceneEngine::GestaltTypes"/>
    template<typename... Views>
        class GestaltResource
    {
    public:
        using ShaderResourceView = RenderCore::Metal::ShaderResourceView;
        using RenderTargetView = RenderCore::Metal::RenderTargetView;
        using DepthStencilView = RenderCore::Metal::DepthStencilView;
        using UnorderedAccessView = RenderCore::Metal::UnorderedAccessView;

        template<typename = std::enable_if<HasType<ShaderResourceView, std::tuple<Views...>>::value>>
            const ShaderResourceView& SRV() const
        {
            return GetByType<ShaderResourceView>(_views); // using c++14 lookup by type
        }

        template<typename = std::enable_if<HasType<RenderTargetView, std::tuple<Views...>>::value>>
            const RenderTargetView& RTV() const
        {
            return GetByType<RenderTargetView>(_views); // using c++14 lookup by type
        }

        template<typename = std::enable_if<HasType<DepthStencilView, std::tuple<Views...>>::value>>
            const DepthStencilView& DSV() const
        {
            return GetByType<DepthStencilView>(_views); // using c++14 lookup by type
        }

        template<typename = std::enable_if<HasType<UnorderedAccessView, std::tuple<Views...>>::value>>
            const UnorderedAccessView& UAV() const
        {
            return GetByType<UnorderedAccessView>(_views); // using c++14 lookup by type
        }

        template<typename Type, typename = std::enable_if<HasType<Type, std::tuple<Views...>>::value>>
            operator const Type&() const
        {
            return std::get<Type>(_views); // using c++14 lookup by type
        }

        const bool IsGood() const { return _locator.get() != nullptr; }
        const BufferUploads::ResourceLocator& Locator() const { return *_locator; }

        GestaltResource();
        GestaltResource(
            const RenderCore::TextureDesc& desc,
            const char name[], BufferUploads::DataPacket* initialData = nullptr);
        GestaltResource(
            const RenderCore::LinearBufferDesc& desc,
            const char name[], BufferUploads::DataPacket* initialData = nullptr,
            unsigned extraBindFlags = 0u);
        GestaltResource(GestaltResource&& moveFrom) never_throws;
        GestaltResource& operator=(GestaltResource&& moveFrom) never_throws;
        ~GestaltResource();

        GestaltResource(const GestaltResource&) = delete;
        GestaltResource& operator=(const GestaltResource&) = delete;

    private:
        std::tuple<Views...> _views;
        intrusive_ptr<BufferUploads::ResourceLocator> _locator;
    };

    namespace GestaltTypes
    {
        using SRV = GestaltResource<RenderCore::Metal::ShaderResourceView>;
        using RTV = GestaltResource<RenderCore::Metal::RenderTargetView>;
        using RTVSRV = GestaltResource<RenderCore::Metal::RenderTargetView, RenderCore::Metal::ShaderResourceView>;
        using DSV = GestaltResource<RenderCore::Metal::DepthStencilView>;
        using DSVSRV = GestaltResource<RenderCore::Metal::DepthStencilView, RenderCore::Metal::ShaderResourceView>;
        using UAV = GestaltResource<RenderCore::Metal::UnorderedAccessView>;
        using UAVSRV = GestaltResource<RenderCore::Metal::UnorderedAccessView, RenderCore::Metal::ShaderResourceView>;
        using RTVUAVSRV = GestaltResource<RenderCore::Metal::RenderTargetView, RenderCore::Metal::UnorderedAccessView, RenderCore::Metal::ShaderResourceView>;
    }
}

