// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../SceneEngine/TerrainCoverageId.h"
#include "../../SceneEngine/TerrainFormat.h"
#include "../../Assets/AssetsCore.h"
#include "../../Math/Vector.h"

namespace ConsoleRig { class IProgress; }
namespace SceneEngine { class TerrainConfig; class ITerrainFormat; class GradientFlagsSettings; }

namespace ToolsRig
{
    UInt2 ConvertDEMData(
        const ::Assets::ResChar outputDir[], const ::Assets::ResChar input[], 
        unsigned destNodeDims, unsigned destCellTreeDepth,
        ConsoleRig::IProgress* progress);

    void GenerateMissingUberSurfaceFiles(
        const SceneEngine::TerrainConfig& outputConfig, 
        const ::Assets::ResChar uberSurfaceDir[],
        ConsoleRig::IProgress* progress = nullptr);

    void GenerateCellFiles(
        const SceneEngine::TerrainConfig& outputConfig, 
        const ::Assets::ResChar uberSurfaceDir[],
        bool overwriteExisting,
        const SceneEngine::GradientFlagsSettings& gradFlagSettings,
        ConsoleRig::IProgress* progress = nullptr);

    void GenerateBlankUberSurface(
        const ::Assets::ResChar outputDir[], 
        unsigned cellCountX, unsigned cellCountY,
        unsigned destNodeDims, unsigned destCellTreeDepth,
        ConsoleRig::IProgress* progress = nullptr);

    void GenerateShadowsSurface(
        const SceneEngine::TerrainConfig& cfg, 
        const ::Assets::ResChar uberSurfaceDir[],
        bool overwriteExisting,
        ConsoleRig::IProgress* progress = nullptr);

    void GenerateAmbientOcclusionSurface(
        const SceneEngine::TerrainConfig& cfg, 
        const ::Assets::ResChar uberSurfaceDir[],
        bool overwriteExisting,
        ConsoleRig::IProgress* progress = nullptr);

    UInt2 GetCellCountFromUberSurface(
        const ::Assets::ResChar inputUberSurfaceDirectory[],
        UInt2 destNodeDims, unsigned destCellTreeDepth);
}