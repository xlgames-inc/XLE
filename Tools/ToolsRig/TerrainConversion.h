// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../SceneEngine/TerrainCoverageId.h"
#include "../../SceneEngine/TerrainFormat.h"
#include "../../Assets/AssetsCore.h"
#include "../../Utility/ParameterBox.h"     // for ImpliedTyping::TypeCat
#include "../../Math/Vector.h"
#include <vector>

namespace ConsoleRig { class IProgress; }
namespace SceneEngine { class TerrainConfig; class ITerrainFormat; class GradientFlagsSettings; }

namespace ToolsRig
{
    class TerrainImportOp
    {
    public:
        enum class SourceFormat { AbsoluteFloats, Quantized };
        UInt2 _sourceDims;
        SourceFormat _sourceFormat;
        Float2 _sourceHeightRange;
        Assets::rstring _sourceFile;
        bool _sourceIsGood;

        UInt2 _importMins;
        UInt2 _importMaxs;
        Float2 _importHeightRange;
        unsigned _importCoverageFormat;

        std::vector<std::string> _warnings;
    };

    TerrainImportOp PrepareTerrainImport(
        const ::Assets::ResChar input[], 
        unsigned destNodeDims, unsigned destCellTreeDepth);

    void ExecuteTerrainImport(
        const TerrainImportOp& operation,
        const ::Assets::ResChar outputDir[],
        unsigned destNodeDims, unsigned destCellTreeDepth,
        SceneEngine::TerrainCoverageId coverageId,
        ImpliedTyping::TypeCat dstType,
        ConsoleRig::IProgress* progress);

    void ExecuteTerrainExport(
        const ::Assets::ResChar dstFile[],
        const SceneEngine::TerrainConfig& srcCfg, 
        const ::Assets::ResChar uberSurfaceDir[],
        SceneEngine::TerrainCoverageId coverageId,
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