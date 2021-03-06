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
#include "../../../RenderCore/Techniques/AutomaticSelectorFiltering.h"
#include "../../../RenderCore/IDevice.h"
#include "../../../ShaderParser/ShaderInstantiation.h"
#include "../../../ShaderParser/DescriptorSetInstantiation.h"
#include "../../../ShaderParser/ShaderAnalysis.h"
#include "../../../Assets/IFileSystem.h"
#include "../../../Assets/OSFileSystem.h"
#include "../../../Assets/MountingTree.h"
#include "../../../Assets/MemoryFile.h"
#include "../../../Assets/AssetServices.h"
#include "../../../Assets/AssetTraits.h"
#include "../../../Assets/CompileAndAsyncManager.h"
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
			)--"))
	};

	TEST_CASE( "TechniqueDelegates-LegacyTechnique", "[shader_parser]" )
	{
		auto globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
		auto xlresmnt = ::Assets::MainFileSystem::GetMountingTree()->Mount("xleres", UnitTests::CreateEmbeddedResFileSystem());
		auto mnt1 = ::Assets::MainFileSystem::GetMountingTree()->Mount("ut-data", ::Assets::CreateFileSystem_Memory(s_techDelUTData, s_defaultFilenameRules, ::Assets::FileSystemMemoryFlags::EnableChangeMonitoring));

		auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
		auto filteringRegistration = RenderCore::Techniques::RegisterShaderSelectorFilteringCompiler(compilers);

		static const char simplePatchCollectionFragments[] = R"--(
		perPixel=~
			ut-data/perpixel.graph::Default_PerPixel
		)--";
		InputStreamFormatter<utf8> formattr { MakeStringSection(simplePatchCollectionFragments) };
		RenderCore::Assets::ShaderPatchCollection patchCollection(formattr, ::Assets::DirectorySearchRules{}, nullptr);
		RenderCore::Techniques::CompiledShaderPatchCollection compiledPatchCollection { patchCollection };

		{
			auto delegate = RenderCore::Techniques::CreateTechniqueDelegateLegacy(
				0,
				RenderCore::AttachmentBlendDesc{},
				RenderCore::RasterizationDesc{},
				RenderCore::DepthStencilDesc{});

			RenderCore::Assets::RenderStateSet stateSet;

			auto pipelineDescFuture = delegate->Resolve(
				compiledPatchCollection.GetInterface(),
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

		compilers.DeregisterCompiler(filteringRegistration._registrationId);
		::Assets::MainFileSystem::GetMountingTree()->Unmount(mnt1);
		::Assets::MainFileSystem::GetMountingTree()->Unmount(xlresmnt);
	}


}

