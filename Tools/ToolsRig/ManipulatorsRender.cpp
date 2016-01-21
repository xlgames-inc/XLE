// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ManipulatorsRender.h"
#include "HighlightEffects.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/ShaderResource.h"
#include "../../RenderCore/Assets/DeferredShaderResource.h"
#include "../../RenderCore/Assets/ModelRunTime.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/CommonResources.h"

#include "../../RenderCore/DX11/Metal/DX11Utils.h"

namespace ToolsRig
{
    void Placements_RenderFiltered(
        RenderCore::Metal::DeviceContext& metalContext,
        RenderCore::Techniques::ParsingContext& parserContext,
        SceneEngine::PlacementsEditor* editor,
        const SceneEngine::PlacementGUID* filterBegin,
        const SceneEngine::PlacementGUID* filterEnd,
        uint64 materialGuid)
    {
        using namespace RenderCore::Assets;
        if (materialGuid == ~0ull) {
            editor->RenderFiltered(&metalContext, parserContext, 0, filterBegin, filterEnd);
        } else {
                //  render with a predicate to compare the material binding index to
                //  the given value
            editor->RenderFiltered(&metalContext, parserContext, 0, filterBegin, filterEnd,
                [=](const DelayedDrawCall& e) { 
                    return ((const ModelRenderer*)e._renderer)->GetMaterialBindingForDrawCall(e._drawCallIndex) == materialGuid; 
                });
        }
    }

    void Placements_RenderHighlight(
        RenderCore::IThreadContext& threadContext,
        RenderCore::Techniques::ParsingContext& parserContext,
        SceneEngine::PlacementsEditor* editor,
        const SceneEngine::PlacementGUID* filterBegin,
        const SceneEngine::PlacementGUID* filterEnd,
        uint64 materialGuid)
    {
        CATCH_ASSETS_BEGIN
            auto& metalContext = *RenderCore::Metal::DeviceContext::Get(threadContext);
            BinaryHighlight highlight(metalContext);
            Placements_RenderFiltered(metalContext, parserContext, editor, filterBegin, filterEnd, materialGuid);
            highlight.FinishWithOutline(metalContext, Float3(.65f, .8f, 1.5f));
        CATCH_ASSETS_END(parserContext)
    }

    void RenderCylinderHighlight(
        RenderCore::IThreadContext& threadContext, 
        RenderCore::Techniques::ParsingContext& parserContext,
        const Float3& centre, float radius)
    {
        using namespace RenderCore::Metal;

        auto& metalContext = *DeviceContext::Get(threadContext);

            // unbind the depth buffer
        SceneEngine::SavedTargets savedTargets(metalContext);
        metalContext.GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);

            // create shader resource view for the depth buffer
        ShaderResourceView depthSrv;
        if (savedTargets.GetDepthStencilView())
            depthSrv = ShaderResourceView(ExtractResource<ID3D::Resource>(
                savedTargets.GetDepthStencilView()).get(), 
                NativeFormat::Enum::R24_UNORM_X8_TYPELESS);     // note -- assuming D24S8 depth buffer! We need a better way to get the depth srv

        TRY
        {
                // note -- we might need access to the MSAA defines for this shader
            auto& shaderProgram = Assets::GetAssetDep<ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*",
                "game/xleres/ui/terrainmanipulators.sh:ps_circlehighlight:ps_*");
            
            struct HighlightParameters
            {
                Float3 _center;
                float _radius;
            } highlightParameters = { centre, radius };
            ConstantBufferPacket constantBufferPackets[2];
            constantBufferPackets[0] = RenderCore::MakeSharedPkt(highlightParameters);

            auto& circleHighlight = ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/circlehighlight.png:L");
            const ShaderResourceView* resources[] = { &depthSrv, &circleHighlight.GetShaderResource() };

            BoundUniforms boundLayout(shaderProgram);
            RenderCore::Techniques::TechniqueContext::BindGlobalUniforms(boundLayout);
            boundLayout.BindConstantBuffer(Hash64("CircleHighlightParameters"), 0, 1);
            boundLayout.BindShaderResource(Hash64("DepthTexture"), 0, 1);
            boundLayout.BindShaderResource(Hash64("HighlightResource"), 1, 1);

            metalContext.Bind(shaderProgram);
            boundLayout.Apply(metalContext, 
                parserContext.GetGlobalUniformsStream(),
                UniformsStream(constantBufferPackets, nullptr, dimof(constantBufferPackets), resources, dimof(resources)));

            metalContext.Bind(RenderCore::Techniques::CommonResources()._blendAlphaPremultiplied);
            metalContext.Bind(RenderCore::Techniques::CommonResources()._dssDisable);
            metalContext.Bind(Topology::TriangleStrip);
            metalContext.GetUnderlying()->IASetInputLayout(nullptr);

                // note --  this will render a full screen quad. we could render cylinder geometry instead,
                //          because this decal only affects the area within a cylinder. But it's just for
                //          tools, so the easy way should be fine.
            metalContext.Draw(4);

