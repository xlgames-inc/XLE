// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../UnitTestHelper.h"
#include "../../EmbeddedRes.h"
#include "../ReusableDataFiles.h"
#include "../../../Assets/IFileSystem.h"
#include "../../../ShaderParser/ShaderInstantiation.h"
#include "../../../ShaderParser/DescriptorSetInstantiation.h"
#include "../../../ShaderParser/GraphSyntax.h"
#include "../../../ShaderParser/ShaderAnalysis.h"
#include "../../../RenderCore/Assets/PredefinedCBLayout.h"
#include "../../../Assets/AssetServices.h"
#include "../../../Assets/IFileSystem.h"
#include "../../../Assets/OSFileSystem.h"
#include "../../../Assets/MountingTree.h"
#include "../../../Assets/MemoryFile.h"
#include "../../../Assets/AssetTraits.h"
#include "../../../Assets/AssetUtils.h"
#include "../../../ConsoleRig/Console.h"
#include "../../../OSServices/Log.h"
#include "../../../ConsoleRig/AttachablePtr.h"
#include "../../../OSServices/RawFS.h"
#include <regex>
#include <sstream>
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"

static const char* s_entryPointsInRawShader = R"--(
	#include "xleres/TechniqueLibrary/Framework/MainGeometry.h"
	#include "xleres/TechniqueLibrary/Framework/gbuffer.h"
	#include "xleres/Nodes/Templates.sh"

	GBufferValues PerPixel(VSOUT geo)
	{
		return GBufferValues_Default();
	}

	bool EarlyRejectionTest(VSOUT geo)
	{
		return false;
	}
)--";

using namespace Catch::literals;
namespace UnitTests
{
	static std::unordered_map<std::string, ::Assets::Blob> s_utData {
		std::make_pair("example-perpixel.pixel.hlsl", ::Assets::AsBlob(s_examplePerPixelShaderFile)),
		std::make_pair("example.graph", ::Assets::AsBlob(s_exampleGraphFile)),
		std::make_pair("complicated.graph", ::Assets::AsBlob(s_complicatedGraphFile)),
		std::make_pair("internalShaderFile.pixel.hlsl", ::Assets::AsBlob(s_internalShaderFile)),
		std::make_pair("internalComplicatedGraph.graph", ::Assets::AsBlob(s_internalComplicatedGraph)),
		std::make_pair("entryPointsInRawShader.pixel.hlsl", ::Assets::AsBlob(s_entryPointsInRawShader))
	};

	static void ExtractSelectorRelevance(
		std::unordered_map<std::string, std::string>& result,
		const GraphLanguage::GraphSyntaxFile& graphFile)
	{
		for (const auto&sg:graphFile._subGraphs)
			ShaderSourceParser::Internal::ExtractSelectorRelevance(result, sg.second._graph);
	}

	static ShaderSourceParser::ShaderSelectorAnalysis AnalyzeSelectorsFromGraphFile(StringSection<> fileName)
	{
		// Extract the shader relevance information from the given inputs
		GraphLanguage::GraphSyntaxFile graphFile;
		{
			size_t fileSize = 0;
			auto blob = ::Assets::TryLoadFileAsMemoryBlock_TolerateSharingErrors(fileName, &fileSize);
			graphFile = GraphLanguage::ParseGraphSyntax(MakeStringSection((char*)blob.get(), (char*)PtrAdd(blob.get(), fileSize)));
		}
		
		auto searchRules = ::Assets::DefaultDirectorySearchRules(fileName);
		
		ShaderSourceParser::ShaderSelectorAnalysis result;
		// extract relrevance from any imports
		// imports can be graph syntax files or simple shaders
		for (const auto& i:graphFile._imports) {
			char resolved[MaxPath];
			searchRules.ResolveFile(resolved, MakeStringSection(i.second));
			if (!resolved[0])
				Throw(::Exceptions::BasicLabel("Could not resolve imported file (%s) when extracting shader relevance", i.second.c_str()));

			size_t fileSize = 0;
			auto blob = ::Assets::TryLoadFileAsMemoryBlock_TolerateSharingErrors(resolved, &fileSize);

			if (XlEqStringI(MakeFileNameSplitter(resolved).Extension(), "graph")) {
				// consider this a graph file, and extract relevance from the graph file recursively
				auto selectors = AnalyzeSelectorsFromGraphFile(resolved);
				ShaderSourceParser::Utility::MergeRelevance(result._selectorRelevance, selectors._selectorRelevance);
			} else {
				// consider this a shader file, and extract 
				auto expanded = ShaderSourceParser::ExpandIncludes(
					MakeStringSection((char*)blob.get(), (char*)PtrAdd(blob.get(), fileSize)),
					resolved, 
					::Assets::DefaultDirectorySearchRules(resolved));
				auto selectors = ShaderSourceParser::AnalyzeSelectors(expanded._processedSource);
				ShaderSourceParser::Utility::MergeRelevance(result._selectorRelevance, selectors._selectorRelevance);
			}
		}

		// extract relevance from the graph file itself
		ExtractSelectorRelevance(result._selectorRelevance, graphFile);
		return result;
	}

