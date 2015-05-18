// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "../RenderCore/Metal/Shader.h"		// for CreateCompileAndAsyncManager
#include "../RenderCore/Assets/ModelRunTime.h"
#include "../RenderCore/Assets/ModelRunTimeInternal.h"
#include "../RenderCore/Assets/Services.h"
#include "../Assets/IntermediateResources.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../Utility/SystemUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/TimeUtils.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
	TEST_CLASS(ModelConversion)
	{
	public:

		TEST_METHOD(ColladaConversion)
		{
				//	In this test, we attempt to compile a model with a
				//	minimal engine startup process.
				//
				//	We will invoke model conversion using ColladaConversion dll
				//
				//	We're going to use Assets::GetAssetComp<> to load a 
				//	model scaffold. So, we'll need to delete the intermediate
				//	file before we start, to guarantee that the system executes
				//	asset compilation
				//
				//	Note that don't have to do any RenderCore initialisation, and
				//	particularly, we don't need to create a RenderCore::IDevice

            UnitTest_SetWorkingDirectory();
            ConsoleRig::GlobalServices services(GetStartupConfig());

			{
                auto aservices = std::make_shared<::Assets::Services>(0);
				auto& asyncMan = aservices->GetAsyncMan();
                auto raservices = std::make_shared<RenderCore::Assets::Services>(nullptr);
                raservices->InitColladaCompilers();

				const char sampleAsset[] = "game/model/galleon/galleon.dae";
				using RenderCore::Assets::ModelScaffold;

				{
					using ::Assets::ResChar;
					ResChar intermediateFile[256];
					asyncMan.GetIntermediateStore().MakeIntermediateName(
						intermediateFile, dimof(intermediateFile),
						StringMeld<256, ResChar>() << sampleAsset << "-skin");
					XlDeleteFile((utf8*)intermediateFile);
				}

				auto startTime = Millisecond_Now();
				for (;;) {
					TRY {
						auto& scaffold = Assets::GetAssetComp<ModelScaffold>(sampleAsset);
						Assert::AreEqual(scaffold.ImmutableData()._geoCount, size_t(9));
						break;
					} CATCH(Assets::Exceptions::PendingResource&) {}
					CATCH_END

					if ((Millisecond_Now() - startTime) > 30 * 1000) {
						Assert::IsTrue(false, L"Timeout while compiling asset in ColladaConversion test! Test failed.");
						break;
					}

					Threading::YieldTimeSlice();
					asyncMan.Update();
				}
			}
		}

	};
}