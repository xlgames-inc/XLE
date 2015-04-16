// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <vector>
#include <memory>

namespace SceneEngine { class TerrainManager; }
namespace ToolsRig
{
    class IManipulator;

    std::vector<std::unique_ptr<IManipulator>> CreateTerrainManipulators(
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager);
}
