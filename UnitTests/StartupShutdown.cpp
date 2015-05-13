// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../RenderCore/IDevice.h"
#include "../RenderCore/Metal/Shader.h"		// for CreateCompileAndAsyncManager
#include "../BufferUploads/IBufferUploads.h"
#include "../Assets/CompileAndAsyncManager.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/SystemUtils.h"
#include <CppUnitTest.h>
#include <random>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
	static void SetWorkingDirectory()
	{
		//
		//      For convenience, set the working directory to be ../Working 
		//              (relative to the application path)
		//
		nchar_t appDir[MaxPath];
		nchar_t workingDir[MaxPath];

		XlGetCurrentDirectory(dimof(appDir), appDir);
		XlSimplifyPath(appDir, dimof(appDir), appDir, a2n("\\/"));
		auto* catPath = a2n("..\\Working");
		XlConcatPath(workingDir, dimof(workingDir), appDir, catPath, &catPath[XlStringLen(catPath)]);
		XlSimplifyPath(workingDir, dimof(workingDir), workingDir, a2n("\\/"));
		XlChDir(workingDir);
	}

	TEST_CLASS(StartupShutdown)
	{
	public:

		TEST_METHOD(CoreStartup)
		{
				//	Startup, and then Shutdown basic services
				//	Just checking for exceptions, leaks or other problems
				//	in the most basic stuff

			#if CLIBRARIES_ACTIVE == CLIBRARIES_MSVC && defined(_DEBUG)
				_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | /*_CRTDBG_CHECK_EVERY_16_DF |*/ _CRTDBG_LEAK_CHECK_DF /*| _CRTDBG_CHECK_ALWAYS_DF*/);
				// _CrtSetBreakAlloc(2319);
			#endif

			{
				SetWorkingDirectory();
				srand(std::random_device().operator()());

				CreateDirectoryRecursive("int");
                ConsoleRig::GlobalServices services;
				ConsoleRig::Logging_Startup("log.cfg", "int/unittest.txt");

				{
					auto console = std::make_unique<ConsoleRig::Console>();
					auto renderDevice = RenderCore::CreateDevice();
					auto bufferUploads = BufferUploads::CreateManager(renderDevice.get());
					auto asyncMan = std::make_shared<::Assets::Services>(0);
                    RenderCore::Metal::InitCompileAndAsyncManager();

					auto renderVersion = renderDevice->GetVersionInformation();
					LogInfo << "RenderCore version (" << renderVersion.first << ") and date (" << renderVersion.second << ")";
				}
			}
		}

	};
}