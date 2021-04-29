// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../UnitTestHelper.h"
#include "../../EmbeddedRes.h"
#include "../ReusableDataFiles.h"
#include "../Metal/MetalTestHelper.h"
#include "../../../RenderCore/Techniques/ImmediateDrawables.h"
#include "../../../RenderCore/Techniques/Drawables.h"
#include "../../../RenderCore/Techniques/ParsingContext.h"
#include "../../../RenderCore/Techniques/Services.h"
#include "../../../RenderCore/Techniques/CompiledShaderPatchCollection.h"
#include "../../../RenderCore/Techniques/Techniques.h"
#include "../../../RenderCore/Techniques/DeferredShaderResource.h"
#include "../../../RenderCore/Assets/PredefinedDescriptorSetLayout.h"
#include "../../../BufferUploads/IBufferUploads.h"
#include "../../../RenderCore/MinimalShaderSource.h"
#include "../../../RenderCore/Format.h"
#include "../../../RenderCore/IDevice.h"
#include "../../../ShaderParser/AutomaticSelectorFiltering.h"
#include "../../../Assets/AssetServices.h"
#include "../../../Assets/IFileSystem.h"
#include "../../../Assets/OSFileSystem.h"
#include "../../../Assets/MountingTree.h"
#include "../../../Assets/MemoryFile.h"
#include "../../../Assets/AssetSetManager.h"
#include "../../../Assets/CompileAndAsyncManager.h"
#include "../../../Assets/CompilerLibrary.h"
#include "../../../Assets/Assets.h"
#include "../../../Tools/ToolsRig/VisualisationGeo.h"
#include "../../../ConsoleRig/Console.h"
#include "../../../OSServices/Log.h"
#include "../../../ConsoleRig/AttachablePtr.h"
#include "../../../Utility/StringFormat.h"
#include "thousandeyes/futures/then.h"
#include "thousandeyes/futures/DefaultExecutor.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include <regex>
#include <chrono>

using namespace Catch::literals;
using namespace std::chrono_literals;
namespace UnitTests
{
	static const char* s_sequencerDescSetLayout = R"(
		ConstantBuffer GlobalTransform;
		ConstantBuffer LocalTransform;
		ConstantBuffer ReciprocalViewportDimensionsCB;
		ConstantBuffer cb0;
		ConstantBuffer cb1;
		ConstantBuffer cb2;

