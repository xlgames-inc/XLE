// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainManipulatorsCommon.h"
#include "ManipulatorsUtil.h"
#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/Terrain.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/Techniques/CommonResources.h"
#include "../../RenderCore/Metal/Shader.h"
#include "../../RenderCore/Metal/InputLayout.h"
#include "../../Utility/TimeUtils.h"

#include "../../RenderCore/DX11/Metal/DX11Utils.h"

extern unsigned FrameRenderCount;

namespace ToolsRig
{
    using SceneEngine::IntersectionTestContext;
    using SceneEngine::IntersectionTestScene;
    std::pair<Float3, bool> FindTerrainIntersection(
        const IntersectionTestContext& context, const IntersectionTestScene& scene,
        const Int2 screenCoords)
    {
        auto result = scene.UnderCursor(context, screenCoords, IntersectionTestScene::Type::Terrain);
        if (result._type == IntersectionTestScene::Type::Terrain) {
            return std::make_pair(result._worldSpaceCollision, true);
        }
        return std::make_pair(Float3(0.f, 0.f, 0.f), false);
    }

    IManipulator::~IManipulator() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    bool    CommonManipulator::OnInputEvent(
        const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt, 
        const SceneEngine::IntersectionTestContext& hitTestContext,
        const SceneEngine::IntersectionTestScene& hitTestScene)
    {
        const bool shiftHeld = evnt.IsHeld(RenderOverlays::DebuggingDisplay::KeyId_Make("shift"));
        if (evnt._wheelDelta) {
                // on wheel delta, change effect size
            if (shiftHeld) {
                _strength = std::max(0.f, _strength + 3.f * evnt._wheelDelta / 120.f);
            } else {
                _size = std::max(1.f, _size + 3.f * evnt._wheelDelta / 120.f);
            }
        }

            //  We can do the terrain intersection test now, but it requires setting up 
            //  new device and lighting parser contexts. We need to know the viewport -- and the only way 
            //  to do that is to get it from the windows HWND!
        Int2 newMouseCoords(evnt._mousePosition[0], evnt._mousePosition[1]);
            // only do the terrain test if we get some kind of movement
        if (((XlAbs(_mouseCoords[0] - newMouseCoords[0]) > 1 || XlAbs(_mouseCoords[1] - newMouseCoords[1]) > 1)
            && (FrameRenderCount > _lastRenderCount0)) || evnt.IsPress_LButton()) {

            _currentWorldSpaceTarget = FindTerrainIntersection(hitTestContext, hitTestScene, newMouseCoords);
            _lastPerform = 0;
            _mouseCoords = newMouseCoords;
            _lastRenderCount0 = FrameRenderCount;

            if (evnt.IsPress_LButton()) {
                _targetOnMouseDown = _currentWorldSpaceTarget;
            }
        }

        if (evnt.IsHeld_LButton()) {
                // perform action -- (like raising or lowering the terrain)
            if (_currentWorldSpaceTarget.second && (Millisecond_Now() - _lastPerform) > 33 && (FrameRenderCount > _lastRenderCount1)) {

                TRY {
                    PerformAction(_currentWorldSpaceTarget.first, _size, shiftHeld?(-_strength):_strength);
                } CATCH (...) {
                } CATCH_END
                
                _lastPerform = Millisecond_Now();
                _lastRenderCount1 = FrameRenderCount;
            }
            return true;
        }
        return false;
    }

    void RenderCylinderHighlight(
        RenderCore::Metal::DeviceContext* context, 
        SceneEngine::LightingParserContext& parserContext,
        Float3& centre, float radius)
    {
        using namespace RenderCore::Metal;
            // unbind the depth buffer
        SceneEngine::SavedTargets savedTargets(context);
        context->GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);

            // create shader resource view for the depth buffer
        ShaderResourceView depthSrv;
        if (savedTargets.GetDepthStencilView())
            depthSrv = ShaderResourceView(ExtractResource<ID3D::Resource>(
                savedTargets.GetDepthStencilView()).get(), 
                (NativeFormat::Enum)DXGI_FORMAT_R24_UNORM_X8_TYPELESS);     // note -- assuming D24S8 depth buffer! We need a better way to get the depth srv

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

            auto& circleHighlight = Assets::GetAssetDep<DeferredShaderResource>("game/xleres/DefaultResources/circlehighlight.png");
            const ShaderResourceView* resources[] = { &depthSrv, &circleHighlight.GetShaderResource() };

            BoundUniforms boundLayout(shaderProgram);
            RenderCore::Techniques::TechniqueContext::BindGlobalUniforms(boundLayout);
            boundLayout.BindConstantBuffer(Hash64("CircleHighlightParameters"), 0, 1);
            boundLayout.BindShaderResource(Hash64("DepthTexture"), 0, 1);
            boundLayout.BindShaderResource(Hash64("HighlightResource"), 1, 1);

