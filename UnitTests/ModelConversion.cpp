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
#include "../ColladaConversion/DLLInterface.h"
#include "../ColladaConversion/NascentModel.h"
#include "../Assets/IntermediateResources.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../ConsoleRig/AttachableLibrary.h"
#include "../Utility/SystemUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/TimeUtils.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include "../Core/SelectConfiguration.h"
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


        TEST_METHOD(ColladaScaffold)
		{
            UnitTest_SetWorkingDirectory();
            ConsoleRig::GlobalServices services(GetStartupConfig());

            // Load all of the .dae files from a hierarchy of folders, look for errors
            // and exceptions from the scaffold parsing. This is intended to be used with
            // large test suites of .dae files (like the collada implementor's test kit)
            auto inputFiles = FindFilesHierarchical("../../work/ColladaImplementors/StandardDataSets", "*.dae", FindFilesFilter::File);

            {
                #if defined(_DEBUG)
                    ConsoleRig::AttachableLibrary lib("../Finals_Debug32/ColladaConversion.dll");
                #else
                    ConsoleRig::AttachableLibrary lib("../Finals_Profile32/ColladaConversion.dll");
                #endif
                lib.TryAttach();
                auto createScaffold = lib.GetFunction<RenderCore::ColladaConversion::CreateColladaScaffoldFn*>(
                    "?CreateColladaScaffold@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@VColladaScaffold@ColladaConversion@RenderCore@@@std@@QBD@Z");
                
                std::vector<std::string> filesWithErrors;
                for (const auto&f:inputFiles) {
                    TRY
                    {
                        (*createScaffold)(f.c_str());
                    }
                    CATCH(...) {
                        filesWithErrors.push_back(f);
                    } CATCH_END
                }

                for (const auto&f:filesWithErrors)
                    LogAlwaysError << "Failure in file: " << f << std::endl;
            }
            
        }

        TEST_METHOD(ColladaConversionPerformance)
		{
            UnitTest_SetWorkingDirectory();
            ConsoleRig::GlobalServices services(GetStartupConfig());

            // profile the performance of the collada model compiler
            // in particular, compare the new path to the old (OpenCollada-based path)
            
            {
                #if defined(_DEBUG)
                    ConsoleRig::AttachableLibrary lib("../Finals_Debug32/ColladaConversion.dll");
                #else
                    ConsoleRig::AttachableLibrary lib("../Finals_Profile32/ColladaConversion.dll");
                #endif
                lib.TryAttach();

                #if !TARGET_64BIT
                    auto newCreateScaffold = lib.GetFunction<RenderCore::ColladaConversion::CreateColladaScaffoldFn*>(
                        "?CreateColladaScaffold@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@VColladaScaffold@ColladaConversion@RenderCore@@@std@@QBD@Z");
                    auto newSerializeSkin = lib.GetFunction<RenderCore::ColladaConversion::ModelSerializeFn*>(
                        "?SerializeSkin@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@ABVColladaScaffold@12@@Z");

                    auto oldCreateModel = lib.GetFunction<RenderCore::ColladaConversion::OCCreateModelFunction*>(
                        "?OCCreateModel@ColladaConversion@RenderCore@@YA?AV?$unique_ptr@VNascentModel@ColladaConversion@RenderCore@@VCrossDLLDeletor@Internal@23@@std@@QBD@Z");
                    auto oldSerializeSkinFunction = lib.GetFunction<RenderCore::ColladaConversion::OCModelSerializeFunction>(
                        "?SerializeSkin@NascentModel@ColladaConversion@RenderCore@@QBE?AV?$shared_ptr@V?$vector@VNascentChunk@ColladaConversion@RenderCore@@V?$allocator@VNascentChunk@ColladaConversion@RenderCore@@@std@@@std@@@std@@XZ");

                    // const ::Assets::ResChar testFile[] = "game/testmodels/ironman/ironman.dae";
                    const ::Assets::ResChar testFile[] = "game/model/galleon/galleon.dae";
                    const unsigned iterations = 100;

                    auto newPathStart = GetPerformanceCounter();
                    for (unsigned c=0; c<iterations; ++c) {
                        auto scaffold = (*newCreateScaffold)(testFile);
                        auto chunks = (*newSerializeSkin)(*scaffold);
                    }
                    auto newPathEnd = GetPerformanceCounter();

                    for (unsigned c=0; c<iterations; ++c) {
                        auto scaffold = (*oldCreateModel)(testFile);
                        auto chunks = (scaffold.get()->*oldSerializeSkinFunction)();
                    }
                    auto oldPathEnd = GetPerformanceCounter();

                    auto freq = GetPerformanceCounterFrequency();

                    LogAlwaysWarning << "New path: " << (newPathEnd-newPathStart) / float(freq/100) << "ms";
                    LogAlwaysWarning << "Old path: " << (oldPathEnd-newPathEnd) / float(freq/100) << "ms";

                #endif
            }
        }

	};
}