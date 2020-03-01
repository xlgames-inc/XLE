// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SunFlare.h"
#include "SceneEngineUtils.h"
#include "SceneParser.h"
#include "LightDesc.h"
#include "MetalStubs.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/TextureView.h"
#include "../RenderCore/Format.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/ParsingContext.h"
#include "../Math/Transformations.h"
#include "../Utility/StringFormat.h"
#include "../ConsoleRig/ResourceBox.h"
#include "../ConsoleRig/Console.h"
#include "../Assets/Assets.h"
#include "../Core/Exceptions.h"
#include "../xleres/FileList.h"

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
            "xleres/effects/occludingsunflare.hlsl:vs_sunflare:vs_*",
            "xleres/effects/occludingsunflare.hlsl:ps_sunflare:ps_*",
            (StringMeld<64>() << "ROWS_OPTIMISATION=" << int(desc._rowsOptimisation)).get());

		UniformsStreamInterface commitUsi;
        commitUsi.BindConstantBuffer(0, {Hash64("Settings")});
        commitUsi.BindShaderResource(0, desc._rowsOptimisation ? Hash64("InputRowsTexture") : Hash64("InputTexture"));
		_commitUniforms = Metal::BoundUniforms(
			*_commitShader,
			Metal::PipelineLayoutConfig{},
			Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
			commitUsi);

        ////////////////////////////////////////////////////////////////////////

        _blurShader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            BASIC2D_VERTEX_HLSL ":fullscreen:vs_*",
            "xleres/effects/occludingsunflare.hlsl:ps_blur:ps_*",
            (StringMeld<128>() 
                << "SINGLE_PASS=" << int(desc._singlePass)
                << ";ROWS_OPTIMISATION=" << int(desc._rowsOptimisation)
                << ";OUTPUT_ROWS=" << int(desc._res[1])).get());

		UniformsStreamInterface blurUsi;
        blurUsi.BindConstantBuffer(0, {Hash64("Settings")});
        blurUsi.BindShaderResource(0, Hash64("InputTexture"));
		_blurUniforms = Metal::BoundUniforms(
			*_blurShader,
			Metal::PipelineLayoutConfig{},
			Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
			blurUsi);

        ////////////////////////////////////////////////////////////////////////

        _toRadialShader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "xleres/effects/occludingsunflare.hlsl:vs_sunflare_full:vs_*",
            "xleres/effects/occludingsunflare.hlsl:ps_toradial:ps_*");

		UniformsStreamInterface toRadialUsi;
        toRadialUsi.BindConstantBuffer(0, {Hash64("Settings")});
        toRadialUsi.BindShaderResource(0, Hash64("InputTexture"));
		_toRadialUniforms = Metal::BoundUniforms(
			*_toRadialShader,
			Metal::PipelineLayoutConfig{},
			Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
			toRadialUsi);

        ////////////////////////////////////////////////////////////////////////

        _directBlurShader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "xleres/effects/occludingsunflare.hlsl:vs_sunflare:vs_*",
            "xleres/effects/occludingsunflare.hlsl:ps_sunflare_directblur:ps_*");

		UniformsStreamInterface directBlurUsi;
        directBlurUsi.BindConstantBuffer(0, {Hash64("Settings")});
        directBlurUsi.BindShaderResource(0, Hash64("InputTexture"));
		_directBlurUniforms = Metal::BoundUniforms(
			*_directBlurShader,
			Metal::PipelineLayoutConfig{},
			Techniques::TechniqueContext::GetGlobalUniformsStreamInterface(),
			directBlurUsi);

        ////////////////////////////////////////////////////////////////////////

        {
            auto desc2D = CreateDesc(
                BindFlag::ShaderResource | BindFlag::RenderTarget, 0, GPUAccess::Read | GPUAccess::Write,
                TextureDesc::Plain2D(desc._res[0], desc._res[1], Format::R8_UNORM),
                "SunFlareTemp");

            if (desc._rowsOptimisation) {
                auto descRows = CreateDesc(
                        BindFlag::ShaderResource | BindFlag::RenderTarget, 0, GPUAccess::Read | GPUAccess::Write,
                        TextureDesc::Plain1D(desc._res[0], Format::R8_UNORM),
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
        RenderCore::IThreadContext& threadContext,
        RenderCore::Techniques::ParsingContext& parserContext,
        const RenderCore::Metal::ShaderResourceView& depthsSRV,
		const LightDesc& sunDesc)
    {
        using namespace RenderCore;

        /*if (!parserContext.GetSceneParser() || !parserContext.GetSceneParser()->GetLightCount())
            return;

            // avoid completely if we're facing away
        const auto& sunDesc = parserContext.GetSceneParser()->GetLightDesc(0);*/
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

		auto& context = *RenderCore::Metal::DeviceContext::Get(threadContext);
        SavedTargets savedTargets(context);
        Metal::ViewportDesc savedViewport = context.GetBoundViewport();
        
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

            context.Bind(Topology::TriangleStrip);
            context.Bind(Techniques::CommonResources()._dssDisable);
            context.UnbindInputLayout();

            const bool doDirectBlur = Tweakable("SunFlareDirectBlur", false);
            const bool singlePass = Tweakable("SunFlareSinglePass", false);
            const bool rowsOptimisation = Tweakable("SunFlareRowsOptimisation", false);
            const unsigned resX = Tweakable("SunFlareResX", 64);
            const unsigned resY = Tweakable("SunFlareResY", 32);

            const auto& res = ConsoleRig::FindCachedBoxDep2<SunFlareRes>(singlePass, rowsOptimisation, UInt2(resX, resY));
            if (!doDirectBlur) {
                context.Bind(Techniques::CommonResources()._blendOpaque);
                context.Bind(Metal::ViewportDesc(0.f, 0.f, float(resX), float(resY), 0.f, 1.f));

                if (!singlePass) {
                    context.Bind(MakeResourceList(res._tempRTV[1]), nullptr);
                    ConstantBufferView constants[] = { settingsPkt };
                    const Metal::ShaderResourceView* srvs[] = { &depthsSRV };
                    res._toRadialUniforms.Apply(context, 0, parserContext.GetGlobalUniformsStream());
					res._toRadialUniforms.Apply(context, 1, 
						UniformsStream{
							MakeIteratorRange(constants), 
							UniformsStream::MakeResources(MakeIteratorRange(srvs))});
                
                    context.Bind(*res._toRadialShader);
                    context.Draw(4);
                    MetalStubs::UnbindPS<Metal::ShaderResourceView>(context, 3, 1);
                }

                if (rowsOptimisation)
                    context.Bind(Metal::ViewportDesc(0.f, 0.f, float(resX), float(1), 0.f, 1.f));

                {
                    context.Bind(MakeResourceList(res._tempRTV[0]), nullptr);
                    ConstantBufferView constants[] = { settingsPkt };
                    const Metal::ShaderResourceView* srvs[] = { singlePass ? &depthsSRV : &res._tempSRV[1] };
                    res._blurUniforms.Apply(context, 0, parserContext.GetGlobalUniformsStream());
					res._blurUniforms.Apply(context, 1, 
						UniformsStream{
							MakeIteratorRange(constants), 
							UniformsStream::MakeResources(MakeIteratorRange(srvs))});

                    context.Bind(*res._blurShader);
                    context.Draw(4);
                    MetalStubs::UnbindPS<Metal::ShaderResourceView>(context, 3, 1);
                }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
                
#if GFXAPI_ACTIVE == GFXAPI_DX11	// platformtemp
                context.GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);
#endif
                context.Bind(savedViewport);
                context.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);

                {
                    ConstantBufferView constants[] = { settingsPkt };
                    const Metal::ShaderResourceView* srvs[] = { &res._tempSRV[0] };
                    res._commitUniforms.Apply(context, 0, parserContext.GetGlobalUniformsStream());
					res._commitUniforms.Apply(context, 1, 
						UniformsStream{
							MakeIteratorRange(constants), 
							UniformsStream::MakeResources(MakeIteratorRange(srvs))});

                    context.Bind(*res._commitShader);
                    context.Bind(Topology::TriangleList);
                    context.Draw(64*3);
                    MetalStubs::UnbindPS<Metal::ShaderResourceView>(context, 3, 1);
                }

            } else {

#if GFXAPI_ACTIVE == GFXAPI_DX11	// platformtemp
                context.GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);
#endif
                context.Bind(savedViewport);
                context.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);

                {
                    ConstantBufferView constants[] = { settingsPkt };
                    const Metal::ShaderResourceView* srvs[] = { &depthsSRV };
                    res._directBlurUniforms.Apply(context, 0, parserContext.GetGlobalUniformsStream());
					res._directBlurUniforms.Apply(context, 1, 
                        UniformsStream{
							MakeIteratorRange(constants), 
							UniformsStream::MakeResources(MakeIteratorRange(srvs))});

                    context.Bind(*res._directBlurShader);
                    context.Bind(Topology::TriangleList);
                    context.Draw(64*3);
                    MetalStubs::UnbindPS<Metal::ShaderResourceView>(context, 3, 1);
                }
                
            }

        CATCH_ASSETS_END(parserContext)

        context.Bind(Topology::TriangleList);
        context.Bind(Techniques::CommonResources()._dssReadWrite);
        savedTargets.ResetToOldTargets(context);
        context.Bind(savedViewport);
    }
}

