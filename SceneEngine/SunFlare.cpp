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
#include "../RenderCore/Metal/ShaderResource.h"
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
            Desc(bool singlePass)
            {
                XlZeroMemory(*this);
                _singlePass = singlePass;
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
            "game/xleres/effects/occludingsunflare.sh:ps_sunflare:ps_*");

        _commitUniforms = Metal::BoundUniforms(*_commitShader);
        Techniques::TechniqueContext::BindGlobalUniforms(_commitUniforms);
        _commitUniforms.BindConstantBuffers(1, {"Settings"});
        _commitUniforms.BindShaderResources(1, {"InputTexture"});

        ////////////////////////////////////////////////////////////////////////

        _blurShader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2d.vsh:fullscreen:vs_*",
            "game/xleres/effects/occludingsunflare.sh:ps_blur:ps_*",
            (StringMeld<64>() << "SINGLE_PASS=" << int(desc._singlePass)).get());

        _blurUniforms = Metal::BoundUniforms(*_blurShader);
        Techniques::TechniqueContext::BindGlobalUniforms(_blurUniforms);
        _blurUniforms.BindConstantBuffers(1, {"Settings"});
        _blurUniforms.BindShaderResources(1, {"InputTexture"});

        ////////////////////////////////////////////////////////////////////////

        _toRadialShader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/effects/occludingsunflare.sh:vs_sunflare:vs_*",
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

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_validationCallback, _commitShader->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, _blurShader->GetDependencyValidation());
        ::Assets::RegisterAssetDependency(_validationCallback, _toRadialShader->GetDependencyValidation());
    }

    void SunFlare_Execute(
        RenderCore::Metal::DeviceContext* context,
        LightingParserContext& parserContext,
        RenderCore::Metal::ShaderResourceView& depthsSRV)
    {
        if (!parserContext.GetSceneParser() || !parserContext.GetSceneParser()->GetLightCount())
            return;

        using namespace RenderCore;
        SavedTargets savedTargets(context);
        Metal::ViewportDesc savedViewport(*context);
        
        TRY 
        {
            const auto& sunDesc = parserContext.GetSceneParser()->GetLightDesc(0);
            const auto cameraPos = ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld);
            const auto transConstants = BuildGlobalTransformConstants(parserContext.GetProjectionDesc());

///////////////////////////////////////////////////////////////////////////////////////////////////

            const auto worldToView = InvertOrthonormalTransform(parserContext.GetProjectionDesc()._cameraToWorld);
            Float4 sunPos = 
                parserContext.GetProjectionDesc()._worldToProjection 
                * Expand(Float3(cameraPos + 10000.f * sunDesc._negativeLightDirection), 1.f);
            float aspect = savedViewport.Height / savedViewport.Width;
            float sunFlareDist = Tweakable("SunFlareDist", .5f);

            struct Settings
            {
                Float2 _projSpaceSunPosition;
                Float2 _aspectCompenstation;
            }
            settings = 
            {
                Float2(sunPos[0] / sunPos[3], sunPos[1] / sunPos[3]),
                Float2(2.f * sunFlareDist * aspect, 2.f * sunFlareDist)
            };

            auto settingsPkt = MakeSharedPkt(settings);

///////////////////////////////////////////////////////////////////////////////////////////////////

            context->Bind(Metal::Topology::TriangleStrip);
            context->Unbind<Metal::VertexBuffer>();
            context->Unbind<Metal::BoundInputLayout>();

            const bool doDirectBlur = Tweakable("SunFlareDirectBlur", false);
            const bool singlePass = Tweakable("SunFlareSinglePass", false);

            const auto& res = Techniques::FindCachedBoxDep2<SunFlareRes>(singlePass);
            if (!doDirectBlur) {
                Metal::RenderTargetView tempRTV[2];
                Metal::ShaderResourceView tempSRV[2];

                const unsigned resX = Tweakable("SunFlareResX", 128);
                const unsigned resY = Tweakable("SunFlareResY", 32);
                {
                    using namespace BufferUploads;
                    auto desc = CreateDesc(
                        BindFlag::ShaderResource | BindFlag::RenderTarget, 0, GPUAccess::Read | GPUAccess::Write,
                        TextureDesc::Plain2D(resX, resY, Metal::NativeFormat::R8_UNORM),
                        "SunFlareTemp");
                    for (unsigned c=0; c<(singlePass?1u:2u); ++c) {
                        auto offscreen = GetBufferUploads().Transaction_Immediate(desc);
                        tempSRV[c] = Metal::ShaderResourceView(offscreen->GetUnderlying());
                        tempRTV[c] = Metal::RenderTargetView(offscreen->GetUnderlying());
                    }
                }

                context->Bind(Techniques::CommonResources()._blendOpaque);
                context->Bind(Metal::ViewportDesc(0.f, 0.f, float(resX), float(resY), 0.f, 1.f));

                if (!singlePass) {
                    context->Bind(MakeResourceList(tempRTV[1]), nullptr);
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

                {
                    context->Bind(MakeResourceList(tempRTV[0]), nullptr);
                    Metal::ConstantBufferPacket constants[] = { settingsPkt };
                    const Metal::ShaderResourceView* srvs[] = { singlePass ? &depthsSRV : &tempSRV[1] };
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
                context->Bind(Techniques::CommonResources()._blendStraightAlpha);

                {
                    Metal::ConstantBufferPacket constants[] = { settingsPkt };
                    const Metal::ShaderResourceView* srvs[] = { &tempSRV[0] };
                    res._commitUniforms.Apply(
                        *context, 
                        parserContext.GetGlobalUniformsStream(),
                        Metal::UniformsStream(constants, srvs));

                    context->Bind(*res._commitShader);
                    context->Draw(4);
                    context->UnbindPS<Metal::ShaderResourceView>(3, 1);
                }

            } else {

                context->GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);
                context->Bind(savedViewport);
                context->Bind(Techniques::CommonResources()._blendStraightAlpha);

                {
                    Metal::ConstantBufferPacket constants[] = { settingsPkt };
                    const Metal::ShaderResourceView* srvs[] = { &depthsSRV };
                    res._directBlurUniforms.Apply(
                        *context, 
                        parserContext.GetGlobalUniformsStream(),
                        Metal::UniformsStream(constants, srvs));

                    context->Bind(*res._directBlurShader);
                    context->Draw(4);
                    context->UnbindPS<Metal::ShaderResourceView>(3, 1);
                }
                
            }
        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END

        context->Bind(Metal::Topology::TriangleList);
        savedTargets.ResetToOldTargets(context);
        context->Bind(savedViewport);
    }
}

