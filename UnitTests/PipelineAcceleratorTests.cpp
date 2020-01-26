// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "ReusableDataFiles.h"
#include "MetalUnitTest.h"
#include "../RenderCore/Techniques/PipelineAccelerator.h"
#include "../RenderCore/Techniques/DrawableDelegates.h"
#include "../RenderCore/Techniques/TechniqueDelegates.h"
#include "../RenderCore/Techniques/CompiledShaderPatchCollection.h"
#include "../RenderCore/Techniques/DescriptorSetAccelerator.h"
#include "../RenderCore/Techniques/Services.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/PipelineLayout.h"
#include "../RenderCore/Assets/Services.h"
#include "../RenderCore/Assets/MaterialScaffold.h"
#include "../RenderCore/ResourceDesc.h"
#include "../RenderCore/FrameBufferDesc.h"
#include "../BufferUploads/BufferUploads_Manager.h"
#include "../Assets/AssetServices.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/OSFileSystem.h"
#include "../Assets/MountingTree.h"
#include "../Assets/MemoryFile.h"
#include "../Assets/DepVal.h"
#include "../Assets/AssetTraits.h"
#include "../Assets/AssetSetManager.h"
#include "../Assets/Assets.h"
#include "../ConsoleRig/Console.h"
#include "../ConsoleRig/Log.h"
#include "../ConsoleRig/AttachablePtr.h"
#include <map>

#if !defined(XC_TEST_ADAPTER)
    #include <CppUnitTest.h>
    using namespace Microsoft::VisualStudio::CppUnitTestFramework;
	#define ThrowsException ExpectException<const std::exception&>
#endif

static const char s_exampleTechniqueFragments[] = R"--(
	~main
		ut-data/complicated.graph::Bind2_PerPixel
)--";

static const char* s_colorFromSelectorShaderFile = R"--(
	#include "xleres/MainGeometry.h"
	#include "xleres/gbuffer.h"
	#include "xleres/Nodes/Templates.sh"

	GBufferValues PerPixel(VSOutput geo)
	{
		GBufferValues result = GBufferValues_Default();
		#if (OUTPUT_TEXCOORD==1)
			#if defined(COLOR_RED)
				result.diffuseAlbedo = float3(1,0,0);
			#elif defined(COLOR_GREEN)
				result.diffuseAlbedo = float3(0,1,0);
			#else
				#error Intentional compile error
			#endif
		#endif
		result.material.roughness = 1.0;		// (since this is written to SV_Target0.a, ensure it's set to 1)
		return result;
	}
)--";

static const char s_techniqueForColorFromSelector[] = R"--(
	~main
		ut-data/colorFromSelector.psh::PerPixel
)--";

static const char* s_basicTexturingGraph = R"--(
	import templates = "xleres/nodes/templates.sh"
	import output = "xleres/nodes/output.sh"
	import texture = "xleres/nodes/texture.sh"
	import basic = "xleres/nodes/basic.sh"
	import materialParam = "xleres/nodes/materialparam.sh"

	GBufferValues Bind_PerPixel(VSOutput geo) implements templates::PerPixel
	{
		captures MaterialUniforms = ( float3 Multiplier = "{1,1,1}", Texture2D BoundTexture );
		node loadFromTexture = texture::LoadAbsolute(
			inputTexture:MaterialUniforms.BoundTexture, 
			pixelCoords:texture::GetPixelCoords(geo:geo).result);
		node multiply = basic::Multiply3(lhs:loadFromTexture.result, rhs:MaterialUniforms.Multiplier);
		node mat = materialParam::CommonMaterialParam_Make(roughness:"1", specular:"1", metal:"1");
		return output::Output_PerPixel(
			diffuseAlbedo:multiply.result, 
			material:mat.result).result;
	}
)--";

static const char s_techniqueBasicTexturing[] = R"--(
	~main
		ut-data/basicTexturingGraph.graph::Bind_PerPixel
)--";


