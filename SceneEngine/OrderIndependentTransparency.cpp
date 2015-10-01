// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "OrderIndependentTransparency.h"
#include "OITInternal.h"
#include "SceneEngineUtils.h"
#include "SceneParser.h"
#include "LightingParserContext.h"
#include "Noise.h"
#include "MetricsBox.h"
#include "LightDesc.h"
#include "Sky.h"

#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Assets/DeferredShaderResource.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/ResourceLocator.h"

namespace SceneEngine
{
    using namespace RenderCore;

    TransparencyTargetsBox::Desc::Desc(unsigned width, unsigned height, bool storeColour)
    {
        XlZeroMemory(*this);
        _width = width; _height = height;
        _storeColour = storeColour;
    }

    TransparencyTargetsBox::TransparencyTargetsBox(const Desc& desc) 
    : _desc(desc)
    {
        using namespace BufferUploads;

        auto& uploads = GetBufferUploads();
        auto textureIdsDesc = BuildRenderTargetDesc(
            BindFlag::UnorderedAccess|BindFlag::ShaderResource,
            TextureDesc::Plain2D(desc._width, desc._height, Metal::NativeFormat::R32_UINT),
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
            Metal::UnorderedAccessView::Flags::AttachedCounter);

        _fragmentIdsTextureSRV = Metal::ShaderResourceView(_fragmentIdsTexture->GetUnderlying());
        _nodeListBufferSRV = Metal::ShaderResourceView(_nodeListBuffer->GetUnderlying());
    }

    TransparencyTargetsBox::~TransparencyTargetsBox() {}

    void OrderIndependentTransparency_ClearAndBind(
        RenderCore::Metal::DeviceContext& metalContext, 
        TransparencyTargetsBox& transparencyTargets, 
        const RenderCore::Metal::ShaderResourceView& depthBufferDupe)
    {
        SavedTargets prevTargets(&metalContext);

        ID3D::UnorderedAccessView* uavs[] = {
            transparencyTargets._fragmentIdsTextureUAV.GetUnderlying(),
            transparencyTargets._nodeListBufferUAV.GetUnderlying() };
        UINT initialCounts[] = { 0, 0 };

        UINT clearValues[4] = { UINT(~0), UINT(~0), UINT(~0), UINT(~0) };
        metalContext.Clear(transparencyTargets._fragmentIdsTextureUAV, clearValues);

        metalContext.GetUnderlying()->OMSetRenderTargetsAndUnorderedAccessViews(
            1, prevTargets.GetRenderTargets(), prevTargets.GetDepthStencilView(),
            1, dimof(uavs), uavs, initialCounts);

        metalContext.BindPS(MakeResourceList(17, depthBufferDupe));
    }

    TransparencyTargetsBox* OrderIndependentTransparency_Prepare(
        Metal::DeviceContext& metalContext, 
        LightingParserContext&, const Metal::ShaderResourceView& depthBufferDupe)
    {
        Metal::ViewportDesc mainViewport(metalContext);

        auto& transparencyTargets = Techniques::FindCachedBox<TransparencyTargetsBox>(
            TransparencyTargetsBox::Desc(unsigned(mainViewport.Width), unsigned(mainViewport.Height), true));

            //
            //      We need to bind the uav for transparency sorting to
            //      the pixel shader output pipeline. Transparency shaders
            //      will write to the uav (instead of the normal render target)
            //
        
        OrderIndependentTransparency_ClearAndBind(metalContext, transparencyTargets, depthBufferDupe);

            //
            //      Bind some resources required by the glass shader
            //

        // auto box5   = ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/refltexture/boxc_5.dds").GetShaderResource();
        // auto box12  = ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/refltexture/boxc_12.dds").GetShaderResource();
        // auto box34  = ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/refltexture/boxc_34.dds").GetShaderResource();
        // context->BindPS(MakeResourceList(8, box12, box34, box5));

        // auto& perlinNoiseRes = Techniques::FindCachedBox<PerlinNoiseResources>(PerlinNoiseResources::Desc());
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
        LightingParserContext& parserContext,
        TransparencyTargetsBox& transparencyTargets,
        const Metal::ShaderResourceView& originalDepthStencilSRV)
    {
        SavedTargets savedTargets(&metalContext);

        auto metricsUAV = parserContext.GetMetricsBox()->_metricsBufferUAV.GetUnderlying();
        metalContext.GetUnderlying()->OMSetRenderTargetsAndUnorderedAccessViews(
            1, savedTargets.GetRenderTargets(), nullptr,
            1, 1, &metricsUAV, nullptr);

        CATCH_ASSETS_BEGIN
            // auto& transparencyTargets = Techniques::FindCachedBox<TransparencyTargetsBox>(
            //     TransparencyTargetsBox::Desc(unsigned(mainViewport.Width), unsigned(mainViewport.Height), true));

            metalContext.BindPS(MakeResourceList(
                transparencyTargets._fragmentIdsTextureSRV, 
                transparencyTargets._nodeListBufferSRV, 
                originalDepthStencilSRV));
            metalContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
            metalContext.Bind(Techniques::CommonResources()._dssDisable);
            auto& resolveShader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                "game/xleres/forward/transparency/resolve.psh:main:ps_*");
            metalContext.Bind(resolveShader);
            SetupVertexGeneratorShader(&metalContext);
            metalContext.Draw(4);

            metalContext.UnbindPS<Metal::ShaderResourceView>(0, 3);
        CATCH_ASSETS_END(parserContext)

        savedTargets.ResetToOldTargets(&metalContext);
    }


}

