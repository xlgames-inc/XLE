// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "OrderIndependentTransparency.h"
#include "OITInternal.h"
#include "SceneEngineUtils.h"
#include "SceneParser.h"
#include "Noise.h"
#include "MetricsBox.h"
#include "LightDesc.h"
#include "Sky.h"
#include "MetalStubs.h"

#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../RenderCore/Techniques/DeferredShaderResource.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Format.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/ResourceBox.h"

#include "../RenderCore/DX11/Metal/IncludeDX11.h"

namespace SceneEngine
{
    using namespace RenderCore;

    TransparencyTargetsBox::Desc::Desc(
        unsigned width, unsigned height, 
        bool storeColour, bool checkInfiniteLoops)
    {
        XlZeroMemory(*this);
        _width = width; _height = height;
        _storeColour = storeColour;
        _checkInfiniteLoops = checkInfiniteLoops;
    }

    TransparencyTargetsBox::TransparencyTargetsBox(const Desc& desc) 
    : _desc(desc)
    {
        auto& uploads = GetBufferUploads();
        auto textureIdsDesc = CreateDesc(
            BindFlag::UnorderedAccess|BindFlag::ShaderResource,
            0, GPUAccess::Read|GPUAccess::Write,
            TextureDesc::Plain2D(desc._width, desc._height, Format::R32_UINT),
            "Trans");
        _fragmentIdsTexture = uploads.Transaction_Immediate(textureIdsDesc);

        unsigned structureSize = (desc._storeColour)?(sizeof(float)*3):(sizeof(float)*2);
        auto nodeListBufferDesc = CreateDesc(
            BindFlag::UnorderedAccess|BindFlag::StructuredBuffer|BindFlag::ShaderResource,
            0, GPUAccess::Read|GPUAccess::Write,
            LinearBufferDesc::Create(16*1024*1024*structureSize, structureSize),
            "OI-NodeBuffer");
        _nodeListBuffer = uploads.Transaction_Immediate(nodeListBufferDesc);

        _fragmentIdsTextureUAV = Metal::UnorderedAccessView(_fragmentIdsTexture->GetUnderlying());
        _nodeListBufferUAV = Metal::UnorderedAccessView(
            _nodeListBuffer->GetUnderlying(),
			TextureViewDesc{
				Format::Unknown,
				TextureViewDesc::All,
				TextureViewDesc::All,
				TextureDesc::Dimensionality::Undefined,
				TextureViewDesc::Flags::AttachedCounter});

        _fragmentIdsTextureSRV = Metal::ShaderResourceView(_fragmentIdsTexture->GetUnderlying());
        _nodeListBufferSRV = Metal::ShaderResourceView(_nodeListBuffer->GetUnderlying());
        _pendingInitialClear = true;

        if (desc._checkInfiniteLoops) {
            _infiniteLoopTexture = uploads.Transaction_Immediate(
                CreateDesc(
                    BindFlag::RenderTarget|BindFlag::ShaderResource,
                    0, GPUAccess::Read|GPUAccess::Write,
                    TextureDesc::Plain2D(desc._width, desc._height, Format::R32_UINT),
                    "Trans"));

            _infiniteLoopRTV = RTV(_infiniteLoopTexture->GetUnderlying());
            _infiniteLoopSRV = SRV(_infiniteLoopTexture->GetUnderlying());
        }
    }

    TransparencyTargetsBox::~TransparencyTargetsBox() {}

