// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../SceneEngine/TerrainCoverageId.h"
#include "../../Assets/AssetsCore.h"
#include "../../Math/Vector.h"

namespace ConsoleRig { class IProgress; }

namespace ToolsRig
{
    UInt2 ConvertDEMData(
        const ::Assets::ResChar outputDir[], const ::Assets::ResChar input[], 
        unsigned destNodeDims, unsigned destCellTreeDepth,
        ConsoleRig::IProgress* progress);

    void GenerateStarterCells(
        const ::Assets::ResChar outputDir[], const ::Assets::ResChar input[], 
        unsigned destNodeDims, unsigned destCellTreeDepth, unsigned overlap, float spacing,
        const std::pair<SceneEngine::TerrainCoverageId, unsigned> layers[], unsigned layerCount,
        ConsoleRig::IProgress* progress);

}