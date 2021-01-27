// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FakeModelCompiler.h"
#include "../../UnitTestHelper.h"
#include "../../../RenderCore/Assets/MaterialCompiler.h"
#include "../../../RenderCore/Assets/MaterialScaffold.h"
#include "../../../Assets/IntermediatesStore.h"
#include "../../../Assets/IntermediateCompilers.h"
#include "../../../Assets/IArtifact.h"
#include "../../../Assets/MountingTree.h"
#include "../../../Assets/MemoryFile.h"
#include "../../../Assets/AssetTraits.h"
#include "../../../Assets/AssetServices.h"
#include "../../../Assets/CompileAndAsyncManager.h"
#include "../../../Math/Vector.h"
#include "../../../Math/MathSerialization.h"
#include "../../../ConsoleRig/AttachablePtr.h"
#include "../../../ConsoleRig/GlobalServices.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include <filesystem>

using namespace Catch::literals;
namespace UnitTests
{
	static std::unordered_map<std::string, ::Assets::Blob> s_utData {
		std::make_pair(
			"test.material",
			::Assets::AsBlob(R"--(
				~*
					~Constants
						OnEverything=75
				~Material0
					~Inherit; ./base.material:BaseSetting
					~ShaderParams
						MAT_DOUBLE_SIDED_LIGHTING=1u
					~Constants
						MaterialDiffuse={0.1f, 0.1f, 0.1f}c
					~States
						DoubleSided=1u
			)--")),
		std::make_pair(
			"base.material",
			::Assets::AsBlob(R"--(
				~BaseSetting
					~Constants
						SharedConstant={1.0f, 1.0f, 1.0f}c
			)--"))
	};

	TEST_CASE( "RenderCoreCompilation-Materials", "[rendercore_assets]" )
	{
		UnitTest_SetWorkingDirectory();
		auto globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
		auto mnt = ::Assets::MainFileSystem::GetMountingTree()->Mount("ut-data", ::Assets::CreateFileSystem_Memory(s_utData));

		auto tempDirPath = std::filesystem::temp_directory_path() / "xle-unit-tests";
		std::filesystem::remove_all(tempDirPath);	// ensure we're starting from an empty temporary directory
		std::filesystem::create_directories(tempDirPath);

		auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();

		auto matRegistration = RenderCore::Assets::RegisterMaterialCompiler(compilers);
		auto modelRegistration = UnitTests::RegisterFakeModelCompiler(compilers);

		SECTION("Compile material scaffold")
		{
			StringSection<> initializers[] = { "ut-data/test.material", "fake-model" };
			auto marker = compilers.Prepare(RenderCore::Assets::MaterialScaffold::CompileProcessType, initializers, dimof(initializers));
			REQUIRE(marker != nullptr);
			REQUIRE(marker->GetExistingAsset() == nullptr);

			auto compile = marker->InvokeCompile();
			REQUIRE(compile != nullptr);

			compile->StallWhilePending();
			REQUIRE(compile->GetAssetState() == ::Assets::AssetState::Ready);

			auto finalScaffold = ::Assets::AutoConstructAsset<RenderCore::Assets::MaterialScaffold>(
				*compile->GetArtifactCollection());
			(void)finalScaffold;

			// The final values in the material are a combination of values that come from
			// FakeModelCompileOperations & the test.material material file

			auto* material0 = finalScaffold->GetMaterial(Hash64("Material0"));
			REQUIRE(material0->_matParams.GetParameter<unsigned>("MAT_DOUBLE_SIDED_LIGHTING").value() == 1);
			auto emissive = material0->_constants.GetParameterAsString("Emissive"); (void)emissive;
			REQUIRE(Equivalent(material0->_constants.GetParameter<Float3>("Emissive").value(), Float3{0.5f, 0.5f, 0.5f}, 1e-3f));
			REQUIRE(Equivalent(material0->_constants.GetParameter<Float3>("MaterialDiffuse").value(), Float3{0.1f, 0.1f, 0.1f}, 1e-3f));
			REQUIRE(Equivalent(material0->_constants.GetParameter<Float3>("SharedConstant").value(), Float3{1.0f, 1.0f, 1.0f}, 1e-3f));
			REQUIRE(material0->_constants.GetParameter<float>("Brightness") == 50_a);
			REQUIRE(material0->_constants.GetParameter<float>("OnEverything") == 75_a);

			auto* material1 = finalScaffold->GetMaterial(Hash64("Material1"));
			REQUIRE(Equivalent(material1->_constants.GetParameter<Float3>("Emissive").value(), Float3{2.5f, 0.25f, 0.15f}, 1e-3f));
			REQUIRE(material1->_constants.GetParameter<float>("Brightness") == 33_a);
			REQUIRE(material1->_constants.GetParameter<float>("OnEverything") == 75_a);
		}

		compilers.DeregisterCompiler(modelRegistration._registrationId);
		compilers.DeregisterCompiler(matRegistration._registrationId);
		::Assets::MainFileSystem::GetMountingTree()->Unmount(mnt);
	}

}
