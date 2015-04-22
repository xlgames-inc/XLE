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
#include "../RenderCore/Metal/Shader.h"
#include "../BufferUploads/IBufferUploads.h"

namespace SceneEngine
{
    using namespace RenderCore;
    using namespace RenderCore::Metal;

    TransparencyTargetsBox::Desc::Desc(unsigned width, unsigned height, bool storeColour)
    {
        _width = width; _height = height;
        _storeColour = storeColour;
    }

    TransparencyTargetsBox::TransparencyTargetsBox(const Desc& desc) 
    : _desc(desc)
    {
        using namespace RenderCore;
        using namespace RenderCore::Metal;
        using namespace BufferUploads;

        auto& uploads = *GetBufferUploads();
        auto textureIdsDesc = BuildRenderTargetDesc(
            BindFlag::UnorderedAccess|BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain2D(desc._width, desc._height, NativeFormat::R32_UINT),
            "Trans");
        auto fragmentIdsTexture = uploads.Transaction_Immediate(textureIdsDesc, nullptr)->AdoptUnderlying();

        BufferUploads::BufferDesc nodeListBufferDesc;
        nodeListBufferDesc._type = BufferUploads::BufferDesc::Type::LinearBuffer;
        nodeListBufferDesc._bindFlags = BindFlag::UnorderedAccess|BindFlag::StructuredBuffer|BindFlag::ShaderResource;
        nodeListBufferDesc._cpuAccess = 0;
        nodeListBufferDesc._gpuAccess = GPUAccess::Read|GPUAccess::Write;
        nodeListBufferDesc._allocationRules = 0;
        nodeListBufferDesc._linearBufferDesc._structureByteSize = (desc._storeColour)?(sizeof(float)*3):(sizeof(float)*2);
        nodeListBufferDesc._linearBufferDesc._sizeInBytes = 16*1024*1024*nodeListBufferDesc._linearBufferDesc._structureByteSize;
        auto nodeListBuffer = uploads.Transaction_Immediate(nodeListBufferDesc, nullptr)->AdoptUnderlying();

        RenderCore::Metal::UnorderedAccessView fragmentIdsTextureUAV(fragmentIdsTexture.get());
        RenderCore::Metal::UnorderedAccessView nodeListBufferUAV(nodeListBuffer.get(),
            RenderCore::Metal::UnorderedAccessView::Flags::AttachedCounter);

        RenderCore::Metal::ShaderResourceView fragmentIdsTextureSRV(fragmentIdsTexture.get());
        RenderCore::Metal::ShaderResourceView nodeListBufferSRV(nodeListBuffer.get());

        _fragmentIdsTexture      = std::move(fragmentIdsTexture);
        _nodeListBuffer          = std::move(nodeListBuffer);
        _fragmentIdsTextureUAV   = std::move(fragmentIdsTextureUAV);
        _nodeListBufferUAV       = std::move(nodeListBufferUAV);
        _fragmentIdsTextureSRV   = std::move(fragmentIdsTextureSRV);
        _nodeListBufferSRV       = std::move(nodeListBufferSRV);
    }

    TransparencyTargetsBox::~TransparencyTargetsBox() {}

    void OrderIndependentTransparency_ClearAndBind(
        RenderCore::Metal::DeviceContext* context, 
        TransparencyTargetsBox& transparencyTargets, 
        const RenderCore::Metal::ShaderResourceView& depthBufferDupe)
    {
        SavedTargets prevTargets(context);

        ID3D::UnorderedAccessView* uavs[] = {
            transparencyTargets._fragmentIdsTextureUAV.GetUnderlying(),
            transparencyTargets._nodeListBufferUAV.GetUnderlying() };
        UINT initialCounts[] = { 0, 0 };

        UINT clearValues[4] = { UINT(~0), UINT(~0), UINT(~0), UINT(~0) };
        context->Clear(transparencyTargets._fragmentIdsTextureUAV, clearValues);

        context->GetUnderlying()->OMSetRenderTargetsAndUnorderedAccessViews(
            1, prevTargets.GetRenderTargets(), prevTargets.GetDepthStencilView(),
            1, dimof(uavs), uavs, initialCounts);

        context->BindPS(MakeResourceList(11, depthBufferDupe));
    }

