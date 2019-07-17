// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/SystemUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/WinAPI/WinAPIWrapper.h"
#include <CppUnitTest.h>

namespace UnitTests
{
	TEST_CLASS(DLLBinding)
	{
	public:
		TEST_METHOD(DLLStartupShutdown)
		{
            UnitTest_SetWorkingDirectory();
            ConsoleRig::GlobalServices services(GetStartupConfig());

			auto library = (*Windows::Fn_LoadLibrary)("..\\Finals_Debug32\\TestDLL.dll");
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
	};

}

