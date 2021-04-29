// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PlacementsOverlaySystem.h"
#include "../../Tools/ToolsRig/PlacementsManipulators.h"
#include "../../PlatformRig/OverlaySystem.h"

namespace Sample
{
    class PlacementsManipulators : public PlatformRig::IOverlaySystem
    {
    public:
        PlacementsManipulators(
            std::shared_ptr<SceneEngine::PlacementsManager> placementsManager,
            std::shared_ptr<SceneEngine::PlacementCellSet> placementsCell,
            std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
            std::shared_ptr<SceneEngine::IntersectionTestContext> intersectionContext)
        {
            _placementsManipulators = std::make_shared<::ToolsRig::PlacementsManipulatorsManager>(
                placementsManager, placementsCell, terrainManager, intersectionContext);
        }

        std::shared_ptr<PlatformRig::IInputListener> GetInputListener()
        {
            return _placementsManipulators->GetInputLister();
        }

        void RenderToScene(
            RenderCore::IThreadContext& devContext,
            RenderCore::Techniques::ParsingContext& parserContext)
        {
            _placementsManipulators->RenderToScene(devContext, parserContext);
        }

        void RenderWidgets(
            RenderCore::IThreadContext& device, 
            RenderCore::Techniques::ParsingContext& parserContext)
        {
            _placementsManipulators->RenderWidgets(device, parserContext);
        }

        void SetActivationState(bool) {}

    private:
        std::shared_ptr<::ToolsRig::PlacementsManipulatorsManager> _placementsManipulators;
    };

    std::shared_ptr<PlatformRig::IOverlaySystem> CreatePlacementsEditorOverlaySystem(
        std::shared_ptr<SceneEngine::PlacementsManager> placementsManager,
        std::shared_ptr<SceneEngine::PlacementCellSet> placementsCell,
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
        std::shared_ptr<SceneEngine::IntersectionTestContext> intersectionContext)
    {
        return std::make_shared<PlacementsManipulators>(
            std::move(placementsManager), std::move(placementsCell),
            std::move(terrainManager), std::move(intersectionContext));
    }
}

