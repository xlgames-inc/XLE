// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../SceneEngine/TerrainCoverageId.h"
#include <vector>
#include <memory>

namespace SceneEngine { class TerrainManager; }
namespace ToolsRig
{
    class IManipulator;

    class TerrainManipulatorContext
    {
    public:
        SceneEngine::TerrainCoverageId  _activeLayer;
        bool _showLockedArea;

        TerrainManipulatorContext();
    };

    std::vector<std::unique_ptr<IManipulator>> CreateTerrainManipulators(
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
        std::shared_ptr<TerrainManipulatorContext> manipulatorContext);
}
