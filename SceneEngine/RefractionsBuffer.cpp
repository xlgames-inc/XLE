// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RefractionsBuffer.h"
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Metal/Format.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../Assets/Assets.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/ResourceLocator.h"

#include "../RenderCore/DX11/Metal/DX11Utils.h"

#pragma warning(disable:4127)       // warning C4127: conditional expression is constant

namespace SceneEngine
{
    using namespace RenderCore;
    using namespace RenderCore::Metal;
    
    RefractionsBuffer::RefractionsBuffer(const Desc& desc) 
    : _width(desc._width), _height(desc._height)
    {
        using namespace BufferUploads;
        auto& uploads = GetBufferUploads();

            // We're loosing a huge amount of colour precision with lower quality
            // pixel formats here... We often duplicate the lighting buffer, so
            // we need a floating point format.
        auto targetDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource|BindFlag::RenderTarget,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, 
                // NativeFormat::R10G10B10A2_UNORM), 
                NativeFormat::R16G16B16A16_FLOAT), 
                // NativeFormat::R8G8B8A8_TYPELESS),
            "Refractions");

        auto _refractionsTexture0 = uploads.Transaction_Immediate(targetDesc);
        auto _refractionsTexture1 = uploads.Transaction_Immediate(targetDesc);

        RenderTargetView refractionsFrontTarget(_refractionsTexture0->GetUnderlying());
        RenderTargetView refractionsBackTarget(_refractionsTexture1->GetUnderlying());
        ShaderResourceView refractionsFrontSRV(_refractionsTexture0->GetUnderlying());
        ShaderResourceView refractionsBackSRV(_refractionsTexture1->GetUnderlying());

        _refractionsTexture[0] = std::move(_refractionsTexture0);
        _refractionsTexture[1] = std::move(_refractionsTexture1);
        _refractionsFrontTarget = std::move(refractionsFrontTarget);
        _refractionsBackTarget = std::move(refractionsBackTarget);
        _refractionsFrontSRV = std::move(refractionsFrontSRV);
        _refractionsBackSRV = std::move(refractionsBackSRV);
    }

    RefractionsBuffer::~RefractionsBuffer() {}

        ////////////////////////////////

    void RefractionsBuffer::Build(
        RenderCore::Metal::DeviceContext& metalContext, 
        LightingParserContext& parserContext, 
        float standardDeviationForBlur)
    {
        CATCH_ASSETS_BEGIN

                // Build a refractions texture
            SavedTargets oldTargets(metalContext);
            ViewportDesc newViewport(0, 0, float(_width), float(_height), 0.f, 1.f);
            metalContext.Bind(newViewport);

            metalContext.Bind(Techniques::CommonResources()._blendOpaque);
            metalContext.UnbindPS<ShaderResourceView>(12, 1);

            auto res = ExtractResource<ID3D::Resource>(oldTargets.GetRenderTargets()[0]);
            ShaderResourceView sourceSRV(res.get());
            TextureDesc2D textureDesc(res.get());
                        
            metalContext.Bind(MakeResourceList(_refractionsFrontTarget), nullptr);
            metalContext.BindPS(MakeResourceList(sourceSRV)); // mainTargets._postResolveSRV));
            SetupVertexGeneratorShader(&metalContext);
            
            bool needStepDown = 
                _width != textureDesc.Width || _height != textureDesc.Height || textureDesc.SampleDesc.Count > 1;
            if (needStepDown) {
                metalContext.Bind(
                    ::Assets::GetAssetDep<Metal::ShaderProgram>(
                        "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                        "game/xleres/Effects/SeparableFilter.psh:SingleStepDownSample:ps_*",
                        (textureDesc.SampleDesc.Count>1)?"MSAA_SAMPLERS=1":""));
                metalContext.Draw(4);

                metalContext.Bind(MakeResourceList(_refractionsBackTarget), nullptr);
                metalContext.BindPS(MakeResourceList(_refractionsFrontSRV));
            }

            float filteringWeights[8];
            XlSetMemory(filteringWeights, 0, sizeof(filteringWeights));
            BuildGaussianFilteringWeights(filteringWeights, standardDeviationForBlur, 7);
            metalContext.BindPS(MakeResourceList(Metal::ConstantBuffer(filteringWeights, sizeof(filteringWeights))));

            metalContext.Bind(MakeResourceList(_refractionsBackTarget), nullptr);
            metalContext.Bind(
                ::Assets::GetAssetDep<Metal::ShaderProgram>(
                    "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                    "game/xleres/Effects/SeparableFilter.psh:HorizontalBlur:ps_*"));
            metalContext.Draw(4);

            metalContext.UnbindPS<ShaderResourceView>(0, 1);

            metalContext.Bind(MakeResourceList(_refractionsFrontTarget), nullptr);
            metalContext.BindPS(MakeResourceList(_refractionsBackSRV));
            metalContext.Bind(
                ::Assets::GetAssetDep<Metal::ShaderProgram>(
                    "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                    "game/xleres/Effects/SeparableFilter.psh:VerticalBlur:ps_*"));
            metalContext.Draw(4);
                        
            metalContext.UnbindPS<ShaderResourceView>(0, 1);
            oldTargets.ResetToOldTargets(metalContext);
        
        CATCH_ASSETS_END(parserContext)
    }

    static NativeFormat::Enum AsResolvableFormat(NativeFormat::Enum format)
    {
            //      Change a "typeless" format into the most logical format
            //      for MSAA Resolve operations
            //      special case for 24 bit depth buffers..
        switch ((unsigned)format) {
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
            return (NativeFormat::Enum)DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        }

        if (GetComponentType(format) != FormatComponentType::Typeless) {
            return format;
        }

        return NativeFormat::Unknown;
    }

    DuplicateDepthBuffer::Desc::Desc(   
                    unsigned width, unsigned height, 
                    RenderCore::Metal::NativeFormat::Enum format, 
                    const BufferUploads::TextureSamples& samping)
    : _width(width), _height(height)
    , _format(format), _sampling(samping)
    {
    }

    DuplicateDepthBuffer::DuplicateDepthBuffer(const Desc& desc)
    {
        using namespace BufferUploads;
        auto& uploads = GetBufferUploads();

        auto targetDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, desc._format, 1, 1, desc._sampling),
            "DepthDupe");

        auto texture = uploads.Transaction_Immediate(targetDesc)->AdoptUnderlying();

        ShaderResourceView srv(texture.get(), AsResolvableFormat(desc._format));

        _srv = std::move(srv);
        _resource = std::move(texture);
    }

    DuplicateDepthBuffer::~DuplicateDepthBuffer() {}
    

    RenderCore::Metal::ShaderResourceView BuildDuplicatedDepthBuffer(
        RenderCore::Metal::DeviceContext* context, ID3D::Resource* sourceDepthBuffer)
    {
            // todo --  should we create a non-msaa depth buffer even when the input is MSAA?
            //          it might be simplier for the shader pipeline. And we don't normally need
            //          MSAA in our duplicated depth buffers;
        TextureDesc2D textureDesc(sourceDepthBuffer);

        const bool resolveMSAA = true;
        if (textureDesc.SampleDesc.Count > 1 && resolveMSAA) {

            DuplicateDepthBuffer::Desc d(
                textureDesc.Width, textureDesc.Height, (NativeFormat::Enum)(textureDesc.Format),
                BufferUploads::TextureSamples::Create());

                //  Resolve into the new buffer
            auto& box = Techniques::FindCachedBox<DuplicateDepthBuffer>(d);
            context->GetUnderlying()->ResolveSubresource(
                box._resource.get(), 0, sourceDepthBuffer, 0,
                AsDXGIFormat(AsResolvableFormat(d._format)));
            return box._srv;

        } else {

            DuplicateDepthBuffer::Desc d(
                textureDesc.Width, textureDesc.Height, (NativeFormat::Enum)(textureDesc.Format),
                BufferUploads::TextureSamples::Create(uint8(textureDesc.SampleDesc.Count), uint8(textureDesc.SampleDesc.Quality)));

                //  Copy into the new buffer
            auto& box = Techniques::FindCachedBox<DuplicateDepthBuffer>(d);
            context->GetUnderlying()->CopyResource(box._resource.get(), sourceDepthBuffer);
            return box._srv;

        }
    }

}

