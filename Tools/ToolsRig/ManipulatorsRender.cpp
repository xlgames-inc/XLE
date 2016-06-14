// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ManipulatorsRender.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/TextureView.h"
#include "../../RenderCore/Assets/DeferredShaderResource.h"
#include "../../RenderCore/Assets/ModelRunTime.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderCore/Techniques/TechniqueMaterial.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Techniques/PredefinedCBLayout.h"
#include "../../RenderCore/Format.h"
#include "../../RenderOverlays/HighlightEffects.h"
#include "../../Math/Transformations.h"

#include "../../RenderCore/DX11/Metal/DX11Utils.h"

namespace ToolsRig
{
    using namespace RenderCore;

    void Placements_RenderFiltered(
        RenderCore::IThreadContext& threadContext,
        Techniques::ParsingContext& parserContext,
        unsigned techniqueIndex,
        SceneEngine::PlacementsRenderer& renderer,
        const SceneEngine::PlacementCellSet& cellSet,
        const SceneEngine::PlacementGUID* filterBegin,
        const SceneEngine::PlacementGUID* filterEnd,
        uint64 materialGuid)
    {
        using namespace RenderCore::Assets;
        auto metalContext = RenderCore::Metal::DeviceContext::Get(threadContext);
        if (materialGuid == ~0ull) {
            renderer.RenderFiltered(*metalContext.get(), parserContext, techniqueIndex, cellSet, filterBegin, filterEnd);
        } else {
                //  render with a predicate to compare the material binding index to
                //  the given value
            renderer.RenderFiltered(
                *metalContext, parserContext, techniqueIndex, cellSet, filterBegin, filterEnd,
                [=](const DelayedDrawCall& e) { 
                    return ((const ModelRenderer*)e._renderer)->GetMaterialBindingForDrawCall(e._drawCallIndex) == materialGuid; 
                });
        }
    }

    void Placements_RenderHighlight(
        RenderCore::IThreadContext& threadContext,
        Techniques::ParsingContext& parserContext,
        SceneEngine::PlacementsRenderer& renderer,
        const SceneEngine::PlacementCellSet& cellSet,
        const SceneEngine::PlacementGUID* filterBegin,
        const SceneEngine::PlacementGUID* filterEnd,
        uint64 materialGuid)
    {
        CATCH_ASSETS_BEGIN
            RenderOverlays::BinaryHighlight highlight(threadContext, parserContext.GetNamedResources());
            Placements_RenderFiltered(
                threadContext, parserContext, RenderCore::Techniques::TechniqueIndex::Forward,
                renderer, cellSet, filterBegin, filterEnd, materialGuid);
            highlight.FinishWithOutline(threadContext, Float3(.65f, .8f, 1.5f));
        CATCH_ASSETS_END(parserContext)
    }

    void Placements_RenderShadow(
        RenderCore::IThreadContext& threadContext,
        Techniques::ParsingContext& parserContext,
        SceneEngine::PlacementsRenderer& renderer,
        const SceneEngine::PlacementCellSet& cellSet,
        const SceneEngine::PlacementGUID* filterBegin,
        const SceneEngine::PlacementGUID* filterEnd,
        uint64 materialGuid)
    {
        CATCH_ASSETS_BEGIN
            RenderOverlays::BinaryHighlight highlight(threadContext, parserContext.GetNamedResources());
            Placements_RenderFiltered(
                threadContext, parserContext, RenderCore::Techniques::TechniqueIndex::Forward,
                renderer, cellSet, filterBegin, filterEnd, materialGuid);
            highlight.FinishWithShadow(threadContext, Float4(.025f, .025f, .025f, 0.85f));
        CATCH_ASSETS_END(parserContext)
    }

    void RenderCylinderHighlight(
        IThreadContext& threadContext, 
        Techniques::ParsingContext& parserContext,
        const Float3& centre, float radius)
    {
#if GFXAPI_ACTIVE == GFXAPI_DX11
        auto& metalContext = *Metal::DeviceContext::Get(threadContext);

            // unbind the depth buffer
        SceneEngine::SavedTargets savedTargets(metalContext);
        metalContext.GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);

            // create shader resource view for the depth buffer
        Metal::ShaderResourceView depthSrv;
        if (savedTargets.GetDepthStencilView())
            depthSrv = Metal::ShaderResourceView(Metal::ExtractResource<ID3D::Resource>(
                savedTargets.GetDepthStencilView()).get(), 
                TextureViewWindow{{TextureViewWindow::Aspect::Depth}});

        TRY
        {
                // note -- we might need access to the MSAA defines for this shader
            auto& shaderProgram = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*",
                "game/xleres/ui/terrainmanipulators.sh:ps_circlehighlight:ps_*");
            