namespace UnitTests
{
	static std::unordered_map<std::string, ::Assets::Blob> s_utData {
		std::make_pair("basic.tech", ::Assets::AsBlob(s_basicTechniqueFile)),
		std::make_pair("example-perpixel.psh", ::Assets::AsBlob(s_examplePerPixelShaderFile)),
		std::make_pair("example.graph", ::Assets::AsBlob(s_exampleGraphFile)),
		std::make_pair("complicated.graph", ::Assets::AsBlob(s_complicatedGraphFile)),
		std::make_pair("internalShaderFile.psh", ::Assets::AsBlob(s_internalShaderFile)),
		std::make_pair("internalComplicatedGraph.graph", ::Assets::AsBlob(s_internalComplicatedGraph)),
		std::make_pair("colorFromSelector.psh", ::Assets::AsBlob(s_colorFromSelectorShaderFile)),
		std::make_pair("basicTexturingGraph.graph", ::Assets::AsBlob(s_basicTexturingGraph))
	};

	static RenderCore::FrameBufferDesc MakeSimpleFrameBufferDesc()
	{
		using namespace RenderCore;
		SubpassDesc mainSubpass;
		mainSubpass.AppendOutput(0);

		std::vector<FrameBufferDesc::Attachment> attachments;
		attachments.push_back({0, AttachmentDesc{}});
		std::vector<SubpassDesc> subpasses;
		subpasses.push_back(mainSubpass);

		return FrameBufferDesc { std::move(attachments), std::move(subpasses) };
	}

	class VertexPCT
    {
    public:
        Float3      _position;
        unsigned    _color;
        Float2      _texCoord;
    };

    static VertexPCT vertices_fullViewport[] = {
        // Counter clockwise-winding triangle
        VertexPCT { Float3 {  -1.0f, -1.0f,  0.0f }, 0xffffffff, Float2 { 0.f, 0.f } },
        VertexPCT { Float3 {   1.0f, -1.0f,  0.0f }, 0xffffffff, Float2 { 1.f, 0.f } },
        VertexPCT { Float3 {  -1.0f,  1.0f,  0.0f }, 0xffffffff, Float2 { 0.f, 1.f } },

        // Counter clockwise-winding triangle
        VertexPCT { Float3 {  -1.0f,  1.0f,  0.0f }, 0xffffffff, Float2 { 0.f, 1.f } },
        VertexPCT { Float3 {   1.0f, -1.0f,  0.0f }, 0xffffffff, Float2 { 1.f, 0.f } },
        VertexPCT { Float3 {   1.0f,  1.0f,  0.0f }, 0xffffffff, Float2 { 1.f, 1.f } }
    };