            ID3D::ShaderResourceView* srv = nullptr;
            metalContext.GetUnderlying()->PSSetShaderResources(3, 1, &srv);
        } 
        CATCH_ASSETS(parserContext)
        CATCH(...) {} 
        CATCH_END

        savedTargets.ResetToOldTargets(metalContext);
    }

    void RenderRectangleHighlight(
        RenderCore::IThreadContext& threadContext, 
        RenderCore::Techniques::ParsingContext& parserContext,
        const Float3& mins, const Float3& maxs)
    {
        using namespace RenderCore::Metal;
        auto& metalContext = *DeviceContext::Get(threadContext);
                
            // unbind the depth buffer
        SceneEngine::SavedTargets savedTargets(metalContext);
        metalContext.GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);

            // create shader resource view for the depth buffer
        ShaderResourceView depthSrv;
        if (savedTargets.GetDepthStencilView())
            depthSrv = ShaderResourceView(ExtractResource<ID3D::Resource>(
                savedTargets.GetDepthStencilView()).get(), 
                NativeFormat::R24_UNORM_X8_TYPELESS);     // note -- assuming D24S8 depth buffer! We need a better way to get the depth srv

        TRY
        {
                // note -- we might need access to the MSAA defines for this shader
            auto& shaderProgram = Assets::GetAssetDep<ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*",
                "game/xleres/ui/terrainmanipulators.sh:ps_rectanglehighlight:ps_*");
            
            struct HighlightParameters
            {
                Float3 _mins; float _dummy0;
                Float3 _maxs; float _dummy1;
            } highlightParameters = {
                mins, 0.f, maxs, 0.f
            };
            ConstantBufferPacket constantBufferPackets[2];
            constantBufferPackets[0] = RenderCore::MakeSharedPkt(highlightParameters);

            auto& circleHighlight = Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/circlehighlight.png:L");
            const ShaderResourceView* resources[] = { &depthSrv, &circleHighlight.GetShaderResource() };

            BoundUniforms boundLayout(shaderProgram);
            RenderCore::Techniques::TechniqueContext::BindGlobalUniforms(boundLayout);
            boundLayout.BindConstantBuffer(Hash64("RectangleHighlightParameters"), 0, 1);
            boundLayout.BindShaderResource(Hash64("DepthTexture"), 0, 1);
            boundLayout.BindShaderResource(Hash64("HighlightResource"), 1, 1);

            metalContext.Bind(shaderProgram);
            boundLayout.Apply(
                metalContext, 
                parserContext.GetGlobalUniformsStream(),
                UniformsStream(constantBufferPackets, nullptr, dimof(constantBufferPackets), resources, dimof(resources)));

            metalContext.Bind(RenderCore::Techniques::CommonResources()._blendAlphaPremultiplied);
            metalContext.Bind(RenderCore::Techniques::CommonResources()._dssDisable);
            metalContext.Bind(Topology::TriangleStrip);
            metalContext.GetUnderlying()->IASetInputLayout(nullptr);

                // note --  this will render a full screen quad. we could render cylinder geometry instead,
                //          because this decal only affects the area within a cylinder. But it's just for
                //          tools, so the easy way should be fine.
            metalContext.Draw(4);

            ID3D::ShaderResourceView* srv = nullptr;
            metalContext.GetUnderlying()->PSSetShaderResources(3, 1, &srv);
        } 
        CATCH_ASSETS(parserContext)
        CATCH(...) {} 
        CATCH_END

        savedTargets.ResetToOldTargets(metalContext);
    }

    void DrawQuadDirect(
        RenderCore::IThreadContext& threadContext, const RenderCore::Metal::ShaderResourceView& srv, 
        Float2 screenMins, Float2 screenMaxs)
    {
        auto& metalContext = *RenderCore::Metal::DeviceContext::Get(threadContext);

        using namespace RenderCore;
        using namespace RenderCore::Metal;
        
        class Vertex
        {
        public:
            Float2  _position;
            Float2  _texCoord;
        } vertices[] = {
            { Float2(screenMins[0], screenMins[1]), Float2(0.f, 0.f) },
            { Float2(screenMins[0], screenMaxs[1]), Float2(0.f, 1.f) },
            { Float2(screenMaxs[0], screenMins[1]), Float2(1.f, 0.f) },
            { Float2(screenMaxs[0], screenMaxs[1]), Float2(1.f, 1.f) }
        };

        InputElementDesc vertexInputLayout[] = {
            InputElementDesc( "POSITION", 0, NativeFormat::R32G32_FLOAT ),
            InputElementDesc( "TEXCOORD", 0, NativeFormat::R32G32_FLOAT )
        };

        VertexBuffer vertexBuffer(vertices, sizeof(vertices));
        metalContext.Bind(MakeResourceList(vertexBuffer), sizeof(Vertex), 0);

        const auto& shaderProgram = ::Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:P2T:" VS_DefShaderModel, 
            "game/xleres/basic.psh:copy_bilinear:" PS_DefShaderModel);
        BoundInputLayout boundVertexInputLayout(std::make_pair(vertexInputLayout, dimof(vertexInputLayout)), shaderProgram);
        metalContext.Bind(boundVertexInputLayout);
        metalContext.Bind(shaderProgram);

        ViewportDesc viewport(metalContext);
        float constants[] = { 1.f / viewport.Width, 1.f / viewport.Height, 0.f, 0.f };
        ConstantBuffer reciprocalViewportDimensions(constants, sizeof(constants));
        const ShaderResourceView* resources[] = { &srv };
        const ConstantBuffer* cnsts[] = { &reciprocalViewportDimensions };
        BoundUniforms boundLayout(shaderProgram);
        boundLayout.BindConstantBuffer(Hash64("ReciprocalViewportDimensions"), 0, 1);
        boundLayout.BindShaderResource(Hash64("DiffuseTexture"), 0, 1);
        boundLayout.Apply(metalContext, UniformsStream(), UniformsStream(nullptr, cnsts, dimof(cnsts), resources, dimof(resources)));

        metalContext.Bind(BlendState(BlendOp::Add, Blend::SrcAlpha, Blend::InvSrcAlpha));
        metalContext.Bind(Topology::TriangleStrip);
        metalContext.Draw(dimof(vertices));

        metalContext.UnbindPS<ShaderResourceView>(0, 1);
    }
}

