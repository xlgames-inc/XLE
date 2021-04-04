// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../UnitTestHelper.h"
#include "../../EmbeddedRes.h"
#include "../ReusableDataFiles.h"
#include "../Metal/MetalTestHelper.h"
#include "../../../RenderCore/Techniques/ImmediateDrawables.h"
#include "../../../RenderCore/Techniques/ParsingContext.h"
#include "../../../RenderCore/Techniques/Services.h"
#include "../../../RenderCore/Techniques/CompiledShaderPatchCollection.h"
#include "../../../RenderCore/Techniques/Techniques.h"
#include "../../../RenderCore/Assets/PredefinedDescriptorSetLayout.h"
#include "../../../RenderCore/MinimalShaderSource.h"
#include "../../../RenderCore/Format.h"
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

		auto executor = std::make_shared<thousandeyes::futures::DefaultExecutor>(std::chrono::milliseconds(2));
		thousandeyes::futures::Default<thousandeyes::futures::Executor>::Setter execSetter(executor);

		auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
		auto filteringRegistration = ShaderSourceParser::RegisterShaderSelectorFilteringCompiler(compilers);
		auto shaderCompilerRegistration = RenderCore::RegisterShaderCompiler(testHelper->_shaderSource, compilers);
		auto shaderCompiler2Registration = RenderCore::Techniques::RegisterInstantiateShaderGraphCompiler(testHelper->_shaderSource, compilers);

        auto sequencerDescriptorSetLayout = std::make_shared<RenderCore::Assets::PredefinedDescriptorSetLayout>(
            s_sequencerDescSetLayout, ::Assets::DirectorySearchRules{}, nullptr);

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

        testHelper->BeginFrameCapture();

        {
            auto data = immediateDrawables->QueueDraw(
                sphereGeo.size() * sizeof(decltype(sphereGeo)::value_type),
                ToolsRig::Vertex3D_InputLayout,
                RenderCore::Assets::RenderStateSet{});
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
                RenderCore::Techniques::TechniqueContext techniqueContext;
                RenderCore::Techniques::ParsingContext parsingContext { techniqueContext };
                immediateDrawables->ExecuteDraws(*threadContext, parsingContext, fbHelper.GetDesc(), 0);
            }

            testHelper->EndFrameCapture();

            auto breakdown = fbHelper.GetFullColorBreakdown(*threadContext);
			REQUIRE(breakdown.size() != 1);
        }

        compilers.DeregisterCompiler(shaderCompiler2Registration._registrationId);
		compilers.DeregisterCompiler(shaderCompilerRegistration._registrationId);
		compilers.DeregisterCompiler(filteringRegistration._registrationId);

		::Assets::MainFileSystem::GetMountingTree()->Unmount(xlresmnt);
    }
}

