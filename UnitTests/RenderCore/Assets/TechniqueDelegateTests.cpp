// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../ReusableDataFiles.h"
#include "../../UnitTestHelper.h"
#include "../../EmbeddedRes.h"
#include "../Metal/MetalTestHelper.h"
#include "../../../RenderCore/Assets/ShaderPatchCollection.h"
#include "../../../RenderCore/Assets/PredefinedCBLayout.h"
#include "../../../RenderCore/Assets/ShaderPatchCollection.h"
#include "../../../RenderCore/Assets/MaterialScaffold.h"
#include "../../../RenderCore/Techniques/CompiledShaderPatchCollection.h"
#include "../../../RenderCore/Techniques/TechniqueDelegates.h"
#include "../../../RenderCore/Techniques/PipelineAccelerator.h"
#include "../../../RenderCore/Format.h"
#include "../../../RenderCore/IDevice.h"
#include "../../../ShaderParser/ShaderInstantiation.h"
#include "../../../ShaderParser/DescriptorSetInstantiation.h"
#include "../../../ShaderParser/ShaderAnalysis.h"
#include "../../../ShaderParser/AutomaticSelectorFiltering.h"
#include "../../../Assets/IFileSystem.h"
#include "../../../Assets/OSFileSystem.h"
#include "../../../Assets/MountingTree.h"
#include "../../../Assets/MemoryFile.h"
#include "../../../Assets/AssetServices.h"
#include "../../../Assets/AssetTraits.h"
#include "../../../Assets/CompileAndAsyncManager.h"
#include "../../../Assets/Assets.h"
#include "../../../ConsoleRig/Console.h"
#include "../../../OSServices/Log.h"
#include "../../../OSServices/FileSystemMonitor.h"
#include "../../../ConsoleRig/AttachablePtr.h"
#include "../../../Utility/Streams/StreamFormatter.h"
#include "../../../Utility/Streams/OutputStreamFormatter.h"
#include "../../../Utility/Streams/StreamTypes.h"
#include "../../../Utility/MemoryUtils.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"