            context->Bind(shaderProgram);
            boundLayout.Apply(*context, 
                parserContext.GetGlobalUniformsStream(),
                UniformsStream(constantBufferPackets, nullptr, dimof(constantBufferPackets), resources, dimof(resources)));

            context->Bind(RenderCore::Techniques::CommonResources()._blendAlphaPremultiplied);
            context->Bind(RenderCore::Techniques::CommonResources()._dssDisable);
            context->Bind(Topology::TriangleStrip);
            context->GetUnderlying()->IASetInputLayout(nullptr);

                // note --  this will render a full screen quad. we could render cylinder geometry instead,
                //          because this decal only affects the area within a cylinder. But it's just for
                //          tools, so the easy way should be fine.
            context->Draw(4);

            ID3D::ShaderResourceView* srv = nullptr;
            context->GetUnderlying()->PSSetShaderResources(3, 1, &srv);
        } 
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH(...) {} 
        CATCH_END

        savedTargets.ResetToOldTargets(context);
    }

    void    CommonManipulator::Render(
                    RenderCore::IThreadContext* context, 
                    SceneEngine::LightingParserContext& parserContext)
    {
            //  Draw a highlight on the area that we're going to modify. Since we want this to behave like a decal, 
            //  it's best to do this rendering after the gbuffer is fully prepared, and we should render onto the
            //  lighting buffer.
            //  In theory, we could stencil out the terrain, as well -- so we only actually draw onto terrain 
            //  geometry.
        if (_currentWorldSpaceTarget.second) {
            RenderCylinderHighlight(
                RenderCore::Metal::DeviceContext::Get(*context).get(), 
                parserContext, _currentWorldSpaceTarget.first, _size);
        }
		++FrameRenderCount;
    }

    CommonManipulator::CommonManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : _terrainManager(std::move(terrainManager))
    {
        _currentWorldSpaceTarget = std::make_pair(Float3(0,0,0), false);
        _targetOnMouseDown = std::make_pair(Float3(0,0,0), false);
        _mouseCoords = Int2(0,0);
        _strength = 1.f;
        _size = 20.f;
        _lastPerform = 0;
        _lastRenderCount0 = _lastRenderCount1 = 0;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    bool    RectangleManipulator::OnInputEvent(
        const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt, 
        const IntersectionTestContext& hitTestContext,
        const IntersectionTestScene& hitTestScene)
    {
        Int2 mousePosition(evnt._mousePosition[0], evnt._mousePosition[1]);

        if (evnt.IsPress_LButton()) {
                // on lbutton press, we should place a new anchor
            auto intersection = FindTerrainIntersection(hitTestContext, hitTestScene, mousePosition);
            _isDragging = intersection.second;
            if (intersection.second) {
                _firstAnchor = intersection.first;
                _secondAnchor = intersection;
            }
        }

        if (_isDragging) {

            if (evnt.IsHeld_LButton() || evnt.IsRelease_LButton()) {
                    // update the second anchor as we drag
                _secondAnchor = FindTerrainIntersection(hitTestContext, hitTestScene, mousePosition);
            }

            if (evnt.IsRelease_LButton()) {
                    // on release, we should perform the action
                    //  (assuming we released at a valid position)
                _isDragging = false;

                if (_secondAnchor.second) {

                    auto& coords = _terrainManager->GetCoords();
                    Float2 faTerrain = coords.WorldSpaceToTerrainCoords(Truncate(_firstAnchor));
                    Float2 fsTerrain = coords.WorldSpaceToTerrainCoords(Truncate(_secondAnchor.first));
                    Float2 terrainCoordsMins(std::min(faTerrain[0], fsTerrain[0]), std::min(faTerrain[1], fsTerrain[1]));
                    Float2 terrainCoordsMaxs(std::max(faTerrain[0], fsTerrain[0]), std::max(faTerrain[1], fsTerrain[1]));
                    Float2 faWorld = coords.TerrainCoordsToWorldSpace(RoundDownToInteger(terrainCoordsMins));
                    Float2 fsWorld = coords.TerrainCoordsToWorldSpace(RoundUpToInteger(terrainCoordsMaxs));

                    TRY {
                        PerformAction(Expand(faWorld, 0.f), Expand(fsWorld, 0.f));
                    } CATCH(...) {
                    } CATCH_END
                }
            }

        }

        return false;
    }

    void    RectangleManipulator::Render(RenderCore::IThreadContext* context, SceneEngine::LightingParserContext& parserContext)
    {
            //  while dragging, we should draw a rectangle highlight on the terrain
        using namespace RenderCore::Metal;
        if (_isDragging && _secondAnchor.second) {

            auto devContext = RenderCore::Metal::DeviceContext::Get(*context);
                
                // unbind the depth buffer
            SceneEngine::SavedTargets savedTargets(devContext.get());
            devContext->GetUnderlying()->OMSetRenderTargets(1, savedTargets.GetRenderTargets(), nullptr);

                // create shader resource view for the depth buffer
            ShaderResourceView depthSrv;
            if (savedTargets.GetDepthStencilView())
                depthSrv = ShaderResourceView(ExtractResource<ID3D::Resource>(
                    savedTargets.GetDepthStencilView()).get(), 
                    (NativeFormat::Enum)DXGI_FORMAT_R24_UNORM_X8_TYPELESS);     // note -- assuming D24S8 depth buffer! We need a better way to get the depth srv

            TRY
            {
                    // note -- we might need access to the MSAA defines for this shader
                auto& shaderProgram = Assets::GetAssetDep<ShaderProgram>(
                    "game/xleres/basic2D.vsh:fullscreen_viewfrustumvector:vs_*",
                    "game/xleres/ui/terrainmanipulators.sh:ps_rectanglehighlight:ps_*");

                    // clamp anchor values to the terrain coords size
                auto& coords = _terrainManager->GetCoords();
                Float2 faTerrain = coords.WorldSpaceToTerrainCoords(Truncate(_firstAnchor));
                Float2 fsTerrain = coords.WorldSpaceToTerrainCoords(Truncate(_secondAnchor.first));
                Float2 terrainCoordsMins(std::min(faTerrain[0], fsTerrain[0]), std::min(faTerrain[1], fsTerrain[1]));
                Float2 terrainCoordsMaxs(std::max(faTerrain[0], fsTerrain[0]), std::max(faTerrain[1], fsTerrain[1]));
                Float2 faWorld = coords.TerrainCoordsToWorldSpace(RoundDownToInteger(terrainCoordsMins));
                Float2 fsWorld = coords.TerrainCoordsToWorldSpace(RoundUpToInteger(terrainCoordsMaxs));
            
                struct HighlightParameters
                {
                    Float3 _mins; float _dummy0;
                    Float3 _maxs; float _dummy1;
                } highlightParameters = { 
                    Float3(std::min(faWorld[0], fsWorld[0]), std::min(faWorld[1], fsWorld[1]), 0.f), 0.f, 
                    Float3(std::max(faWorld[0], fsWorld[0]), std::max(faWorld[1], fsWorld[1]), 0.f), 0.f
                };
                ConstantBufferPacket constantBufferPackets[2];
                constantBufferPackets[0] = RenderCore::MakeSharedPkt(highlightParameters);

                auto& circleHighlight = Assets::GetAssetDep<DeferredShaderResource>("game/xleres/DefaultResources/circlehighlight.png");
                const ShaderResourceView* resources[] = { &depthSrv, &circleHighlight.GetShaderResource() };

                BoundUniforms boundLayout(shaderProgram);
                RenderCore::Techniques::TechniqueContext::BindGlobalUniforms(boundLayout);
                boundLayout.BindConstantBuffer(Hash64("RectangleHighlightParameters"), 0, 1);
                boundLayout.BindShaderResource(Hash64("DepthTexture"), 0, 1);
                boundLayout.BindShaderResource(Hash64("HighlightResource"), 1, 1);

                devContext->Bind(shaderProgram);
                boundLayout.Apply(
                    *devContext.get(), 
                    parserContext.GetGlobalUniformsStream(),
                    UniformsStream(constantBufferPackets, nullptr, dimof(constantBufferPackets), resources, dimof(resources)));

                devContext->Bind(RenderCore::Techniques::CommonResources()._blendAlphaPremultiplied);
                devContext->Bind(RenderCore::Techniques::CommonResources()._dssDisable);
                devContext->Bind(Topology::TriangleStrip);
                devContext->GetUnderlying()->IASetInputLayout(nullptr);

                    // note --  this will render a full screen quad. we could render cylinder geometry instead,
                    //          because this decal only affects the area within a cylinder. But it's just for
                    //          tools, so the easy way should be fine.
                devContext->Draw(4);

                ID3D::ShaderResourceView* srv = nullptr;
                devContext->GetUnderlying()->PSSetShaderResources(3, 1, &srv);
            } 
            CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
            CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
            CATCH(...) {} 
            CATCH_END

            savedTargets.ResetToOldTargets(devContext.get());
        }
    }

    RectangleManipulator::RectangleManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : _terrainManager(terrainManager)
    {
        _isDragging = false;
    }

}

