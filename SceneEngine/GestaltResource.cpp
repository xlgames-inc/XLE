// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GestaltResource.h"
#include "../RenderCore/Metal/RenderTargetView.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/Format.h"
#include "../RenderCore/Assets/Services.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../Utility/ParameterPackUtils.h"
#include <tuple>

namespace SceneEngine
{
    using namespace RenderCore;

    namespace Internal
    {
        template<typename Tuple, int Index>
            static void InitViews(Tuple& tuple, BufferUploads::ResourceLocator& loc) {}

        template<typename Tuple, int Index, typename Top, typename... V>
            static void InitViews(Tuple& tuple, BufferUploads::ResourceLocator& loc)
            {
                std::get<Index>(tuple) = Top(loc.GetUnderlying());
                InitViews<Tuple, Index+1, V...>(tuple, loc);
            }

        static bool NeedTypelessFormat(Metal::NativeFormat::Enum fmt)
        {
            return (fmt >= Metal::NativeFormat::R24G8_TYPELESS && fmt <= Metal::NativeFormat::X24_TYPELESS_G8_UINT)
                || (fmt >= Metal::NativeFormat::D32_FLOAT);
        }

        template<typename View>
            static Metal::NativeFormat::Enum SpecializeFormat(Metal::NativeFormat::Enum fmt) { return fmt; }

        template<>
            static Metal::NativeFormat::Enum SpecializeFormat<Metal::ShaderResourceView>(Metal::NativeFormat::Enum fmt)
            { 
                if (    fmt >= Metal::NativeFormat::R24G8_TYPELESS
                    &&  fmt <= Metal::NativeFormat::X24_TYPELESS_G8_UINT) {
                    return Metal::NativeFormat::R24_UNORM_X8_TYPELESS;      // only the depth parts are accessible in this way
                }
                else if (   fmt == Metal::NativeFormat::D32_FLOAT) {
                    return Metal::NativeFormat::R32_FLOAT;
                }
                return fmt;
            }

        template<>
            static Metal::NativeFormat::Enum SpecializeFormat<Metal::DepthStencilView>(Metal::NativeFormat::Enum fmt)
            { 
                if (    fmt >= Metal::NativeFormat::R24G8_TYPELESS
                    &&  fmt <= Metal::NativeFormat::X24_TYPELESS_G8_UINT) {
                    return Metal::NativeFormat::D24_UNORM_S8_UINT;
                }
                return fmt;
            }

        template<typename Tuple, int Index>
            static void InitViews(Tuple&, BufferUploads::ResourceLocator&, Metal::NativeFormat::Enum) {}

        template<typename Tuple, int Index, typename Top, typename... V>
            static void InitViews(Tuple& tuple, BufferUploads::ResourceLocator& loc, Metal::NativeFormat::Enum fmt)
            {
                std::get<Index>(tuple) = Top(loc.GetUnderlying(), SpecializeFormat<Top>(fmt));
                InitViews<Tuple, Index+1, V...>(tuple, loc, fmt);
            }

        using BindFlags = BufferUploads::BindFlag::BitField;
        template<typename... V>
            static BindFlags MakeBindFlags()
            {
                using namespace BufferUploads;
                return (HasType< Metal::ShaderResourceView, std::tuple<V...>>::value ? BindFlag::ShaderResource : 0)
                    |  (HasType<   Metal::RenderTargetView, std::tuple<V...>>::value ? BindFlag::RenderTarget : 0)
                    |  (HasType<Metal::UnorderedAccessView, std::tuple<V...>>::value ? BindFlag::UnorderedAccess : 0)
                    |  (HasType<   Metal::DepthStencilView, std::tuple<V...>>::value ? BindFlag::DepthStencil : 0)
                    ;
            }
    }

    template<typename... Views>
        GestaltResource<Views...>::GestaltResource(
            const BufferUploads::TextureDesc& tdesc,
            const char name[],
            BufferUploads::DataPacket* initialData)
    {
        using namespace BufferUploads;
        auto& uploads = RenderCore::Assets::Services::GetBufferUploads();

        auto tdescCopy = tdesc;

        const bool needTypelessFmt = Internal::NeedTypelessFormat(Metal::NativeFormat::Enum(tdescCopy._nativePixelFormat));
        if (needTypelessFmt)
            tdescCopy._nativePixelFormat = Metal::AsTypelessFormat(Metal::NativeFormat::Enum(tdescCopy._nativePixelFormat));

        auto desc = CreateDesc(
            Internal::MakeBindFlags<Views...>(),
            0, GPUAccess::Read|GPUAccess::Write,
            tdescCopy, name);
        _locator = uploads.Transaction_Immediate(desc, initialData);

        if (needTypelessFmt) {
            Internal::InitViews<std::tuple<Views...>, 0, Views...>(_views, *_locator, Metal::NativeFormat::Enum(tdesc._nativePixelFormat));
        } else {
            Internal::InitViews<std::tuple<Views...>, 0, Views...>(_views, *_locator);
        }
    }

    template<typename... Views>
        GestaltResource<Views...>::GestaltResource(
            const BufferUploads::LinearBufferDesc& lbDesc,
            const char name[], BufferUploads::DataPacket* initialData,
            BufferUploads::BindFlag::BitField extraBindFlags)
    {
        using namespace BufferUploads;
        auto& uploads = RenderCore::Assets::Services::GetBufferUploads();

        auto desc = CreateDesc(
            Internal::MakeBindFlags<Views...>() | extraBindFlags,
            0, GPUAccess::Read|GPUAccess::Write,
            lbDesc, name);
        _locator = uploads.Transaction_Immediate(desc, initialData);

        Internal::InitViews<std::tuple<Views...>, 0, Views...>(_views, *_locator);
    }

    template<typename... Views>
        GestaltResource<Views...>::GestaltResource(GestaltResource&& moveFrom) never_throws
        : _views(std::move(moveFrom._views))
        , _locator(std::move(moveFrom._locator))
    {}

    template<typename... Views>
        GestaltResource<Views...>& GestaltResource<Views...>::operator=(GestaltResource&& moveFrom) never_throws
    {
        _views = std::move(moveFrom._views);
        _locator = std::move(moveFrom._locator);
        return *this;
    }

    template<typename... Views> GestaltResource<Views...>::GestaltResource() {}
    template<typename... Views> GestaltResource<Views...>::~GestaltResource() {}

    template class GestaltResource<RenderCore::Metal::ShaderResourceView>;
    template class GestaltResource<RenderCore::Metal::RenderTargetView>;
    template class GestaltResource<RenderCore::Metal::RenderTargetView, RenderCore::Metal::ShaderResourceView>;
    template class GestaltResource<RenderCore::Metal::DepthStencilView>;
    template class GestaltResource<RenderCore::Metal::DepthStencilView, RenderCore::Metal::ShaderResourceView>;
    template class GestaltResource<RenderCore::Metal::UnorderedAccessView>;
    template class GestaltResource<RenderCore::Metal::UnorderedAccessView, RenderCore::Metal::ShaderResourceView>;
    template class GestaltResource<RenderCore::Metal::RenderTargetView, RenderCore::Metal::UnorderedAccessView, RenderCore::Metal::ShaderResourceView>;
}
