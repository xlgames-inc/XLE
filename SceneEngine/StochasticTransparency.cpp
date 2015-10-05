// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StochasticTransparency.h"
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../RenderCore/Metal/RenderTargetView.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../ConsoleRig/Console.h"
#include <tuple>
#include <type_traits>
#include <random>
#include <algorithm>

namespace SceneEngine
{
    using namespace RenderCore;

///////////////////////////////////////////////////////////////////////////////////////////////////
        // this implementation from stack overflow:
        //      http://stackoverflow.com/questions/25958259/how-do-i-find-out-if-a-tuple-contains-a-type
    template <typename T, typename Tuple>
    struct HasType;

    template <typename T>
    struct HasType<T, std::tuple<>> : std::false_type {};

    template <typename T, typename U, typename... Ts>
    struct HasType<T, std::tuple<U, Ts...>> : HasType<T, std::tuple<Ts...>> {};

    template <typename T, typename... Ts>
    struct HasType<T, std::tuple<T, Ts...>> : std::true_type {};
///////////////////////////////////////////////////////////////////////////////////////////////////
    namespace Internal
    {
        template <class T, std::size_t N, class... Args>
            struct IndexOfType
        {
            static const auto value = N;
        };

        template <class T, std::size_t N, class... Args>
            struct IndexOfType<T, N, T, Args...>
        {
            static const auto value = N;
        };

        template <class T, std::size_t N, class U, class... Args>
            struct IndexOfType<T, N, U, Args...>
        {
            static const auto value = IndexOfType<T, N + 1, Args...>::value;
        };
    }

    template <class T, class... Args>
        const T& GetByType(const std::tuple<Args...>& t)
    {
        return std::get<Internal::IndexOfType<T, 0, Args...>::value>(t);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename... Views>
        class GestaltResource
    {
    public:
        std::tuple<Views...> _views;
        intrusive_ptr<BufferUploads::ResourceLocator> _locator;

        template<typename = std::enable_if<HasType<Metal::ShaderResourceView, std::tuple<Views...>>::value>>
            const Metal::ShaderResourceView& SRV() const
        {
            return GetByType<Metal::ShaderResourceView>(_views); // using c++14 lookup by type
        }

        template<typename = std::enable_if<HasType<Metal::RenderTargetView, std::tuple<Views...>>::value>>
            const Metal::RenderTargetView& RTV() const
        {
            return GetByType<Metal::RenderTargetView>(_views); // using c++14 lookup by type
        }

        template<typename = std::enable_if<HasType<Metal::DepthStencilView, std::tuple<Views...>>::value>>
            const Metal::DepthStencilView& DSV() const
        {
            return GetByType<Metal::DepthStencilView>(_views); // using c++14 lookup by type
        }

        template<typename Type, typename = std::enable_if<HasType<Type, std::tuple<Views...>>::value>>
            operator const Type&() const
        {
            return std::get<Type>(_views); // using c++14 lookup by type
        }

        GestaltResource();
        GestaltResource(
            BufferUploads::IManager& bufferUploads,
            const BufferUploads::TextureDesc& desc,
            const char name[], BufferUploads::DataPacket* initialData = nullptr);
        GestaltResource(GestaltResource&& moveFrom) never_throws;
        GestaltResource& operator=(GestaltResource&& moveFrom) never_throws;
        ~GestaltResource();

        GestaltResource(const GestaltResource&) = delete;
        GestaltResource& operator=(const GestaltResource&) = delete;
    };

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
            return  fmt >= Metal::NativeFormat::R24G8_TYPELESS
                &&  fmt <= Metal::NativeFormat::X24_TYPELESS_G8_UINT;
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
            BufferUploads::IManager& bufferUploads,
            const BufferUploads::TextureDesc& tdesc,
            const char name[],
            BufferUploads::DataPacket* initialData)
    {
        using namespace BufferUploads;

        auto tdescCopy = tdesc;

        const bool needTypelessFmt = Internal::NeedTypelessFormat((Metal::NativeFormat::Enum)tdescCopy._nativePixelFormat);
        if (needTypelessFmt)
            tdescCopy._nativePixelFormat = Metal::AsTypelessFormat((Metal::NativeFormat::Enum)tdescCopy._nativePixelFormat);

        auto desc = CreateDesc(
            Internal::MakeBindFlags<Views...>(),
            0, GPUAccess::Read|GPUAccess::Write,
            tdescCopy, name);
        _locator = bufferUploads.Transaction_Immediate(desc, initialData);

        if (needTypelessFmt) {
            Internal::InitViews<std::tuple<Views...>, 0, Views...>(_views, *_locator, (Metal::NativeFormat::Enum)tdesc._nativePixelFormat);
        } else {
            Internal::InitViews<std::tuple<Views...>, 0, Views...>(_views, *_locator);
        }
    }

    template<typename... Views>
        GestaltResource<Views...>::GestaltResource(GestaltResource&& moveFrom) never_throws
        : _views(std::move(moveFrom._views))
        , _locator(std::move(moveForm._locator))
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

    using RTVSRV = GestaltResource<Metal::RenderTargetView, Metal::ShaderResourceView>;
    using SRV = GestaltResource<Metal::ShaderResourceView>;
    using DSV = GestaltResource<Metal::DepthStencilView>;
    using DSVSRV = GestaltResource<Metal::DepthStencilView, Metal::ShaderResourceView>;

