// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SunFlare.h"
#include "LightingParserContext.h"
#include "SceneEngineUtils.h"
#include "SceneParser.h"
#include "LightDesc.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../Math/Transformations.h"
#include "../Utility/StringFormat.h"
#include "../ConsoleRig/Console.h"
#include "../Assets/Assets.h"
#include "../Core/Exceptions.h"

#include "../RenderCore/DX11/Metal/IncludeDX11.h"       // for unbinding depth buffer

namespace SceneEngine
{
    class SunFlareRes
    {
    public:
        class Desc 
        {
        public:
            bool _singlePass;
            bool _rowsOptimisation;
            UInt2 _res;
            Desc(bool singlePass, bool rowsOptimisation, UInt2 res)
            {
                XlZeroMemory(*this);
                _singlePass = singlePass;
                _rowsOptimisation = rowsOptimisation;
                _res = res;
            }
        };

        const RenderCore::Metal::ShaderProgram* _toRadialShader;
        RenderCore::Metal::BoundUniforms _toRadialUniforms;

        const RenderCore::Metal::ShaderProgram* _blurShader;
        RenderCore::Metal::BoundUniforms _blurUniforms;
        
        const RenderCore::Metal::ShaderProgram* _commitShader;
        RenderCore::Metal::BoundUniforms _commitUniforms;

        const RenderCore::Metal::ShaderProgram* _directBlurShader;
        RenderCore::Metal::BoundUniforms _directBlurUniforms;

        RenderCore::Metal::RenderTargetView _tempRTV[2];
        RenderCore::Metal::ShaderResourceView _tempSRV[2];

        SunFlareRes(const Desc& desc);

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const
            { return _validationCallback; }

    private:
        std::shared_ptr<Assets::DependencyValidation>  _validationCallback;
    };

    SunFlareRes::SunFlareRes(const Desc& desc)
    {
        using namespace RenderCore;

        ////////////////////////////////////////////////////////////////////////

        _commitShader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/effects/occludingsunflare.sh:vs_sunflare:vs_*",
            "game/xleres/effects/occludingsunflare.sh:ps_sunflare:ps_*",
            (StringMeld<64>() << "ROWS_OPTIMISATION=" << int(desc._rowsOptimisation)).get());

        _commitUniforms = Metal::BoundUniforms(*_commitShader);
        Techniques::TechniqueContext::BindGlobalUniforms(_commitUniforms);
        _commitUniforms.BindConstantBuffers(1, {"Settings"});
        _commitUniforms.BindShaderResources(1, {desc._rowsOptimisation ? "InputRowsTexture" : "InputTexture"});

        ////////////////////////////////////////////////////////////////////////

