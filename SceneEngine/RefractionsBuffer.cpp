// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RefractionsBuffer.h"
#include "SceneEngineUtils.h"
#include "MetalStubs.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../Assets/Assets.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../xleres/FileList.h"

#include "../RenderCore/DX11/Metal/DX11Utils.h"
#include "../RenderCore/DX11/Metal/Format.h"

#pragma warning(disable:4127)       // warning C4127: conditional expression is constant

namespace SceneEngine
{
    using namespace RenderCore;
    
    RefractionsBuffer::RefractionsBuffer(const Desc& desc) 
    : _width(desc._width), _height(desc._height)
    {
        auto& uploads = GetBufferUploads();

            // We're loosing a huge amount of colour precision with lower quality
            // pixel formats here... We often duplicate the lighting buffer, so
            // we need a floating point format.
        auto targetDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource|BindFlag::RenderTarget,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, 
                // NativeFormat::R10G10B10A2_UNORM), 
                Format::R16G16B16A16_FLOAT), 
                // NativeFormat::R8G8B8A8_TYPELESS),
            "Refractions");

        auto _refractionsTexture0 = uploads.Transaction_Immediate(targetDesc);
        auto _refractionsTexture1 = uploads.Transaction_Immediate(targetDesc);

        Metal::RenderTargetView refractionsFrontTarget(_refractionsTexture0->GetUnderlying());
        Metal::RenderTargetView refractionsBackTarget(_refractionsTexture1->GetUnderlying());
        Metal::ShaderResourceView refractionsFrontSRV(_refractionsTexture0->GetUnderlying());
        Metal::ShaderResourceView refractionsBackSRV(_refractionsTexture1->GetUnderlying());

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
        RenderCore::Techniques::ParsingContext& parserContext, 
        float standardDeviationForBlur)
    {
#if GFXAPI_TARGET == GFXAPI_DX11	// platformtemp
        CATCH_ASSETS_BEGIN

                // Build a refractions texture
            SavedTargets oldTargets(metalContext);
            Metal::ViewportDesc newViewport(0, 0, float(_width), float(_height), 0.f, 1.f);
            metalContext.Bind(newViewport);

            metalContext.Bind(Techniques::CommonResources()._blendOpaque);
            MetalStubs::UnbindPS<Metal::ShaderResourceView>(metalContext, 12, 1);

            auto res = Metal::ExtractResource<ID3D::Resource>(oldTargets.GetRenderTargets()[0]);
            Metal::ShaderResourceView sourceSRV(Metal::AsResourcePtr(res.get()));
            Metal::TextureDesc2D textureDesc(res.get());
                        
            metalContext.Bind(MakeResourceList(_refractionsFrontTarget), nullptr);
            metalContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(sourceSRV)); // mainTargets._postResolveSRV));
            SetupVertexGeneratorShader(metalContext);
            
            bool needStepDown = 
                _width != textureDesc.Width || _height != textureDesc.Height || textureDesc.SampleDesc.Count > 1;
            if (needStepDown) {
                metalContext.Bind(
                    ::Assets::GetAssetDep<Metal::ShaderProgram>(
                        BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
                        SCENE_ENGINE_RES "/Effects/SeparableFilter.pixel.hlsl:SingleStepDownSample:ps_*",
                        (textureDesc.SampleDesc.Count>1)?"MSAA_SAMPLERS=1":""));
                metalContext.Draw(4);

                metalContext.Bind(MakeResourceList(_refractionsBackTarget), nullptr);
                metalContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(_refractionsFrontSRV));
            }

            float filteringWeights[8];
            XlSetMemory(filteringWeights, 0, sizeof(filteringWeights));
            BuildGaussianFilteringWeights(filteringWeights, standardDeviationForBlur, 7);
            metalContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(MakeMetalCB(filteringWeights, sizeof(filteringWeights))));

            metalContext.Bind(MakeResourceList(_refractionsBackTarget), nullptr);
            metalContext.Bind(
                ::Assets::GetAssetDep<Metal::ShaderProgram>(
                    BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
                    SCENE_ENGINE_RES "/Effects/SeparableFilter.pixel.hlsl:HorizontalBlur:ps_*"));
            metalContext.Draw(4);

            MetalStubs::UnbindPS<Metal::ShaderResourceView>(metalContext, 0, 1);
			metalContext.GetNumericUniforms(ShaderStage::Pixel).Reset();

            metalContext.Bind(MakeResourceList(_refractionsFrontTarget), nullptr);
            metalContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(_refractionsBackSRV));
            metalContext.Bind(
                ::Assets::GetAssetDep<Metal::ShaderProgram>(
                    BASIC2D_VERTEX_HLSL ":fullscreen:vs_*", 
                    SCENE_ENGINE_RES "/Effects/SeparableFilter.pixel.hlsl:VerticalBlur:ps_*"));
            metalContext.Draw(4);
                        
            MetalStubs::UnbindPS<Metal::ShaderResourceView>(metalContext, 0, 1);
            oldTargets.ResetToOldTargets(metalContext);
        
        CATCH_ASSETS_END(parserContext)