    class StochasticTransparencyBox
    {
    public:
        class Desc 
        {
        public:
            unsigned _width, _height;
            Desc(unsigned width, unsigned height) : _width(width), _height(height) {}
        };
        RTVSRV _res;
        SRV _masksTable;
        DSVSRV _stochasticDepths;

        StochasticTransparencyBox(const Desc& desc);
    };

    static void CreateCoverageMasks(
        uint8 destination[], size_t destinationBytes,
        unsigned alphaValues, unsigned masksPerAlphaValue)
    {
        // Create a series of coverage masks for possible alpha values
        // The masks determine which samples are written to in the MSAA buffer
        // For example, if our alpha value is 30%, we want to write to approximately
        // 30% of the samples.
        //
        // But imagine that there is another layer of 30% transparency on top -- it will
        // also write to 30% of the samples. Which samples are selected is important: 
        // depending on what samples overlap for the 2 layers, the result will be different.
        //
        // Each mask calculated here is just a random selection of samples. The randomness
        // adds noise, but it means that the image will gradually coverge on the right result.
        //
        // We will quantize alpha values down to a limited resolution, and for each unique 
        // alpha value we will calculate a number of different coverage masks.

        std::mt19937 rng(0);
        for (unsigned y=0; y<alphaValues; y++) {
            for (unsigned x=0; x<masksPerAlphaValue; x++) {
                const auto maskSize = 8u;
                unsigned numbers[maskSize];
                for (unsigned i=0; i<dimof(numbers); i++)
                    numbers[i] = i;

                std::shuffle(numbers, &numbers[dimof(numbers)], rng);
                std::shuffle(numbers, &numbers[dimof(numbers)], rng);

                    // Create the mask
                    // derived from DX sample by Eric Enderton
                    // This will create purely random masks.
                unsigned int mask = 0;
                auto setBitCount = (float(y) / float(alphaValues-1)) * float(maskSize);
                for (int bit = 0; bit < int(setBitCount); bit++)
                    mask |= (1 << numbers[bit]);

                    // since we floor above, the last bit will only be set in some of the masks
                float prob_of_last_bit = (setBitCount - XlFloor(setBitCount));
                if (std::uniform_real_distribution<>(0, 1.f)(rng) < prob_of_last_bit)
                    mask |= (1 << numbers[int(setBitCount)]);

                assert(((y * masksPerAlphaValue + x)+1)*sizeof(uint8) <= destinationBytes);
                destination[y * masksPerAlphaValue + x] = (uint8)mask;
            }
        }
    }

    StochasticTransparencyBox::StochasticTransparencyBox(const Desc& desc)
    {
        auto& uploads = GetBufferUploads();
        _res = RTVSRV(
            uploads,
            BufferUploads::TextureDesc::Plain2D(1024, 1024, Metal::NativeFormat::R8_UINT),
            "StochasticTransparency");

        const auto alphaValues = 256u;
        const auto masksPerAlphaValue = 2048u;
        auto masksTableData = BufferUploads::CreateBasicPacket(alphaValues*masksPerAlphaValue, nullptr, BufferUploads::TexturePitches(masksPerAlphaValue, alphaValues*masksPerAlphaValue));
        CreateCoverageMasks((uint8*)masksTableData->GetData(), masksTableData->GetDataSize(), alphaValues, masksPerAlphaValue);
        _masksTable = SRV(
            uploads,
            BufferUploads::TextureDesc::Plain2D(masksPerAlphaValue, alphaValues, Metal::NativeFormat::R8_UINT),
            "StochasticTransMasks", masksTableData.get());

        auto samples = BufferUploads::TextureSamples::Create(8, 0);
        _stochasticDepths = DSVSRV(
            uploads,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::D24_UNORM_S8_UINT, 1, 0, samples),
            "StochasticDepths");
    }

    StochasticTransparencyBox* StochasticTransparency_Prepare(
        RenderCore::Metal::DeviceContext& context, 
        LightingParserContext& parserContext)
    {
            // bind the resources we'll need for the initial passes
        Metal::ViewportDesc viewport(context);
        auto& box = Techniques::FindCachedBox2<StochasticTransparencyBox>(
            unsigned(viewport.Width), unsigned(viewport.Height));

        context.Clear(box._stochasticDepths.DSV(), 1.f, 0u);
        context.BindPS(MakeResourceList(18, box._masksTable.SRV()));
        context.Bind(ResourceList<Metal::RenderTargetView, 0>(), &box._stochasticDepths.DSV());

        return &box;
    }

    void StochasticTransparencyBox_Resolve(  
        RenderCore::Metal::DeviceContext& context,
        LightingParserContext& parserContext,
        StochasticTransparencyBox& box)
    {
        if (Tweakable("StochTransDebug", true)) {
            SetupVertexGeneratorShader(context);
            context.BindPS(MakeResourceList(box._stochasticDepths.SRV()));
            auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2d.vsh:fullscreen:vs_*",
                "game/xleres/forward/transparency/stochasticdebug.sh:ps_depthave:ps_*");
            Metal::BoundUniforms uniforms(shader);
            Techniques::TechniqueContext::BindGlobalUniforms(uniforms);
            uniforms.BindShaderResources(1, {"DepthsTexture"});
            uniforms.Apply(
                context, parserContext.GetGlobalUniformsStream(),
                Metal::UniformsStream({}, {&box._stochasticDepths.SRV()}));

            context.Bind(shader);
            context.Draw(4);
        }
    }

}