    void OrderIndependentTransparency_ClearAndBind(
        RenderCore::Metal::DeviceContext& metalContext, 
        TransparencyTargetsBox& transparencyTargets, 
        const RenderCore::Metal::ShaderResourceView& depthBufferDupe)
    {
#if GFXAPI_ACTIVE == GFXAPI_DX11	// platformtemp
        SavedTargets prevTargets(metalContext);

        ID3D::UnorderedAccessView* uavs[] = {
            transparencyTargets._fragmentIdsTextureUAV.GetUnderlying(),
            transparencyTargets._nodeListBufferUAV.GetUnderlying() };
        UINT initialCounts[] = { 0, 0 };

        UINT clearValues[4] = { UINT(~0), UINT(~0), UINT(~0), UINT(~0) };
        metalContext.ClearUInt(transparencyTargets._fragmentIdsTextureUAV, clearValues);

        if (transparencyTargets._pendingInitialClear) {
            metalContext.ClearUInt(transparencyTargets._nodeListBufferUAV, clearValues);
            transparencyTargets._pendingInitialClear = false;
        }

        metalContext.GetUnderlying()->OMSetRenderTargetsAndUnorderedAccessViews(
            1, prevTargets.GetRenderTargets(), prevTargets.GetDepthStencilView(),
            1, dimof(uavs), uavs, initialCounts);

        metalContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(17, depthBufferDupe));
#endif
    }

    TransparencyTargetsBox* OrderIndependentTransparency_Prepare(
        Metal::DeviceContext& metalContext, 
        RenderCore::Techniques::ParsingContext&, const Metal::ShaderResourceView& depthBufferDupe)
    {
        Metal::ViewportDesc mainViewport = metalContext.GetBoundViewport();

        const auto checkInfiniteLoop = Tweakable("OITransCheckInfinite", true);
        auto& transparencyTargets = ConsoleRig::FindCachedBox<TransparencyTargetsBox>(
            TransparencyTargetsBox::Desc(
            unsigned(mainViewport.Width), unsigned(mainViewport.Height), true, checkInfiniteLoop));

            //
            //      We need to bind the uav for transparency sorting to
            //      the pixel shader output pipeline. Transparency shaders
            //      will write to the uav (instead of the normal render target)
            //
        
        OrderIndependentTransparency_ClearAndBind(metalContext, transparencyTargets, depthBufferDupe);

            //
            //      Bind some resources required by the glass shader
            //

        // auto box5   = ::Assets::GetAssetDep<RenderCore::Techniques::DeferredShaderResource>("xleres/refltexture/boxc_5.dds").GetShaderResource();
        // auto box12  = ::Assets::GetAssetDep<RenderCore::Techniques::DeferredShaderResource>("xleres/refltexture/boxc_12.dds").GetShaderResource();
        // auto box34  = ::Assets::GetAssetDep<RenderCore::Techniques::DeferredShaderResource>("xleres/refltexture/boxc_34.dds").GetShaderResource();
        // context->BindPS(MakeResourceList(8, box12, box34, box5));

        // auto& perlinNoiseRes = ConsoleRig::FindCachedBox<PerlinNoiseResources>(PerlinNoiseResources::Desc());
        // context->BindPS(MakeResourceList(12, perlinNoiseRes._gradShaderResource, perlinNoiseRes._permShaderResource));

            //
            //      We need to disable multisample antialiasing while rendering
            //      order independent transparency stuff... it just gets to weird
            //      and expensive if MSAA is enabled while doing this. We can still
            //      render to an MSAA buffer... We just need to disable the rasterizer
            //      state flag (so that every pixel write is complete coverage)
            //

        return &transparencyTargets;
    }

    void OrderIndependentTransparency_Resolve(  
        RenderCore::Metal::DeviceContext& metalContext,
        RenderCore::Techniques::ParsingContext& parserContext,
        TransparencyTargetsBox& transparencyTargets,
        const Metal::ShaderResourceView& originalDepthStencilSRV,
		MetricsBox& metricsBox)
    {
#if GFXAPI_ACTIVE == GFXAPI_DX11	// platformtemp
        SavedTargets savedTargets(metalContext);
        auto resetMarker = savedTargets.MakeResetMarker(metalContext);

        CATCH_ASSETS_BEGIN
            auto metricsUAV = metricsBox._metricsBufferUAV.GetUnderlying();
            metalContext.GetUnderlying()->OMSetRenderTargetsAndUnorderedAccessViews(
                1, savedTargets.GetRenderTargets(), nullptr,
                1, 1, &metricsUAV, nullptr);

            metalContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(
                transparencyTargets._fragmentIdsTextureSRV, 
                transparencyTargets._nodeListBufferSRV, 
                originalDepthStencilSRV));
            metalContext.Bind(Techniques::CommonResources()._dssDisable);
            SetupVertexGeneratorShader(metalContext);

            const auto checkForInfiniteLoops = transparencyTargets._desc._checkInfiniteLoops;
            if (checkForInfiniteLoops) {
                metalContext.Bind(::Assets::GetAssetDep<Metal::ShaderProgram>(
                    "xleres/basic2D.vsh:fullscreen:vs_*", 
                    "xleres/forward/transparency/resolve.psh:FindInfiniteLoops:ps_*"));
                metalContext.Bind(MakeResourceList(transparencyTargets._infiniteLoopRTV), nullptr);
                metalContext.Bind(Techniques::CommonResources()._blendOpaque);
                metalContext.Draw(4);

                metalContext.GetUnderlying()->OMSetRenderTargetsAndUnorderedAccessViews(
                    1, savedTargets.GetRenderTargets(), nullptr,
                    1, 1, &metricsUAV, nullptr);
                metalContext.GetNumericUniforms(ShaderStage::Pixel).Bind(MakeResourceList(3, transparencyTargets._infiniteLoopSRV));
            }

            metalContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);

            auto& resolveShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "xleres/basic2D.vsh:fullscreen:vs_*", 
                "xleres/forward/transparency/resolve.psh:main:ps_*",
                checkForInfiniteLoops ? "DETECT_INFINITE_LISTS=1" : nullptr);
            metalContext.Bind(resolveShader);

            metalContext.Draw(4);

            MetalStubs::UnbindPS<Metal::ShaderResourceView>(metalContext, 0, 4);
        CATCH_ASSETS_END(parserContext)
#endif
    }


}

