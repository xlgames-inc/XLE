// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "ReusableDataFiles.h"
#include "../Assets/IFileSystem.h"
#include "../ShaderParser/ShaderInstantiation.h"
#include "../ShaderParser/DescriptorSetInstantiation.h"
#include "../ShaderParser/GraphSyntax.h"
#include "../ShaderParser/ShaderAnalysis.h"
#include "../RenderCore/Assets/PredefinedCBLayout.h"
#include "../Assets/AssetServices.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/OSFileSystem.h"
#include "../Assets/MountingTree.h"
#include "../Assets/MemoryFile.h"
#include "../Assets/AssetTraits.h"
#include "../Assets/AssetUtils.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/AttachablePtr.h"
#include "../Utility/Streams/FileUtils.h"
#include <regex>
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace UnitTests
{
	static std::unordered_map<std::string, ::Assets::Blob> s_utData {
		std::make_pair("example-perpixel.psh", ::Assets::AsBlob(s_examplePerPixelShaderFile)),
		std::make_pair("example.graph", ::Assets::AsBlob(s_exampleGraphFile)),
		std::make_pair("complicated.graph", ::Assets::AsBlob(s_complicatedGraphFile)),
		std::make_pair("internalShaderFile.psh", ::Assets::AsBlob(s_internalShaderFile)),
		std::make_pair("internalComplicatedGraph.graph", ::Assets::AsBlob(s_internalComplicatedGraph))
	};

	TEST_CLASS(NodeGraphInstantiationTests)
	{
	public:
		TEST_METHOD(InstantiateFromNodeGraphFile)
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

				::Assert::AreNotEqual(inst._sourceFragments.size(), (size_t)0);		// ensure that we at least got some output
				::Assert::AreEqual(inst._entryPoints.size(), (size_t)1);
				::Assert::AreEqual(inst._entryPoints[0]._name, std::string{"Bind_PerPixel"});
				::Assert::AreEqual(inst._entryPoints[0]._implementsName, std::string{"PerPixel"});
			}

			// Test with a slightly more complicated graph file
			try {
				ShaderSourceParser::InstantiationRequest instRequests[] {
					{ "ut-data/complicated.graph" }
				};

				ShaderSourceParser::GenerateFunctionOptions generateOptions;
				generateOptions._selectors.SetParameter(u("SIMPLE_BIND"), 1);
				generateOptions._filterWithSelectors = true;
				
				auto inst = ShaderSourceParser::InstantiateShader(
					MakeIteratorRange(instRequests), 
					generateOptions,
					RenderCore::ShaderLanguage::GLSL);

				::Assert::AreNotEqual(inst._sourceFragments.size(), (size_t)0);		// ensure that we at least got some output
				::Assert::AreEqual(inst._entryPoints.size(), (size_t)3);

				// We must have all of the following entry points, and "implements" behaviour
				const char* expectedEntryPoints[] = { "Internal_PerPixel", "Bind_PerPixel", "Bind_EarlyRejectionTest" };
				const char* expectedImplements[] = { "PerPixel", "EarlyRejectionTest" };

				for (const char* entryPoint:expectedEntryPoints) {
					auto i = std::find_if(
						inst._entryPoints.begin(), inst._entryPoints.end(),
						[entryPoint](const ShaderSourceParser::ShaderEntryPoint& e) {
							return e._name == entryPoint;
						});
					::Assert::IsTrue(i != inst._entryPoints.end());
				}

				for (const char* entryPoint:expectedImplements) {
					auto i = std::find_if(
						inst._entryPoints.begin(), inst._entryPoints.end(),
						[entryPoint](const ShaderSourceParser::ShaderEntryPoint& e) {
							return e._implementsName == entryPoint;
						});
					::Assert::IsTrue(i != inst._entryPoints.end());
				}

				// There should be a "MaterialUniforms" CB in the instantiation
				::Assert::AreEqual(inst._descriptorSet->_constantBuffers.size(), (size_t)2);
				auto i = std::find_if(
					inst._descriptorSet->_constantBuffers.begin(),
					inst._descriptorSet->_constantBuffers.end(),
					[](const ShaderSourceParser::MaterialDescriptorSet::ConstantBuffer& cb) {
						return cb._name == "MaterialUniforms";
					});
				::Assert::IsTrue(i != inst._descriptorSet->_constantBuffers.end());

				const char* expectedMaterialUniforms[] = { "DiffuseColor", "AlphaWeight" };
				for (auto u:expectedMaterialUniforms) {
					auto i2 = std::find_if(
						i->_layout->_elements.begin(),
						i->_layout->_elements.end(),
						[u](const RenderCore::Assets::PredefinedCBLayout::Element& ele) {
							return ele._name == u;
						});
					::Assert::IsTrue(i2 != i->_layout->_elements.end());
				}

			} catch (const std::exception& e) {
				std::stringstream str;
				str << "Failed in complicated graph test, with exception message: " << e.what() << std::endl;
				::Assert::Fail(ToString(str.str()).c_str());
			}
		}

		

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

		TEST_METHOD(TestExtractSelectorRelevance)
		{
			auto relevanceFromDirectAnalysis = AnalyzeSelectorsFromGraphFile("ut-data/complicated.graph");
			::Assert::AreNotEqual(relevanceFromDirectAnalysis._selectorRelevance.size(), (size_t)0);		// ensure that we got at least some

			// We're expecting the analysis to have found the link between SELECTOR_0 and SELECTOR_1
			::Assert::IsTrue(relevanceFromDirectAnalysis._selectorRelevance["SELECTOR_0"] == "defined(SELECTOR_1)");
			::Assert::IsTrue(relevanceFromDirectAnalysis._selectorRelevance["SELECTOR_1"] == "defined(SELECTOR_0)");
			::Assert::IsTrue(relevanceFromDirectAnalysis._selectorRelevance["ALPHA_TEST"] == "1");
			::Assert::IsTrue(relevanceFromDirectAnalysis._selectorRelevance["SIMPLE_BIND"] == "1");

			// ensure that there are no selectors that end in "_H" -- these indicate other defines in the shader, not
			// selectors specifically
			for (const auto& sel:relevanceFromDirectAnalysis._selectorRelevance)
				if (sel.first.size() > 2 && *(sel.first.end()-2) == '_' && *(sel.first.end()-1) == 'H')
					::Assert::Fail(ToString(std::string{"Found suspicious selector in selector relevance: "} + sel.first).c_str());

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

			::Assert::AreEqual(relevanceFromDirectAnalysis._selectorRelevance.size(), relevanceViaInstantiateShader.size());
			for (const auto&r:relevanceFromDirectAnalysis._selectorRelevance)
				::Assert::AreEqual(r.second, relevanceViaInstantiateShader[r.first]);
		}

		TEST_METHOD(TestGenerateTechniquePrebindData)
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

			::Assert::IsTrue(!inst._entryPoints.empty());
			::Assert::IsTrue(!inst._descriptorSet->_constantBuffers.empty());
			::Assert::IsTrue(!inst._selectorRelevance.empty());
			::Assert::IsTrue(!inst._rawShaderFileIncludes.empty());
			::Assert::IsTrue(!inst._depVals.empty());
		}

		static ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> _globalServices;
		static ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;

		TEST_CLASS_INITIALIZE(Startup)
		{
			UnitTest_SetWorkingDirectory();
			_globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
			::Assets::MainFileSystem::GetMountingTree()->Mount(u("xleres"), ::Assets::CreateFileSystem_OS(u("Game/xleres")));
			::Assets::MainFileSystem::GetMountingTree()->Mount(u("ut-data"), ::Assets::CreateFileSystem_Memory(s_utData));
			_assetServices = ConsoleRig::MakeAttachablePtr<::Assets::Services>(0);
		}

		TEST_CLASS_CLEANUP(Shutdown)
		{
			_assetServices.reset();
			_globalServices.reset();
		}
    };

	ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> NodeGraphInstantiationTests::_globalServices;
	ConsoleRig::AttachablePtr<::Assets::Services> NodeGraphInstantiationTests::_assetServices;
}


