// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/AssetsCore.h"
#include <memory>

namespace Utility { class OutputStream; }

namespace SceneEngine
{
    class TerrainConfig;
    class ITerrainFormat;

    void ExecuteTerrainConversion(
        const ::Assets::ResChar destinationUberSurfaceDirectory[],
        const TerrainConfig& outputConfig, 
        const TerrainConfig& inputConfig, 
        std::shared_ptr<ITerrainFormat> inputIOFormat);

    void GenerateMissingUberSurfaceFiles(
        const TerrainConfig& outputConfig, 
        std::shared_ptr<ITerrainFormat> outputIOFormat,
        const ::Assets::ResChar uberSurfaceDir[]);

    void GenerateMissingCellFiles(
        const TerrainConfig& outputConfig, 
        std::shared_ptr<ITerrainFormat> outputIOFormat,
        const ::Assets::ResChar uberSurfaceDir[]);
}
