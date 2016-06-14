// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShadowFrustumDebugger.h"
#include "../IOverlayContext.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../SceneEngine/LightInternal.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/ResourceBox.h"
#include "../../RenderCore/Techniques/ParsingContext.h"
#include "../../RenderCore/Techniques/RenderPass.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Format.h"
#include "../../Assets/Assets.h"
#include "../../ConsoleRig/Console.h"
#include "../../Math/Transformations.h"
#include "../../Utility/StringFormat.h"

namespace Overlays
{
    using namespace RenderOverlays;
    using namespace RenderCore;

    class SFDResources
    {
    public:
        class Desc 
        {
        public:
            unsigned    _cascadeMode;
            bool        _enableNearCascade;

            Desc(unsigned cascadeMode, bool enableNearCascade) 
            : _cascadeMode(cascadeMode), _enableNearCascade(enableNearCascade) {}
        };

        const Metal::ShaderProgram*    _shader;
        Metal::BoundUniforms           _uniforms;
        
        const ::Assets::DepValPtr& GetDependencyValidation() const   { return _depVal; }
        SFDResources(const Desc&);
        ~SFDResources();
    protected:
        ::Assets::DepValPtr _depVal;
    };

    SFDResources::SFDResources(const Desc& desc)
    {
        _shader = &::Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*",
            "game/xleres/deferred/debugging/cascadevis.psh:main:ps_*",
            (const ::Assets::ResChar*)(StringMeld<128, ::Assets::ResChar>() 
                << "SHADOW_CASCADE_MODE=" << desc._cascadeMode 
                << ";SHADOW_ENABLE_NEAR_CASCADE=" << (desc._enableNearCascade?1:0)));

        _uniforms = Metal::BoundUniforms(*_shader);
        Techniques::TechniqueContext::BindGlobalUniforms(_uniforms);
        _uniforms.BindConstantBuffer(Hash64("ArbitraryShadowProjection"), 0, 1);
        _uniforms.BindConstantBuffer(Hash64("OrthogonalShadowProjection"), 1, 1);
        _uniforms.BindConstantBuffer(Hash64("ScreenToShadowProjection"), 2, 1);
        _uniforms.BindShaderResource(Hash64("DepthTexture"), 0, 1);
        
        _depVal = std::make_shared<::Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_depVal, _shader->GetDependencyValidation());
    }

    SFDResources::~SFDResources() {}

    static void OverlayShadowFrustums(
        Metal::DeviceContext& devContext, 
        const RenderCore::Techniques::ProjectionDesc& mainCameraProjDesc,
        const SceneEngine::ShadowProjectionDesc& projectionDesc,
        RenderCore::Techniques::NamedResources* namedResources,
        const Metal::UniformsStream& globalUniforms)
    {
        devContext.Bind(Techniques::CommonResources()._dssDisable);
        devContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);

        Metal::ShaderResourceView* depthSrv = nullptr;
        if (namedResources)
            depthSrv = namedResources->GetSRV(2u);

        auto& res = Techniques::FindCachedBoxDep2<SFDResources>(
            (projectionDesc._projections._mode == SceneEngine::ShadowProjectionDesc::Projections::Mode::Ortho)?2:1,
            projectionDesc._projections._useNearProj);
        devContext.Bind(*res._shader);

        SceneEngine::CB_ArbitraryShadowProjection arbitraryCB;
        SceneEngine::CB_OrthoShadowProjection orthoCB;
        BuildShadowConstantBuffers(arbitraryCB, orthoCB, projectionDesc._projections);

        Metal::ConstantBufferPacket constantBufferPackets[3];
        constantBufferPackets[0] = RenderCore::MakeSharedPkt(arbitraryCB);
        constantBufferPackets[1] = RenderCore::MakeSharedPkt(orthoCB);
        constantBufferPackets[2] = BuildScreenToShadowConstants(
            projectionDesc._projections._normalProjCount,
            arbitraryCB, orthoCB, 
            mainCameraProjDesc._cameraToWorld,
            mainCameraProjDesc._cameraToProjection);
        const Metal::ShaderResourceView* srv[] = { depthSrv };

        res._uniforms.Apply(
            devContext, globalUniforms,
            Metal::UniformsStream(
                constantBufferPackets, nullptr, dimof(constantBufferPackets),
                srv, dimof(srv)));

        devContext.Bind(Topology::TriangleStrip);
        devContext.Draw(4);

        devContext.UnbindPS<Metal::ShaderResourceView>(4, 1);
    }

    void ShadowFrustumDebugger::Render( 
        IOverlayContext& context, Layout& layout, 
        Interactables& interactables, InterfaceState& interfaceState)
    {
        assert(_scene.get());

        if (!_scene->GetShadowProjectionCount()) {
            return;
        }

        static SceneEngine::ShadowProjectionDesc projectionDesc;
        if (!Tweakable("ShadowDebugLock", false)) {
            projectionDesc = _scene->GetShadowProjectionDesc(0, context.GetProjectionDesc());
        }

        auto devContext = Metal::DeviceContext::Get(*context.GetDeviceContext());
        context.ReleaseState();
        OverlayShadowFrustums(
            *devContext, context.GetProjectionDesc(), projectionDesc,
            context.GetNamedResources(), context.GetGlobalUniformsStream());
        context.CaptureState();
        
            //  Get the first shadow projection from the scene, and draw an
            //  outline of all sub-projections with in.
            //  We could also add a control to select different projections
            //  when there are more than one...
        devContext->Bind(Techniques::CommonResources()._dssReadOnly);

        ColorB cols[]= {
            ColorB(196, 230, 230),
            ColorB(255, 128, 128),
            ColorB(128, 255, 128),
            ColorB(128, 128, 255),
            ColorB(255, 255, 128),
            ColorB(128, 255, 255)
        };

        const auto& projections = projectionDesc._projections;
        for (unsigned c=0; c<projections._normalProjCount; ++c) {
            DebuggingDisplay::DrawFrustum(
                &context, Combine(projections._fullProj[c]._viewMatrix, projections._fullProj[c]._projectionMatrix),
                cols[std::min((unsigned)dimof(cols), c)], 0x1);
        }

        if (projections._useNearProj) {
            DebuggingDisplay::DrawFrustum(
                &context, projections._specialNearProjection,
                cols[std::min((unsigned)dimof(cols), projections._normalProjCount)], 0x1);
        }

        for (unsigned c=0; c<projections._normalProjCount; ++c) {
            DebuggingDisplay::DrawFrustum(
                &context, Combine(projections._fullProj[c]._viewMatrix, projections._fullProj[c]._projectionMatrix),
                cols[std::min((unsigned)dimof(cols), c)], 0x2);
        }

        if (projections._useNearProj) {
            DebuggingDisplay::DrawFrustum(
                &context, projections._specialNearProjection,
                cols[std::min((unsigned)dimof(cols), projections._normalProjCount)], 0x2);
        }
    }

    bool ShadowFrustumDebugger::ProcessInput(InterfaceState& interfaceState, const InputSnapshot& input)
    {
        return false;
    }

    ShadowFrustumDebugger::ShadowFrustumDebugger(std::shared_ptr<SceneEngine::ISceneParser> scene)
    : _scene(std::move(scene))
    {}

    ShadowFrustumDebugger::~ShadowFrustumDebugger()
    {
    }

}