            struct HighlightParameters
            {
                Float3 _center;
                float _radius;
            } highlightParameters = { centre, radius };
            Metal::ConstantBufferPacket constantBufferPackets[2];
            constantBufferPackets[0] = MakeSharedPkt(highlightParameters);

            auto& circleHighlight = ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/circlehighlight.png:L");
            const Metal::ShaderResourceView* resources[] = { &depthSrv, &circleHighlight.GetShaderResource() };

            Metal::BoundUniforms boundLayout(shaderProgram);
            RenderCore::Techniques::TechniqueContext::BindGlobalUniforms(boundLayout);
            boundLayout.BindConstantBuffer(Hash64("CircleHighlightParameters"), 0, 1);
            boundLayout.BindShaderResource(Hash64("DepthTexture"), 0, 1);
            boundLayout.BindShaderResource(Hash64("HighlightResource"), 1, 1);

            metalContext.Bind(shaderProgram);
            boundLayout.Apply(metalContext, 
                parserContext.GetGlobalUniformsStream(),
                Metal::UniformsStream(constantBufferPackets, nullptr, dimof(constantBufferPackets), resources, dimof(resources)));

            metalContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
            metalContext.Bind(Techniques::CommonResources()._dssDisable);
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
#endif
    }

    void RenderRectangleHighlight(
        IThreadContext& threadContext, 
        Techniques::ParsingContext& parserContext,
        const Float3& mins, const Float3& maxs,
		RectangleHighlightType type)
    {
#if GFXAPI_ACTIVE == GFXAPI_DX11
        auto& metalContext = *Metal::DeviceContext::Get(threadContext);
                
            // unbind the depth buffer
        SceneEngine::SavedTargets savedTargets(metalContext);
        metalContext.GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);

            // create shader resource view for the depth buffer
        Metal::ShaderResourceView depthSrv;
        if (savedTargets.GetDepthStencilView())
            depthSrv = Metal::ShaderResourceView(Metal::ExtractResource<ID3D::Resource>(
                savedTargets.GetDepthStencilView()).get(), 
				TextureViewWindow{{TextureViewWindow::Aspect::Depth}});

        TRY
        {
                // note -- we might need access to the MSAA defines for this shader
            auto& shaderProgram = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*",
                (type == RectangleHighlightType::Tool)
					? "game/xleres/ui/terrainmanipulators.sh:ps_rectanglehighlight:ps_*"
					: "game/xleres/ui/terrainmanipulators.sh:ps_lockedareahighlight:ps_*");
            
            struct HighlightParameters
            {
                Float3 _mins; float _dummy0;
                Float3 _maxs; float _dummy1;
            } highlightParameters = {
                mins, 0.f, maxs, 0.f
            };
            Metal::ConstantBufferPacket constantBufferPackets[2];
            constantBufferPackets[0] = MakeSharedPkt(highlightParameters);

            auto& circleHighlight = ::Assets::GetAssetDep<RenderCore::Assets::DeferredShaderResource>("game/xleres/DefaultResources/circlehighlight.png:L");
            const Metal::ShaderResourceView* resources[] = { &depthSrv, &circleHighlight.GetShaderResource() };

            Metal::BoundUniforms boundLayout(shaderProgram);
            Techniques::TechniqueContext::BindGlobalUniforms(boundLayout);
            boundLayout.BindConstantBuffer(Hash64("RectangleHighlightParameters"), 0, 1);
            boundLayout.BindShaderResource(Hash64("DepthTexture"), 0, 1);
            boundLayout.BindShaderResource(Hash64("HighlightResource"), 1, 1);

            metalContext.Bind(shaderProgram);
            boundLayout.Apply(
                metalContext, 
                parserContext.GetGlobalUniformsStream(),
                Metal::UniformsStream(constantBufferPackets, nullptr, dimof(constantBufferPackets), resources, dimof(resources)));

            metalContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
            metalContext.Bind(Techniques::CommonResources()._dssDisable);
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
#endif
    }

    class ManipulatorResBox
    {
    public:
        class Desc {};

        const ::Assets::DepValPtr& GetDependencyValidation() { return _depVal; }

        Techniques::TechniqueMaterial _materialGenCylinder;

        ManipulatorResBox(const Desc&);
    private:
        ::Assets::DepValPtr _depVal;
    };

    ManipulatorResBox::ManipulatorResBox(const Desc&)
    : _materialGenCylinder(
        InputLayout((const InputElementDesc*)nullptr, 0),
        { Techniques::ObjectCB::LocalTransform, Techniques::ObjectCB::BasicMaterialConstants },
        ParameterBox({ std::make_pair(u("SHAPE"), "4") }))
    {
        _depVal = std::make_shared<::Assets::DependencyValidation>();
    }

    void DrawWorldSpaceCylinder(
        RenderCore::IThreadContext& threadContext, Techniques::ParsingContext& parserContext,
        Float3 origin, Float3 axis, float radius)
    {
        CATCH_ASSETS_BEGIN
            auto& box = Techniques::FindCachedBoxDep2<ManipulatorResBox>();
            auto localToWorld = Identity<Float4x4>();
            SetTranslation(localToWorld, origin);
            SetUp(localToWorld, axis);

            Float3 forward = Float3(0.f, 0.f, 1.f);
            Float3 right = Cross(forward, axis);
            if (XlAbs(MagnitudeSquared(right)) < 1e-10f)
                right = Cross(Float3(0.f, 1.f, 0.f), axis);
            right = Normalize(right);
            Float3 adjustedForward = Normalize(Cross(axis, right));
            SetForward(localToWorld, radius * adjustedForward);
            SetRight(localToWorld, radius * right);

            auto shader = box._materialGenCylinder.FindVariation(
                parserContext, Techniques::TechniqueIndex::Forward, 
                "game/xleres/ui/objgen/arealight.tech");
            
            if (shader._shader._shaderProgram) {
                auto& metalContext = *Metal::DeviceContext::Get(threadContext);
                ParameterBox matParams;
                matParams.SetParameter(u("MaterialDiffuse"), Float3(0.03f, 0.03f, .33f));
                matParams.SetParameter(u("Opacity"), 0.125f);
                auto transformPacket = Techniques::MakeLocalTransformPacket(
                    localToWorld, ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld));
                shader._shader.Apply(
                    metalContext, parserContext, 
                    { transformPacket, shader._cbLayout->BuildCBDataAsPkt(matParams) });

                auto& commonRes = Techniques::CommonResources();
                metalContext.Bind(commonRes._blendStraightAlpha);
                metalContext.Bind(commonRes._dssReadOnly);
                metalContext.Unbind<Metal::VertexBuffer>();
                metalContext.Bind(Topology::TriangleList);
                
                const unsigned vertexCount = 32 * 6;	// (must agree with the shader!)
                metalContext.Draw(vertexCount);
            }

        CATCH_ASSETS_END(parserContext)
    }

    void DrawQuadDirect(
        RenderCore::IThreadContext& threadContext, const RenderCore::Metal::ShaderResourceView& srv, 
        Float2 screenMins, Float2 screenMaxs)
    {
        auto& metalContext = *RenderCore::Metal::DeviceContext::Get(threadContext);

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
            InputElementDesc( "POSITION", 0, Format::R32G32_FLOAT ),
            InputElementDesc( "TEXCOORD", 0, Format::R32G32_FLOAT )
        };

        Metal::VertexBuffer vertexBuffer(vertices, sizeof(vertices));
        metalContext.Bind(MakeResourceList(vertexBuffer), sizeof(Vertex), 0);

        const auto& shaderProgram = ::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:P2T:" VS_DefShaderModel, 
            "game/xleres/basic.psh:copy_bilinear:" PS_DefShaderModel);
        Metal::BoundInputLayout boundVertexInputLayout(std::make_pair(vertexInputLayout, dimof(vertexInputLayout)), shaderProgram);
        metalContext.Bind(boundVertexInputLayout);
        metalContext.Bind(shaderProgram);

        Metal::ViewportDesc viewport(metalContext);
        float constants[] = { 1.f / viewport.Width, 1.f / viewport.Height, 0.f, 0.f };
        Metal::ConstantBuffer reciprocalViewportDimensions(constants, sizeof(constants));
        const Metal::ShaderResourceView* resources[] = { &srv };
        const Metal::ConstantBuffer* cnsts[] = { &reciprocalViewportDimensions };
        Metal::BoundUniforms boundLayout(shaderProgram);
        boundLayout.BindConstantBuffer(Hash64("ReciprocalViewportDimensionsCB"), 0, 1);
        boundLayout.BindShaderResource(Hash64("DiffuseTexture"), 0, 1);
        boundLayout.Apply(metalContext, Metal::UniformsStream(), Metal::UniformsStream(nullptr, cnsts, dimof(cnsts), resources, dimof(resources)));

        metalContext.Bind(Metal::BlendState(BlendOp::Add, Blend::SrcAlpha, Blend::InvSrcAlpha));
        metalContext.Bind(Topology::TriangleStrip);
        metalContext.Draw(dimof(vertices));

        metalContext.UnbindPS<Metal::ShaderResourceView>(0, 1);
    }
}

