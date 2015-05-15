// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/TerrainFormat.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/SystemUtils.h"
#include "../../Utility/PtrUtils.h"
#include <regex>

#include "../../SceneEngine/TerrainUberSurface.h"

#include "../../Core/WinAPI/IncludeWindows.h"

#pragma warning(disable:4505)   // unreferenced local function has been removed

static void SetWorkingDirectory()
{
        //
        //      For convenience, set the working directory to be ../Working 
        //              (relative to the application path)
        //
    nchar_t appPath     [MaxPath];
    nchar_t appDir      [MaxPath];
    nchar_t workingDir  [MaxPath];

    XlGetProcessPath    (appPath, dimof(appPath));
    XlSimplifyPath      (appPath, dimof(appPath), appPath, a2n("\\/"));
    XlDirname           (appDir, dimof(appDir), appPath);
    const auto* fn = a2n("..\\Working");
    XlConcatPath        (workingDir, dimof(workingDir), appDir, fn, &fn[XlStringLen(fn)]);
    XlSimplifyPath      (workingDir, dimof(workingDir), workingDir, a2n("\\/"));
    XlChDir             (workingDir);
}

class DEMConfig
{
public:
    UInt2 _dims;

    DEMConfig(const char inputHdr[]);
};

DEMConfig::DEMConfig(const char inputHdr[])
{
    _dims = UInt2(0, 0);

    size_t fileSize = 0;
    auto block = LoadFileAsMemoryBlock(inputHdr, &fileSize);
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
}

class TerrainUberHeader
{
public:
    unsigned _magic;
    unsigned _width, _height;
    unsigned _dummy;

    static const unsigned Magic = 0xa3d3e3c3;
};

static UInt2 ConvertDEMData(
    const char outputDir[], const char input[], 
    unsigned destNodeDims, unsigned destCellTreeDepth)
{
    StringMeld<MaxPath> inputCfgFile; inputCfgFile << input << ".hdr";
    DEMConfig inCfg(inputCfgFile);

    if (!(inCfg._dims[0]*inCfg._dims[1])) {
        ThrowException(
            ::Exceptions::BasicLabel("Bad or missing input terrain config file (%s)", inputCfgFile));
    }

        //  we have to make sure the width and height are multiples of the
        //  dimensions of a cell (in elements). We'll pad out the edges if
        //  they don't match
    const unsigned cellWidthInNodes = 1<<(destCellTreeDepth-1);
    const unsigned clampingDim = destNodeDims * cellWidthInNodes;
    UInt2 finalDims = inCfg._dims;
    finalDims[0] = std::min(finalDims[0], 512u * 6u);
    finalDims[1] = std::min(finalDims[1], 512u * 6u);
    if ((finalDims[0] % clampingDim) != 0) { finalDims[0] += clampingDim - (finalDims[0] % clampingDim); }
    if ((finalDims[1] % clampingDim) != 0) { finalDims[1] += clampingDim - (finalDims[1] % clampingDim); }

    CreateDirectoryRecursive(outputDir);

    uint64 resultSize = 
        sizeof(TerrainUberHeader)
        + finalDims[0] * finalDims[1] * sizeof(float)
        ;
    StringMeld<MaxPath> outputUberFileName; outputUberFileName << outputDir << "/ubersurface.dat";
    MemoryMappedFile outputUberFile(outputUberFileName, resultSize, MemoryMappedFile::Access::Write);
    if (!outputUberFile.IsValid())
        ThrowException(::Exceptions::BasicLabel("Couldn't open output file (%s)", outputUberFile));

    auto& hdr   = *(TerrainUberHeader*)outputUberFile.GetData();
    hdr._magic  = TerrainUberHeader::Magic;
    hdr._width  = finalDims[0];
    hdr._height = finalDims[1];
    hdr._dummy  = 0;

    StringMeld<MaxPath> inputFileName; inputFileName << input << ".flt";
    MemoryMappedFile inputFile((const char*)inputFileName, 0, MemoryMappedFile::Access::Read);
    if (!inputFile.IsValid())
        ThrowException(::Exceptions::BasicLabel("Couldn't open input file (%s)", inputFileName));

    float* outputArray = (float*)PtrAdd(outputUberFile.GetData(), sizeof(TerrainUberHeader));
    auto inputArray = (const float*)inputFile.GetData();

    for (unsigned y=0; y<std::min(finalDims[1], inCfg._dims[1]); ++y) {
        std::copy(
            &inputArray[y * inCfg._dims[0]],
            &inputArray[y * inCfg._dims[0] + std::min(inCfg._dims[0], finalDims[0])],
            &outputArray[y * finalDims[0]]);
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
    

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    ConsoleRig::GlobalServices services("terrconvlog");

    auto compileAndAsync = std::make_unique<::Assets::CompileAndAsyncManager>();

    using namespace SceneEngine;

    auto fmt = std::make_shared<TerrainFormat>();

    TerrainConfig cfg("game/centralcal");
    GenerateMissingUberSurfaceFiles(cfg, fmt);
    GenerateMissingCellFiles(cfg, fmt);

    // const unsigned nodeDims = 32;
    // const unsigned cellTreeDepth = 5;
    // 
    // auto cellCount = ConvertDEMData(
    //     "game/centralcal", "../SampleSourceData/n38w120/floatn38w120_1",
    //     nodeDims, cellTreeDepth);
    // 
    // TerrainConfig cfg("game/centralcal", cellCount, TerrainConfig::XLE, nodeDims, cellTreeDepth);
    // cfg.Save();
    // 
    // GenerateMissingUberSurfaceFiles(cfg, fmt);
    // GenerateMissingCellFiles(cfg, fmt);

    return 0;
}

