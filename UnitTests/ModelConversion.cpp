// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "../RenderCore/Metal/Shader.h"		// for CreateCompileAndAsyncManager
#include "../FixedFunctionModel/ModelRunTime.h"
#include "../RenderCore/Assets/ModelImmutableData.h"
#include "../RenderCore/Assets/ModelScaffold.h"
#include "../RenderCore/Assets/Services.h"
#include "../Assets/IntermediateAssets.h"
#include "../Assets/Assets.h"
#include "../Assets/AssetServices.h"
#include "../Assets/ICompileOperation.h"
#include "../Assets/CompileAndAsyncManager.h"
#include "../Assets/IFileSystem.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../ConsoleRig/AttachableLibrary.h"
#include "../Utility/SystemUtils.h"
#include "../Utility/StringFormat.h"
#include "../Utility/TimeUtils.h"
#include "../Utility/Threading/ThreadingUtils.h"
#include "../Utility/Conversion.h"
#include "../Utility/Streams/StreamDOM.h"
#include "../Utility/Streams/XmlStreamFormatter.h"
#include "../Core/SelectConfiguration.h"
#include <CppUnitTest.h>

#include "../Core/WinAPI/IncludeWindows.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
    __declspec(noinline) static void StreamDomPerformanceTest(const utf8*start, const utf8*end, uint64 iterationCount)
    {
        for (uint64 c=0; c<iterationCount; ++c) {
            XmlInputStreamFormatter<utf8> formatter(MemoryMappedInputStream(start, end));
            StreamDOM<XmlInputStreamFormatter<utf8>> doc(formatter);
            (void) doc;
        }
    }

    size_t GlobalHack = 0;

    __declspec(noinline) static void StrLenPerformanceTest(const utf8*start, uint64 iterationCount)
    {
        for (uint64 c=0; c<iterationCount; ++c) {
            GlobalHack += std::strlen((const char*)start);
        }
    }

    __declspec(noinline) static void XlStrLenPerformanceTest(const utf8*start, uint64 iterationCount)
    {
        for (uint64 c=0; c<iterationCount; ++c) {
            GlobalHack += XlGlyphCount(start);
        }
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

            UnitTest_SetWorkingDirectory();
            ConsoleRig::GlobalServices services(GetStartupConfig());

			{
                auto aservices = std::make_shared<::Assets::Services>(0);
				auto& asyncMan = aservices->GetAsyncMan();
                auto raservices = std::make_shared<RenderCore::Assets::Services>(nullptr);
                raservices->InitModelCompilers();

				const char sampleAsset[] = "game/model/galleon/galleon.dae";
				using RenderCore::Assets::ModelScaffold;

				{
					using ::Assets::ResChar;
					ResChar intermediateFile[MaxPath];
					asyncMan.GetIntermediateStore()->MakeIntermediateName(
						intermediateFile,
						(StringMeld<MaxPath, ResChar>() << sampleAsset << "-skin").AsStringSection());
					XlDeleteFile((utf8*)intermediateFile);
				}

				auto startTime = Millisecond_Now();
				for (;;) {
					TRY {
						auto& scaffold = Assets::GetAssetComp<ModelScaffold>(sampleAsset);
						Assert::AreEqual(scaffold.ImmutableData()._geoCount, size_t(8));
						break;
					} 
                    CATCH(const Assets::Exceptions::PendingAsset&) {}
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

        TEST_METHOD(ColladaParsePerformance)
        {
            UnitTest_SetWorkingDirectory();
            ConsoleRig::GlobalServices services(GetStartupConfig());

            {
                #if defined(_DEBUG)
                    ConsoleRig::AttachableLibrary lib("../Finals_Debug32/ColladaConversion.dll");
                #else
                    ConsoleRig::AttachableLibrary lib("../Finals_Release32/ColladaConversion.dll");
                #endif
                std::string attachError;
				lib.TryAttach(attachError);
                auto createScaffold = lib.GetFunction<Assets::CreateCompileOperationFn*>(
                    "?CreateCompileOperation@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@VICompileOperation@ColladaConversion@RenderCore@@@std@@QEBD@Z");

                const char sampleAsset[] = "game/testmodels/ironman/ironman.dae";
                // const char sampleAsset[] = "game/model/spaceship/mccv/mccv.dae";
                // const char sampleAsset[] = "game/model/Towns/BarrackHouseA/BarrackHouseA.dae";

                WIN32_FILE_ATTRIBUTE_DATA fileAttrib;
                GetFileAttributesExW(
                    (const wchar_t*)Conversion::Convert<std::basic_string<utf16>>(std::string(sampleAsset)).c_str(), 
                    GetFileExInfoStandard, &fileAttrib);
                uint64 bytes = uint64(fileAttrib.nFileSizeHigh)<<32 | fileAttrib.nFileSizeLow;

                auto start = __rdtsc();
                const auto iterationCount = (uint64)std::max(100ull, 10000000000ull / uint64(bytes));
                for (uint64 c=0; c<iterationCount; ++c) {
                    auto mdl = (*createScaffold)(sampleAsset);
                    mdl.reset();    // (incur the cost of deletion here)
                }
                auto end = __rdtsc();

                Log(Warning) << "New path: " << (end-start) / iterationCount << " cycles per collada file." << std::endl;
                Log(Warning) << "That's about " << (end-start) / (uint64(bytes)*iterationCount) << " cycles per byte in the input file." << std::endl;
            }
        }

        TEST_METHOD(StreamDOMParsePerformance)
        {
            UnitTest_SetWorkingDirectory();
            ConsoleRig::GlobalServices services(GetStartupConfig());

            {
                const char sampleAsset[] = "game/testmodels/ironman/ironman.dae";
                // const char sampleAsset[] = "game/model/galleon/galleon.dae";
                // const char sampleAsset[] = "game/model/spaceship/mccv/mccv.dae";

                    // (automatically appends null terminator)
                size_t size = 0;
                auto chars = ::Assets::TryLoadFileAsMemoryBlock(sampleAsset, &size);

                const uint64 iterationCount = (uint64)std::max(100ull, 10000000000ull / uint64(size));
                
                auto startXML = __rdtsc();
                StreamDomPerformanceTest(chars.get(), PtrAdd(chars.get(), size), iterationCount);
                auto middleSample = __rdtsc();
                StrLenPerformanceTest(chars.get(), iterationCount);
                auto middleSample2 = __rdtsc();
                XlStrLenPerformanceTest(chars.get(), iterationCount);
                auto endStrLen = __rdtsc();

                Log(Warning) << "Ran " << iterationCount << " iterations." << std::endl;
                Log(Warning) << "XML parser: " << (middleSample-startXML) / (iterationCount*uint64(size)) << " cycles per byte. (" << (middleSample-startXML) << ") total" << std::endl;
                Log(Warning) << "std::strlen: " << (middleSample2-middleSample) / (iterationCount*uint64(size)) << " cycles per byte. (" << (middleSample2-middleSample) << ") total" << std::endl;
                Log(Warning) << "XlGlyphCount: " << (endStrLen-middleSample2) / (iterationCount*uint64(size)) << " cycles per byte. (" << (endStrLen-middleSample2) << ") total" << std::endl;
            }
        }

        TEST_METHOD(ColladaScaffold)
		{
            UnitTest_SetWorkingDirectory();
            ConsoleRig::GlobalServices services(GetStartupConfig());

            // Load all of the .dae files from a hierarchy of folders, look for errors
            // and exceptions from the scaffold parsing. This is intended to be used with
            // large test suites of .dae files (like the collada implementor's test kit)
            auto inputFiles = RawFS::FindFilesHierarchical("../../work/ColladaImplementors/StandardDataSets", "*.dae", RawFS::FindFilesFilter::File);

            {
                #if defined(_DEBUG)
                    ConsoleRig::AttachableLibrary lib("../Finals_Debug32/ColladaConversion.dll");
                #else
                    ConsoleRig::AttachableLibrary lib("../Finals_Profile32/ColladaConversion.dll");
                #endif
                std::string attachError;
				lib.TryAttach(attachError);
                auto createScaffold = lib.GetFunction<Assets::CreateCompileOperationFn*>(
					"?CreateCompileOperation@ColladaConversion@RenderCore@@YA?AV?$shared_ptr@VICompileOperation@ColladaConversion@RenderCore@@@std@@QEBD@Z");
                
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
                    Log(Error) << "Failure in file: " << f << std::endl;
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
				std::string attachError;
                lib.TryAttach(attachError);

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
                        auto chunks = (*newSerializeSkin)(*scaffold, "name");
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