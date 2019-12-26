// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
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
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
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
					VSOutput geo,
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

				auto Default_PerPixel(VSOutput geo) implements templates::PerPixel
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
			"shader_with_selectors.psh",
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

				GBufferValues PerPixel(VSOutput geo)
				{
					GBufferValues result = GBufferValues_Default();
					result.material = DefaultMaterialValues();

					float4 diffuseTextureSample = 1.0.xxxx;
					#if (OUTPUT_TEXCOORD==1) && (RES_HAS_Texture0!=0)
						diffuseTextureSample = Texture0.Sample(MaybeAnisotropicSampler, geo.texCoord);
						result.diffuseAlbedo = diffuseTextureSample.rgb;
						result.blendingAlpha = diffuseTextureSample.a;
					#endif

					#if (OUTPUT_TEXCOORD==1) && (RES_HAS_Texture1!=0)
						float3 normalMapSample = SampleNormalMap(Texture1, DefaultSampler, true, geo.texCoord);
						result.worldSpaceNormal = normalMapSample; // TransformNormalMapToWorld(normalMapSample, geo);
					#elif (OUTPUT_NORMAL==1)
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
				import shader = "ut-data/shader_with_selectors.psh"

				auto Default_PerPixel(VSOutput geo) implements templates::PerPixel
				{
					return shader::PerPixel(geo:geo).result;
				}
			)--")),
	};

    TEST_CLASS(ShaderPatchCollection)
	{
	public:

		TEST_METHOD(DeserializeShaderPatchCollection)
		{
			// Normally a ShaderPatchCollection is deserialized from a material file
			// We'll test the serialization and deserialization code here, and ensure
			InputStreamFormatter<utf8> formattr { s_exampleTechniqueFragments };
			auto patchCollection = RenderCore::Assets::DeserializeShaderPatchCollection(formattr);

			// Verify that a few things got deserialized correctly
			Assert::AreEqual(patchCollection.GetPatches()[1].first, std::string("main"));
			Assert::AreEqual(patchCollection.GetPatches()[1].second._parameterBindings.size(), (size_t)1);
			Assert::AreEqual(patchCollection.GetPatches()[1].second._parameterBindings.begin()->first, std::string("perPixel"));
			Assert::AreEqual(patchCollection.GetPatches()[1].second._parameterBindings.begin()->second._archiveName, std::string("xleres/Techniques/Graph/Object_Default.graph::Default_PerPixel"));

			// Write out the patch collection again
			MemoryOutputStream<uint8> strm;
			OutputStreamFormatter outFmttr(strm);
			patchCollection.Serialize(outFmttr);

			// Now let's verify that we can deserialize in what we just wrote out
			auto& serializedStream = strm.GetBuffer();
			InputStreamFormatter<utf8> formattr2 { MemoryMappedInputStream { serializedStream.Begin(), serializedStream.End() } };
			auto patchCollection2 = RenderCore::Assets::DeserializeShaderPatchCollection(formattr2);

			// we should have the same contents in both patch collections
			Assert::IsTrue(patchCollection.GetPatches().size() == patchCollection2.GetPatches().size());
			Assert::IsTrue(patchCollection.GetHash() == patchCollection2.GetHash());
		}

		TEST_METHOD(CompileShaderGraph)
		{
			// Ensure that we can correctly compile the shader graph in the test data
			// (otherwise the following tests won't work)
			InputStreamFormatter<utf8> formattr { s_exampleTechniqueFragments };
			auto patchCollection = RenderCore::Assets::DeserializeShaderPatchCollection(formattr);

			std::vector<ShaderSourceParser::InstantiationRequest_ArchiveName> instantiations;
			for (const auto& p:patchCollection.GetPatches())
				instantiations.push_back(p.second);

			auto instantiation = ShaderSourceParser::InstantiateShader(MakeIteratorRange(instantiations));
			::Assert::AreNotEqual(instantiation._sourceFragments.size(), (size_t)0);
		}

		TEST_METHOD(CompileShaderPatchCollection1)
		{
			InputStreamFormatter<utf8> formattr { s_exampleTechniqueFragments };
			auto patchCollection = RenderCore::Assets::DeserializeShaderPatchCollection(formattr);

			using RenderCore::Techniques::CompiledShaderPatchCollection;
			CompiledShaderPatchCollection compiledCollection(patchCollection);

			// Check for some of the expected interface elements
			Assert::IsTrue(compiledCollection.GetInterface().HasPatchType(Hash64("CoordinatesToColor")));
			Assert::AreEqual((unsigned)compiledCollection._illumDelegate._type, (unsigned)CompiledShaderPatchCollection::IllumDelegateAttachment::IllumType::NoPerPixel);
			Assert::AreEqual(compiledCollection.GetInterface().GetMaterialDescriptorSet()->_constantBuffers.size(), (size_t)2);
			Assert::AreEqual(compiledCollection.GetInterface().GetMaterialDescriptorSet()->_constantBuffers[0]._name, std::string{"MaterialUniforms"});
			Assert::AreEqual(compiledCollection.GetInterface().GetMaterialDescriptorSet()->_constantBuffers[0]._layout->_elements.size(), (size_t)2);
			Assert::AreEqual(compiledCollection.GetInterface().GetMaterialDescriptorSet()->_constantBuffers[1]._name, std::string{"SecondUnifomBuffer"});
			Assert::AreEqual(compiledCollection.GetInterface().GetMaterialDescriptorSet()->_constantBuffers[1]._layout->_elements.size(), (size_t)1);
			Assert::AreEqual(compiledCollection.GetInterface().GetMaterialDescriptorSet()->_srvs.size(), (size_t)3);
		}

		TEST_METHOD(CompileShaderPatchCollection2)
		{
			InputStreamFormatter<utf8> formattr { s_fragmentsWithSelectors };
			auto patchCollection = RenderCore::Assets::DeserializeShaderPatchCollection(formattr);

			using RenderCore::Techniques::CompiledShaderPatchCollection;
			CompiledShaderPatchCollection compiledCollection(patchCollection);

			// Check for some of the recognized properties, in particular look for shader selectors
			// We're expecting the selectors "RES_HAS_Texture0" and "RES_HAS_Texture1"
			Assert::AreEqual((unsigned)compiledCollection._illumDelegate._type, (unsigned)CompiledShaderPatchCollection::IllumDelegateAttachment::IllumType::PerPixel);
			Assert::AreEqual(compiledCollection.GetInterface().GetSelectors().size(), (size_t)2);
			Assert::IsTrue(
				std::find(compiledCollection.GetInterface().GetSelectors().begin(), compiledCollection.GetInterface().GetSelectors().end(), std::string{"RES_HAS_Texture0"})
				!=compiledCollection.GetInterface().GetSelectors().end());
			Assert::IsTrue(
				std::find(compiledCollection.GetInterface().GetSelectors().begin(), compiledCollection.GetInterface().GetSelectors().end(), std::string{"RES_HAS_Texture1"})
				!=compiledCollection.GetInterface().GetSelectors().end());
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
			_globalServices.reset();
		}
	};

	ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> ShaderPatchCollection::_globalServices;
	ConsoleRig::AttachablePtr<::Assets::Services> ShaderPatchCollection::_assetServices;
}
