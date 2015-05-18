// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../../PlatformRig/AllocationProfiler.h"
#include "../../../ConsoleRig/Log.h"
#include "../../../ConsoleRig/GlobalServices.h"
#include "../../../Utility/SystemUtils.h"
#include "../../../Core/Exceptions.h"

    // Note --  when you need to include <windows.h>, generally
    //          prefer to to use the following header ---
    //          This helps prevent name conflicts with 
    //          windows #defines and so forth...
    //  (this is only actually required for the "WinMain" signature)
#include "../../../Core/WinAPI/IncludeWindows.h"

namespace Sample
{
    void ExecuteSample();
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    #if CLIBRARIES_ACTIVE == CLIBRARIES_MSVC && defined(_DEBUG)
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | /*_CRTDBG_CHECK_CRT_DF |*/ /*_CRTDBG_CHECK_EVERY_16_DF |*/ _CRTDBG_LEAK_CHECK_DF /*| _CRTDBG_CHECK_ALWAYS_DF*/);
    #endif

    using namespace Sample;

        //  There maybe a few basic platform-specific initialisation steps we might need to
        //  perform. We can do these here, before calling into platform-specific code.

            // ...

        //  Initialize the "AccumulatedAllocations" profiler as soon as possible, to catch
        //  startup allocation counts.
    PlatformRig::AccumulatedAllocations accumulatedAllocations;

    ConsoleRig::GlobalServices services;
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
