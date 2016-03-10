// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>

namespace SceneEngine 
{ 
    class PlacementsManager; class PlacementCellSet; 
    class TerrainManager; class IntersectionTestContext;
}
namespace PlatformRig { class IOverlaySystem; }

namespace Sample
{
    std::shared_ptr<PlatformRig::IOverlaySystem> CreatePlacementsEditorOverlaySystem(
        std::shared_ptr<SceneEngine::PlacementsManager> placementsManager,
        std::shared_ptr<SceneEngine::PlacementCellSet> placementsCell,
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
        std::shared_ptr<SceneEngine::IntersectionTestContext> intersectionContext);
}
