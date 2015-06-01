// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "../../Tools/ToolsRig/TerrainConversion.h"
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

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    ConsoleRig::GlobalServices services("terrconvlog");

    auto compileAndAsync = std::make_unique<::Assets::CompileAndAsyncManager>();

    using namespace SceneEngine;

    auto fmt = std::make_shared<TerrainFormat>();

    TerrainConfig cfg("game/centralcal");
    GenerateMissingUberSurfaceFiles(cfg, fmt, "game/centralcal");
    GenerateMissingCellFiles(cfg, fmt, "game/centralcal");

    // const unsigned nodeDims = 32;
    // const unsigned cellTreeDepth = 5;
    // 
    // auto cellCount = ToolsRig::ConvertDEMData(
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

