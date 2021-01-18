// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../../PlatformRig/AllocationProfiler.h"
#include "../../../OSServices/Log.h"
#include "../../../ConsoleRig/GlobalServices.h"
#include "../../../ConsoleRig/AttachablePtr.h"
#include "../../../OSServices/RawFS.h"
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
    void ExecuteSample(const char finalsDirectory[]);
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
	auto services = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>("environmentsample");
    Log(Verbose) << "------------------------------------------------------------------------------------------" << std::endl;

    auto finalsDirectory = lpCmdLine;
    if (!finalsDirectory[0]
        || OSServices::FindFiles(
            std::string(finalsDirectory) + "/*.*", 
			OSServices::FindFilesFilter::File).empty()) {
        MessageBox(0, "Expecting the same of a directory on the command line. This should be the 'finals' directory exported from the level editor.", "Environment Sample", MB_OK);
    }

    TRY {
        Sample::ExecuteSample(lpCmdLine);
    } CATCH (const std::exception& e) {
        Log(Error) << "Hit top level exception. Aborting program!" << std::endl;
        Log(Error) << e.what() << std::endl;
    } CATCH_END

    return 0;
}
