// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/AssetsCore.h"
#include "../../Math/Vector.h"

namespace ConsoleRig { class IProgress; }

namespace ToolsRig
{
    UInt2 ConvertDEMData(
        const ::Assets::ResChar outputDir[], const ::Assets::ResChar input[], 
        unsigned destNodeDims, unsigned destCellTreeDepth);

    void GenerateStarterCells(
        const ::Assets::ResChar outputDir[], const ::Assets::ResChar input[], 
        unsigned destNodeDims, unsigned destCellTreeDepth, unsigned overlap,
        ConsoleRig::IProgress* progress);

}