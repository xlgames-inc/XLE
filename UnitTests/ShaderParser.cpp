// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "../Assets/IFileSystem.h"
#include "../ShaderParser/ShaderSignatureParser.h"
#include "../ShaderParser/NodeGraphSignature.h"
#include "../ShaderParser/ShaderAnalysis.h"
#include "../Assets/AssetServices.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/OSFileSystem.h"
#include "../Assets/MountingTree.h"
#include "../Assets/MemoryFile.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/AttachablePtr.h"
#include "../Utility/Streams/FileUtils.h"
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
	// The following data is mounted as virtual files in the folder "ut-data"
	static std::unordered_map<std::string, ::Assets::Blob> s_utData {
		std::make_pair(
			"outershader.sh",
			::Assets::AsBlob(R"--(
#include "ut-data/innershader.sh"
static const int NonPreprocessorLine0 = 0;
#include "innershader.sh"					
				static const int NonPreprocessorLine1 = 0; /*
					block comment
				*/ #inClUdE "innershader.sh"
				#    INCLUDE "middleshader.sh"
	/*  */		#	INCLUDE		<middleshader.sh>			   
		/**/	#  include<ut-data/innershader.sh>			   random trailing stuff
			)--")),

		std::make_pair(
			"innershader.sh",
			::Assets::AsBlob(R"--(static const int ThisIsFromTheInnerShader = 0;)--")),

		std::make_pair(
			"middleshader.sh",
			::Assets::AsBlob(R"--(
				static const int ThisIsFromTheMiddleShader0 = 0;
				#include "innershader.sh"
				static const int ThisIsFromTheMiddleShader1 = 0;
			)--")),


		std::make_pair(
			"outershader-noincludes.sh",
			::Assets::AsBlob(R"--(
#include__ "ut-data/innershader0.sh"
/*#include "innershader1.sh"*/					
				/*
					block comment
				#inClUdE "innershader2.sh" */ 
				// #    INCLUDE "innershader3.sh"
				// extended line comment \
	/*  */		#	INCLUDE		<innershader4.sh>			   
		// /**/	#  include<ut-data/innershader5.sh>			   random trailing stuff
			)--"))
	};

    TEST_CLASS(ShaderParser)
	{
	public:
		TEST_METHOD(ParserAllShaderSources)
		{
            UnitTest_SetWorkingDirectory();
            ConsoleRig::GlobalServices services(GetStartupConfig());

                // Search for all of the shader sources in the xleres directory
            auto inputFiles = RawFS::FindFilesHierarchical("xleres", "*.h", RawFS::FindFilesFilter::File);
            auto inputFiles1 = RawFS::FindFilesHierarchical("xleres", "*.sh", RawFS::FindFilesFilter::File);
            auto inputFiles2 = RawFS::FindFilesHierarchical("xleres", "*.?sh", RawFS::FindFilesFilter::File);

            inputFiles.insert(inputFiles.end(), inputFiles1.begin(), inputFiles1.end());
            inputFiles.insert(inputFiles.end(), inputFiles2.begin(), inputFiles2.end());

                // Now try to parse each one using the shader parser...
                // Look for parsing errors and unsupported syntax

            for (auto& i:inputFiles) {
                size_t blockSize = 0;
                auto memBlock = ::Assets::TryLoadFileAsMemoryBlock(MakeStringSection(i), &blockSize);

                const char* flgId = "FunctionLinkingGraph";
                if (blockSize > XlStringLen(flgId) && 
                    XlEqString(MakeStringSection((const char*)memBlock.get(), (const char*)&memBlock[XlStringLen(flgId)]), flgId))
                    continue;

                auto signature = ShaderSourceParser::ParseHLSL(
                    MakeStringSection((const char*)memBlock.get(), (const char*)PtrAdd(memBlock.get(), blockSize)));

                (void)signature;
            }
        }

		TEST_METHOD(ExpandOutIncludes)
		{
			{
				auto outerShader = ::Assets::TryLoadFileAsBlob("ut-data/outershader.sh");
				Assert::IsNotNull(outerShader.get());
				Assert::AreNotEqual(outerShader->size(), (size_t)0);
				auto expanded = ShaderSourceParser::ExpandIncludes(
					StringSection<char>{(char*)AsPointer(outerShader->begin()), (char*)AsPointer(outerShader->end())},
					"ut-data/outershader.sh",
					::Assets::DefaultDirectorySearchRules("ut-data/outershader.sh"));
				Assert::AreEqual(expanded._lineMarkers.size(), (size_t)16);
				Assert::AreEqual(expanded._processedSourceLineCount, 21u);
			}

			{
				// In the following test, none of the #include statements should actually be followed
				// We will probably get an exception if they are (inside the files don't exist).
				// But we can also check the output
				auto outerShader = ::Assets::TryLoadFileAsBlob("ut-data/outershader-noincludes.sh");
				Assert::IsNotNull(outerShader.get());
				Assert::AreNotEqual(outerShader->size(), (size_t)0);
				auto expanded = ShaderSourceParser::ExpandIncludes(
					StringSection<char>{(char*)AsPointer(outerShader->begin()), (char*)AsPointer(outerShader->end())},
					"ut-data/outershader.sh",
					::Assets::DefaultDirectorySearchRules("ut-data/outershader-noincludes.sh"));
				Assert::AreEqual(expanded._lineMarkers.size(), (size_t)1);	// one straight block of text, no includes are followed
			}
		}


		static ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> _globalServices;
		static ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;

		TEST_CLASS_INITIALIZE(Startup)
		{
			UnitTest_SetWorkingDirectory();
			_globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
			::Assets::MainFileSystem::GetMountingTree()->Mount(u("xleres"), ::Assets::CreateFileSystem_OS(u("Game/xleres")));
			::Assets::MainFileSystem::GetMountingTree()->Mount(u("ut-data"), ::Assets::CreateFileSystem_Memory(s_utData));
		}

		TEST_CLASS_CLEANUP(Shutdown)
		{
			_globalServices.reset();
		}
    };

	ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> ShaderParser::_globalServices;
	ConsoleRig::AttachablePtr<::Assets::Services> ShaderParser::_assetServices;
}