	TEST_CLASS(PipelineAcceleratorTests)
	{
	public:
		static std::shared_ptr<RenderCore::Techniques::CompiledShaderPatchCollection> GetCompiledPatchCollectionFromText(StringSection<> techniqueText)
		{
			using namespace RenderCore;

			InputStreamFormatter<utf8> formattr { techniqueText.Cast<utf8>() };
			RenderCore::Assets::ShaderPatchCollection patchCollection(formattr);
			return ::Assets::ActualizePtr<RenderCore::Techniques::CompiledShaderPatchCollection>(patchCollection);
		}

		TEST_METHOD(ConfigurationAndCreation)
		{
			using namespace RenderCore;

			std::shared_ptr<Techniques::TechniqueSetFile> techniqueSetFile = ::Assets::AutoConstructAsset<Techniques::TechniqueSetFile>("ut-data/basic.tech");
			auto techniqueSharedResources = std::make_shared<Techniques::TechniqueSharedResources>();
			auto techniqueDelegate = Techniques::CreateTechniqueDelegate_Deferred(techniqueSetFile, techniqueSharedResources);

			Techniques::PipelineAcceleratorPool mainPool;
			mainPool.SetGlobalSelector("GLOBAL_SEL", 55);
			auto cfgId = mainPool.CreateSequencerConfig(
				techniqueDelegate,
				ParameterBox { std::make_pair(u("SEQUENCER_SEL"), "37") },
				FrameBufferProperties { 1024, 1024, TextureSamples::Create() },
				MakeSimpleFrameBufferDesc());

			RenderCore::Assets::RenderStateSet doubledSidedStateSet;
			doubledSidedStateSet._doubleSided = true;

				//
				//	Create a pipeline, and ensure that we get something valid out of it
				//
			{
				auto compiledPatches = GetCompiledPatchCollectionFromText(s_exampleTechniqueFragments);
				auto pipelineAccelerator = mainPool.CreatePipelineAccelerator(
					compiledPatches,
					ParameterBox { std::make_pair(u("SIMPLE_BIND"), "1") },
					GlobalInputLayouts::PNT,
					Topology::TriangleList,
					doubledSidedStateSet);

				auto finalPipeline = mainPool.GetPipeline(*pipelineAccelerator, *cfgId);
				finalPipeline->StallWhilePending();
				::Assert::IsTrue(finalPipeline->GetAssetState() == ::Assets::AssetState::Ready);
				auto pipelineActual = finalPipeline->Actualize();
				::Assert::IsNotNull(pipelineActual.get());
			}

				//
				//	Now create another pipeline, this time one that will react to some of the
				//	selectors as we change them
				//
			{
				auto compiledPatches = GetCompiledPatchCollectionFromText(s_techniqueForColorFromSelector);
				auto pipelineNoTexCoord = mainPool.CreatePipelineAccelerator(
					compiledPatches,
					ParameterBox {},
					GlobalInputLayouts::P,
					Topology::TriangleList,
					doubledSidedStateSet);

				{
						//
						//	We should get a valid pipeline in this case; since there are no texture coordinates
						//	on the geometry, this disables the code that triggers a compiler warning
						//
					auto finalPipeline = mainPool.GetPipeline(*pipelineNoTexCoord, *cfgId);
					finalPipeline->StallWhilePending();
					::Assert::IsTrue(finalPipeline->GetAssetState() == ::Assets::AssetState::Ready);
				}

				auto pipelineWithTexCoord = mainPool.CreatePipelineAccelerator(
					compiledPatches,
					ParameterBox {},
					GlobalInputLayouts::PCT,
					Topology::TriangleList,
					doubledSidedStateSet);

				{
						//
						//	Here, the pipeline will fail to compile. We should ensure we get a reasonable
						//	error message -- that is the shader compile error should propagate through
						//	to the pipeline error log
						//
					auto finalPipeline = mainPool.GetPipeline(*pipelineWithTexCoord, *cfgId);
					finalPipeline->StallWhilePending();
					::Assert::IsTrue(finalPipeline->GetAssetState() == ::Assets::AssetState::Invalid);
					auto log = ::Assets::AsString(finalPipeline->GetActualizationLog());
					::Assert::IsTrue(!log.empty());
				}

				// Now we'll create a new sequencer config, and we're actually going to use
				// this to render
				
				auto threadContext = _device->GetImmediateContext();
				auto targetDesc = CreateDesc(
					BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
					TextureDesc::Plain2D(64, 64, Format::R8G8B8A8_UNORM),
					"temporary-out");
				UnitTestFBHelper fbHelper(*_device, *threadContext, targetDesc);
				auto cfgIdWithColor = mainPool.CreateSequencerConfig(
					techniqueDelegate,
					ParameterBox { std::make_pair(u("COLOR_RED"), "1") },
					FrameBufferProperties { 64, 64, TextureSamples::Create() },
					MakeSimpleFrameBufferDesc());

				{
					auto finalPipeline = mainPool.GetPipeline(*pipelineWithTexCoord, *cfgIdWithColor);
					finalPipeline->StallWhilePending();
					::Assert::IsTrue(finalPipeline->GetAssetState() == ::Assets::AssetState::Ready);

					auto rpi = fbHelper.BeginRenderPass();
					RenderQuad(*threadContext, MakeIteratorRange(vertices_fullViewport), *finalPipeline->Actualize());
				}

				// We should have filled the entire framebuffer with red 
				// (due to the COLOR_RED selector in the sequencer config)
				auto breakdown0 = fbHelper.GetFullColorBreakdown();
				Assert::IsTrue(breakdown0.size() == 1);
				Assert::IsTrue(breakdown0.begin()->first == 0xff0000ff);

				// Change the sequencer config to now set the COLOR_GREEN selector
				cfgIdWithColor = mainPool.CreateSequencerConfig(
					techniqueDelegate,
					ParameterBox { std::make_pair(u("COLOR_GREEN"), "1") },
					FrameBufferProperties { 64, 64, TextureSamples::Create() },
					MakeSimpleFrameBufferDesc());

				{
					auto finalPipeline = mainPool.GetPipeline(*pipelineWithTexCoord, *cfgIdWithColor);
					finalPipeline->StallWhilePending();
					::Assert::IsTrue(finalPipeline->GetAssetState() == ::Assets::AssetState::Ready);

					auto rpi = fbHelper.BeginRenderPass();
					RenderQuad(*threadContext, MakeIteratorRange(vertices_fullViewport), *finalPipeline->Actualize());
				}

				auto breakdown1 = fbHelper.GetFullColorBreakdown();
				Assert::IsTrue(breakdown1.size() == 1);
				Assert::IsTrue(breakdown1.begin()->first == 0xff00ff00);
			}
		}

		TEST_METHOD(DescriptorSetAcceleratorConstruction)
		{
			{
				// Create a CompiledShaderPatchCollection from a typical input, and get the 
				// descriptor set layout from that.
				// Construct a DestructorSetAccelerator from it
				auto compiledPatches = GetCompiledPatchCollectionFromText(s_exampleTechniqueFragments);
				auto descriptorSetLayout = compiledPatches->GetInterface().GetMaterialDescriptorSet();

				ParameterBox constantBindings;
				constantBindings.SetParameter(u("DiffuseColor"), Float3{1.0f, 0.5f, 0.2f});

				ParameterBox resourceBindings;
				auto descriptorSetAcceleratorFuture = RenderCore::Techniques::MakeDescriptorSetAccelerator(
					constantBindings, resourceBindings,
					*descriptorSetLayout,
					"TestDescriptorSet");
				descriptorSetAcceleratorFuture->StallWhilePending();
				auto descriptorSetAccelerator = descriptorSetAcceleratorFuture->Actualize();

				// we should have 2 constant buffers and no shader resources
				Assert::IsTrue(descriptorSetAccelerator->_shaderResources.size() == 0);
				Assert::IsTrue(descriptorSetAccelerator->_constantBuffers.size() == 2);
				Assert::IsTrue(descriptorSetAccelerator->_usi._srvBindings.size() == 0);
				Assert::IsTrue(descriptorSetAccelerator->_usi._cbBindings.size() == 2);

				struct MaterialUniformsExpected
				{
					Float3 DiffuseColor; float SomeFloat;
				};

				struct AnotherCapturesExpected
				{
					Float4 Test2;	// note; the system will automatically rearrange the members in optimized order (regardless of how they appear in the original shader text)
					Float2 Test0;
					float SecondaryCaptures;
					unsigned _dummy;
				};

				auto materialUniformsI = 
					std::find_if(
						descriptorSetAccelerator->_usi._cbBindings.begin(), descriptorSetAccelerator->_usi._cbBindings.end(),
						[](const RenderCore::UniformsStreamInterface::RetainedCBBinding& cb) {
							return cb._hashName == Hash64("MaterialUniforms");
						});

				auto anotherCapturesI = 
					std::find_if(
						descriptorSetAccelerator->_usi._cbBindings.begin(), descriptorSetAccelerator->_usi._cbBindings.end(),
						[](const RenderCore::UniformsStreamInterface::RetainedCBBinding& cb) {
							return cb._hashName == Hash64("AnotherCaptures");
						});


				Assert::IsTrue(materialUniformsI != descriptorSetAccelerator->_usi._cbBindings.end());
				Assert::IsTrue(anotherCapturesI != descriptorSetAccelerator->_usi._cbBindings.end());

				// Check the data in the constants buffers we would bind
				// here, we're checking that the layout is what we expect, and that values (either from constantBindings or preset defaults)
				// actually got through

				auto threadContext = _device->GetImmediateContext();
				auto materialUniformsData = descriptorSetAccelerator->_constantBuffers[materialUniformsI-descriptorSetAccelerator->_usi._cbBindings.begin()]->ReadBack(*threadContext);
				Assert::IsTrue(materialUniformsData.size() == sizeof(MaterialUniformsExpected));
				auto& mu = *(const MaterialUniformsExpected*)AsPointer(materialUniformsData.begin());
				Assert::IsTrue(Equivalent(mu.DiffuseColor, Float3{1.0f, 0.5f, 0.2f}, 1e-3f));
				Assert::IsTrue(Equivalent(mu.SomeFloat, 0.25f, 1e-3f));

				auto anotherCapturesData = descriptorSetAccelerator->_constantBuffers[anotherCapturesI-descriptorSetAccelerator->_usi._cbBindings.begin()]->ReadBack(*threadContext);
				Assert::IsTrue(anotherCapturesData.size() == sizeof(AnotherCapturesExpected));
				auto& ac = *(const AnotherCapturesExpected*)AsPointer(anotherCapturesData.begin());
				Assert::IsTrue(Equivalent(ac.Test2, Float4{1.0f, 2.0f, 3.0f, 4.0f}, 1e-3f));
				Assert::IsTrue(Equivalent(ac.Test0, Float2{0.0f, 0.0f}, 1e-3f));
				Assert::IsTrue(Equivalent(ac.SecondaryCaptures, 0.7f, 1e-3f));
			}

			// try actually rendering (including background loading of textures)
			{
				auto compiledPatches = GetCompiledPatchCollectionFromText(s_techniqueBasicTexturing);
				auto descriptorSetLayout = compiledPatches->GetInterface().GetMaterialDescriptorSet();

				ParameterBox constantBindings;
				constantBindings.SetParameter(u("Multiplier"), Float3{1.0f, 0.5f, 0.0f});

				ParameterBox resourceBindings;
				resourceBindings.SetParameter(u("BoundTexture"), "xleres/DefaultResources/waternoise.png");

				auto descriptorSetAcceleratorFuture = RenderCore::Techniques::MakeDescriptorSetAccelerator(
					constantBindings, resourceBindings,
					*descriptorSetLayout,
					"TestDescriptorSet");

				// Put together the pieces we need to create a pipeline
				using namespace RenderCore;
				std::shared_ptr<Techniques::TechniqueSetFile> techniqueSetFile = ::Assets::AutoConstructAsset<Techniques::TechniqueSetFile>("ut-data/basic.tech");

				Techniques::PipelineAcceleratorPool mainPool;
				auto cfgId = mainPool.CreateSequencerConfig(
					Techniques::CreateTechniqueDelegate_Deferred(techniqueSetFile, std::make_shared<Techniques::TechniqueSharedResources>()),
					ParameterBox {},
					FrameBufferProperties { 64, 64, TextureSamples::Create() },
					MakeSimpleFrameBufferDesc());

				RenderCore::Assets::RenderStateSet doubledSidedStateSet;
				doubledSidedStateSet._doubleSided = true;

				auto pipelineWithTexCoord = mainPool.CreatePipelineAccelerator(
					compiledPatches,
					ParameterBox {},
					GlobalInputLayouts::PCT,
					Topology::TriangleList,
					doubledSidedStateSet);

				auto threadContext = _device->GetImmediateContext();
				auto targetDesc = CreateDesc(
					BindFlag::RenderTarget, CPUAccess::Read, GPUAccess::Write,
					TextureDesc::Plain2D(64, 64, Format::R8G8B8A8_UNORM),
					"temporary-out");
				UnitTestFBHelper fbHelper(*_device, *threadContext, targetDesc);
				
				{
					auto finalPipeline = mainPool.GetPipeline(*pipelineWithTexCoord, *cfgId);
					finalPipeline->StallWhilePending();
					auto log = ::Assets::AsString(finalPipeline->GetActualizationLog());
					Assert::IsTrue(finalPipeline->GetAssetState() == ::Assets::AssetState::Ready);

					for (;;) {
						auto state = descriptorSetAcceleratorFuture->StallWhilePending(std::chrono::milliseconds(10));
						if (state.has_value() && state.value() != ::Assets::AssetState::Pending) break;

						// we have to cycle these if anything is actually going to complete --
						_techniquesServices->GetBufferUploads().Update(*threadContext, false);
						::Assets::Services::GetAssetSets().OnFrameBarrier();
					}
					Assert::IsTrue(descriptorSetAcceleratorFuture->GetAssetState() == ::Assets::AssetState::Ready);

					auto rpi = fbHelper.BeginRenderPass();
					RenderQuad(*threadContext, MakeIteratorRange(vertices_fullViewport), *finalPipeline->Actualize(), descriptorSetAcceleratorFuture->Actualize().get());
				}

				auto breakdown = fbHelper.GetFullColorBreakdown();
				
				// If it's successful, we should get a lot of different color. And in each one, the blue channel will be zero
				// Because we're checking that there are a number of unique colors (and because the alpha values are fixed)
				// this can only succeed if the red and/or green channels have non-zero data for at least some pixels
				Assert::IsTrue(breakdown.size() > 32);
				for (const auto&c:breakdown)
					Assert::IsTrue((c.first & 0x00ff0000) == 0);
			}

			// check that depvals react to texture updates

			// check rendering "invalid" textures

			// default descriptor set for "legacy" techniques
		}

		ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> _globalServices;
		ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;

		std::shared_ptr<RenderCore::IDevice> _device;
		ConsoleRig::AttachablePtr<RenderCore::Assets::Services> _renderCoreAssetServices;
		ConsoleRig::AttachablePtr<RenderCore::Techniques::Services> _techniquesServices;

		PipelineAcceleratorTests()
		{
			UnitTest_SetWorkingDirectory();
			_globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
			::Assets::MainFileSystem::GetMountingTree()->Mount(u("xleres"), ::Assets::CreateFileSystem_OS(u("Game/xleres")));
			::Assets::MainFileSystem::GetMountingTree()->Mount(u("ut-data"), ::Assets::CreateFileSystem_Memory(s_utData));
			_assetServices = ConsoleRig::MakeAttachablePtr<::Assets::Services>(0);

			#if GFXAPI_TARGET == GFXAPI_APPLEMETAL
				auto api = RenderCore::UnderlyingAPI::AppleMetal;
			#elif GFXAPI_TARGET == GFXAPI_DX11
				auto api = RenderCore::UnderlyingAPI::DX11;
			#else
				auto api = RenderCore::UnderlyingAPI::OpenGLES;
			#endif

			_device = RenderCore::CreateDevice(api);

			_renderCoreAssetServices = ConsoleRig::MakeAttachablePtr<RenderCore::Assets::Services>(_device);
			_techniquesServices = ConsoleRig::MakeAttachablePtr<RenderCore::Techniques::Services>(_device);
		}

		~PipelineAcceleratorTests()
		{
			_techniquesServices.reset();
			_renderCoreAssetServices.reset();
			_device.reset();
			_assetServices.reset();
			_globalServices.reset();
		}

		static void BindPassThroughTransform(
			RenderCore::Metal::DeviceContext& metalContext,
			const RenderCore::Metal::GraphicsPipeline& pipeline,
			const RenderCore::Techniques::DescriptorSetAccelerator* descSet = nullptr)
		{
			// Bind the 2 main transform packets ("GlobalTransformConstants" and "LocalTransformConstants")
			// with identity transforms for local to clip
			static const auto GlobalTransformConstants = Hash64("GlobalTransform");
			static const auto LocalTransformConstants = Hash64("LocalTransform");
			using namespace RenderCore;
			UniformsStreamInterface usi;
			usi.BindConstantBuffer(0, {GlobalTransformConstants});
			usi.BindConstantBuffer(1, {LocalTransformConstants});

			auto globalTransformPkt = MakeSharedPktSize(sizeof(Techniques::GlobalTransformConstants));
			auto& globalTransform = *(Techniques::GlobalTransformConstants*)globalTransformPkt.get();
			XlZeroMemory(globalTransform);
			globalTransform._worldToClip = Identity<Float4x4>();
			auto localTransformPkt = Techniques::MakeLocalTransformPacket(Identity<Float4x4>(), Float3{0.f, 0.f, 0.f});

			Metal::BoundUniforms boundUniforms(
				pipeline, Metal::PipelineLayoutConfig{},
				usi,
				descSet ? descSet->_usi : UniformsStreamInterface{});

			ConstantBufferView cbvs[] = { globalTransformPkt, localTransformPkt };
			boundUniforms.Apply(metalContext, 0, UniformsStream{MakeIteratorRange(cbvs)});

			if (descSet)
				descSet->Apply(metalContext, boundUniforms, 1);
		}

		static void RenderQuad(
            RenderCore::IThreadContext& threadContext,
            IteratorRange<const VertexPCT*> vertices,
            const RenderCore::Metal::GraphicsPipeline& pipeline,
			const RenderCore::Techniques::DescriptorSetAccelerator* descSet = nullptr)
        {
			auto& metalContext = *RenderCore::Metal::DeviceContext::Get(threadContext);
            RenderCore::Metal::BoundInputLayout inputLayout(RenderCore::GlobalInputLayouts::PCT, pipeline.GetShaderProgram());

            auto vertexBuffer = CreateVB(*threadContext.GetDevice(), vertices);
			RenderCore::VertexBufferView vbv { vertexBuffer.get() };
            inputLayout.Apply(metalContext, MakeIteratorRange(&vbv, &vbv+1));

			BindPassThroughTransform(metalContext, pipeline, descSet);

            metalContext.Draw(pipeline, (unsigned)vertices.size());
        }
	};
}