    void OrderIndependentTransparency_Prepare(RenderCore::Metal::DeviceContext* context, LightingParserContext&, const ShaderResourceView& depthBufferDupe)
    {
        ViewportDesc mainViewport(*context);

        auto& transparencyTargets = Techniques::FindCachedBox<TransparencyTargetsBox>(
            TransparencyTargetsBox::Desc(unsigned(mainViewport.Width), unsigned(mainViewport.Height), true));

            //
            //      We need to bind the uav for transparency sorting to
            //      the pixel shader output pipeline. Transparency shaders
            //      will write to the uav (instead of the normal render target)
            //
        
        OrderIndependentTransparency_ClearAndBind(context, transparencyTargets, depthBufferDupe);

            //
            //      Bind some resources required by the glass shader
            //

        auto box5   = Assets::GetAssetDep<RenderCore::Metal::DeferredShaderResource>("game/xleres/refltexture/boxc_5.dds", Metal::DeferredShaderResource::SRGBSpace).GetShaderResource();
        auto box12  = Assets::GetAssetDep<RenderCore::Metal::DeferredShaderResource>("game/xleres/refltexture/boxc_12.dds", Metal::DeferredShaderResource::SRGBSpace).GetShaderResource();
        auto box34  = Assets::GetAssetDep<RenderCore::Metal::DeferredShaderResource>("game/xleres/refltexture/boxc_34.dds", Metal::DeferredShaderResource::SRGBSpace).GetShaderResource();
        context->BindPS(MakeResourceList(8, box12, box34, box5));

        auto& perlinNoiseRes = Techniques::FindCachedBox<PerlinNoiseResources>(PerlinNoiseResources::Desc());
        context->BindPS(MakeResourceList(12, perlinNoiseRes._gradShaderResource, perlinNoiseRes._permShaderResource));

            //
            //      We need to disable multisample antialiasing while rendering
            //      order independent transparency stuff... it just gets to weird
            //      and expensive if MSAA is enabled while doing this. We can still
            //      render to an MSAA buffer... We just need to disable the rasterizer
            //      state flag (so that every pixel write is complete coverage)
            //
    }

    void OrderIndependentTransparency_Resolve(  RenderCore::Metal::DeviceContext* context,
                                                LightingParserContext& parserContext,
                                                const ShaderResourceView& originalDepthStencilSRV)
    {
        ViewportDesc mainViewport(*context);
        SavedTargets savedTargets(context);

        auto metricsUAV = parserContext.GetMetricsBox()->_metricsBufferUAV.GetUnderlying();
        context->GetUnderlying()->OMSetRenderTargetsAndUnorderedAccessViews(
            1, savedTargets.GetRenderTargets(), nullptr,
            1, 1, &metricsUAV, nullptr);

        TRY {
            auto skyTexture = parserContext.GetSceneParser()->GetGlobalLightingDesc()._skyTexture;
            if (skyTexture[0]) {
                SkyTexture_BindPS(context, parserContext, skyTexture, 7);
            }

            auto& transparencyTargets = Techniques::FindCachedBox<TransparencyTargetsBox>(
                TransparencyTargetsBox::Desc(unsigned(mainViewport.Width), unsigned(mainViewport.Height), true));

            context->BindPS(MakeResourceList(
                transparencyTargets._fragmentIdsTextureSRV, transparencyTargets._nodeListBufferSRV, originalDepthStencilSRV));
            context->Bind(Metal::BlendState(
                Metal::BlendOp::Add, Metal::Blend::One, Metal::Blend::InvSrcAlpha));
            context->Bind(Metal::DepthStencilState(false));
            auto& resolveShader = Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen:vs_*", 
                "game/xleres/forward/transparency/resolve.psh:main:ps_*");
            context->Bind(resolveShader);
            SetupVertexGeneratorShader(context);
            context->Draw(4);

            context->UnbindPS<Metal::ShaderResourceView>(0, 3);
        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END

        savedTargets.ResetToOldTargets(context);
    }


}

