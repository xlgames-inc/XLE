// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Metal/MetalUnitTest.h"
#include "../../UnitTestHelper.h"
#include "../../../ShaderParser/ShaderAnalysis.h"
#include "../../../RenderCore/Metal/Shader.h"
#include "../../../Assets/IFileSystem.h"
#include "../../../Assets/MountingTree.h"
#include "../../../Assets/MemoryFile.h"
#include "../../../Assets/IArtifact.h"
#include "../../../Assets/CompileAndAsyncManager.h"
#include "../../../Assets/DeferredConstruction.h"
#include "../../../Assets/InitializerPack.h"
#include "../../../ConsoleRig/GlobalServices.h"
#include "../../../ConsoleRig/AttachablePtr.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"

#if GFXAPI_TARGET == GFXAPI_APPLEMETAL
	#include "../Metal/InputLayoutShaders_MSL.h"
#elif GFXAPI_TARGET == GFXAPI_OPENGLES
	#include "../Metal/InputLayoutShaders_GLSL.h"
#elif GFXAPI_TARGET == GFXAPI_DX11
	#include "../Metal/InputLayoutShaders_HLSL.h"
#else
	#error Unit test shaders not written for this graphics API
#endif

using namespace Catch::literals;
namespace UnitTests
{
	class ExpandIncludesPreprocessor : public RenderCore::ISourceCodePreprocessor
	{
	public:
		virtual SourceCodeWithRemapping RunPreprocessor(
            StringSection<> inputSource, 
            StringSection<> definesTable,
            const ::Assets::DirectorySearchRules& searchRules) override
		{
			return ShaderSourceParser::ExpandIncludes(inputSource, "main", searchRules);
		}
	};

	static std::unordered_map<std::string, ::Assets::Blob> s_utData {
		std::make_pair(
			"IncludeDirective.hlsl",
			::Assets::AsBlob(R"--(
				#include "vsText.hlsl"
			)--")),
		std::make_pair(
			"vsText.hlsl",
			::Assets::AsBlob(vsText))
	};

	static std::unique_ptr<MetalTestHelper> MakeTestHelper()
	{
		#if GFXAPI_TARGET == GFXAPI_APPLEMETAL
			return std::make_unique<MetalTestHelper>(RenderCore::UnderlyingAPI::AppleMetal);
		#elif GFXAPI_TARGET == GFXAPI_OPENGLES
			return std::make_unique<MetalTestHelper>(RenderCore::UnderlyingAPI::OpenGLES);
		#elif GFXAPI_TARGET == GFXAPI_DX11
			auto res = std::make_unique<MetalTestHelper>(RenderCore::UnderlyingAPI::DX11);
			// hack -- required for D3D11 currently
			auto metalContext = RenderCore::Metal::DeviceContext::Get(*res->_device->GetImmediateContext());
			metalContext->Bind(RenderCore::Metal::RasterizerState{RenderCore::CullMode::None});
			return res;
		#endif
	}

	TEST_CASE( "ShaderCompilation-Compile", "[rendercore_metal]" )
	{
		using namespace RenderCore;
		UnitTest_SetWorkingDirectory();
		auto _globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
		auto assetServices = ConsoleRig::MakeAttachablePtr<::Assets::Services>(0);
		auto testHelper = MakeTestHelper();

		SECTION("UnitTestHelper shaders") {
			// Ensure that we can compile a shader from string input, via the MakeShaderProgram
			// utility function
			auto compiledFromString = MakeShaderProgram(*testHelper, vsText_clipInput, psText);
			REQUIRE(compiledFromString.GetDependencyValidation() != nullptr);
		}

		// Let's load and compile a basic shader from a mounted filesystem
		// We'll use a custom shader source that will expand #include directives
		// for us (some gfx-apis can already do this, others need a little help)
		auto mnt = ::Assets::MainFileSystem::GetMountingTree()->Mount("ut-data", ::Assets::CreateFileSystem_Memory(s_utData));

		auto customShaderSource = std::make_shared<RenderCore::MinimalShaderSource>(
			testHelper->_device->CreateShaderCompiler(),
			std::make_shared<ExpandIncludesPreprocessor>());

		auto compilerRegistration = RenderCore::RegisterShaderCompiler(
			customShaderSource, 
			::Assets::Services::GetAsyncMan().GetIntermediateCompilers());

		SECTION("MinimalShaderSource") {
			auto compiledFromString = MakeShaderProgram(*testHelper, vsText_clipInput, psText);
			REQUIRE(compiledFromString.GetDependencyValidation() != nullptr);

			// Using RenderCore::ShaderService, ensure that we can compile a simple shader (this shader should compile successfully)
			auto compileMarker = ::Assets::Internal::BeginCompileOperation(
				RenderCore::CompiledShaderByteCode::CompileProcessType,
				::Assets::InitializerPack { "ut-data/IncludeDirective.hlsl:main:vs_*", "SOME_DEFINE=1" });
			REQUIRE(compileMarker != nullptr);
			auto compiledFromFile = compileMarker->InvokeCompile();
			REQUIRE(compiledFromFile != nullptr);
			compiledFromFile->StallWhilePending();
			REQUIRE(compiledFromFile->GetAssetState() == ::Assets::AssetState::Ready);
			auto artifacts = compiledFromFile->GetArtifactCollection();
			REQUIRE(artifacts != nullptr);
			REQUIRE(artifacts->GetDependencyValidation() != nullptr);
			REQUIRE(artifacts->GetAssetState() == ::Assets::AssetState::Ready);
			::Assets::ArtifactRequest request { "", RenderCore::CompiledShaderByteCode::CompileProcessType, ~0u, ::Assets::ArtifactRequest::DataType::SharedBlob };
        	auto reqRes = artifacts->ResolveRequests(MakeIteratorRange(&request, &request+1));
        	REQUIRE(reqRes.size() == 1);
			auto blob = reqRes[0]._sharedBlob;
			REQUIRE(blob != nullptr);
			REQUIRE(blob->size() >= sizeof(ShaderService::ShaderHeader));
			const auto& hdr = *(const ShaderService::ShaderHeader*)blob->data();
			REQUIRE(hdr._version == ShaderService::ShaderHeader::Version);
			REQUIRE(XlEqString(hdr._identifier, "ut-data/IncludeDirective.hlsl[SOME_DEFINE=1]"));
		}

		::Assets::Services::GetAsyncMan().GetIntermediateCompilers().DeregisterCompiler(compilerRegistration._registrationId);
		::Assets::MainFileSystem::GetMountingTree()->Unmount(mnt);
	}
}

