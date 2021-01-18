// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../../PlatformRig/AllocationProfiler.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../ConsoleRig/GlobalServices.h"
#include "../../../OSServices/RawFS.h"
#include "../../../Core/Exceptions.h"
#include <stdio.h>

    // Note --  when you need to include <windows.h>, generally
    //          prefer to to use the following header ---
    //          This helps prevent name conflicts with 
    //          windows #defines and so forth...
    //  (this is only actually required for the "WinMain" signature)
#include "../../../OSServices/WinAPI/IncludeWindows.h"

namespace Sample
{
    void ExecuteSample();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
    #if CLIBRARIES_ACTIVE == CLIBRARIES_MSVC && defined(_DEBUG)
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | /*_CRTDBG_CHECK_CRT_DF |*/ /*_CRTDBG_CHECK_EVERY_16_DF |*/ _CRTDBG_LEAK_CHECK_DF /*| _CRTDBG_CHECK_ALWAYS_DF*/);
        // _CrtSetBreakAlloc(2497);
    #endif

    using namespace Sample;

        //  Initialize the "AccumulatedAllocations" profiler as soon as possible, to catch
        //  startup allocation counts.
    PlatformRig::AccumulatedAllocations accumulatedAllocations;

        //  We need to initialize logging output.
        //  The "int" directory stands for "intermediate." We cache processed 
        //  models and textures in this directory
        //  But it's also a convenient place for log files (since it's excluded from
        //  git and it contains only temporary data).
        //  Note that we overwrite the log file every time, destroying previous data.
    ConsoleRig::GlobalServices services("testplatform");
    Log(Verbose) << "------------------------------------------------------------------------------------------";

    TRY {
        Sample::ExecuteSample();
    } CATCH (const std::exception& e) {
        Log(Error) << "Hit top level exception. Aborting program!";
        Log(Error) << e.what();
    } CATCH_END

    return 0;
}
