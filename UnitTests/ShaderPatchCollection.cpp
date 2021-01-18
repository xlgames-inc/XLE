// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "ReusableDataFiles.h"
#include "../RenderCore/Assets/ShaderPatchCollection.h"
#include "../RenderCore/Assets/PredefinedCBLayout.h"
#include "../RenderCore/Techniques/CompiledShaderPatchCollection.h"
#include "../ShaderParser/ShaderInstantiation.h"
#include "../ShaderParser/DescriptorSetInstantiation.h"
#include "../Assets/AssetServices.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/OSFileSystem.h"
#include "../Assets/MountingTree.h"
#include "../Assets/MemoryFile.h"
#include "../Assets/DepVal.h"
#include "../ConsoleRig/Console.h"
#include "../OSServices/Log.h"
#include "../ConsoleRig/AttachablePtr.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/StreamTypes.h"
#include "../Utility/MemoryUtils.h"
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
	static const char s_exampleTechniqueFragments[] = R"--(
		~fragment
			ut-data/fragment.graph::Fragment
		~main
			ut-data/outergraph.graph::deferred_pass_main
			~perPixel
				ut-data/perpixel.graph::Default_PerPixel
		~coordsToColor
			ut-data/outergraph.graph::CoordsToColor
		)--";

	static const char s_fragmentsWithSelectors[] = R"--(
		~perPixel
			ut-data/shader_with_selectors_adapter.graph::Default_PerPixel
		)--";

	// The following data is mounted as virtual files in the folder "ut-data"
	static std::unordered_map<std::string, ::Assets::Blob> s_utData {
		std::make_pair(
			"fragment.graph",
			::Assets::AsBlob(R"--(
				float3 Fragment() 
				{
					return "float3(0,0,0)";
				}
			)--")),

		std::make_pair(
			"outergraph.graph",
			::Assets::AsBlob(R"--(
				import templates = "xleres/Nodes/Templates.sh"
				import texture = "xleres/Nodes/Texture.sh"
				import gbuffer = "xleres/gbuffer.h"

				auto deferred_pass_main(
					VSOUT geo,
					graph<templates::EarlyRejectionTest> rejectionTest,
					graph<templates::PerPixel> perPixel)
				{
					/*if (rejectionTest(geo:geo).result) {
						discard;
					}*/

					node perPixelEval = perPixel(geo:geo);
					return gbuffer::Encode(values:perPixelEval.result).result;
				}

				float3 CoordsToColor(float3 coords) implements templates::CoordinatesToColor
				{
					captures MaterialUniforms = (Texture2D DiffuseTexture, Texture2D ParametersTexture, float3 MaterialSpecular, float3 MaterialDiffuse);
					captures SecondUnifomBuffer = (Texture2D AnotherTexture, float4 MoreParameters);
					return texture::Sample(inputTexture:MaterialUniforms.DiffuseTexture, texCoord:coords).result;
				}
			)--")),

		std::make_pair(
			"perpixel.graph",
			::Assets::AsBlob(R"--(
				import templates = "xleres/Nodes/Templates.sh"
				import output = "xleres/Nodes/Output.sh"
				import materialParam = "xleres/Nodes/MaterialParam.sh"

				auto Default_PerPixel(VSOUT geo) implements templates::PerPixel
				{
					return output::Output_PerPixel(
						diffuseAlbedo:"float3(1,1,1)",
						worldSpaceNormal:"float3(0,1,0)",
						material:materialParam::CommonMaterialParam_Default().result,
						blendingAlpha:"1",
						normalMapAccuracy:"1",
						cookedAmbientOcclusion:"1",
						cookedLightOcclusion:"1",
						transmission:"float3(0,0,0)").result;
				}
			)--")),

		std::make_pair(
			"shader_with_selectors.pixel.hlsl",
			::Assets::AsBlob(R"--(
				#include "xleres/MainGeometry.h"
				#include "xleres/CommonResources.h"
				#include "xleres/gbuffer.h"
				#include "xleres/surface.h"
				#include "xleres/colour.h"

				Texture2D       Texture0		BIND_MAT_T0;		// Diffuse
				Texture2D       Texture1		BIND_MAT_T1;		// Normal/Gloss

				PerPixelMaterialParam DefaultMaterialValues()
				{
					PerPixelMaterialParam result;
					result.roughness = 0.5f;
					result.specular = 0.1f;
					result.metal = 0.0f;
					return result;
				}

				GBufferValues PerPixel(VSOUT geo)
				{
					GBufferValues result = GBufferValues_Default();
					result.material = DefaultMaterialValues();

					float4 diffuseTextureSample = 1.0.xxxx;
					#if (VSOUT_HAS_TEXCOORD>=1) && (RES_HAS_Texture0!=0)
						diffuseTextureSample = Texture0.Sample(MaybeAnisotropicSampler, geo.texCoord);
						result.diffuseAlbedo = diffuseTextureSample.rgb;
						result.blendingAlpha = diffuseTextureSample.a;
					#endif

					#if (VSOUT_HAS_TEXCOORD>=1) && (RES_HAS_Texture1!=0)
						float3 normalMapSample = SampleNormalMap(Texture1, DefaultSampler, true, geo.texCoord);
						result.worldSpaceNormal = normalMapSample; // TransformNormalMapToWorld(normalMapSample, geo);
					#elif (VSOUT_HAS_NORMAL==1)
						result.worldSpaceNormal = normalize(geo.normal);
					#endif

					return result;
				}
			)--")),

		std::make_pair(
			"shader_with_selectors_adapter.graph",
			::Assets::AsBlob(R"--(
				import templates = "xleres/Nodes/Templates.sh"
				import output = "xleres/Nodes/Output.sh"
				import materialParam = "xleres/Nodes/MaterialParam.sh"
				import shader = "ut-data/shader_with_selectors.pixel.hlsl"

				GBufferValues Default_PerPixel(VSOUT geo) implements templates::PerPixel
				{
					return shader::PerPixel(geo:geo).result;
				}
			)--")),

		std::make_pair("example-perpixel.pixel.hlsl", ::Assets::AsBlob(s_examplePerPixelShaderFile)),
		std::make_pair("example.graph", ::Assets::AsBlob(s_exampleGraphFile)),
		std::make_pair("complicated.graph", ::Assets::AsBlob(s_complicatedGraphFile)),
		std::make_pair("internalShaderFile.pixel.hlsl", ::Assets::AsBlob(s_internalShaderFile)),
		std::make_pair("internalComplicatedGraph.graph", ::Assets::AsBlob(s_internalComplicatedGraph))
	};

    TEST_CLASS(ShaderPatchCollection)
	{
	public:

		TEST_METHOD(DeserializeShaderPatchCollection)
		{
			// Normally a ShaderPatchCollection is deserialized from a material file
			// We'll test the serialization and deserialization code here, and ensure
			InputStreamFormatter<utf8> formattr { s_exampleTechniqueFragments };
			RenderCore::Assets::ShaderPatchCollection patchCollection(formattr);

			// Verify that a few things got deserialized correctly
			auto i = std::find_if(
				patchCollection.GetPatches().begin(),
				patchCollection.GetPatches().end(),
				[](const std::pair<std::string, ShaderSourceParser::InstantiationRequest>& r) {
					return r.first == "main";
				});
			Assert::IsTrue(i!=patchCollection.GetPatches().end());
			Assert::AreEqual(i->second._parameterBindings.size(), (size_t)1);
			Assert::AreEqual(i->second._parameterBindings.begin()->first, std::string("perPixel"));
			Assert::AreEqual(i->second._parameterBindings.begin()->second._archiveName, std::string("ut-data/perpixel.graph::Default_PerPixel"));

			// Write out the patch collection again
			MemoryOutputStream<uint8> strm;
			OutputStreamFormatter outFmttr(strm);
			SerializationOperator(outFmttr, patchCollection);

			// Now let's verify that we can deserialize in what we just wrote out
			auto& serializedStream = strm.GetBuffer();
			InputStreamFormatter<utf8> formattr2 { MemoryMappedInputStream { serializedStream.Begin(), serializedStream.End() } };
			RenderCore::Assets::ShaderPatchCollection patchCollection2(formattr2);

			// we should have the same contents in both patch collections
			Assert::IsTrue(patchCollection.GetPatches().size() == patchCollection2.GetPatches().size());
			Assert::IsTrue(patchCollection.GetHash() == patchCollection2.GetHash());
		}

		TEST_METHOD(CompileShaderGraph)
		{
			// Ensure that we can correctly compile the shader graph in the test data
			// (otherwise the following tests won't work)
			InputStreamFormatter<utf8> formattr { s_exampleTechniqueFragments };
			RenderCore::Assets::ShaderPatchCollection patchCollection(formattr);

			std::vector<ShaderSourceParser::InstantiationRequest> instantiations;
			for (const auto& p:patchCollection.GetPatches())
				instantiations.push_back(p.second);

			ShaderSourceParser::GenerateFunctionOptions generateOptions;
			auto instantiation = ShaderSourceParser::InstantiateShader(MakeIteratorRange(instantiations), generateOptions, RenderCore::ShaderLanguage::HLSL);
			::Assert::AreNotEqual(instantiation._sourceFragments.size(), (size_t)0);
		}

		TEST_METHOD(CompileShaderPatchCollection1)
		{
			InputStreamFormatter<utf8> formattr { s_exampleTechniqueFragments };
			RenderCore::Assets::ShaderPatchCollection patchCollection(formattr);

			using RenderCore::Techniques::CompiledShaderPatchCollection;
			CompiledShaderPatchCollection compiledCollection(patchCollection);

			// Check for some of the expected interface elements
			Assert::IsTrue(compiledCollection.GetInterface().HasPatchType(Hash64("CoordinatesToColor")));
			Assert::AreEqual(compiledCollection.GetInterface().GetMaterialDescriptorSet()->_constantBuffers.size(), (size_t)2);
			Assert::AreEqual(compiledCollection.GetInterface().GetMaterialDescriptorSet()->_constantBuffers[0]._name, std::string{"MaterialUniforms"});
			Assert::AreEqual(compiledCollection.GetInterface().GetMaterialDescriptorSet()->_constantBuffers[0]._layout->_elements.size(), (size_t)2);
			Assert::AreEqual(compiledCollection.GetInterface().GetMaterialDescriptorSet()->_constantBuffers[1]._name, std::string{"SecondUnifomBuffer"});
			Assert::AreEqual(compiledCollection.GetInterface().GetMaterialDescriptorSet()->_constantBuffers[1]._layout->_elements.size(), (size_t)1);
			Assert::AreEqual(compiledCollection.GetInterface().GetMaterialDescriptorSet()->_resources.size(), (size_t)3);
		}

		TEST_METHOD(CompileShaderPatchCollection2)
		{
			InputStreamFormatter<utf8> formattr { s_fragmentsWithSelectors };
			RenderCore::Assets::ShaderPatchCollection patchCollection(formattr);

			using RenderCore::Techniques::CompiledShaderPatchCollection;
			CompiledShaderPatchCollection compiledCollection(patchCollection);

			// Check for some of the recognized properties, in particular look for shader selectors
			// We're expecting the selectors "RES_HAS_Texture0" and "RES_HAS_Texture1"
			Assert::IsTrue(
				compiledCollection.GetInterface().GetSelectorRelevance().find("RES_HAS_Texture0")
				!=compiledCollection.GetInterface().GetSelectorRelevance().end());
			Assert::IsTrue(
				compiledCollection.GetInterface().GetSelectorRelevance().find("RES_HAS_Texture1")
				!=compiledCollection.GetInterface().GetSelectorRelevance().end());
		}

		static void FakeChange(const char* fn)
		{
			// Note - FakeFileChange() just happens to work, even for files in virtual filesystems
			// this is because even in these cases, we register the name in the main filesystem
			// monitor (we can monitor both existing and non-existing files). It won't work
			// correctly with aliasing, but it will at least catch the case where the names match
			// exactly.
			// It's a little dodgy; but we get by -- 
			auto split = MakeFileNameSplitter(fn);
			Utility::FakeFileChange(split.DriveAndPath().Cast<utf8>(), split.FileAndExtension().Cast<utf8>());
		}

		TEST_METHOD(TestCompiledShaderDependencyChecking)
		{
			// Let's make sure that the CompiledShaderPatchCollection recognizes when it has become 
			// out-of-date due to a source file change
			{
				const char* dependenciesToCheck[] = {
					"ut-data/shader_with_selectors_adapter.graph",		// root graph
					"xleres/Nodes/Templates.sh",						// import into root graph, used only by "implements" part of signature
					"ut-data/shader_with_selectors.pixel.sh",			// shader directly imported by root graph
					"xleres/TechniqueLibrary/Framework/gbuffer.hlsl",	// 1st level include from shader
					"xleres/TechniqueLibrary/Framework/Binding.hlsl"	// 2nd level include from shader
				};

				const char* nonDependencies[] = {
					"xleres/Nodes/Output.hlsl",				// imported but not used
					"ut-data/complicated.graph",			// not even referenced
					"shader_with_selectors_adapter.graph"	// incorrect path
				};

				InputStreamFormatter<utf8> formattr { s_fragmentsWithSelectors };
				RenderCore::Assets::ShaderPatchCollection patchCollection(formattr);

				for (unsigned c=0; c<std::max(dimof(dependenciesToCheck), dimof(nonDependencies)); ++c) {
					RenderCore::Techniques::CompiledShaderPatchCollection compiledCollection(patchCollection);
					Assert::AreEqual(compiledCollection._depVal->GetValidationIndex(), 0u);
					
					if (c < dimof(nonDependencies)) {
						FakeChange(nonDependencies[c]);
						Assert::AreEqual(compiledCollection._depVal->GetValidationIndex(), 0u);
					}

					if (c < dimof(dependenciesToCheck)) {
						FakeChange(dependenciesToCheck[c]);
						Assert::IsTrue(compiledCollection._depVal->GetValidationIndex() > 0u);
					}
				}
			}

			// Same thing again, this time with a different shader graph, with a slightly difference
			// construction process
			{
				const char* dependenciesToCheck[] = {
					"ut-data/complicated.graph",
					"ut-data/internalComplicatedGraph.graph",
					"ut-data/example.graph",
					"ut-data/example-perpixel.pixel.hlsl"
				};

				const char* nonDependencies[] = {
					"xleres/CommonResources.h",			// raw shaders will be imported, but will not show up as dep vals from InstantiateShader
					"xleres/MainGeometry.h"
				};

				for (unsigned c=0; c<std::max(dimof(dependenciesToCheck), dimof(nonDependencies)); ++c) {
					using namespace ShaderSourceParser;
					InstantiationRequest instRequest { "ut-data/complicated.graph" };
					GenerateFunctionOptions options;
					auto inst = ShaderSourceParser::InstantiateShader(
						MakeIteratorRange(&instRequest, &instRequest+1),
						options, RenderCore::ShaderLanguage::HLSL);

					// Create one dep val that references all of the children
					auto depVal = std::make_shared<::Assets::DependencyValidation>();
					for (const auto&d:inst._depVals)
						::Assets::RegisterAssetDependency(depVal, d);

					if (c < dimof(nonDependencies)) {
						FakeChange(nonDependencies[c]);
						Assert::AreEqual(depVal->GetValidationIndex(), 0u);
					}

					if (c < dimof(dependenciesToCheck)) {
						FakeChange(dependenciesToCheck[c]);
						Assert::IsTrue(depVal->GetValidationIndex() > 0u);
					}
				}
			}
		}
	
		static ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> _globalServices;
		static ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;

		TEST_CLASS_INITIALIZE(Startup)
		{
			UnitTest_SetWorkingDirectory();
			_globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
			::Assets::MainFileSystem::GetMountingTree()->Mount("xleres", ::Assets::CreateFileSystem_OS("Game/xleres"));
			::Assets::MainFileSystem::GetMountingTree()->Mount("ut-data", ::Assets::CreateFileSystem_Memory(s_utData));
			_assetServices = ConsoleRig::MakeAttachablePtr<::Assets::Services>(0);
		}

		TEST_CLASS_CLEANUP(Shutdown)
		{
			_assetServices.reset();
			_globalServices.reset();
		}
	};

	ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> ShaderPatchCollection::_globalServices;
	ConsoleRig::AttachablePtr<::Assets::Services> ShaderPatchCollection::_assetServices;
}