        _blurShader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2d.vsh:fullscreen:vs_*",
            "game/xleres/effects/occludingsunflare.sh:ps_blur:ps_*",
            (StringMeld<128>() 
                << "SINGLE_PASS=" << int(desc._singlePass)
                << ";ROWS_OPTIMISATION=" << int(desc._rowsOptimisation)
                << ";OUTPUT_ROWS=" << int(desc._res[1])).get());

        _blurUniforms = Metal::BoundUniforms(*_blurShader);
        Techniques::TechniqueContext::BindGlobalUniforms(_blurUniforms);
        _blurUniforms.BindConstantBuffers(1, {"Settings"});
        _blurUniforms.BindShaderResources(1, {"InputTexture"});

        ////////////////////////////////////////////////////////////////////////

        _toRadialShader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/effects/occludingsunflare.sh:vs_sunflare_full:vs_*",
            "game/xleres/effects/occludingsunflare.sh:ps_toradial:ps_*");

        _toRadialUniforms = Metal::BoundUniforms(*_toRadialShader);
        Techniques::TechniqueContext::BindGlobalUniforms(_toRadialUniforms);
        _toRadialUniforms.BindConstantBuffers(1, {"Settings"});
        _toRadialUniforms.BindShaderResources(1, {"InputTexture"});

        ////////////////////////////////////////////////////////////////////////

        _directBlurShader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/effects/occludingsunflare.sh:vs_sunflare:vs_*",
            "game/xleres/effects/occludingsunflare.sh:ps_sunflare_directblur:ps_*");

        _directBlurUniforms = Metal::BoundUniforms(*_directBlurShader);
        Techniques::TechniqueContext::BindGlobalUniforms(_directBlurUniforms);
        _directBlurUniforms.BindConstantBuffers(1, {"Settings"});
        _directBlurUniforms.BindShaderResources(1, {"InputTexture"});

        ////////////////////////////////////////////////////////////////////////

        {
            using namespace BufferUploads;

            auto desc2D = CreateDesc(
                BindFlag::ShaderResource | BindFlag::RenderTarget, 0, GPUAccess::Read | GPUAccess::Write,
                TextureDesc::Plain2D(desc._res[0], desc._res[1], Metal::NativeFormat::R8_UNORM),
                "SunFlareTemp");

            if (desc._rowsOptimisation) {
                auto descRows = CreateDesc(
                        BindFlag::ShaderResource | BindFlag::RenderTarget, 0, GPUAccess::Read | GPUAccess::Write,
                        TextureDesc::Plain1D(desc._res[0], Metal::NativeFormat::R8_UNORM),
                        "SunFlareTemp");
                auto offscreen = GetBufferUploads().Transaction_Immediate(descRows);
                _tempSRV[0] = Metal::ShaderResourceView(offscreen->GetUnderlying());
                _tempRTV[0] = Metal::RenderTargetView(offscreen->GetUnderlying());
            } else {
                auto offscreen = GetBufferUploads().Transaction_Immediate(desc2D);
                _tempSRV[0] = Metal::ShaderResourceView(offscreen->GetUnderlying());
                _tempRTV[0] = Metal::RenderTargetView(offscreen->GetUnderlying());
            }

            if (!desc._singlePass) {
                auto offscreen = GetBufferUploads().Transaction_Immediate(desc2D);
                _tempSRV[1] = Metal::ShaderResourceView(offscreen->GetUnderlying());
                _tempRTV[1] = Metal::RenderTargetView(offscreen->GetUnderlying());
            }
        }

        ////////////////////////////////////////////////////////////////////////

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_validationCallback, _commitShader->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, _blurShader->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, _toRadialShader->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, _directBlurShader->GetDependencyValidation());
    }

    void SunFlare_Execute(
        RenderCore::Metal::DeviceContext* context,
        LightingParserContext& parserContext,
        RenderCore::Metal::ShaderResourceView& depthsSRV)
    {
        using namespace RenderCore;

        if (!parserContext.GetSceneParser() || !parserContext.GetSceneParser()->GetLightCount())
            return;

            // avoid completely if we're facing away
        const auto& sunDesc = parserContext.GetSceneParser()->GetLightDesc(0);
        const auto& projDesc = parserContext.GetProjectionDesc();
        if (Dot(ExtractForward_Cam(projDesc._cameraToWorld), sunDesc._position) < 0.f)
            return;

        const auto cameraPos = ExtractTranslation(projDesc._cameraToWorld);
        const auto transConstants = BuildGlobalTransformConstants(projDesc);
            
        const auto worldToView = InvertOrthonormalTransform(projDesc._cameraToWorld);
        Float3 sunWorld(cameraPos + 10000.f * sunDesc._position);
        Float4 sunPos = projDesc._worldToProjection * Expand(sunWorld, 1.f);
        float aspect = projDesc._aspectRatio;
        float flareAngle = Tweakable("SunFlareAngle", 25.f) * .5f * gPI / 180.f;

        Float3 sunView = TransformPoint(worldToView, sunWorld);
        if (sunView[2] >= 0.f) return;

        float hAngle = XlATan2(sunView[0], -sunView[2]);
        float vAngle = XlATan2(sunView[1], -sunView[2]);

///////////////////////////////////////////////////////////////////////////////////////////////////
            // Cull the sunflare when it's off screen
            // It's not perfectly accurate, because the projection of the sprite is a little primitive -- but it works ok.
        float horizFov = projDesc._verticalFov * aspect;
        if (    (hAngle + flareAngle) < -.5f * horizFov
            ||  (hAngle - flareAngle) >  .5f * horizFov
            ||  (vAngle + flareAngle) < -.5f * projDesc._verticalFov
            ||  (vAngle - flareAngle) >  .5f * projDesc._verticalFov)
            return;

        SavedTargets savedTargets(*context);
        Metal::ViewportDesc savedViewport(*context);
        
        CATCH_ASSETS_BEGIN

            Float2 aspectCompen(flareAngle / (0.5f * horizFov), flareAngle / (0.5f * projDesc._verticalFov));

            struct Settings
            {
                Float2 _projSpaceSunPosition;
                Float2 _aspectCompenstation;
            }
            settings = 
            {
                Float2(sunPos[0] / sunPos[3], sunPos[1] / sunPos[3]),
                aspectCompen
            };

            auto settingsPkt = MakeSharedPkt(settings);

///////////////////////////////////////////////////////////////////////////////////////////////////

            context->Bind(Metal::Topology::TriangleStrip);
            context->Bind(Techniques::CommonResources()._dssDisable);
            context->Unbind<Metal::VertexBuffer>();
            context->Unbind<Metal::BoundInputLayout>();

            const bool doDirectBlur = Tweakable("SunFlareDirectBlur", false);
            const bool singlePass = Tweakable("SunFlareSinglePass", false);
            const bool rowsOptimisation = Tweakable("SunFlareRowsOptimisation", false);
            const unsigned resX = Tweakable("SunFlareResX", 64);
            const unsigned resY = Tweakable("SunFlareResY", 32);

            const auto& res = Techniques::FindCachedBoxDep2<SunFlareRes>(singlePass, rowsOptimisation, UInt2(resX, resY));
            if (!doDirectBlur) {
                context->Bind(Techniques::CommonResources()._blendOpaque);
                context->Bind(Metal::ViewportDesc(0.f, 0.f, float(resX), float(resY), 0.f, 1.f));

                if (!singlePass) {
                    context->Bind(MakeResourceList(res._tempRTV[1]), nullptr);
                    Metal::ConstantBufferPacket constants[] = { settingsPkt };
                    const Metal::ShaderResourceView* srvs[] = { &depthsSRV };
                    res._toRadialUniforms.Apply(
                        *context, 
                        parserContext.GetGlobalUniformsStream(),
                        Metal::UniformsStream(constants, srvs));
                
                    context->Bind(*res._toRadialShader);
                    context->Draw(4);
                    context->UnbindPS<Metal::ShaderResourceView>(3, 1);
                }

                if (rowsOptimisation)
                    context->Bind(Metal::ViewportDesc(0.f, 0.f, float(resX), float(1), 0.f, 1.f));

                {
                    context->Bind(MakeResourceList(res._tempRTV[0]), nullptr);
                    Metal::ConstantBufferPacket constants[] = { settingsPkt };
                    const Metal::ShaderResourceView* srvs[] = { singlePass ? &depthsSRV : &res._tempSRV[1] };
                    res._blurUniforms.Apply(
                        *context, 
                        parserContext.GetGlobalUniformsStream(),
                        Metal::UniformsStream(constants, srvs));

                    context->Bind(*res._blurShader);
                    context->Draw(4);
                    context->UnbindPS<Metal::ShaderResourceView>(3, 1);
                }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
                
                context->GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);
                context->Bind(savedViewport);
                context->Bind(Techniques::CommonResources()._blendAlphaPremultiplied);

                {
                    Metal::ConstantBufferPacket constants[] = { settingsPkt };
                    const Metal::ShaderResourceView* srvs[] = { &res._tempSRV[0] };
                    res._commitUniforms.Apply(
                        *context, 
                        parserContext.GetGlobalUniformsStream(),
                        Metal::UniformsStream(constants, srvs));

                    context->Bind(*res._commitShader);
                    context->Bind(Metal::Topology::TriangleList);
                    context->Draw(64*3);
                    context->UnbindPS<Metal::ShaderResourceView>(3, 1);
                }

            } else {

                context->GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);
                context->Bind(savedViewport);
                context->Bind(Techniques::CommonResources()._blendAlphaPremultiplied);

                {
                    Metal::ConstantBufferPacket constants[] = { settingsPkt };
                    const Metal::ShaderResourceView* srvs[] = { &depthsSRV };
                    res._directBlurUniforms.Apply(
                        *context, 
                        parserContext.GetGlobalUniformsStream(),
                        Metal::UniformsStream(constants, srvs));

                    context->Bind(*res._directBlurShader);
                    context->Bind(Metal::Topology::TriangleList);
                    context->Draw(64*3);
                    context->UnbindPS<Metal::ShaderResourceView>(3, 1);
                }
                
            }

        CATCH_ASSETS_END(parserContext)

        context->Bind(Metal::Topology::TriangleList);
        context->Bind(Techniques::CommonResources()._dssReadWrite);
        savedTargets.ResetToOldTargets(*context);
        context->Bind(savedViewport);
    }
}