using namespace Catch::literals;
namespace UnitTests
{
	static std::unordered_map<std::string, ::Assets::Blob> s_techDelUTData {
		// ---------------------------- minimal_perpixel.graph ----------------------------
		std::make_pair(
			"minimal_perpixel.graph",
			::Assets::AsBlob(R"--(
				import templates = "xleres/Nodes/Templates.sh"
				import output = "xleres/Nodes/Output.sh"
				import materialParam = "xleres/Nodes/MaterialParam.sh"

				auto Minimal_PerPixel(VSOUT geo) implements templates::PerPixel
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

		// ---------------------------- spherical_prefix.hlsl ----------------------------
		std::make_pair(
			"spherical_prefix.hlsl",
			::Assets::AsBlob(R"--(
				#if !defined(SPHERICAL_PREFIX_HLSL)
				#define SPHERICAL_PREFIX_HLSL

				#include "xleres/TechniqueLibrary/Framework/MainGeometry.hlsl"
				float3 SphericalToNormal(float2 coord, float time);
				float4 SphericalToColor(float2 coord, float time);

				float3 SphericalToCartesian(float3 spherical)
				{
					float s0, c0, s1, c1;
					sincos(spherical.x, s0, c0);
					sincos(spherical.y, s1, c1);
					return float3(
						spherical.z * s0 * c1,
						spherical.z * s0 * s1,
						spherical.z * c0);
				}

				float3 CartesianToSpherical(float3 direction)
				{
					float3 result;
					float rDist = rsqrt(dot(direction, direction));
					result.x = acos(direction[2] * rDist);
					result.y = atan2(direction[1], direction[0]);
					result.z = 1.0f / rDist;
					return result;
				}

				float2 GetSphericalCoord(VSOUT geo)
				{
					#if VSOUT_HAS_TEXCOORD>=1
						return float2(geo.texCoord.x, geo.texCoord.y);
					#else
						return float2(0,0);
					#endif
				}

				#endif
			)--")),

		// ---------------------------- spherical.graph ----------------------------
		std::make_pair(
			"spherical.graph",
			::Assets::AsBlob(R"--(
				import templates = "xleres/Nodes/Templates.sh"
				import output = "xleres/Nodes/Output.sh"
				import basic = "xleres/Nodes/Basic.sh"
				import materialParam = "xleres/Nodes/MaterialParam.sh"
				import spherical_prefix = "spherical_prefix.hlsl"

				GBufferValues Spherical_PerPixel(
					VSOUT geo,
					graph<spherical_prefix::SphericalToColor> colorGenerator,
					graph<spherical_prefix::SphericalToNormal> normalGenerator) implements templates::PerPixel
				{
					// We've been given a couple of functions that can produce color and normal information
					// from spherical coordinates. Let's just use these to generate the output per-pixel
					// information
					captures SystemCaptures = (float Time = "0");
					node coord = spherical_prefix::GetSphericalCoord(geo:geo);
					node col = colorGenerator(coord:coord.result, time:SystemCaptures.Time);
					node norm = normalGenerator(coord:coord.result, time:SystemCaptures.Time);
					return output::Output_PerPixel(
						diffuseAlbedo:col.result,
						worldSpaceNormal:norm.result,
						material:materialParam::CommonMaterialParam_Default().result,
						blendingAlpha:"1",
						normalMapAccuracy:"1",
						cookedAmbientOcclusion:"1",
						cookedLightOcclusion:"1",
						transmission:"float3(0,0,0)").result;
				}
			)--")),

		// ---------------------------- spherical_generators.hlsl ----------------------------
		std::make_pair(
			"spherical_generators.hlsl",
			::Assets::AsBlob(R"--(
				#include "spherical_prefix.hlsl"

				float3 MainSphericalToNormal(float2 coord, float time)
				{
					return SphericalToCartesian(float3(coord.xy, 1));
				}

				float4 MainSphericalToColor(float2 coord, float time)
				{
					float t = 0.5f + 0.5f * sin(time + coord.x);
					return float4(t,t,t,1);
				}
			)--")),

		// ---------------------------- framework-entry.vertex.hlsl ----------------------------
		std::make_pair(
			"framework-entry.vertex.hlsl",
			::Assets::AsBlob(R"--(
				#include "xleres/TechniqueLibrary/Framework/SystemUniforms.hlsl"
				#include "xleres/TechniqueLibrary/Framework/MainGeometry.hlsl"
				#include "spherical_prefix.hlsl"

				VSOUT frameworkEntry(VSIN input)
				{
					VSOUT output;
					output.position = mul(SysUniform_GetWorldToClip(), float4(input.position,1));
					#if VSOUT_HAS_TEXCOORD>=1
						output.texCoord = CartesianToSpherical(input.position).xy;
					#endif
					return output;
				}				
			)--")),

		// ---------------------------- framework-entry.pixel.hlsl ----------------------------
		std::make_pair(
			"framework-entry.pixel.hlsl",
			::Assets::AsBlob(R"--(
				#include "xleres/TechniqueLibrary/Framework/MainGeometry.hlsl"
				#include "xleres/Nodes/Templates.sh"
				#include "spherical_prefix.hlsl"

				#if (VULKAN!=1)
					[earlydepthstencil]
				#endif
				float4 frameworkEntry(VSOUT geo) : SV_Target0
				{
					GBufferValues sample = PerPixel(geo);

					#if defined(OUTPUT_NORMAL)
						return float4(0.5.xxx + 0.5 * sample.normal, 1.0);
					#else
						return float4(sample.diffuseAlbedo, sample.blendingAlpha);
					#endif
				}
			)--")),
	};

	class UnitTestTechniqueDelegate : public RenderCore::Techniques::ITechniqueDelegate
	{
	public:
		::Assets::FuturePtr<GraphicsPipelineDesc> Resolve(
			const RenderCore::Techniques::CompiledShaderPatchCollection::Interface& shaderPatches,
			const RenderCore::Assets::RenderStateSet& input) override
		{
			const uint64_t perPixelPatchName = Hash64("PerPixel");

			auto perPixelPatch = std::find_if(
				shaderPatches.GetPatches().begin(), shaderPatches.GetPatches().end(),
				[perPixelPatchName](const auto& c) { return c._implementsHash == perPixelPatchName; });
			if (perPixelPatch == shaderPatches.GetPatches().end())
				Throw(std::runtime_error("A patch implementing the PerPixel interface must be implemented"));

			auto desc = std::make_shared<GraphicsPipelineDesc>();
			desc->_shaders[(unsigned)RenderCore::ShaderStage::Vertex]._initializer = "ut-data/framework-entry.vertex.hlsl:frameworkEntry";
			desc->_shaders[(unsigned)RenderCore::ShaderStage::Pixel]._initializer = "ut-data/framework-entry.pixel.hlsl:frameworkEntry";

			for (unsigned c=0; c<dimof(desc->_shaders); ++c) {
				if (desc->_shaders[c]._initializer.empty()) continue;
				auto future = ::Assets::MakeAsset<ShaderSourceParser::SelectorFilteringRules>(MakeFileNameSplitter(desc->_shaders[c]._initializer).AllExceptParameters());
				future->StallWhilePending();
				desc->_shaders[c]._automaticFiltering = future->Actualize();
			}

			desc->_blend.push_back(RenderCore::AttachmentBlendDesc{});

			desc->_patchExpansions.push_back(perPixelPatchName);
			desc->_depVal = std::make_shared<::Assets::DependencyValidation>();

			auto result = std::make_shared<::Assets::AssetFuture<GraphicsPipelineDesc>>("pipeline-for-unit-test");
			result->SetAsset(std::move(desc), {});
			return result;
		}
	};

	static RenderCore::InputElementDesc inputElePC[] = {
		RenderCore::InputElementDesc { "position", 0, RenderCore::Format::R32G32B32A32_FLOAT },
		RenderCore::InputElementDesc { "color", 0, RenderCore::Format::R8G8B8A8_UNORM }
	};

	TEST_CASE( "TechniqueDelegates-LegacyTechnique", "[shader_parser]" )
	{
		auto globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
		auto xlresmnt = ::Assets::MainFileSystem::GetMountingTree()->Mount("xleres", UnitTests::CreateEmbeddedResFileSystem());
		auto mnt1 = ::Assets::MainFileSystem::GetMountingTree()->Mount("ut-data", ::Assets::CreateFileSystem_Memory(s_techDelUTData, s_defaultFilenameRules, ::Assets::FileSystemMemoryFlags::EnableChangeMonitoring));

		auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
		auto filteringRegistration = ShaderSourceParser::RegisterShaderSelectorFilteringCompiler(compilers);

		SECTION("Retrieve minimal legacy technique")
		{
			static const char simplePatchCollectionFragments[] = R"--(
			main=~
				ut-data/minimal_perpixel.graph::Minimal_PerPixel
			)--";
			InputStreamFormatter<utf8> formattr { MakeStringSection(simplePatchCollectionFragments) };
			RenderCore::Assets::ShaderPatchCollection patchCollection(formattr, ::Assets::DirectorySearchRules{}, nullptr);
			auto compiledPatchCollection = std::make_shared<RenderCore::Techniques::CompiledShaderPatchCollection>(patchCollection);

			{
				auto delegate = RenderCore::Techniques::CreateTechniqueDelegateLegacy(
					0,
					RenderCore::AttachmentBlendDesc{},
					RenderCore::RasterizationDesc{},
					RenderCore::DepthStencilDesc{});

				RenderCore::Assets::RenderStateSet stateSet;

				auto pipelineDescFuture = delegate->Resolve(
					compiledPatchCollection->GetInterface(),
					stateSet);
				pipelineDescFuture->StallWhilePending();
				auto pipelineDesc = pipelineDescFuture->TryActualize();
				if (!pipelineDesc) {
					auto log = ::Assets::AsString(pipelineDescFuture->GetActualizationLog());
					std::cout << "Failed to get pipeline from technique delegate; with exception message: " << std::endl << log << std::endl;
				}
				REQUIRE(pipelineDesc);
				REQUIRE(!pipelineDesc->_shaders[0]._initializer.empty());
			}
		}

		SECTION("Retrieve graph based technique")
		{
			static const char sphericalCollectionFragments[] = R"--(
			main=~
				ut-data/spherical.graph::Spherical_PerPixel
				colorGenerator=~
					ut-data/spherical_generators.hlsl::MainSphericalToColor
				normalGenerator=~
					ut-data/spherical_generators.hlsl::MainSphericalToNormal
			)--";
			InputStreamFormatter<utf8> formattr { MakeStringSection(sphericalCollectionFragments) };
			RenderCore::Assets::ShaderPatchCollection patchCollection(formattr, ::Assets::DirectorySearchRules{}, nullptr);
			auto compiledPatchCollection = std::make_shared<RenderCore::Techniques::CompiledShaderPatchCollection>(patchCollection);

			using namespace RenderCore;
			auto testHelper = MakeTestHelper();
			auto threadContext = testHelper->_device->GetImmediateContext();
			auto targetDesc = CreateDesc(
				BindFlag::RenderTarget | BindFlag::TransferSrc | BindFlag::TransferDst, 0, GPUAccess::Write,
				TextureDesc::Plain2D(1024, 1024, Format::R8G8B8A8_UNORM),
				"temporary-out");

			auto shaderCompilerRegistration = RenderCore::RegisterShaderCompiler(testHelper->_shaderSource, compilers);
			auto shaderCompiler2Registration = RenderCore::Techniques::RegisterInstantiateShaderGraphCompiler(testHelper->_shaderSource, compilers);
			auto pipelinePool = Techniques::CreatePipelineAcceleratorPool(
				testHelper->_device,
				testHelper->_pipelineLayout);

			UnitTestFBHelper fbHelper(*testHelper->_device, *threadContext, targetDesc);

			// The final pipeline we want will be determined by two main things:
			//		Properties related to how and when we're rendering the object (ie, render pass, technique delegate)
			//		Properties realted to the object itself (ie, material settings, geometry input)
			// We abstract this into two configuration objects:
			//		SequencerConfig
			//		PipelineAccelerator
			// For each pair, we can generate a pipeline
			// The two change independantly, and we need to mix and match them frequently (eg, rendering the same
			// object in multiple different ways). The PipelineAcceleratorPool takes care of managing that -- including
			// try to maintain in memory only those pipelines that are required
			auto techniqueDelegate = std::make_shared<UnitTestTechniqueDelegate>();
			auto cfg = pipelinePool->CreateSequencerConfig(
				techniqueDelegate,
				ParameterBox{}, 
				fbHelper.GetDesc(), 0);

			auto pipelineAccelerator = pipelinePool->CreatePipelineAccelerator(
				compiledPatchCollection,
				ParameterBox{},
				inputElePC, Topology::TriangleList,
				RenderCore::Assets::RenderStateSet{});
			
			auto pipelineFuture = pipelinePool->GetPipeline(*pipelineAccelerator, *cfg);
			REQUIRE(pipelineFuture != nullptr);
			pipelineFuture->StallWhilePending();
			if (pipelineFuture->GetAssetState() == ::Assets::AssetState::Invalid) {
				Log(Error) << ::Assets::AsString(pipelineFuture->GetActualizationLog()) << std::endl;
			}
			auto pipeline = pipelineFuture->Actualize();
			REQUIRE(pipeline != nullptr);

			compilers.DeregisterCompiler(shaderCompiler2Registration._registrationId);
			compilers.DeregisterCompiler(shaderCompilerRegistration._registrationId);
		}

		compilers.DeregisterCompiler(filteringRegistration._registrationId);
		::Assets::MainFileSystem::GetMountingTree()->Unmount(mnt1);
		::Assets::MainFileSystem::GetMountingTree()->Unmount(xlresmnt);
	}


}

