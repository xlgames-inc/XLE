// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainConversion.h"
#include "Terrain.h"
#include "TerrainConfig.h"
#include "TerrainUberSurface.h"
#include "../RenderCore/Metal/Format.h"     // for BitsPerPixel
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/IProgress.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Conversion.h"

namespace SceneEngine
{
    //////////////////////////////////////////////////////////////////////////////////////////
    template<typename Sample>
        static void WriteCellCoverageData(
            const TerrainConfig& cfg, ITerrainFormat& ioFormat, 
            const ::Assets::ResChar uberSurfaceName[], unsigned layerIndex)
    {
        ::Assets::ResChar path[MaxPath];

        auto cells = BuildPrimedCells(cfg);
        auto layerId = cfg.GetCoverageLayer(layerIndex)._id;

        TerrainUberSurface<Sample> uberSurface(uberSurfaceName);
        for (auto c=cells.cbegin(); c!=cells.cend(); ++c) {

            char cellFile[MaxPath];
            cfg.GetCellFilename(cellFile, dimof(cellFile), c->_cellIndex, layerId);
            if (!DoesFileExist(cellFile)) {
                XlDirname(path, dimof(path), cellFile);
                CreateDirectoryRecursive(path);

                TRY {
                    ioFormat.WriteCell(
                        cellFile, uberSurface, 
                        c->_coverageUber[layerIndex].first, c->_coverageUber[layerIndex].second, cfg.CellTreeDepth(), 1);
                } CATCH(...) {
                    LogAlwaysError << "Error while writing cell coverage file to: " << cellFile;
                } CATCH_END
            }

        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    void ExecuteTerrainConversion(
        const ::Assets::ResChar destinationUberSurfaceDirectory[],
        const TerrainConfig& outputConfig, 
        const TerrainConfig& inputConfig, 
        std::shared_ptr<ITerrainFormat> inputIOFormat)
    {
        CreateDirectoryRecursive(destinationUberSurfaceDirectory);

        //////////////////////////////////////////////////////////////////////////////////////
            // If we don't have an uber surface file, then we should create it
        ::Assets::ResChar heightsFile[MaxPath];
        TerrainConfig::GetUberSurfaceFilename(heightsFile, dimof(heightsFile), destinationUberSurfaceDirectory, CoverageId_Heights);
        if (!DoesFileExist(heightsFile) && inputIOFormat) {
            BuildUberSurfaceFile(
                heightsFile, inputConfig, inputIOFormat.get(), 
                0, 0, inputConfig._cellCount[0], inputConfig._cellCount[1]);
        }
    }

    void GenerateMissingCellFiles(
        const TerrainConfig& outputConfig, 
        std::shared_ptr<ITerrainFormat> outputIOFormat,
        const ::Assets::ResChar uberSurfaceDir[],
        ConsoleRig::IProgress* progress)
    {
        assert(outputIOFormat);

        //////////////////////////////////////////////////////////////////////////////////////
            // for each coverage layer, we must write all of the component parts
        for (unsigned l=0; l<outputConfig.GetCoverageLayerCount(); ++l) {
            const auto& layer = outputConfig.GetCoverageLayer(l);

            ::Assets::ResChar layerUberSurface[MaxPath];
            TerrainConfig::GetUberSurfaceFilename(layerUberSurface, dimof(layerUberSurface), uberSurfaceDir, layer._id);

            if (DoesFileExist(layerUberSurface)) {
                    //  open and destroy these coverage uber shadowing surface before we open the uber heights surface
                    //  (opening them both at the same time requires too much memory)
                if (layer._format == 35) {
                    WriteCellCoverageData<ShadowSample>(outputConfig, *outputIOFormat, layerUberSurface, l);
                } else if (layer._format == 62) {
                    WriteCellCoverageData<uint8>(outputConfig, *outputIOFormat, layerUberSurface, l);
                } else {
                    LogAlwaysError << "Unknown format (" << layer._format << ") for terrain coverage file for layer: " << 
                        Conversion::Convert<std::string>(layer._name);
                }
            }
        }

        //////////////////////////////////////////////////////////////////////////////////////
            //  load the uber height surface, and uber surface interface (but only temporarily
            //  while we initialise the data)
        ::Assets::ResChar uberSurfaceFile[MaxPath];
        TerrainConfig::GetUberSurfaceFilename(uberSurfaceFile, dimof(uberSurfaceFile), uberSurfaceDir, CoverageId_Heights);
        TerrainUberHeightsSurface heightsData(uberSurfaceFile);
        HeightsUberSurfaceInterface uberSurfaceInterface(heightsData, outputIOFormat);

        //////////////////////////////////////////////////////////////////////////////////////
        auto cells = BuildPrimedCells(outputConfig);
        auto step = progress ? progress->BeginStep("Generate Cell Files", (unsigned)cells.size()) : nullptr;
        for (auto c=cells.cbegin(); c!=cells.cend(); ++c) {
            char heightMapFile[MaxPath];
            outputConfig.GetCellFilename(heightMapFile, dimof(heightMapFile), c->_cellIndex, CoverageId_Heights);
            if (!DoesFileExist(heightMapFile)) {
                char path[MaxPath];
                XlDirname(path, dimof(path), heightMapFile);
                CreateDirectoryRecursive(path);
                TRY {
                    outputIOFormat->WriteCell(
                        heightMapFile, *uberSurfaceInterface.GetUberSurface(), 
                        c->_heightUber.first, c->_heightUber.second, outputConfig.CellTreeDepth(), outputConfig.NodeOverlap());
                } CATCH(...) { // sometimes throws (eg, if the directory doesn't exist)
                } CATCH_END
            }

            if (step) step->Advance();
        }
    }

    void GenerateMissingUberSurfaceFiles(
        const TerrainConfig& cfg, 
        std::shared_ptr<ITerrainFormat> outputIOFormat,
        const ::Assets::ResChar uberSurfaceDir[],
        ConsoleRig::IProgress* progress)
    {
        auto step = progress ? progress->BeginStep("Generate UberSurface Files", (unsigned)cfg.GetCoverageLayerCount()) : nullptr;
        for (unsigned l=0; l<cfg.GetCoverageLayerCount(); ++l) {
            const auto& layer = cfg.GetCoverageLayer(l);

            // bool hasShadows = false;
            // for (unsigned c=0; c<outputConfig.GetCoverageLayerCount(); ++c)
            //     hasShadows |= outputConfig.GetCoverageLayer(c)._id == CoverageId_AngleBasedShadows;
            // if (!hasShadows) return;

            ::Assets::ResChar uberSurfaceFile[MaxPath];
            TerrainConfig::GetUberSurfaceFilename(uberSurfaceFile, dimof(uberSurfaceFile), uberSurfaceDir, layer._id);
            if (DoesFileExist(uberSurfaceFile)) continue;

            if (layer._id == CoverageId_AngleBasedShadows) {

                    // this is the shadows layer... We need to build the shadows procedurally
                ::Assets::ResChar uberHeightsFile[MaxPath];
                TerrainConfig::GetUberSurfaceFilename(uberHeightsFile, dimof(uberHeightsFile), uberSurfaceDir, CoverageId_Heights);

                TerrainUberHeightsSurface heightsData(uberHeightsFile);
                HeightsUberSurfaceInterface uberSurfaceInterface(heightsData, outputIOFormat);

                //////////////////////////////////////////////////////////////////////////////////////
                    // build the uber shadowing file, and then write out the shadowing textures for each node
                // Int2 interestingMins((9-1) * 16 * 32, (19-1) * 16 * 32), interestingMaxs((9+4) * 16 * 32, (19+4) * 16 * 32);
                UInt2 interestingMins(0, 0);
                UInt2 interestingMaxs = UInt2(
                    cfg._cellCount[0] * cfg.CellDimensionsInNodes()[0] * cfg.NodeDimensionsInElements()[0],
                    cfg._cellCount[1] * cfg.CellDimensionsInNodes()[1] * cfg.NodeDimensionsInElements()[1]);

                float xyScale = cfg.ElementSpacing();
                Float2 sunDirectionOfMovement = Normalize(Float2(1.f, 0.33f));
                uberSurfaceInterface.BuildShadowingSurface(
                    uberSurfaceFile, interestingMins, interestingMaxs, sunDirectionOfMovement, xyScale);

            } else {
                HeightsUberSurfaceInterface::BuildEmptyFile(
                    uberSurfaceFile, 
                    cfg._cellCount[0] * cfg.CellDimensionsInNodes()[0] * (layer._dimensions[0]+1),
                    cfg._cellCount[1] * cfg.CellDimensionsInNodes()[1] * (layer._dimensions[1]+1),
                    RenderCore::Metal::BitsPerPixel((RenderCore::Metal::NativeFormat::Enum)layer._format));
            }

            if (step) step->Advance();
        }
    }

}

