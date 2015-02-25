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
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../Assets/Assets.h"
#include "../../ConsoleRig/Console.h"
#include "../../Math/Transformations.h"
#include "../../Utility/StringFormat.h"

#include "../../SceneEngine/SceneEngineUtility.h"
#include "../../RenderCore/DX11/Metal/DX11Utils.h"

namespace Overlays
{
    using namespace RenderOverlays;
    using namespace RenderCore;
    using namespace RenderCore::Metal;

    class SFDResources
    {
    public:
        class Desc 
        {
        public:
            unsigned _cascadeMode;
            Desc(unsigned cascadeMode) : _cascadeMode(cascadeMode) {}
        };

        ShaderProgram*  _shader;
        BoundUniforms   _uniforms;
        
        const Assets::DependencyValidation& GetDependencyValidation() const   { return *_depVal; }
        SFDResources(const Desc&);
        ~SFDResources();
    protected:
        std::shared_ptr<Assets::DependencyValidation> _depVal;
    };

    SFDResources::SFDResources(const Desc& desc)
    {
        _shader = &Assets::GetAssetDep<ShaderProgram>(
            "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*",
            "game/xleres/deferred/debugging/cascadevis.psh:main:ps_*",
            StringMeld<128>() << "SHADOW_CASCADE_MODE=" << desc._cascadeMode);

        _uniforms = BoundUniforms(*_shader);
        Techniques::TechniqueContext::BindGlobalUniforms(_uniforms);
        _uniforms.BindConstantBuffer(Hash64("ArbitraryShadowProjection"), 0, 1);
        _uniforms.BindConstantBuffer(Hash64("OrthogonalShadowProjection"), 1, 1);
        _uniforms.BindConstantBuffer(Hash64("ScreenToShadowProjection"), 2, 1);
        _uniforms.BindShaderResource(Hash64("DepthTexture"), 0, 1);
        
        _depVal = std::make_shared<Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(_depVal, &_shader->GetDependencyValidation());
    }

    SFDResources::~SFDResources() {}

    static void OverlayShadowFrustums(
        DeviceContext& devContext, 
        const UniformsStream& globalUniforms,
        const RenderCore::Techniques::ProjectionDesc& mainCameraProjectionDesc,
        const SceneEngine::ShadowProjectionDesc& projectionDesc)
    {
        devContext.Bind(Techniques::CommonResources()._dssDisable);
        devContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);

        SceneEngine::SavedTargets savedTargets(&devContext);
        devContext.GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);

        ShaderResourceView depthSrv;
        if (savedTargets.GetDepthStencilView())
            depthSrv = ShaderResourceView(ExtractResource<ID3D::Resource>(
                savedTargets.GetDepthStencilView()).get(), 
                (NativeFormat::Enum)DXGI_FORMAT_R24_UNORM_X8_TYPELESS);

        auto& res = Techniques::FindCachedBoxDep<SFDResources>(
            SFDResources::Desc(
                (projectionDesc._projections._mode == SceneEngine::ShadowProjectionDesc::Projections::Mode::Ortho)?2:1));
        devContext.Bind(*res._shader);

        SceneEngine::CB_ArbitraryShadowProjection arbitraryCB;
        SceneEngine::CB_OrthoShadowProjection orthoCB;
        BuildShadowConstantBuffers(arbitraryCB, orthoCB, projectionDesc._projections);

        ConstantBufferPacket constantBufferPackets[3];
        constantBufferPackets[0] = RenderCore::MakeSharedPkt(arbitraryCB);
        constantBufferPackets[1] = RenderCore::MakeSharedPkt(orthoCB);
        constantBufferPackets[2] = BuildScreenToShadowConstants(
            projectionDesc._projections._count,
            arbitraryCB, orthoCB, 
            mainCameraProjectionDesc._cameraToWorld);
        const ShaderResourceView* srv[] = { &depthSrv };

        res._uniforms.Apply(
            devContext, globalUniforms,
            UniformsStream(
                constantBufferPackets, nullptr, dimof(constantBufferPackets),
                srv, dimof(srv)));

        devContext.Bind(Topology::TriangleStrip);
        devContext.Draw(4);

        devContext.UnbindPS<ShaderResourceView>(4, 1);
        savedTargets.ResetToOldTargets(&devContext);
    }

    void ShadowFrustumDebugger::Render( 
        IOverlayContext* context, Layout& layout, 
        Interactables& interactables, InterfaceState& interfaceState)
    {
        assert(_scene.get());

        if (!_scene->GetShadowProjectionCount()) {
            return;
        }

        static SceneEngine::ShadowProjectionDesc projectionDesc;
        if (!Tweakable("ShadowDebugLock", false)) {
            projectionDesc = _scene->GetShadowProjectionDesc(0, context->GetProjectionDesc());
        }

        auto& devContext = *context->GetDeviceContext();
        context->ReleaseState();
        OverlayShadowFrustums(
            devContext, context->GetGlobalUniformsStream(),
            context->GetProjectionDesc(), projectionDesc);
        context->CaptureState();
        
            //  Get the first shadow projection from the scene, and draw an
            //  outline of all sub-projections with in.
            //  We could also add a control to select different projections
            //  when there are more than one...
        devContext.Bind(Techniques::CommonResources()._dssReadOnly);

        ColorB cols[]= {
            ColorB(196, 230, 230),
            ColorB(255, 128, 128),
            ColorB(128, 255, 128),
            ColorB(128, 128, 255),
            ColorB(255, 255, 128),
            ColorB(128, 255, 255)
        };

        const auto& projections = projectionDesc._projections;
        for (unsigned c=0; c<projections._count; ++c) {
            DebuggingDisplay::DrawFrustum(
                context, Combine(projections._fullProj[c]._viewMatrix, projections._fullProj[c]._projectionMatrix),
                cols[std::min(dimof(cols), c)], 0x1);
        }

        for (unsigned c=0; c<projections._count; ++c) {
            DebuggingDisplay::DrawFrustum(
                context, Combine(projections._fullProj[c]._viewMatrix, projections._fullProj[c]._projectionMatrix),
                cols[std::min(dimof(cols), c)], 0x2);
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

