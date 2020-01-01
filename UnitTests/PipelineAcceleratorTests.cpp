// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "UnitTestHelper.h"
#include "ReusableDataFiles.h"
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

static const char* s_basicTechniqueFile = R"--(
	~Shared
		~Parameters
			~GlobalEnvironment
				CLASSIFY_NORMAL_MAP
				SKIP_MATERIAL_DIFFUSE=0

	~NoPatches
		~Inherit; Shared
		VertexShader=xleres/deferred/basic.vsh:main
		PixelShader=xleres/deferred/basic.psh:main

	~PerPixel
		~Inherit; Shared
		VertexShader=xleres/deferred/basic.vsh:main
		PixelShader=xleres/deferred/main.psh:frameworkEntry

	~PerPixelAndEarlyRejection
		~Inherit; Shared
		VertexShader=xleres/deferred/basic.vsh:main
		PixelShader=xleres/deferred/main.psh:frameworkEntryWithEarlyRejection
)--";

static const char s_exampleTechniqueFragments[] = R"--(
	~main
		ut-data/complicated.graph::Bind2_PerPixel
)--";

namespace UnitTests
{
	static std::unordered_map<std::string, ::Assets::Blob> s_utData {
		std::make_pair("basic.tech", ::Assets::AsBlob(s_basicTechniqueFile)),
		std::make_pair("example-perpixel.psh", ::Assets::AsBlob(s_examplePerPixelShaderFile)),
		std::make_pair("example.graph", ::Assets::AsBlob(s_exampleGraphFile)),
		std::make_pair("complicated.graph", ::Assets::AsBlob(s_complicatedGraphFile)),
		std::make_pair("internalShaderFile.psh", ::Assets::AsBlob(s_internalShaderFile)),
		std::make_pair("internalComplicatedGraph.graph", ::Assets::AsBlob(s_internalComplicatedGraph))
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

	TEST_CLASS(PipelineAcceleratorTests)
	{
	public:

		TEST_METHOD(ConfigurationAndCreation)
		{
			using namespace RenderCore;

			std::shared_ptr<Techniques::TechniqueSetFile> techniqueSetFile = ::Assets::AutoConstructAsset<Techniques::TechniqueSetFile>("ut-data/basic.tech");
			auto techniqueSharedResources = std::make_shared<Techniques::TechniqueSharedResources>();
			auto techniqueDelegate = Techniques::CreateTechniqueDelegatePrototype(techniqueSetFile, techniqueSharedResources);

			Techniques::PipelineAcceleratorPool mainPool;
			mainPool.SetGlobalSelector("GLOBAL_SEL", 55);
			auto cfgId = mainPool.AddSequencerConfig(
				techniqueDelegate,
				ParameterBox { std::make_pair(u("SEQUENCER_SEL"), "37") },
				FrameBufferProperties { 1024, 1024, TextureSamples::Create() },
				MakeSimpleFrameBufferDesc());

			std::shared_ptr<Techniques::CompiledShaderPatchCollection> compiledPatches;
			Techniques::ShaderPatchCollectionRegistry globalRegistry;
			{
				InputStreamFormatter<utf8> formattr { s_exampleTechniqueFragments };
				RenderCore::Assets::ShaderPatchCollection patchCollection(formattr);
				// compiledPatches = std::make_shared<Techniques::CompiledShaderPatchCollection>(patchCollection);

				// todo -- avoid the need for this global registry
				Techniques::ShaderPatchCollectionRegistry::GetInstance().RegisterShaderPatchCollection(patchCollection);
				compiledPatches = Techniques::ShaderPatchCollectionRegistry::GetInstance().GetCompiledShaderPatchCollection(patchCollection.GetHash());
			}

			auto pipelineAccelerator = mainPool.CreatePipelineAccelerator(
				compiledPatches,
				ParameterBox { std::make_pair(u("SIMPLE_BIND"), "1") },
				GlobalInputLayouts::PNT,
				Topology::TriangleList,
				Metal::DepthStencilDesc{},
				Metal::AttachmentBlendDesc{},
				Metal::RasterizationDesc{});

			auto finalPipeline = pipelineAccelerator->GetPipeline(cfgId);
			finalPipeline->StallWhilePending();
			auto pipelineActual = finalPipeline->Actualize();
			::Assert::IsNotNull(pipelineActual.get());
		}

		ConsoleRig::AttachablePtr<ConsoleRig::GlobalServices> _globalServices;
		ConsoleRig::AttachablePtr<::Assets::Services> _assetServices;

		std::shared_ptr<RenderCore::IDevice> _device;
		ConsoleRig::AttachablePtr<RenderCore::Assets::Services> _renderCoreAssetServices;

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
		}

		~PipelineAcceleratorTests()
		{
			_renderCoreAssetServices.reset();
			_device.reset();
			_assetServices.reset();
			_globalServices.reset();
		}
	};
}

