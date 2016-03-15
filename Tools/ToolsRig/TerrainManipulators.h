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

    class TerrainManipulatorException : public std::exception
    {
    public:
        virtual const char* what() const;
        SceneEngine::TerrainToolResult GetErrorCode() const;

        TerrainManipulatorException(SceneEngine::TerrainToolResult);
        ~TerrainManipulatorException();
    private:
        SceneEngine::TerrainToolResult _errorCode;
    };

    class TerrainManipulatorContext
    {
    public:
        SceneEngine::TerrainCoverageId  _activeLayer;
        bool _showLockedArea;
        bool _showCoverage;

        TerrainManipulatorContext();
    };

    std::vector<std::unique_ptr<IManipulator>> CreateTerrainManipulators(
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager,
        std::shared_ptr<TerrainManipulatorContext> manipulatorContext);
}
