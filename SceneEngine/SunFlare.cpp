// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "SunFlare.h"
#include "LightingParserContext.h"
#include "SceneEngineUtils.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../Assets/Assets.h"
#include "../Core/Exceptions.h"

#include "../RenderCore/DX11/Metal/IncludeDX11.h"       // for unbinding depth buffer

namespace SceneEngine
{
    class SunFlareRes
    {
    public:
        class Desc {};

        const RenderCore::Metal::ShaderProgram* _shader;
        RenderCore::Metal::BoundUniforms _uniforms;

        SunFlareRes(const Desc& desc);

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const   { return _validationCallback; }
    private:
        std::shared_ptr<Assets::DependencyValidation>  _validationCallback;
    };

    SunFlareRes::SunFlareRes(const Desc& desc)
    {
        using namespace RenderCore;
        _shader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/effects/occludingsunflare.sh:vs_sunflare:vs_*",
            "game/xleres/effects/occludingsunflare.sh:ps_sunflare:ps_*");

        _uniforms = RenderCore::Metal::BoundUniforms(*_shader);
        Techniques::TechniqueContext::BindGlobalUniforms(_uniforms);
        _uniforms.BindConstantBuffer(Hash64("Settings"), 0, 1);
        _uniforms.BindShaderResource(Hash64("DepthTexture"), 0, 1);

        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_validationCallback, _shader->GetDependencyValidation());
    }

    void SunFlare_Execute(
        RenderCore::Metal::DeviceContext* context,
        LightingParserContext& parserContext,
        RenderCore::Metal::ShaderResourceView& depthsSRV)
    {
        SavedTargets savedTargets(context);
        
        TRY 
        {
            using namespace RenderCore;
            context->GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);

            struct Settings
            {
                Float4x4 _projectionmatrix;
                Float3 _viewSpaceSunPosition;
                unsigned _dummy;
            } 
            settings = 
            {
                parserContext.GetProjectionDesc()._cameraToProjection,
                Float3(0.f, 0.f, 100.f), 0
            };
            Metal::ConstantBufferPacket constants[] = { MakeSharedPkt(settings) };
            const Metal::ShaderResourceView* srvs[] = { &depthsSRV };

            const auto& res = Techniques::FindCachedBoxDep2<SunFlareRes>();
            res._uniforms.Apply(
                *context, 
                parserContext.GetGlobalUniformsStream(),
                Metal::UniformsStream(constants, srvs));

            context->Bind(Techniques::CommonResources()._blendStraightAlpha);
            context->Bind(*res._shader);

            context->Bind(Metal::Topology::TriangleStrip);
            context->Unbind<RenderCore::Metal::VertexBuffer>();
            context->Unbind<RenderCore::Metal::BoundInputLayout>();
            context->Draw(4);

            context->UnbindPS<RenderCore::Metal::ShaderResourceView>(3, 1);
        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END

        context->Bind(RenderCore::Metal::Topology::TriangleList);
        savedTargets.ResetToOldTargets(context);
    }
}

