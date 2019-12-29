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

static const char* s_complicatedGraphFile = R"--(
	import simple_example = "example.graph";
	import simple_example_dupe = "ut-data/example.graph";
	import example_perpixel = "example-perpixel.psh";
	import templates = "xleres/nodes/templates.sh";
	import conditions = "xleres/nodes/conditions.sh";
	import internalComplicatedGraph = "internalComplicatedGraph.graph";

	GBufferValues Internal_PerPixel(VSOutput geo)
	{
		return example_perpixel::PerPixel(geo:geo).result;
	}

	GBufferValues Bind_PerPixel(VSOutput geo) implements templates::PerPixel
	{
		captures MaterialUniforms = ( float3 DiffuseColor );
		captures AnotherCaptures = ( float SecondaryCaptures );
		if "defined(SIMPLE_BIND)" return simple_example::Bind_PerPixel(geo:geo).result;
		if "!defined(SIMPLE_BIND)" return Internal_PerPixel(geo:geo).result;
	}

	bool Bind_EarlyRejectionTest(VSOutput geo) implements templates::EarlyRejectionTest
	{
		captures MaterialUniforms = ( float AlphaWeight = "0.5" );
		if "defined(ALPHA_TEST)" return conditions::LessThan(lhs:MaterialUniforms.AlphaWeight, rhs:"0.5").result;
		return internalComplicatedGraph::Bind_EarlyRejectionTest(geo:geo).result;
	}
)--";

static const char* s_internalShaderFile = R"--(
	#include "xleres/MainGeometry.h"

	bool ShouldBeRejected(VSOutput geo, float threshold)
	{
		#if defined(SELECTOR_0) && defined(SELECTOR_1)
			return true;
		#else
			return false;
		#endif
	}
)--";

static const char* s_internalComplicatedGraph = R"--(
	import internal_shader_file = "internalShaderFile.psh";
	
	bool Bind_EarlyRejectionTest(VSOutput geo) implements templates::EarlyRejectionTest
	{
		captures MaterialUniforms = ( float AnotherHiddenUniform = "0.5" );
		return internal_shader_file::ShouldBeRejected(geo:geo, threshold:MaterialUniforms.AnotherHiddenUniform).result;
	}
)--";

namespace UnitTests
{

	static std::unordered_map<std::string, ::Assets::Blob> s_utData {
		std::make_pair("example-perpixel.psh", ::Assets::AsBlob(s_examplePerPixelShaderFile)),
		std::make_pair("example.graph", ::Assets::AsBlob(s_exampleGraphFile)),
		std::make_pair("complicated.graph", ::Assets::AsBlob(s_complicatedGraphFile)),
		std::make_pair("internalShaderFile.psh", ::Assets::AsBlob(s_internalShaderFile)),
		std::make_pair("internalComplicatedGraph.graph", ::Assets::AsBlob(s_internalComplicatedGraph))
	};

	static const std::string s_alwaysRelevant { "1" };

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

		static void MergeRelevance(
			std::unordered_map<std::string, std::string>& result,
			const std::unordered_map<std::string, std::string>& src)
		{
			for (const auto&s:src) {
				auto i = result.find(s.first);
				if (i != result.end()) {
					if (i->second == s_alwaysRelevant) {
						// already always relevant; just continue
					} else if (s.second == s_alwaysRelevant) {
						// becoming always relevance, no merging necessary
						result.insert(s);
					} else if (i->second == s.second) {
						// the conditions are just the same; just continue
					} else {
						std::string merged = std::string("(") + i->second + ") || (" + s.second + ")";
						result.insert(std::make_pair(s.first, merged));
					}
				} else {
					result.insert(s);
				}
			}
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
					MergeRelevance(result._selectorRelevance, selectors._selectorRelevance);
				} else {
					// consider this a shader file, and extract 
					auto expanded = ShaderSourceParser::ExpandIncludes(
						MakeStringSection((char*)blob.get(), (char*)PtrAdd(blob.get(), fileSize)),
						resolved, 
						::Assets::DefaultDirectorySearchRules(resolved));
					auto selectors = ShaderSourceParser::AnalyzeSelectors(expanded._processedSource);
					MergeRelevance(result._selectorRelevance, selectors._selectorRelevance);
				}
			}

			// extract relevance from the graph file itself
			ExtractSelectorRelevance(result._selectorRelevance, graphFile);
			return result;
		}

		TEST_METHOD(TestExtractSelectorRelevance)
		{
			auto selector = AnalyzeSelectorsFromGraphFile("ut-data/complicated.graph");
			::Assert::AreNotEqual(selector._selectorRelevance.size(), (size_t)0);		// ensure that we got at least some

			// We're expecting the analysis to have found the link between SELECTOR_0 and SELECTOR_1
			::Assert::IsTrue(selector._selectorRelevance["SELECTOR_0"] == "defined(SELECTOR_1)");
			::Assert::IsTrue(selector._selectorRelevance["SELECTOR_1"] == "defined(SELECTOR_0)");
			::Assert::IsTrue(selector._selectorRelevance["ALPHA_TEST"] == "1");
			::Assert::IsTrue(selector._selectorRelevance["SIMPLE_BIND"] == "1");

			// ensure that there are no selectors that end in "_H" -- these indicate other defines in the shader, not
			// selectors specifically
			for (const auto& sel:selector._selectorRelevance)
				if (sel.first.size() > 2 && *(sel.first.end()-2) == '_' && *(sel.first.end()-1) == 'H')
					::Assert::Fail(ToString(std::string{"Found suspicious selector in selector relevance: "} + sel.first).c_str());
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


