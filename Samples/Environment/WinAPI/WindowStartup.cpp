// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../../PlatformRig/AllocationProfiler.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../Utility/Streams/PathUtils.h"
#include "../../../Utility/Streams/FileUtils.h"
#include "../../../Utility/SystemUtils.h"
#include "../../../Core/Exceptions.h"
#include <stdio.h>
#include <random>

    // Note --  when you need to include <windows.h>, generally
    //          prefer to to use the following header ---
    //          This helps prevent name conflicts with 
    //          windows #defines and so forth...
    //  (this is only actually required for the "WinMain" signature)
#include "../../../Core/WinAPI/IncludeWindows.h"

namespace Sample
{
    void ExecuteSample();

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
        XlConcatPath        (workingDir, dimof(workingDir), appDir, a2n("..\\Working"));
        XlSimplifyPath      (workingDir, dimof(workingDir), workingDir, a2n("\\/"));
        XlChDir             (workingDir);
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    #if CLIBRARIES_ACTIVE == CLIBRARIES_MSVC && defined(_DEBUG)
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | /*_CRTDBG_CHECK_EVERY_16_DF |*/ _CRTDBG_LEAK_CHECK_DF /*| _CRTDBG_CHECK_ALWAYS_DF*/);
        // _CrtSetBreakAlloc(2497);
    #endif

    using namespace Sample;

        //  There maybe a few basic platform-specific initialisation steps we might need to
        //  perform. We can do these here, before calling into platform-specific code.
    SetWorkingDirectory();
    srand(std::random_device().operator()());

        //  Initialize the "AccumulatedAllocations" profiler as soon as possible, to catch
        //  startup allocation counts.
    PlatformRig::AccumulatedAllocations accumulatedAllocations;

        //  We need to initialize logging output.
        //  The "int" directory stands for "intermediate." We cache processed 
        //  models and textures in this directory
        //  But it's also a convenient place for log files (since it's excluded from
        //  git and it contains only temporary data).
        //  Note that we overwrite the log file every time, destroying previous data.
    CreateDirectoryRecursive("int");
    ConsoleRig::Logging_Startup("log.cfg", "int/environmentsamplelog.txt");
    LogInfo << "------------------------------------------------------------------------------------------";

    TRY {
        Sample::ExecuteSample();
    } CATCH (const std::exception& e) {
        XlOutputDebugString("Hit top-level exception: ");
        XlOutputDebugString(e.what());
        XlOutputDebugString("\n");

        LogAlwaysError << "Hit top level exception. Aborting program!";
        LogAlwaysError << e.what();
        XlMessageBox(e.what(), "Top level exception");
    } CATCH_END

    return 0;
}