	TEST_CASE( "NodeGraphInstantiationTests", "[shader_parser]" )
	{
		auto globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());

		auto xlresmnt = ::Assets::MainFileSystem::GetMountingTree()->Mount("xleres", UnitTests::CreateEmbeddedResFileSystem());
		auto mnt1 = ::Assets::MainFileSystem::GetMountingTree()->Mount("ut-data", ::Assets::CreateFileSystem_Memory(s_utData));

		SECTION( "NodeGraphInstantiationTests")
		{
			// Test with a simple example graph file
			{
				ShaderSourceParser::InstantiationRequest instRequests[] {
					{ "ut-data/example.graph" }
				};

				ShaderSourceParser::GenerateFunctionOptions generateOptions;
				auto inst = ShaderSourceParser::InstantiateShader(
					MakeIteratorRange(instRequests), 
					generateOptions,
					RenderCore::ShaderLanguage::GLSL);

				REQUIRE(inst._sourceFragments.size() != (size_t)0);		// ensure that we at least got some output
				REQUIRE(inst._entryPoints.size() == (size_t)1);
				REQUIRE(inst._entryPoints[0]._name == std::string{"Bind_PerPixel"});
				REQUIRE(inst._entryPoints[0]._implementsName == std::string{"PerPixel"});
			}

			// Test with a slightly more complicated graph file
			try {
				ShaderSourceParser::InstantiationRequest instRequests[] {
					{ "ut-data/complicated.graph" }
				};

				ShaderSourceParser::GenerateFunctionOptions generateOptions;
				generateOptions._selectors.SetParameter("SIMPLE_BIND", 1);
				generateOptions._filterWithSelectors = true;
				
				auto inst = ShaderSourceParser::InstantiateShader(
					MakeIteratorRange(instRequests), 
					generateOptions,
					RenderCore::ShaderLanguage::GLSL);

				REQUIRE(inst._sourceFragments.size() != (size_t)0);		// ensure that we at least got some output
				REQUIRE(inst._entryPoints.size() == (size_t)3);

				// We must have all of the following entry points, and "implements" behaviour
				const char* expectedEntryPoints[] = { "Internal_PerPixel", "Bind2_PerPixel", "Bind_EarlyRejectionTest" };
				const char* expectedImplements[] = { "PerPixel", "EarlyRejectionTest" };

				for (const char* entryPoint:expectedEntryPoints) {
					auto i = std::find_if(
						inst._entryPoints.begin(), inst._entryPoints.end(),
						[entryPoint](const ShaderSourceParser::ShaderEntryPoint& e) {
							return e._name == entryPoint;
						});
					REQUIRE(i != inst._entryPoints.end());
				}

				for (const char* entryPoint:expectedImplements) {
					auto i = std::find_if(
						inst._entryPoints.begin(), inst._entryPoints.end(),
						[entryPoint](const ShaderSourceParser::ShaderEntryPoint& e) {
							return e._implementsName == entryPoint;
						});
					REQUIRE(i != inst._entryPoints.end());
				}

				// There should be a "MaterialUniforms" CB in the instantiation
				REQUIRE(inst._descriptorSet->_constantBuffers.size() == (size_t)2);
				auto i = std::find_if(
					inst._descriptorSet->_constantBuffers.begin(),
					inst._descriptorSet->_constantBuffers.end(),
					[](const RenderCore::Assets::PredefinedDescriptorSetLayout::ConstantBuffer& cb) {
						return cb._name == "MaterialUniforms";
					});
				REQUIRE(i != inst._descriptorSet->_constantBuffers.end());

				const char* expectedMaterialUniforms[] = { "DiffuseColor", "AlphaWeight" };
				for (auto u:expectedMaterialUniforms) {
					auto i2 = std::find_if(
						i->_layout->_elements.begin(),
						i->_layout->_elements.end(),
						[u](const RenderCore::Assets::PredefinedCBLayout::Element& ele) {
							return ele._name == u;
						});
					REQUIRE(i2 != i->_layout->_elements.end());
				}

			} catch (const std::exception& e) {
				std::stringstream str;
				str << "Failed in complicated graph test, with exception message: " << e.what() << std::endl;
				FAIL(ToString(str.str()).c_str());
			}
		}