		SampledTexture tex0;
		SampledTexture tex1;
		SampledTexture tex2;
		SampledTexture tex3;
		SampledTexture tex4;
		SampledTexture tex5;
		SampledTexture tex6;
	)";

	TEST_CASE( "ImmediateDrawablesTests", "[rendercore_techniques]" )
	{
		using namespace RenderCore;
		auto globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
		auto xlresmnt = ::Assets::MainFileSystem::GetMountingTree()->Mount("xleres", UnitTests::CreateEmbeddedResFileSystem());
		auto testHelper = MakeTestHelper();

		Verbose.SetConfiguration(OSServices::MessageTargetConfiguration{});

		auto techniqueServices = ConsoleRig::MakeAttachablePtr<Techniques::Services>(testHelper->_device);
		techniqueServices->RegisterTextureLoader(std::regex(R"(.*\.[dD][dD][sS])"), Techniques::CreateDDSTextureLoader());
		techniqueServices->RegisterTextureLoader(std::regex(R"(.*)"), Techniques::CreateWICTextureLoader());

		auto executor = std::make_shared<thousandeyes::futures::DefaultExecutor>(std::chrono::milliseconds(2));
		thousandeyes::futures::Default<thousandeyes::futures::Executor>::Setter execSetter(executor);

		auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
		auto filteringRegistration = ShaderSourceParser::RegisterShaderSelectorFilteringCompiler(compilers);
		auto shaderCompilerRegistration = RenderCore::RegisterShaderCompiler(testHelper->_shaderSource, compilers);
		auto shaderCompiler2Registration = RenderCore::Techniques::RegisterInstantiateShaderGraphCompiler(testHelper->_shaderSource, compilers);

		auto sequencerDescriptorSetLayout = std::make_shared<RenderCore::Assets::PredefinedDescriptorSetLayout>(
			s_sequencerDescSetLayout, ::Assets::DirectorySearchRules{}, ::Assets::DependencyValidation{});

		auto immediateDrawables = RenderCore::Techniques::CreateImmediateDrawables(
			testHelper->_device, testHelper->_pipelineLayout,
			RenderCore::Techniques::Internal::GetDefaultDescriptorSetLayoutAndBinding(),
			RenderCore::Techniques::DescriptorSetLayoutAndBinding { sequencerDescriptorSetLayout, 0 });

		auto threadContext = testHelper->_device->GetImmediateContext();
		auto targetDesc = CreateDesc(
			BindFlag::RenderTarget | BindFlag::TransferSrc, 0, GPUAccess::Write,
			TextureDesc::Plain2D(256, 256, Format::R8G8B8A8_UNORM),
			"temporary-out");
		UnitTestFBHelper fbHelper(*testHelper->_device, *threadContext, targetDesc);

		auto sphereGeo = ToolsRig::BuildGeodesicSphere();

		// Try drawing just a basic sphere with no material assigments
		{
			auto data = immediateDrawables->QueueDraw(
				sphereGeo.size(),
				ToolsRig::Vertex3D_MiniInputLayout);
			REQUIRE(data.size() == (sphereGeo.size() * sizeof(decltype(sphereGeo)::value_type)));
			std::memcpy(data.data(), sphereGeo.data(), data.size());
			
			auto asyncMarker = immediateDrawables->PrepareResources(fbHelper.GetDesc(), 0);
			if (asyncMarker) {
				auto finalState = asyncMarker->StallWhilePending();
				REQUIRE(finalState.has_value());
				REQUIRE(finalState.value() == ::Assets::AssetState::Ready);
				REQUIRE(asyncMarker->GetAssetState() == ::Assets::AssetState::Ready);
			}

			{
				auto rpi = fbHelper.BeginRenderPass(*threadContext);
				auto techniqueContext = std::make_shared<RenderCore::Techniques::TechniqueContext>();
				techniqueContext->_drawablesSharedResources = RenderCore::Techniques::CreateDrawablesSharedResources();
				RenderCore::Techniques::ParsingContext parsingContext { *techniqueContext };
				immediateDrawables->ExecuteDraws(*threadContext, parsingContext, fbHelper.GetDesc(), 0, Float2(targetDesc._textureDesc._width, targetDesc._textureDesc._height));
			}

			auto breakdown = fbHelper.GetFullColorBreakdown(*threadContext);
			REQUIRE(breakdown.size() != 1);
		}

		// Try drawing with a texture and a little bit of material information
		{
			auto tex = ::Assets::MakeAsset<Techniques::DeferredShaderResource>("xleres/DefaultResources/waternoise.png");
			for (;;) {
				using namespace std::chrono_literals;
				auto res = tex->StallWhilePending(4ms);
				if (res.has_value()) break;
				RenderCore::Techniques::Services::GetBufferUploads().Update(*threadContext);
			}
			// hack -- 
			// we need to pump buffer uploads a bit to ensure the texture load gets completed
			for (unsigned c=0; c<5; ++c) {
				RenderCore::Techniques::Services::GetBufferUploads().Update(*threadContext);
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(16ms);
			}

			Techniques::ImmediateDrawableMaterial material;
			material._uniformStreamInterface = std::make_shared<UniformsStreamInterface>();
			material._uniformStreamInterface->BindResourceView(0, Hash64("InputTexture"));
			material._uniforms._resourceViews.push_back(tex->Actualize()->GetShaderResource());
			auto data = immediateDrawables->QueueDraw(
				sphereGeo.size(),
				ToolsRig::Vertex3D_MiniInputLayout,
				material);
			REQUIRE(data.size() == (sphereGeo.size() * sizeof(decltype(sphereGeo)::value_type)));
			std::memcpy(data.data(), sphereGeo.data(), data.size());
			
			auto asyncMarker = immediateDrawables->PrepareResources(fbHelper.GetDesc(), 0);
			if (asyncMarker) {
				auto finalState = asyncMarker->StallWhilePending();
				REQUIRE(finalState.has_value());
				REQUIRE(finalState.value() == ::Assets::AssetState::Ready);
				REQUIRE(asyncMarker->GetAssetState() == ::Assets::AssetState::Ready);
			}

			{
				auto rpi = fbHelper.BeginRenderPass(*threadContext);
				auto techniqueContext = std::make_shared<RenderCore::Techniques::TechniqueContext>();
				techniqueContext->_drawablesSharedResources = RenderCore::Techniques::CreateDrawablesSharedResources();
				RenderCore::Techniques::ParsingContext parsingContext { *techniqueContext };
				immediateDrawables->ExecuteDraws(*threadContext, parsingContext, fbHelper.GetDesc(), 0, Float2(targetDesc._textureDesc._width, targetDesc._textureDesc._height));
			}

			auto breakdown = fbHelper.GetFullColorBreakdown(*threadContext);
			REQUIRE(breakdown.size() > 5);
		}

		compilers.DeregisterCompiler(shaderCompiler2Registration._registrationId);
		compilers.DeregisterCompiler(shaderCompilerRegistration._registrationId);
		compilers.DeregisterCompiler(filteringRegistration._registrationId);

		::Assets::MainFileSystem::GetMountingTree()->Unmount(xlresmnt);
	}
}

