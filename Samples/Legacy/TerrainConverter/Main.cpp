// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "../../Tools/ToolsRig/TerrainConversion.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/TerrainFormat.h"
#include "../../SceneEngine/TerrainConfig.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../ConsoleRig/Log.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../OSServices/RawFS.h"
#include "../../Utility/StringFormat.h"
#include "../../OSServices/RawFS.h"
#include "../../Utility/PtrUtils.h"
#include <regex>

#include "../../SceneEngine/TerrainUberSurface.h"

#include "../../OSServices/WinAPI/IncludeWindows.h"

#pragma warning(disable:4505)   // unreferenced local function has been removed

static void SetWorkingDirectory()
{
        //
        //      For convenience, set the working directory to be ../Working 
        //              (relative to the application path)
        //
    utf8 appPath[MaxPath];
    GetProcessPath    (appPath, dimof(appPath));
	auto splitter = MakeFileNameSplitter(appPath);
	ChDir((splitter.DriveAndPath().AsString() + "/../Working").c_str());
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    ConsoleRig::GlobalServices services("terrconvlog");

    auto compileAndAsync = std::make_unique<::Assets::CompileAndAsyncManager>();

    ::Assets::ConfigFileContainer<SceneEngine::TerrainConfig> cfg("game/centralcal");
    ToolsRig::GenerateMissingUberSurfaceFiles(cfg._asset, "game/centralcal");
    ToolsRig::GenerateCellFiles(cfg._asset, "game/centralcal", false, SceneEngine::GradientFlagsSettings());

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

