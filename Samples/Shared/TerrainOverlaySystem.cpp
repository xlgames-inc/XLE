// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainOverlaySystem.h"
#include "../../PlatformRig/TerrainManipulators.h"
#include "../../PlatformRig/OverlaySystem.h"

namespace Sample
{
    using RenderOverlays::DebuggingDisplay::DebugScreensSystem;

    class TerrainManipulators
        : public PlatformRig::IOverlaySystem
        , public RenderOverlays::DebuggingDisplay::IInputListener
        , public std::enable_shared_from_this<TerrainManipulators>
    {
    public:
        TerrainManipulators(
            std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
            std::shared_ptr<SceneEngine::IntersectionTestContext> intersectionContext)
        {
            _manipulatorsInterface = std::make_shared<::Tools::ManipulatorsInterface>(terrainManager, intersectionContext);
            _terrainManipulators = std::make_shared<::Tools::ManipulatorsDisplay>(_manipulatorsInterface);
            _manipInputListener = _manipulatorsInterface->CreateInputListener();

            _screens = std::make_shared<DebugScreensSystem>();
            _screens->Register(_terrainManipulators, "Terrain", DebugScreensSystem::SystemDisplay);
        }

        std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener> GetInputListener()
        {
            return shared_from_this();
        }

        bool OnInputEvent(const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
        {
            return  _screens->OnInputEvent(evnt)
                ||  _manipInputListener->OnInputEvent(evnt);
        }

        void RenderToScene(
            RenderCore::Metal::DeviceContext* devContext, 
            SceneEngine::LightingParserContext& parserContext)
        {
            _manipulatorsInterface->Render(devContext, parserContext);
        }

        void RenderWidgets(
            RenderCore::IDevice* device, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc)
        {
            _screens->Render(device, projectionDesc);
        }

        void SetActivationState(bool) {}

    private:
        std::shared_ptr<::Tools::ManipulatorsInterface> _manipulatorsInterface;
        std::shared_ptr<::Tools::ManipulatorsDisplay>   _terrainManipulators;
        std::shared_ptr<DebugScreensSystem>             _screens;
        std::shared_ptr<RenderOverlays::DebuggingDisplay::IInputListener>                 _manipInputListener;
    };

    std::shared_ptr<PlatformRig::IOverlaySystem> CreateTerrainEditorOverlaySystem(
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
        std::shared_ptr<SceneEngine::IntersectionTestContext> intersectionContext)
    {
        return std::make_shared<TerrainManipulators>(
            std::move(terrainManager), 
            std::move(intersectionContext));
    }
}

