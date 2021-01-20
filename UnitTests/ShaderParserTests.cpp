// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ReusableDataFiles.h"
#include "../Assets/IFileSystem.h"
#include "../ShaderParser/ShaderSignatureParser.h"
#include "../ShaderParser/NodeGraphSignature.h"
#include "../ShaderParser/ShaderAnalysis.h"
#include "../Assets/AssetServices.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/OSFileSystem.h"
#include "../Assets/MountingTree.h"
#include "../Assets/MemoryFile.h"
#include "../Assets/AssetTraits.h"
#include "../ConsoleRig/Console.h"
#include "../OSServices/Log.h"
#include "../ConsoleRig/AttachablePtr.h"
#include "../OSServices/RawFS.h"
#include "../Utility/Streams/PathUtils.h"
#include "../Utility/Conversion.h"
#include <cctype>
#include <sstream>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Assets/LocalCompiledShaderSource.h"
#include "../ShaderParser/ShaderInstantiation.h"
#include "../ShaderParser/GraphSyntax.h"

using namespace Catch::literals;
namespace UnitTests
{
	// The following data is mounted as virtual files in the folder "ut-data"
	static std::unordered_map<std::string, ::Assets::Blob> s_utData {
		std::make_pair(
			"outershader.hlsl",
			::Assets::AsBlob(R"--(
#include "ut-data/innershader.hlsl"
static const int NonPreprocessorLine0 = 0;
#include "innershader.hlsl"					
				static const int NonPreprocessorLine1 = 0; /*
					block comment
				*/ #inClUdE "innershader.hlsl"
				#    INCLUDE "middleshader.hlsl"
	/*  */		#	INCLUDE		<middleshader.hlsl>			   
		/**/	#  include<ut-data/innershader.hlsl>			   random trailing stuff
			)--")),

		std::make_pair(
			"innershader.hlsl",
			::Assets::AsBlob(R"--(static const int ThisIsFromTheInnerShader = 0;)--")),

		std::make_pair(
			"middleshader.hlsl",
			::Assets::AsBlob(R"--(
				static const int ThisIsFromTheMiddleShader0 = 0;
				#include "innershader.hlsl"
				static const int ThisIsFromTheMiddleShader1 = 0;
			)--")),


		std::make_pair(
			"outershader-noincludes.hlsl",
			::Assets::AsBlob(R"--(
#include__ "ut-data/innershader0.hlsl"
/*#include "innershader1.hlsl"*/					
				/*
					block comment
				#inClUdE "innershader2.hlsl" */ 
				// #    INCLUDE "innershader3.hlsl"
				// extended line comment \
	/*  */		#	INCLUDE		<innershader4.hlsl>			   
		// /**/	#  include<ut-data/innershader5.hlsl>			   random trailing stuff
			)--")),

		std::make_pair(
			"example.tech",
			::Assets::AsBlob(R"--(
				~NoPatches
					~Inherit; xleres/Techniques/Illum.tech:Deferred

				~PerPixel
					~Inherit; xleres/Techniques/Illum.tech:Deferred
					PixelShader=xleres/TechniqueLibrary/Standard/deferred.pixel.hlsl:frameworkEntry
			)--")),

		std::make_pair("example-perpixel.pixel.hlsl", ::Assets::AsBlob(s_examplePerPixelShaderFile)),
		std::make_pair("example.graph", ::Assets::AsBlob(s_exampleGraphFile))
	};

	static void FindShaderSources(
		std::vector<std::string>& dest,
		const ::Assets::FileSystemWalker& walker)
	{
		for (auto file = walker.begin_files(); file != walker.end_files(); ++file) {
			auto naturalName = Conversion::Convert<std::string>(file.Desc()._naturalName);
			auto splitter = MakeFileNameSplitter(naturalName);
			if (	XlEqStringI(splitter.Extension(), "h")
				||	XlEqStringI(splitter.Extension(), "sh")
				||	(splitter.Extension().size() == 3 && std::tolower(splitter.Extension()[1]) == 's' && std::tolower(splitter.Extension()[1]) == 'h')) {
				dest.push_back(naturalName);
			}
		}
		for (auto dir = walker.begin_directories(); dir != walker.end_directories(); ++dir) {
			if (dir.Name().empty() || dir.Name()[0] == '.') continue;
			FindShaderSources(dest, *dir);
		}
	}

	class LocalHelper
	{
	public:
		// ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> _globalServices;
		// ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;
		::Assets::MountingTree::MountID _utDataMount;

		LocalHelper()
		{
			// UnitTest_SetWorkingDirectory();
			// _globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
			// ::Assets::MainFileSystem::GetMountingTree()->Mount("xleres", ::Assets::CreateFileSystem_OS("Game/xleres"));
			_utDataMount = ::Assets::MainFileSystem::GetMountingTree()->Mount("ut-data", ::Assets::CreateFileSystem_Memory(s_utData));
			// _assetServices = ConsoleRig::MakeAttachablePtr<::Assets::Services>(0);
		}

		~LocalHelper()
		{
			::Assets::MainFileSystem::GetMountingTree()->Unmount(_utDataMount);
			// _assetServices.reset();
			// _globalServices.reset();
		}
	};

	TEST_CASE( "ShaderParser-ParseAllSystemShaderSources", "[shader_parser]" )
	{
			// Search for all of the shader sources in the xleres directory
		std::vector<std::string> inputFiles;
		FindShaderSources(inputFiles, ::Assets::MainFileSystem::BeginWalk("xleres"));
		for (auto& i:inputFiles) {
			auto memBlock = ::Assets::TryLoadFileAsBlob(MakeStringSection(i));

			const char* flgId = "FunctionLinkingGraph";
			if (XlFindString(MakeStringSection((const char*)AsPointer(memBlock->begin()), (const char*)AsPointer(memBlock->end())), flgId))
				continue;

			try
			{
				auto signature = ShaderSourceParser::ParseHLSL(
					MakeStringSection((const char*)AsPointer(memBlock->begin()), (const char*)AsPointer(memBlock->end())));

				(void)signature;
			} catch (const std::exception& e) {
				Log(Warning) << "Got parsing exception in (" << i << ")" << std::endl << e.what() << std::endl;
			}
		}
	}

	TEST_CASE( "ShaderParser-ExpandOutIncludes", "[shader_parser]" )
	{
		LocalHelper localHelper;

		{
			auto outerShader = ::Assets::TryLoadFileAsBlob("ut-data/outershader.hlsl");
			REQUIRE(outerShader != nullptr);
			REQUIRE(outerShader->size() != (size_t)0);
			auto expanded = ShaderSourceParser::ExpandIncludes(
				StringSection<char>{(char*)AsPointer(outerShader->begin()), (char*)AsPointer(outerShader->end())},
				"ut-data/outershader.hlsl",
				::Assets::DefaultDirectorySearchRules("ut-data/outershader.hlsl"));
			REQUIRE(expanded._lineMarkers.size() == (size_t)16);
			REQUIRE(expanded._processedSourceLineCount == 21u);
		}

		{
			// In the following test, none of the #include statements should actually be followed
			// We will probably get an exception if they are (inside the files don't exist).
			// But we can also check the output
			auto outerShader = ::Assets::TryLoadFileAsBlob("ut-data/outershader-noincludes.hlsl");
			REQUIRE(outerShader != nullptr);
			REQUIRE(outerShader->size() != (size_t)0);
			auto expanded = ShaderSourceParser::ExpandIncludes(
				StringSection<char>{(char*)AsPointer(outerShader->begin()), (char*)AsPointer(outerShader->end())},
				"ut-data/outershader.hlsl",
				::Assets::DefaultDirectorySearchRules("ut-data/outershader-noincludes.hlsl"));
			REQUIRE(expanded._lineMarkers.size() == (size_t)1);	// one straight block of text, no includes are followed
		}
	}

	TEST_CASE( "ShaderParser-TestAnalyzeSelectors", "[shader_parser]" )
	{
		const char exampleShader[] = R"--(
			#if defined(SOME_SELECTOR) || defined(ANOTHER_SELECTOR)
				#if defined(THIRD_SELECTOR)
				#endif
			#endif

			#if defined(SELECTOR_0) || defined(SELECTOR_1)
				#define SECONDARY_DEFINE
			#endif

			#if defined(SECONDARY_DEFINE) && defined(DEPENDENT_SELECTOR)
			#endif

			#if defined(SELECTOR_3) && defined(SELECTOR_4)
				#define SECONDARY_DEFINE_2 1
			#endif

			#if (SECONDARY_DEFINE_2 == 1) && defined(DEPENDENT_SELECTOR_2)
			#endif

		)--";
		auto analysis = ShaderSourceParser::AnalyzeSelectors(exampleShader);
		REQUIRE(analysis._selectorRelevance["SOME_SELECTOR"] == std::string{"1"});
		REQUIRE(analysis._selectorRelevance["ANOTHER_SELECTOR"] == std::string{"1"});
		REQUIRE(analysis._selectorRelevance["THIRD_SELECTOR"] == std::string{"defined(SOME_SELECTOR) || defined(ANOTHER_SELECTOR)"});

		// Check some filtering conditions
		{
			auto filter0 = ShaderSourceParser::FilterSelectors(
				ParameterBox {
					std::make_pair("THIRD_SELECTOR", "1"),
				},
				analysis._selectorRelevance);
			REQUIRE(filter0.GetCount() == (size_t)0);
		}

		{
			auto filter1 = ShaderSourceParser::FilterSelectors(
				ParameterBox {
					std::make_pair("SOME_SELECTOR", "1"),
					std::make_pair("THIRD_SELECTOR", "1"),
				},
				analysis._selectorRelevance);
			REQUIRE(filter1.GetCount() == (size_t)2);
		}
	}

	TEST_CASE( "ShaderParser-BindShaderToTechnique", "[shader_parser]" )
	{
		LocalHelper localHelper;

		// Given some shader (either straight-up shader code, or something generated from a shader graph)
		// bind it to a technique, and produce both the final shader text and required meta-data

		auto tech = ::Assets::AutoConstructAsset<RenderCore::Techniques::TechniqueSetFile>("ut-data/example.tech");
		const auto* entry = tech->FindEntry(Hash64("PerPixel"));
		REQUIRE(entry != nullptr);

		const std::string exampleGraphFN = "ut-data/example.graph";
		ShaderSourceParser::InstantiationRequest instRequests[] {
			{ exampleGraphFN }
		};
		
		using namespace ShaderSourceParser;
		ShaderSourceParser::GenerateFunctionOptions generateOptions;
		auto inst = InstantiateShader(MakeIteratorRange(instRequests), generateOptions, RenderCore::ShaderLanguage::GLSL);

		auto i = std::find_if(
			inst._entryPoints.begin(), inst._entryPoints.end(),
			[](const ShaderEntryPoint& ep) {
				return ep._name == "Bind_PerPixel" && ep._implementsName == "PerPixel";
			});
		REQUIRE(i != inst._entryPoints.end());

		// Expand shader and extract the selector relevance table information

		{
			std::stringstream str;
			for (const auto&f:inst._sourceFragments)
				str << f << std::endl;

			auto expanded = ExpandIncludes(str.str(), exampleGraphFN, ::Assets::DefaultDirectorySearchRules(exampleGraphFN));
			auto relevanceTable = AnalyzeSelectors(expanded._processedSource);
			(void)relevanceTable;
		}
	}

}

