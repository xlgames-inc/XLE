// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainManipulatorsCommon.h"
#include "ManipulatorsUtil.h"
#include "ManipulatorsRender.h"
#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/Terrain.h"
#include "../../Utility/TimeUtils.h"

static unsigned FrameRenderCount;

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
                *context, parserContext, _currentWorldSpaceTarget.first, _size);
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

                // clamp anchor values to the terrain coords size
            auto& coords = _terrainManager->GetCoords();
            Float2 faTerrain = coords.WorldSpaceToTerrainCoords(Truncate(_firstAnchor));
            Float2 fsTerrain = coords.WorldSpaceToTerrainCoords(Truncate(_secondAnchor.first));
            Float2 terrainCoordsMins(std::min(faTerrain[0], fsTerrain[0]), std::min(faTerrain[1], fsTerrain[1]));
            Float2 terrainCoordsMaxs(std::max(faTerrain[0], fsTerrain[0]), std::max(faTerrain[1], fsTerrain[1]));
            Float2 faWorld = coords.TerrainCoordsToWorldSpace(RoundDownToInteger(terrainCoordsMins));
            Float2 fsWorld = coords.TerrainCoordsToWorldSpace(RoundUpToInteger(terrainCoordsMaxs));

            RenderRectangleHighlight(
                *context, parserContext,
                Float3(std::min(faWorld[0], fsWorld[0]), std::min(faWorld[1], fsWorld[1]), 0.f),
                Float3(std::max(faWorld[0], fsWorld[0]), std::max(faWorld[1], fsWorld[1]), 0.f));
        }
    }

    RectangleManipulator::RectangleManipulator(std::shared_ptr<SceneEngine::TerrainManager> terrainManager)
        : _terrainManager(terrainManager)
    {
        _isDragging = false;
    }

}

