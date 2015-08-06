// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "TerrainConversion.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/TerrainFormat.h"
#include "../../SceneEngine/TerrainConfig.h"
#include "../../SceneEngine/TerrainUberSurface.h"
#include "../../SceneEngine/TerrainScaffold.h"
#include "../../RenderCore/Metal/Format.h"      // (for BitsPerPixel)
#include "../../Math/Vector.h"
#include "../../ConsoleRig/IProgress.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Conversion.h"
#include <vector>
#include <regex>

#include "../../Foreign/LibTiff/tiff.h"
#include "../../Foreign/LibTiff/tiffio.h"

namespace ToolsRig
{
    using namespace SceneEngine;

    //////////////////////////////////////////////////////////////////////////////////////////
    template<typename Sample>
        static void WriteCellCoverageData(
            const TerrainConfig& cfg, ITerrainFormat& ioFormat, 
            const ::Assets::ResChar uberSurfaceName[], unsigned layerIndex,
            bool overwriteExisting,
            ConsoleRig::IProgress* progress)
    {
        ::Assets::ResChar path[MaxPath];

        auto cells = BuildPrimedCells(cfg);
        auto& layer = cfg.GetCoverageLayer(layerIndex);

        auto step = progress ? progress->BeginStep("Write coverage cells", (unsigned)cells.size(), true) : nullptr;

        TerrainUberSurface<Sample> uberSurface(uberSurfaceName);
        for (auto c=cells.cbegin(); c!=cells.cend(); ++c) {

            char cellFile[MaxPath];
            cfg.GetCellFilename(cellFile, dimof(cellFile), c->_cellIndex, layer._id);
            if (!DoesFileExist(cellFile) || overwriteExisting) {
                XlDirname(path, dimof(path), cellFile);
                CreateDirectoryRecursive(path);

                TRY {
                    ioFormat.WriteCell(
                        cellFile, uberSurface, 
                        c->_coverageUber[layerIndex].first, c->_coverageUber[layerIndex].second, 
                        cfg.CellTreeDepth(), layer._overlap);
                } CATCH(...) {
                    LogAlwaysError << "Error while writing cell coverage file to: " << cellFile;
                } CATCH_END
            }

            if (step) {
                if (step->IsCancelled()) break;
                step->Advance();
            }

        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    void GenerateCellFiles(
        const TerrainConfig& outputConfig, 
        const ::Assets::ResChar uberSurfaceDir[],
        bool overwriteExisting,
        const GradientFlagsSettings& gradFlagSettings,
        ConsoleRig::IProgress* progress)
    {
        auto outputIOFormat = std::make_shared<TerrainFormat>(gradFlagSettings);
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
                    WriteCellCoverageData<ShadowSample>(outputConfig, *outputIOFormat, layerUberSurface, l, overwriteExisting, progress);
                } else if (layer._format == 62) {
                    WriteCellCoverageData<uint8>(outputConfig, *outputIOFormat, layerUberSurface, l, overwriteExisting, progress);
                } else {
                    LogAlwaysError 
                        << "Unknown format (" << layer._format 
                        << ") for terrain coverage file for layer: " << Conversion::Convert<std::string>(layer._name);
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
        auto step = progress ? progress->BeginStep("Generate Cell Files", (unsigned)cells.size(), true) : nullptr;
        for (auto c=cells.cbegin(); c!=cells.cend(); ++c) {
            char heightMapFile[MaxPath];
            outputConfig.GetCellFilename(heightMapFile, dimof(heightMapFile), c->_cellIndex, CoverageId_Heights);
            if (overwriteExisting || !DoesFileExist(heightMapFile)) {
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

            if (step) {
                if (step->IsCancelled()) break;
                step->Advance();
            }
        }
    }

    void GenerateShadowsSurface(
        const TerrainConfig& cfg, 
        const ::Assets::ResChar uberSurfaceDir[],
        bool overwriteExisting,
        ConsoleRig::IProgress* progress)
    {
        unsigned shadowLayerIndex = ~0u;
        for (unsigned l=0; l<cfg.GetCoverageLayerCount(); ++l)
            if (cfg.GetCoverageLayer(l)._id == CoverageId_AngleBasedShadows) {
                shadowLayerIndex = l;
                break;
            }
        if (shadowLayerIndex == ~0u) return;

        ::Assets::ResChar shadowUberFn[MaxPath];
        TerrainConfig::GetUberSurfaceFilename(shadowUberFn, dimof(shadowUberFn), uberSurfaceDir, CoverageId_AngleBasedShadows);

        if (overwriteExisting || !DoesFileExist(shadowUberFn)) {
            Float2 sunAxisOfMovement(XlCos(cfg.SunPathAngle()), XlSin(cfg.SunPathAngle()));

            {
                //////////////////////////////////////////////////////////////////////////////////////
                    // this is the shadows layer... We need to build the shadows procedurally
                ::Assets::ResChar uberHeightsFile[MaxPath];
                TerrainConfig::GetUberSurfaceFilename(uberHeightsFile, dimof(uberHeightsFile), uberSurfaceDir, CoverageId_Heights);
                TerrainUberHeightsSurface heightsData(uberHeightsFile);
                HeightsUberSurfaceInterface uberSurfaceInterface(heightsData);

                //////////////////////////////////////////////////////////////////////////////////////
                    // build the uber shadowing file, and then write out the shadowing textures for each node
                // Int2 interestingMins((9-1) * 16 * 32, (19-1) * 16 * 32), interestingMaxs((9+4) * 16 * 32, (19+4) * 16 * 32);
                float shadowToHeightsScale = 
                    cfg.NodeDimensionsInElements()[0] 
                    / float(cfg.GetCoverageLayer(shadowLayerIndex)._nodeDimensions[0]);
            
                UInt2 interestingMins(0, 0);
                UInt2 interestingMaxs = UInt2(
                    unsigned((cfg._cellCount[0] * cfg.CellDimensionsInNodes()[0] * cfg.NodeDimensionsInElements()[0]) / shadowToHeightsScale),
                    unsigned((cfg._cellCount[1] * cfg.CellDimensionsInNodes()[1] * cfg.NodeDimensionsInElements()[1]) / shadowToHeightsScale));

                //////////////////////////////////////////////////////////////////////////////////////
                uberSurfaceInterface.BuildShadowingSurface(
                    shadowUberFn, interestingMins, interestingMaxs, 
                    sunAxisOfMovement, cfg.ElementSpacing(), 
                    shadowToHeightsScale, progress);
            }
        }

        //////////////////////////////////////////////////////////////////////////////////////
            // write cell files
        auto fmt = std::make_shared<TerrainFormat>();
        WriteCellCoverageData<ShadowSample>(
            cfg, *fmt, shadowUberFn, shadowLayerIndex, overwriteExisting, progress);
    }

    void GenerateAmbientOcclusionSurface(
        const TerrainConfig& cfg, 
        const ::Assets::ResChar uberSurfaceDir[],
        bool overwriteExisting,
        ConsoleRig::IProgress* progress)
    {
        unsigned layerIndex = ~0u;
        for (unsigned l=0; l<cfg.GetCoverageLayerCount(); ++l)
            if (cfg.GetCoverageLayer(l)._id == CoverageId_AmbientOcclusion) {
                layerIndex = l;
                break;
            }
        if (layerIndex == ~0u) return;

        ::Assets::ResChar shadowUberFn[MaxPath];
        TerrainConfig::GetUberSurfaceFilename(shadowUberFn, dimof(shadowUberFn), uberSurfaceDir, CoverageId_AmbientOcclusion);

        if (overwriteExisting || !DoesFileExist(shadowUberFn)) {
            Float2 sunAxisOfMovement(XlCos(cfg.SunPathAngle()), XlSin(cfg.SunPathAngle()));

            {
                //////////////////////////////////////////////////////////////////////////////////////
                    // this is the shadows layer... We need to build the shadows procedurally
                ::Assets::ResChar uberHeightsFile[MaxPath];
                TerrainConfig::GetUberSurfaceFilename(uberHeightsFile, dimof(uberHeightsFile), uberSurfaceDir, CoverageId_Heights);
                TerrainUberHeightsSurface heightsData(uberHeightsFile);
                HeightsUberSurfaceInterface uberSurfaceInterface(heightsData);

                //////////////////////////////////////////////////////////////////////////////////////
                    // build the uber shadowing file, and then write out the shadowing textures for each node
                // Int2 interestingMins((9-1) * 16 * 32, (19-1) * 16 * 32), interestingMaxs((9+4) * 16 * 32, (19+4) * 16 * 32);
                float shadowToHeightsScale = 
                    cfg.NodeDimensionsInElements()[0] 
                    / float(cfg.GetCoverageLayer(layerIndex)._nodeDimensions[0]);
            
                UInt2 interestingMins(0, 0);
                UInt2 interestingMaxs = UInt2(
                    unsigned((cfg._cellCount[0] * cfg.CellDimensionsInNodes()[0] * cfg.NodeDimensionsInElements()[0]) / shadowToHeightsScale),
                    unsigned((cfg._cellCount[1] * cfg.CellDimensionsInNodes()[1] * cfg.NodeDimensionsInElements()[1]) / shadowToHeightsScale));

                //////////////////////////////////////////////////////////////////////////////////////
                static auto testRadius = 24u;
                uberSurfaceInterface.BuildAmbientOcclusion(
                    shadowUberFn, interestingMins, interestingMaxs, 
                    cfg.ElementSpacing(), shadowToHeightsScale, testRadius,
                    progress);
            }
        }

        //////////////////////////////////////////////////////////////////////////////////////
            // write cell files
        auto fmt = std::make_shared<TerrainFormat>();
        WriteCellCoverageData<uint8>(
            cfg, *fmt, shadowUberFn, layerIndex, overwriteExisting, progress);
    }

    void GenerateMissingUberSurfaceFiles(
        const TerrainConfig& cfg, 
        const ::Assets::ResChar uberSurfaceDir[],
        ConsoleRig::IProgress* progress)
    {
        CreateDirectoryRecursive(uberSurfaceDir);

        auto step = progress ? progress->BeginStep("Generate UberSurface Files", (unsigned)cfg.GetCoverageLayerCount(), true) : nullptr;
        for (unsigned l=0; l<cfg.GetCoverageLayerCount(); ++l) {
            const auto& layer = cfg.GetCoverageLayer(l);

            ::Assets::ResChar uberSurfaceFile[MaxPath];
            TerrainConfig::GetUberSurfaceFilename(uberSurfaceFile, dimof(uberSurfaceFile), uberSurfaceDir, layer._id);
            if (DoesFileExist(uberSurfaceFile)) continue;

            if (layer._id == CoverageId_AngleBasedShadows || layer._id == CoverageId_AmbientOcclusion) {

                continue;      // (skip, we'll get this later in GenerateShadowsSurface)

            } else {

                HeightsUberSurfaceInterface::BuildEmptyFile(
                    uberSurfaceFile, 
                    cfg._cellCount[0] * cfg.CellDimensionsInNodes()[0] * layer._nodeDimensions[0],
                    cfg._cellCount[1] * cfg.CellDimensionsInNodes()[1] * layer._nodeDimensions[1],
                    RenderCore::Metal::BitsPerPixel((RenderCore::Metal::NativeFormat::Enum)layer._format));

            }

            if (step) {
                if (step->IsCancelled()) break;
                step->Advance();
            }
        }

        GenerateShadowsSurface(cfg, uberSurfaceDir, false, progress);
        GenerateAmbientOcclusionSurface(cfg, uberSurfaceDir, false, progress);
    }
    
///////////////////////////////////////////////////////////////////////////////////////////////////
    class DEMConfig
    {
    public:
        UInt2 _dims;

        DEMConfig(const char inputHdr[]);
    };

    static void TIFFWarningHandler(const char* module, const char* fmt, va_list args)
    {
        // suppress warnings
        char buffer[1024];
        _vsnprintf_s(buffer, dimof(buffer), _TRUNCATE, fmt, args);
        LogWarning << "Tiff reader warning: " << buffer;
    }

    DEMConfig::DEMConfig(const char inputHdr[])
    {
        _dims = UInt2(0, 0);

        auto ext = XlExtension(inputHdr);
        if (ext && (!XlCompareStringI(ext, "hdr") || !XlCompareStringI(ext, "flt"))) {

            ::Assets::ResChar inputFile[MaxPath];
            XlCopyString(inputFile, inputHdr);
            XlChopExtension(inputFile);
            XlCatString(inputFile, dimof(inputFile), ".hdr");

            size_t fileSize = 0;
            auto block = LoadFileAsMemoryBlock(inputFile, &fileSize);
            std::string configAsString(block.get(), &block[fileSize]);
            std::regex parse("^(\\S+)\\s+(.*)");

            std::vector<int> captureGroups;
            captureGroups.push_back(1);
            captureGroups.push_back(2);
    
            const std::sregex_token_iterator end;
            std::sregex_token_iterator iter(configAsString.begin(), configAsString.end(), parse, captureGroups);
            for (;iter != end;) {
                auto paramName = *iter++;
                auto paramValue = *iter++;

                    //  we ignore many parameters. But we at least need to get ncols & nrows
                    //  These tell us the dimensions of the input data
                if (!XlCompareStringI(paramName.str().c_str(), "ncols")) { _dims[0] = XlAtoI32(paramValue.str().c_str()); }
                if (!XlCompareStringI(paramName.str().c_str(), "nrows")) { _dims[1] = XlAtoI32(paramValue.str().c_str()); }
            }

        } else if (ext && (!XlCompareStringI(ext, "tif") || !XlCompareStringI(ext, "tiff"))) {
            
            auto* tif = TIFFOpen(inputHdr, "r");
            if (tif) {
                TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &_dims[0]);
                TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &_dims[1]);
            }

        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    class TerrainUberHeader
    {
    public:
        unsigned _magic;
        unsigned _width, _height;
        unsigned _dummy;

        static const unsigned Magic = 0xa3d3e3c3;
    };

    UInt2 ConvertDEMData(
        const ::Assets::ResChar outputDir[], const ::Assets::ResChar input[], 
        unsigned destNodeDims, unsigned destCellTreeDepth,
        ConsoleRig::IProgress* progress)
    {
        auto initStep = progress ? progress->BeginStep("Load source data", 1, false) : nullptr;

        TIFFSetWarningHandler(&TIFFWarningHandler);

        DEMConfig inCfg(input);
        if (!(inCfg._dims[0]*inCfg._dims[1])) {
            Throw(
                ::Exceptions::BasicLabel("Bad or missing input terrain config file (%s)", input));
        }

            //  we have to make sure the width and height are multiples of the
            //  dimensions of a cell (in elements). We'll pad out the edges if
            //  they don't match
        const unsigned cellWidthInNodes = 1<<(destCellTreeDepth-1);
        const unsigned clampingDim = destNodeDims * cellWidthInNodes;
        UInt2 finalDims = inCfg._dims;
        if ((finalDims[0] % clampingDim) != 0) { finalDims[0] += clampingDim - (finalDims[0] % clampingDim); }
        if ((finalDims[1] % clampingDim) != 0) { finalDims[1] += clampingDim - (finalDims[1] % clampingDim); }

        CreateDirectoryRecursive(outputDir);

        uint64 resultSize = 
            sizeof(TerrainUberHeader)
            + finalDims[0] * finalDims[1] * sizeof(float)
            ;

        ::Assets::ResChar outputUberFileName[MaxPath]; 
        SceneEngine::TerrainConfig::GetUberSurfaceFilename(
            outputUberFileName, dimof(outputUberFileName),
            outputDir, SceneEngine::CoverageId_Heights);

        MemoryMappedFile outputUberFile(outputUberFileName, resultSize, MemoryMappedFile::Access::Write);
        if (!outputUberFile.IsValid())
            Throw(::Exceptions::BasicLabel("Couldn't open output file (%s)", outputUberFile));

        auto& hdr   = *(TerrainUberHeader*)outputUberFile.GetData();
        hdr._magic  = TerrainUberHeader::Magic;
        hdr._width  = finalDims[0];
        hdr._height = finalDims[1];
        hdr._dummy  = 0;

        float* outputArray = (float*)PtrAdd(outputUberFile.GetData(), sizeof(TerrainUberHeader));

        auto ext = XlExtension(input);

        if (ext && (!XlCompareStringI(ext, "hdr") || !XlCompareStringI(ext, "flt"))) {
            MemoryMappedFile inputFileData(input, 0, MemoryMappedFile::Access::Read);
            if (!inputFileData.IsValid())
                Throw(::Exceptions::BasicLabel("Couldn't open input file (%s)", input));

            if (initStep) {
                initStep->Advance();
                initStep.reset();
            }

            auto copyRows = std::min(finalDims[1], inCfg._dims[1]);
            const unsigned progressStep = 16;
            auto copyStep = progress ? progress->BeginStep("Create uber surface data", copyRows / progressStep, true) : nullptr;

            auto inputArray = (const float*)inputFileData.GetData();

            unsigned y2=0;
            for (; (y2+progressStep)<=copyRows; y2+=progressStep) {
                for (unsigned y=0; y<progressStep; ++y) {
                    std::copy(
                        &inputArray[(y2+y) * inCfg._dims[0]],
                        &inputArray[(y2+y) * inCfg._dims[0] + std::min(inCfg._dims[0], finalDims[0])],
                        &outputArray[(y2+y) * finalDims[0]]);
                }

                if (copyStep) {
                    if (copyStep->IsCancelled())
                        Throw(::Exceptions::BasicLabel("User cancelled"));
                    copyStep->Advance();
                }
            }

                // remainder rows left over after dividing by progressStep
            for (; y2<copyRows; ++y2) {
                std::copy(
                    &inputArray[y2 * inCfg._dims[0]],
                    &inputArray[y2 * inCfg._dims[0] + std::min(inCfg._dims[0], finalDims[0])],
                    &outputArray[y2 * finalDims[0]]);
            }
        } else if (ext && (!XlCompareStringI(ext, "tif") || !XlCompareStringI(ext, "tiff"))) {
                // attempt to read geotiff file
            auto* tif = TIFFOpen(input, "r");
            if (!tif)
                Throw(::Exceptions::BasicLabel("Couldn't open input file (%s)", input));

            // auto buf = _TIFFmalloc(TIFFStripSize(tif));
            auto stripCount = TIFFNumberOfStrips(tif);

            auto copyStep = 
                progress 
                ? progress->BeginStep("Create uber surface data", stripCount, true)
                : nullptr;

            uint32 rowsperstrip = 1;
            TIFFGetFieldDefaulted(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);

                // assuming that we're going to load in an array of floats here
                // Well, tiff can store other types of elements... But we'll just
                // assume it's want we want
            for (tstrip_t strip = 0; strip < stripCount; strip++) {
                TIFFReadEncodedStrip(tif, strip, &outputArray[strip * rowsperstrip * finalDims[0]], (tsize_t) -1);

                if (copyStep) {
                    if (copyStep->IsCancelled())
                        Throw(::Exceptions::BasicLabel("User cancelled"));
                    copyStep->Advance();
                }
            }

            // _TIFFfree(buf);
            TIFFClose(tif);
        }

            // fill in the extra space caused by rounding up
        if (finalDims[0] > inCfg._dims[0]) {
            for (unsigned y=0; y<inCfg._dims[1]; ++y) {
                std::fill(
                    &outputArray[y * finalDims[0] + inCfg._dims[0]],
                    &outputArray[y * finalDims[0] + finalDims[0]],
                    0.f);
            }
        }

        for (unsigned y=inCfg._dims[1]; y < finalDims[1]; ++y) {
            std::fill(
                &outputArray[y * finalDims[0]],
                &outputArray[y * finalDims[0] + finalDims[0]],
                0.f);
        }

        return UInt2(finalDims[0] / clampingDim, finalDims[1] / clampingDim);
    }

    void GenerateBlankUberSurface(
        const ::Assets::ResChar outputDir[], 
        unsigned cellCountX, unsigned cellCountY,
        unsigned destNodeDims, unsigned destCellTreeDepth,
        ConsoleRig::IProgress* progress)
    {
        CreateDirectoryRecursive(outputDir);

        UInt2 finalDims(
            cellCountX * destNodeDims * (1<<(destCellTreeDepth-1)),
            cellCountY * destNodeDims * (1<<(destCellTreeDepth-1)));

        uint64 resultSize = 
            sizeof(TerrainUberHeader)
            + finalDims[0] * finalDims[1] * sizeof(float)
            ;

        ::Assets::ResChar outputUberFileName[MaxPath]; 
        SceneEngine::TerrainConfig::GetUberSurfaceFilename(
            outputUberFileName, dimof(outputUberFileName),
            outputDir, SceneEngine::CoverageId_Heights);

        MemoryMappedFile outputUberFile(outputUberFileName, resultSize, MemoryMappedFile::Access::Write);
        if (!outputUberFile.IsValid())
            Throw(::Exceptions::BasicLabel("Couldn't open output file (%s)", outputUberFile));

        auto& hdr   = *(TerrainUberHeader*)outputUberFile.GetData();
        hdr._magic  = TerrainUberHeader::Magic;
        hdr._width  = finalDims[0];
        hdr._height = finalDims[1];
        hdr._dummy  = 0;

        float* outputArray = (float*)PtrAdd(outputUberFile.GetData(), sizeof(TerrainUberHeader));
        std::fill(
            outputArray,
            &outputArray[finalDims[0] * finalDims[1]],
            0.f);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    static UInt2 GetUberSurfaceDimensions(const ::Assets::ResChar fn[])
    {
        BasicFile file(fn, "rb", BasicFile::ShareMode::Read|BasicFile::ShareMode::Write);
        TerrainUberHeader hdr;
        if ((file.Read(&hdr, sizeof(hdr), 1) != 1) || (hdr._magic != TerrainUberHeader::Magic))
            Throw(::Exceptions::BasicLabel("Error while reading from: (%s)", fn));
        return UInt2(hdr._width, hdr._height);
    }

    UInt2 GetCellCountFromUberSurface(
        const ::Assets::ResChar inputUberSurfaceDirectory[],
        UInt2 destNodeDims, unsigned destCellTreeDepth)
    {
        using namespace SceneEngine;

        ::Assets::ResChar uberSurfaceHeights[MaxPath]; 
        TerrainConfig::GetUberSurfaceFilename(
            uberSurfaceHeights, dimof(uberSurfaceHeights),
            inputUberSurfaceDirectory, SceneEngine::CoverageId_Heights);
        auto eleCount = GetUberSurfaceDimensions(uberSurfaceHeights);

        auto cellDimsInEles = (1 << (destCellTreeDepth - 1)) * destNodeDims;
        if ((eleCount[0] % cellDimsInEles[0])!=0 || (eleCount[1] % cellDimsInEles[1])!=0)
            Throw(::Exceptions::BasicLabel("Uber surface size is not divisable by cell size (uber surface size:(%ix%i), cell size:(%i))", 
            eleCount[0], eleCount[1], cellDimsInEles[0]));

        return UInt2(eleCount[0] / cellDimsInEles[0], eleCount[1] / cellDimsInEles[1]);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
    static void WriteNode(  
        float destination[], TerrainCell::Node& node, 
        const char sourceFileName[], const char secondaryCacheName[], 
        size_t stride, signed downsample)
    {
            // load the raw data either from the source file, or the cache file
        std::unique_ptr<uint16[]> rawData;
        
        const unsigned expectedCount = node._widthInElements*node._widthInElements;

            // todo -- check for incomplete nodes (ie, with holes)
        if (node._heightMapFileSize) {
            auto count = node._heightMapFileSize/sizeof(uint16);
            if (count == expectedCount) {
                BasicFile file(sourceFileName, "rb");
                rawData = std::make_unique<uint16[]>(count);
                file.Seek(node._heightMapFileOffset, SEEK_SET);
                file.Read(rawData.get(), sizeof(uint16), count);
            }
        } else if (node._secondaryCacheSize) {
            auto count = node._secondaryCacheSize/sizeof(uint16);
            if (count == expectedCount) {
                BasicFile file(secondaryCacheName, "rb");
                rawData = std::make_unique<uint16[]>(count);
                file.Seek(node._secondaryCacheOffset, SEEK_SET);
                file.Read(rawData.get(), sizeof(uint16), count);
            }
        }

        const unsigned dimsNoOverlay = node._widthInElements - node.GetOverlapWidth();

        for (unsigned y=0; y<dimsNoOverlay; ++y)
            for (unsigned x=0; x<dimsNoOverlay; ++x) {
                assert(((y*node._widthInElements)+x) < expectedCount);
                auto inputValue = rawData ? rawData[(y*node._widthInElements)+x] : uint16(0);

                float cellSpaceHeight = node._localToCell(2,2) * float(inputValue) + node._localToCell(2,3);
                
                assert(downsample==0);
                *PtrAdd(destination, y*stride + x*sizeof(float)) = cellSpaceHeight;
            }
    }

    static void WriteBlankNode(float destination[], size_t stride, signed downsample, const UInt2 elementDims)
    {
        assert(downsample==0);
        for (unsigned y=0; y<elementDims[1]; ++y)
            for (unsigned x=0; x<elementDims[0]; ++x) {
                *PtrAdd(destination, y*stride + x*sizeof(float)) = 0.f;
            }
    }

    static bool BuildUberSurfaceFile(
        const char filename[], const TerrainConfig& config, 
        ITerrainFormat* ioFormat,
        unsigned xStart, unsigned yStart, unsigned xDims, unsigned yDims)
    {
            //
            //  Read in the existing terrain data, and generate a uber surface file
            //        --  this uber surface file should contain the height values for the
            //            entire "world", at the highest resolution
            //  The "uber" surface file is initialized from the source crytek terrain files, 
            //  but becomes our new authoritative source for terrain data.
            //
        
        const unsigned cellCount        = xDims * yDims;
        const auto cellDimsInNodes      = config.CellDimensionsInNodes();
        const auto nodeDimsInElements   = config.NodeDimensionsInElements();
        const unsigned nodesPerCell     = cellDimsInNodes[0] * cellDimsInNodes[1];
        const unsigned heightsPerNode   = nodeDimsInElements[0] * nodeDimsInElements[1];

        size_t stride = nodeDimsInElements[0] * cellDimsInNodes[0] * xDims * sizeof(float);

        uint64 resultSize = 
            sizeof(TerrainUberHeader)
            + cellCount * nodesPerCell * heightsPerNode * sizeof(float)
            ;
        MemoryMappedFile mappedFile(filename, resultSize, MemoryMappedFile::Access::Write, BasicFile::ShareMode::Read);
        if (!mappedFile.IsValid())
            return false;

        auto& hdr   = *(TerrainUberHeader*)mappedFile.GetData();
        hdr._magic  = TerrainUberHeader::Magic;
        hdr._width  = nodeDimsInElements[0] * cellDimsInNodes[0] * xDims;
        hdr._height = nodeDimsInElements[1] * cellDimsInNodes[1] * yDims;
        hdr._dummy  = 0;

        void* heightArrayStart = PtrAdd(mappedFile.GetData(), sizeof(TerrainUberHeader));

        TRY
        {
                // fill in the "uber" surface file with all of the terrain information
            for (unsigned cy=0; cy<yDims; ++cy)
                for (unsigned cx=0; cx<xDims; ++cx) {

                    char buffer[MaxPath];
                    config.GetCellFilename(buffer, dimof(buffer), UInt2(cx, cy),
                        CoverageId_Heights);
                    auto& cell = ioFormat->LoadHeights(buffer);

                        //  the last "field" in the input data should be the resolution that we want
                        //  however, if we don't have enough fields, we may have to upsample from
                        //  the lower resolution ones

                    if (cell._nodeFields.size() >= 5) {

                        auto& field = cell._nodeFields[4];
                        assert(field._widthInNodes == cellDimsInNodes[0]);
                        assert(field._heightInNodes == cellDimsInNodes[1]);
                        for (auto n=field._nodeBegin; n!=field._nodeEnd; ++n) {
                            auto& node = *cell._nodes[n];
                            unsigned nx = (unsigned)std::floor(node._localToCell(0, 3) / 64.f + 0.5f);
                            unsigned ny = (unsigned)std::floor(node._localToCell(1, 3) / 64.f + 0.5f);

                            auto* nodeDataStart = (float*)PtrAdd(
                                heightArrayStart, 
                                    (cy * cellDimsInNodes[1] + ny) * nodeDimsInElements[1] * stride 
                                +   (cx * cellDimsInNodes[0] + nx) * nodeDimsInElements[0] * sizeof(float));

                            WriteNode(
                                nodeDataStart, node, 
                                cell.SourceFile().c_str(), cell.SecondaryCacheFile().c_str(), 
                                stride, 0);
                        }

                    } else {

                        for (unsigned y=0; y<cellDimsInNodes[1]; ++y)
                            for (unsigned x=0; x<cellDimsInNodes[0]; ++x) {
                                auto* nodeDataStart = (float*)PtrAdd(
                                    heightArrayStart, 
                                        (cy * cellDimsInNodes[1] + y) * nodeDimsInElements[1] * stride 
                                    +   (cx * cellDimsInNodes[0] + x) * nodeDimsInElements[0] * sizeof(float));
                                WriteBlankNode(nodeDataStart, stride, 0, nodeDimsInElements);
                            }

                    }

                }
        }
        CATCH(...) { return false; } 
        CATCH_END

        return true;
    }
#endif

}
