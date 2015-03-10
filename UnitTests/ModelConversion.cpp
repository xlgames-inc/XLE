// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../RenderCore/Metal/Shader.h"		// for CreateCompileAndAsyncManager
#include "../RenderCore/Assets/ModelRunTime.h"
#include "../RenderCore/Assets/ModelRunTimeInternal.h"
#include "../RenderCore/Assets/ColladaCompilerInterface.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/SystemUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/TimeUtils.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Assets
{
	template<> uint64 GetCompileProcessType<RenderCore::Assets::ModelScaffold>()
	{
		return RenderCore::Assets::ColladaCompiler::Type_Model;
	}
}

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
		XlConcatPath(workingDir, dimof(workingDir), appDir, a2n("..\\Working"));
		XlSimplifyPath(workingDir, dimof(workingDir), workingDir, a2n("\\/"));
		XlChDir(workingDir);
	}

	void SetupColladaCompilers(::Assets::CompileAndAsyncManager& asyncMan)
	{
		//  Here, we can attach whatever asset compilers we might need
		//  A common compiler is used for converting Collada data into
		//  our native run-time format.
		auto& compilers = asyncMan.GetIntermediateCompilers();

		using RenderCore::Assets::ColladaCompiler;
		auto colladaProcessor = std::make_shared<ColladaCompiler>();
		compilers.AddCompiler(ColladaCompiler::Type_Model, colladaProcessor);
		compilers.AddCompiler(ColladaCompiler::Type_AnimationSet, colladaProcessor);
		compilers.AddCompiler(ColladaCompiler::Type_Skeleton, colladaProcessor);
	}

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

			SetWorkingDirectory();
			CreateDirectoryRecursive("int");
			ConsoleRig::Logging_Startup("log.cfg", "int/unittest.txt");
			auto console = std::make_unique<ConsoleRig::Console>();

			{
				auto asyncMan = RenderCore::Metal::CreateCompileAndAsyncManager();
				SetupColladaCompilers(*asyncMan);

				const char sampleAsset[] = "game/model/galleon/galleon.dae";
				using RenderCore::Assets::ModelScaffold;

				{
					using ::Assets::ResChar;
					ResChar intermediateFile[256];
					asyncMan->GetIntermediateStore().MakeIntermediateName(
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
					asyncMan->Update();
				}

				asyncMan.reset();
			}
		}

	};
}