		SECTION( "InstantiateFromRawShader" )
		{
			ShaderSourceParser::InstantiationRequest instRequests[] {
				{ "ut-data/entryPointsInRawShader.pixel.hlsl" }
			};

			ShaderSourceParser::GenerateFunctionOptions generateOptions;
			auto inst = ShaderSourceParser::InstantiateShader(
				MakeIteratorRange(instRequests), 
				generateOptions,
				RenderCore::ShaderLanguage::GLSL);

			// We're existing 2 entry points
			// The "name" and "implementsName" should be the same in both cases, since
			// we don't distinguish between these for raw shader files
			REQUIRE(inst._entryPoints.size() == (size_t)2);
			auto perPixel = std::find_if(
				inst._entryPoints.begin(), inst._entryPoints.end(),
				[](const ShaderSourceParser::ShaderEntryPoint& s) { return s._name == "PerPixel"; });
			REQUIRE(perPixel != inst._entryPoints.end());
			REQUIRE(perPixel->_implementsName == std::string{"PerPixel"});

			auto earlyRejectionTest = std::find_if(
				inst._entryPoints.begin(), inst._entryPoints.end(),
				[](const ShaderSourceParser::ShaderEntryPoint& s) { return s._name == "EarlyRejectionTest"; });
			REQUIRE(earlyRejectionTest != inst._entryPoints.end());
			REQUIRE(earlyRejectionTest->_implementsName == std::string{"EarlyRejectionTest"});
		}

		SECTION( "TestExtractSelectorRelevance" )
		{
			auto relevanceFromDirectAnalysis = AnalyzeSelectorsFromGraphFile("ut-data/complicated.graph");
			REQUIRE(relevanceFromDirectAnalysis._selectorRelevance.size() != (size_t)0);		// ensure that we got at least some

			// We're expecting the analysis to have found the link between SELECTOR_0 and SELECTOR_1
			REQUIRE(relevanceFromDirectAnalysis._selectorRelevance["SELECTOR_0"] == "defined(SELECTOR_1)");
			REQUIRE(relevanceFromDirectAnalysis._selectorRelevance["SELECTOR_1"] == "defined(SELECTOR_0)");
			REQUIRE(relevanceFromDirectAnalysis._selectorRelevance["ALPHA_TEST"] == "1");
			REQUIRE(relevanceFromDirectAnalysis._selectorRelevance["SIMPLE_BIND"] == "1");

			// ensure that there are no selectors that end in "_H" -- these indicate other defines in the shader, not
			// selectors specifically
			for (const auto& sel:relevanceFromDirectAnalysis._selectorRelevance)
				if (sel.first.size() > 2 && *(sel.first.end()-2) == '_' && *(sel.first.end()-1) == 'H')
					FAIL(ToString(std::string{"Found suspicious selector in selector relevance: "} + sel.first).c_str());

			// Now do something similar using InstantiateShader, and ensure that we get the same result
			// as we did in the case above
			// This test might not be 100%, because the order in which we interpret the raw shader #includes
			// could impact how the relevance expression is finally built up

			using namespace ShaderSourceParser;
			InstantiationRequest instRequest { "ut-data/complicated.graph" };
			GenerateFunctionOptions options;
			auto inst = ShaderSourceParser::InstantiateShader(
				MakeIteratorRange(&instRequest, &instRequest+1),
				options, RenderCore::ShaderLanguage::HLSL);
			auto relevanceViaInstantiateShader = inst._selectorRelevance;
			ShaderSourceParser::Utility::MergeRelevanceFromShaderFiles(relevanceViaInstantiateShader, inst._rawShaderFileIncludes);

			REQUIRE(relevanceFromDirectAnalysis._selectorRelevance.size() != relevanceViaInstantiateShader.size());
			for (const auto&r:relevanceFromDirectAnalysis._selectorRelevance)
				REQUIRE(r.second != relevanceViaInstantiateShader[r.first]);
		}

		SECTION( "TestGenerateTechniquePrebindData" )
		{
			using namespace ShaderSourceParser;
			InstantiationRequest instRequest { "ut-data/complicated.graph" };
			GenerateFunctionOptions options;
			auto inst = ShaderSourceParser::InstantiateShader(
				MakeIteratorRange(&instRequest, &instRequest+1),
				options, RenderCore::ShaderLanguage::HLSL);

			//
			// We need these things for the pre-technique binding
			//		1. list of entry points (+ "implements" information)
			//		2. material descriptor set
			//		3. selector relevance table
			//		4. raw shader includes (in order to generate shader related relevance information)
			//		5. dependency validations
			//
			// Let's just ensure that we have something for all of these
			//

			REQUIRE(!inst._entryPoints.empty());
			REQUIRE(!inst._descriptorSet->_constantBuffers.empty());
			REQUIRE(!inst._selectorRelevance.empty());
			REQUIRE(!inst._rawShaderFileIncludes.empty());
			REQUIRE(!inst._depVals.empty());
		}

		::Assets::MainFileSystem::GetMountingTree()->Unmount(mnt1);
		::Assets::MainFileSystem::GetMountingTree()->Unmount(xlresmnt);
	}
}