#endif
    }

    static Format AsResolvableFormat(Format format)
    {
            //      Change a "typeless" format into the most logical format
            //      for MSAA Resolve operations
            //      special case for 24 bit depth buffers..
        switch (format) {
        case Format::R24G8_TYPELESS:
        case Format::D24_UNORM_S8_UINT:
        case Format::R24_UNORM_X8_TYPELESS:
        case Format::X24_TYPELESS_G8_UINT:
            return Format::R24_UNORM_X8_TYPELESS;
        }

        if (GetComponentType(format) != FormatComponentType::Typeless) {
            return format;
        }

        return Format::Unknown;
    }

    DuplicateDepthBuffer::Desc::Desc(   
                    unsigned width, unsigned height, 
                    RenderCore::Format format, 
                    const BufferUploads::TextureSamples& samping)
    : _width(width), _height(height)
    , _format(format), _sampling(samping)
    {
    }

    DuplicateDepthBuffer::DuplicateDepthBuffer(const Desc& desc)
    {
        auto& uploads = GetBufferUploads();

        auto targetDesc = BuildRenderTargetDesc(
            BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, desc._format, 1, 1, desc._sampling),
            "DepthDupe");

        auto texture = uploads.Transaction_Immediate(targetDesc);

        Metal::ShaderResourceView srv(texture->GetUnderlying(), {AsResolvableFormat(desc._format)});

        _srv = std::move(srv);
        _resource = std::move(texture);
    }

    DuplicateDepthBuffer::~DuplicateDepthBuffer() {}
    

    RenderCore::Metal::ShaderResourceView BuildDuplicatedDepthBuffer(
        RenderCore::Metal::DeviceContext* context, 
		RenderCore::Resource& sourceDepthBuffer)
    {
            // todo --  should we create a non-msaa depth buffer even when the input is MSAA?
            //          it might be simplier for the shader pipeline. And we don't normally need
            //          MSAA in our duplicated depth buffers;
		auto desc = sourceDepthBuffer.GetDesc();

        const bool resolveMSAA = true;
        if (desc._textureDesc._samples._sampleCount > 1 && resolveMSAA) {

            DuplicateDepthBuffer::Desc d(
				desc._textureDesc._width, desc._textureDesc._height, desc._textureDesc._format,
                BufferUploads::TextureSamples::Create());

                //  Resolve into the new buffer
            auto& box = ConsoleRig::FindCachedBox<DuplicateDepthBuffer>(d);
			#if GFXAPI_TARGET == GFXAPI_DX11	// platformtemp
				context->GetUnderlying()->ResolveSubresource(
					Metal::AsResource(*box._resource->GetUnderlying()).GetUnderlying().get(), 0, 
					Metal::AsResource(sourceDepthBuffer).GetUnderlying().get(), 0,
					Metal::AsDXGIFormat(AsResolvableFormat(d._format)));
			#endif
            return box._srv;

        } else {

            DuplicateDepthBuffer::Desc d(
				desc._textureDesc._width, desc._textureDesc._height, desc._textureDesc._format,
				desc._textureDesc._samples);

                //  Copy into the new buffer
            auto& box = ConsoleRig::FindCachedBox<DuplicateDepthBuffer>(d);
            Metal::Copy(*context, Metal::AsResource(*box._resource->GetUnderlying()), Metal::AsResource(sourceDepthBuffer));
            return box._srv;

        }
    }

}

