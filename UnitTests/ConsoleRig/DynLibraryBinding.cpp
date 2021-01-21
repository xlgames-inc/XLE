// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../UnitTestHelper.h"
#include "../../ConsoleRig/AttachableLibrary.h"
#include "../../ConsoleRig/AttachablePtr.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../OSServices/Log.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../OSServices/RawFS.h"
#include "../../Utility/StringFormat.h"
#include <stdexcept>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <iostream>

#if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
	#include "../Utility/WinAPI/WinAPIWrapper.h"
#endif

using namespace Catch::literals;
namespace UnitTests
{
#if PLATFORMOS_TARGET == PLATFORMOS_WINDOWS
	TEST_METHOD("DynLibraryBinding-StartupShutdown-Win32", "[consoleRig]")
	{
		auto library = (*Windows::Fn_LoadLibrary)("TestDynLibrary.dll");
		if (library && library != INVALID_HANDLE_VALUE) {

			auto startupFn = (void (*)())(*Windows::Fn_GetProcAddress)(library, "?Startup@Samples@@YAXXZ");
			auto shutdownFn = (void (*)())(*Windows::Fn_GetProcAddress)(library, "?Shutdown@Samples@@YAXXZ");
			auto allocateBlkFn = (void* (*)(size_t))(*Windows::Fn_GetProcAddress)(library, "?AllocateBlock@Samples@@YAPAXI@Z");
			auto deallocateBlkFn = (void (*)(void*))(*Windows::Fn_GetProcAddress)(library, "?DeallocateBlock@Samples@@YAXPAX@Z");

			(*startupFn)();

			{
				void* testBlk = (*allocateBlkFn)(16*1024);
				(*deallocateBlkFn)(testBlk);

				testBlk = (*allocateBlkFn)(16*1024);
				delete[] (char*)testBlk;

				(*deallocateBlkFn)(std::make_unique<char[]>(16*1024).release());
			}

				// get version information
			const char VersionInformationName[] = "?GetVersionInformation@Samples@@YA?AU?$pair@PBDPBD@std@@XZ";
			typedef std::pair<const char*, const char*> VersionQueryFn();
			auto queryFn = (VersionQueryFn*)(*Windows::Fn_GetProcAddress)(library, VersionInformationName);
			if (queryFn) {
				auto version = (*queryFn)();
				Log(Verbose) << "Attached test DLL version: (" << version.first << "), (" << version.second << ")" << std::endl;
			}

			(*shutdownFn)();
		}
	}
#endif

	TEST_CASE("DynLibraryBinding-StartupShutdown", "[consoleRig]")
	{
		auto globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
		Verbose.SetConfiguration(OSServices::MessageTargetConfiguration{"<<configured-template>>"});
		ConsoleRig::AttachableLibrary testLibrary("libUnitTestDynLibrary.so");
		
		std::string attachErrorMsg;
		auto tryAttachResult = testLibrary.TryAttach(attachErrorMsg);
		std::cout << attachErrorMsg << std::endl;
		REQUIRE(tryAttachResult == true);

		using FnSig = std::string(*)(std::string);
		auto fn = testLibrary.GetFunction<FnSig>("ExampleFunctionReturnsString");
		REQUIRE(fn);
		auto interfaceTest = (*fn)("Passed Over Interface");
		REQUIRE(interfaceTest == "This is a string from ExampleFunctionReturnsString <<Passed Over Interface>>");
	}

}

