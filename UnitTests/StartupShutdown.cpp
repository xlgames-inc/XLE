// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "../RenderCore/IDevice.h"
#include "../RenderCore/Metal/Shader.h"		// for CreateCompileAndAsyncManager
#include "../BufferUploads/IBufferUploads.h"
#include "../Assets/CompileAndAsyncManager.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/SystemUtils.h"
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
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
                UnitTest_SetWorkingDirectory();
                ConsoleRig::GlobalServices services(GetStartupConfig());

				{
					auto renderDevice = RenderCore::CreateDevice();
                    BufferUploads::Attach(services);
					auto bufferUploads = BufferUploads::CreateManager(renderDevice.get());
					auto asyncMan = std::make_shared<::Assets::Services>(0);
                    RenderCore::Metal::InitCompileAndAsyncManager();

					auto renderVersion = renderDevice->GetVersionInformation();
					LogInfo << "RenderCore version (" << renderVersion.first << ") and date (" << renderVersion.second << ")";
				}

                BufferUploads::Detach();
			}
		}

	};
}