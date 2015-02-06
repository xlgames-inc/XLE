// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PlacementsOverlaySystem.h"
#include "OverlaySystem.h"
#include "../../PlatformRig/PlacementsManipulators.h"

namespace Sample
{
    class Manipulators : public IOverlaySystem
    {
    public:
        Manipulators(
            std::shared_ptr<SceneEngine::PlacementsManager> placementsManager,
            std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
            std::shared_ptr<SceneEngine::IntersectionTestContext> intersectionContext)
        {
            _placementsManipulators = std::make_shared<::Tools::PlacementsManipulatorsManager>(
                placementsManager, terrainManager, intersectionContext);
        }

        std::shared_ptr<IInputListener> GetInputListener()
        {
            return _placementsManipulators->GetInputLister();
        }

        void RenderToScene(
            RenderCore::Metal::DeviceContext* devContext, 
            SceneEngine::LightingParserContext& parserContext)
        {
            _placementsManipulators->RenderToScene(devContext, parserContext);
        }

        void RenderWidgets(
            RenderCore::IDevice* device, 
            const RenderCore::ProjectionDesc& projectionDesc)
        {
            _placementsManipulators->RenderWidgets(device, projectionDesc);
        }

        void SetActivationState(bool) {}

    private:
        std::shared_ptr<::Tools::PlacementsManipulatorsManager> _placementsManipulators;
    };

    std::shared_ptr<IOverlaySystem> CreatePlacementsEditorOverlaySystem(
        std::shared_ptr<SceneEngine::PlacementsManager> placementsManager,
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
        std::shared_ptr<SceneEngine::IntersectionTestContext> intersectionContext)
    {
        return std::make_shared<Manipulators>(
            std::move(placementsManager), std::move(terrainManager), std::move(intersectionContext));
    }
}

