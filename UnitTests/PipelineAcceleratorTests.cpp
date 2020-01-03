// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "ReusableDataFiles.h"
#include "MetalUnitTest.h"
#include "../RenderCore/Techniques/PipelineAccelerator.h"
#include "../RenderCore/Techniques/DrawableDelegates.h"
#include "../RenderCore/Techniques/DrawableMaterial.h"		// just for ShaderPatchCollectionRegistry
#include "../RenderCore/Techniques/TechniqueDelegates.h"
#include "../RenderCore/Techniques/CompiledShaderPatchCollection.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/PipelineLayout.h"
#include "../RenderCore/Assets/Services.h"
#include "../RenderCore/ResourceDesc.h"
#include "../RenderCore/FrameBufferDesc.h"
#include "../Assets/AssetServices.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/OSFileSystem.h"
#include "../Assets/MountingTree.h"
#include "../Assets/MemoryFile.h"
#include "../Assets/DepVal.h"
#include "../Assets/AssetTraits.h"
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


namespace UnitTests
{
	static std::unordered_map<std::string, ::Assets::Blob> s_utData {
		std::make_pair("basic.tech", ::Assets::AsBlob(s_basicTechniqueFile)),
		std::make_pair("example-perpixel.psh", ::Assets::AsBlob(s_examplePerPixelShaderFile)),
		std::make_pair("example.graph", ::Assets::AsBlob(s_exampleGraphFile)),
		std::make_pair("complicated.graph", ::Assets::AsBlob(s_complicatedGraphFile)),
		std::make_pair("internalShaderFile.psh", ::Assets::AsBlob(s_internalShaderFile)),
		std::make_pair("internalComplicatedGraph.graph", ::Assets::AsBlob(s_internalComplicatedGraph)),
		std::make_pair("colorFromSelector.psh", ::Assets::AsBlob(s_colorFromSelectorShaderFile))
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
			// return std::make_shared<Techniques::CompiledShaderPatchCollection>(patchCollection);

			// todo -- avoid the need for this global registry
			Techniques::ShaderPatchCollectionRegistry::GetInstance().RegisterShaderPatchCollection(patchCollection);
			return Techniques::ShaderPatchCollectionRegistry::GetInstance().GetCompiledShaderPatchCollection(patchCollection.GetHash());
		}

		TEST_METHOD(ConfigurationAndCreation)
		{
			using namespace RenderCore;

			std::shared_ptr<Techniques::TechniqueSetFile> techniqueSetFile = ::Assets::AutoConstructAsset<Techniques::TechniqueSetFile>("ut-data/basic.tech");
			auto techniqueSharedResources = std::make_shared<Techniques::TechniqueSharedResources>();
			auto techniqueDelegate = Techniques::CreateTechniqueDelegatePrototype(techniqueSetFile, techniqueSharedResources);

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

				auto finalPipeline = pipelineAccelerator->GetPipeline(cfgId);
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
					auto finalPipeline = pipelineNoTexCoord->GetPipeline(cfgId);
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
					auto finalPipeline = pipelineWithTexCoord->GetPipeline(cfgId);
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
					auto finalPipeline = pipelineWithTexCoord->GetPipeline(cfgIdWithColor);
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
					auto finalPipeline = pipelineWithTexCoord->GetPipeline(cfgIdWithColor);
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

		ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> _globalServices;
		ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;

		std::shared_ptr<RenderCore::IDevice> _device;
		ConsoleRig::AttachablePtr<RenderCore::Assets::Services> _renderCoreAssetServices;
		std::unique_ptr<RenderCore::Techniques::ShaderPatchCollectionRegistry> _shaderPatchCollectionRegistry;

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

			_shaderPatchCollectionRegistry = std::make_unique<RenderCore::Techniques::ShaderPatchCollectionRegistry>();
		}

		~PipelineAcceleratorTests()
		{
			_shaderPatchCollectionRegistry.reset();
			_renderCoreAssetServices.reset();
			_device.reset();
			_assetServices.reset();
			_globalServices.reset();
		}

		static void BindPassThroughTransform(
			RenderCore::Metal::DeviceContext& metalContext,
			const RenderCore::Metal::GraphicsPipeline& pipeline)
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
				usi);

			ConstantBufferView cbvs[] = { globalTransformPkt, localTransformPkt };
			boundUniforms.Apply(metalContext, 0, UniformsStream{MakeIteratorRange(cbvs)});
		}

		static void RenderQuad(
            RenderCore::IThreadContext& threadContext,
            IteratorRange<const VertexPCT*> vertices,
            const RenderCore::Metal::GraphicsPipeline& pipeline)
        {
			auto& metalContext = *RenderCore::Metal::DeviceContext::Get(threadContext);
            RenderCore::Metal::BoundInputLayout inputLayout(RenderCore::GlobalInputLayouts::PCT, pipeline.GetShaderProgram());

            auto vertexBuffer = CreateVB(*threadContext.GetDevice(), vertices);
			RenderCore::VertexBufferView vbv { vertexBuffer.get() };
            inputLayout.Apply(metalContext, MakeIteratorRange(&vbv, &vbv+1));

			BindPassThroughTransform(metalContext, pipeline);

            metalContext.Draw(pipeline, (unsigned)vertices.size());
        }
	};
}

