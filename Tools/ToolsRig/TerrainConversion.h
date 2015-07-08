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
namespace SceneEngine { class TerrainConfig; class ITerrainFormat; }

namespace ToolsRig
{
    UInt2 ConvertDEMData(
        const ::Assets::ResChar outputDir[], const ::Assets::ResChar input[], 
        unsigned destNodeDims, unsigned destCellTreeDepth,
        ConsoleRig::IProgress* progress);

    void GenerateStarterCells(
        const SceneEngine::TerrainConfig& cfg, 
        const ::Assets::ResChar uberSurfaceDirectory[], 
        ConsoleRig::IProgress* progress);

    void GenerateMissingUberSurfaceFiles(
        const SceneEngine::TerrainConfig& outputConfig, 
        std::shared_ptr<SceneEngine::ITerrainFormat> outputIOFormat,
        const ::Assets::ResChar uberSurfaceDir[],
        ConsoleRig::IProgress* progress = nullptr);

    void GenerateMissingCellFiles(
        const SceneEngine::TerrainConfig& outputConfig, 
        std::shared_ptr<SceneEngine::ITerrainFormat> outputIOFormat,
        const ::Assets::ResChar uberSurfaceDir[],
        ConsoleRig::IProgress* progress = nullptr);

    void GenerateBlankUberSurface(
        const ::Assets::ResChar outputDir[], 
        unsigned cellCountX, unsigned cellCountY,
        unsigned destNodeDims, unsigned destCellTreeDepth,
        ConsoleRig::IProgress* progress = nullptr);

    void GenerateShadowsSurface(
        const SceneEngine::TerrainConfig& cfg, 
        const ::Assets::ResChar uberSurfaceDir[],
        ConsoleRig::IProgress* progress = nullptr);

    UInt2 GetCellCountFromUberSurface(
        const ::Assets::ResChar inputUberSurfaceDirectory[],
        UInt2 destNodeDims, unsigned destCellTreeDepth);
}