// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Metal/MetalTestHelper.h"
#include "../Metal/MetalTestShaders.h"
#include "../../UnitTestHelper.h"
#include "../../EmbeddedRes.h"
#include "../../../ShaderParser/ShaderAnalysis.h"
#include "../../../RenderCore/Metal/Shader.h"
#include "../../../RenderCore/IDevice.h"
#include "../../../RenderCore/ShaderService.h"
#include "../../../Assets/IFileSystem.h"
#include "../../../Assets/MountingTree.h"
#include "../../../Assets/MemoryFile.h"
#include "../../../Assets/IArtifact.h"
#include "../../../Assets/CompileAndAsyncManager.h"
#include "../../../Assets/DeferredConstruction.h"
#include "../../../Assets/InitializerPack.h"
#include "../../../Assets/AssetServices.h"
#include "../../../Assets/IntermediatesStore.h"
#include "../../../Assets/AssetServices.h"
#include "../../../ConsoleRig/GlobalServices.h"
#include "../../../ConsoleRig/AttachablePtr.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"
#include <filesystem>

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

	TEST_CASE( "ShaderCompilation-Compile", "[rendercore_metal]" )
	{
		using namespace RenderCore;
		UnitTest_SetWorkingDirectory();
		auto _globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
		// auto assetServices = ConsoleRig::MakeAttachablePtr<::Assets::Services>(0);
		auto testHelper = MakeTestHelper();

		SECTION("UnitTestHelper shaders") {
			// Ensure that we can compile a shader from string input, via the MakeShaderProgram
			// utility function
			auto compiledFromString = testHelper->MakeShaderProgram(vsText_clipInput, psText);
			REQUIRE(compiledFromString.GetDependencyValidation() != nullptr);
		}

		// Let's load and compile a basic shader from a mounted filesystem
		// We'll use a custom shader source that will expand #include directives
		// for us (some gfx-apis can already do this, others need a little help)
		auto mnt = ::Assets::MainFileSystem::GetMountingTree()->Mount("ut-data", ::Assets::CreateFileSystem_Memory(s_utData));

		auto customShaderSource = std::make_shared<RenderCore::MinimalShaderSource>(
			CreateDefaultShaderCompiler(*testHelper->_device),
			std::make_shared<ExpandIncludesPreprocessor>());

		auto compilerRegistration = RenderCore::RegisterShaderCompiler(
			customShaderSource, 
			::Assets::Services::GetAsyncMan().GetIntermediateCompilers());

		SECTION("MinimalShaderSource") {
			auto compiledFromString = testHelper->MakeShaderProgram(vsText_clipInput, psText);
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
			auto artifacts = compiledFromFile->GetArtifactCollection(RenderCore::CompiledShaderByteCode::CompileProcessType);
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
			REQUIRE(hdr._identifier == std::string{"ut-data/IncludeDirective.hlsl-main[SOME_DEFINE=1]"});
		}

		::Assets::Services::GetAsyncMan().GetIntermediateCompilers().DeregisterCompiler(compilerRegistration._registrationId);
		::Assets::MainFileSystem::GetMountingTree()->Unmount(mnt);
	}

	class CountingShaderSource : public RenderCore::ShaderService::IShaderSource
	{
	public:
		ShaderByteCodeBlob CompileFromFile(
			const RenderCore::ILowLevelCompiler::ResId& resId, 
			StringSection<> definesTable) const override
		{
			++_compileFromFileCount;
			return _chain->CompileFromFile(resId, definesTable);
		}
		
		ShaderByteCodeBlob CompileFromMemory(
			StringSection<> shaderInMemory, StringSection<> entryPoint, 
			StringSection<> shaderModel, StringSection<> definesTable) const override
		{
			return _chain->CompileFromMemory(shaderInMemory, entryPoint, shaderModel, definesTable);
		}

		RenderCore::ILowLevelCompiler::ResId MakeResId(
			StringSection<> initializer) const override
		{
			return _chain->MakeResId(initializer);
		}

		std::string GenerateMetrics(
			IteratorRange<const void*> byteCodeBlob) const override
		{
			return _chain->GenerateMetrics(byteCodeBlob);
		}

		CountingShaderSource(const std::shared_ptr<RenderCore::ShaderService::IShaderSource>& chain)
		: _chain(chain), _compileFromFileCount(0) {}
		std::shared_ptr<RenderCore::ShaderService::IShaderSource> _chain;
		mutable std::atomic<unsigned> _compileFromFileCount = 0;
	};

	using ByteCodeFuture = ::Assets::AssetFuture<RenderCore::CompiledShaderByteCode>;
	static ByteCodeFuture BeginShaderCompile(StringSection<> fn)
	{
		static_assert(::Assets::Internal::AssetTraits<RenderCore::CompiledShaderByteCode>::HasCompileProcessType);
		static_assert(!::Assets::Internal::HasConstructToFutureOverride<RenderCore::CompiledShaderByteCode, const char*>::value);
		ByteCodeFuture byteCodeFuture("unit test compile for " + fn.AsString());
		::Assets::DefaultCompilerConstruction<RenderCore::CompiledShaderByteCode>(byteCodeFuture, RenderCore::CompiledShaderByteCode::CompileProcessType, fn);
		return byteCodeFuture;
	}
	
	template<typename Type>
		void StallAndRequireReady(::Assets::AssetFuture<Type>& future)
	{
		future.StallWhilePending();
		INFO(::Assets::AsString(future.GetActualizationLog()));
		REQUIRE(future.GetAssetState() == ::Assets::AssetState::Ready);
	}

	template<typename Type>
		void StallAndRequireInvalid(::Assets::AssetFuture<Type>& future)
	{
		future.StallWhilePending();
		REQUIRE(!future.GetActualizationLog()->empty());
		INFO(::Assets::AsString(future.GetActualizationLog()));
		REQUIRE(future.GetAssetState() == ::Assets::AssetState::Invalid);
	}

	TEST_CASE( "ShaderCompilation-ArchivingAndFlushing", "[rendercore_metal]" )
	{
		using namespace RenderCore;

		// ensure that we're beginning from clean temporaries directory for this test
		{
			auto tempDirPath = std::filesystem::temp_directory_path() / "xle-unit-tests";
			std::filesystem::remove_all(tempDirPath);	// ensure we're starting from an empty temporary directory
			std::filesystem::create_directories(tempDirPath);
		}

		auto _globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
		auto xleresmnt = ::Assets::MainFileSystem::GetMountingTree()->Mount("xleres", UnitTests::CreateEmbeddedResFileSystem());
		auto testHelper = MakeTestHelper();

		auto& compilers = ::Assets::Services::GetAsyncMan().GetIntermediateCompilers();
		auto countingShaderSource = std::make_shared<CountingShaderSource>(testHelper->_shaderSource);
		auto shaderCompilerRegistration = RenderCore::RegisterShaderCompiler(countingShaderSource, compilers);

		SECTION("Successful Compilation")
		{
			auto initialCount = countingShaderSource->_compileFromFileCount.load();
			auto byteCodeFuture = BeginShaderCompile("xleres/TechniqueLibrary/Standard/nopatches.vertex.hlsl:main:vs_*");
			StallAndRequireReady(byteCodeFuture);
			REQUIRE(countingShaderSource->_compileFromFileCount.load() == initialCount+1);
		}

		SECTION("Failed Compilation")
		{
			auto initialCount = countingShaderSource->_compileFromFileCount.load();
			// Compiling vertex shader as pixel shader = compile error
			auto byteCodeFuture = BeginShaderCompile("xleres/TechniqueLibrary/Standard/nopatches.vertex.hlsl:main:ps_*");
			StallAndRequireInvalid(byteCodeFuture);
			REQUIRE(countingShaderSource->_compileFromFileCount.load() == initialCount+1);
		}

		SECTION("Compile multiple in quick succession")
		{
			auto initialCount = countingShaderSource->_compileFromFileCount.load();
			std::vector<ByteCodeFuture> futures;
			for (unsigned c=0; c<5; ++c)
				futures.push_back(BeginShaderCompile("xleres/TechniqueLibrary/Standard/nopatches.vertex.hlsl:main:vs_*"));
			for (auto&byteCodeFuture:futures)
				StallAndRequireReady(byteCodeFuture);
			// We requested the same compile multiple times, but it should only actually do the low level compile once
			REQUIRE(countingShaderSource->_compileFromFileCount.load() == initialCount+1);

			// load up a few more, now that we know that the first few have definitely completed
			for (unsigned c=0; c<5; ++c)
				futures.push_back(BeginShaderCompile("xleres/TechniqueLibrary/Standard/nopatches.vertex.hlsl:main:vs_*"));
			for (auto&byteCodeFuture:futures)
				StallAndRequireReady(byteCodeFuture);
			// still no extra low level compiles
			REQUIRE(countingShaderSource->_compileFromFileCount.load() == initialCount+1);
		}

		SECTION("Compile and retrieve from cache")
		{
			auto initialCount = countingShaderSource->_compileFromFileCount.load();

			{
				auto initialSuccessfulFuture = BeginShaderCompile("xleres/TechniqueLibrary/Standard/nopatches.vertex.hlsl:main:vs_*");
				auto initialFailedFuture = BeginShaderCompile("xleres/TechniqueLibrary/Standard/nopatches.vertex.hlsl:main:ps_*");
				StallAndRequireReady(initialSuccessfulFuture);
				StallAndRequireInvalid(initialFailedFuture);
			}

			REQUIRE(countingShaderSource->_compileFromFileCount.load() == initialCount+2);
			compilers.FlushCachedMarkers();

			// Next should 2 retrieve from the cache with no further compiles

			{
				auto initialSuccessfulFuture = BeginShaderCompile("xleres/TechniqueLibrary/Standard/nopatches.vertex.hlsl:main:vs_*");
				auto initialFailedFuture = BeginShaderCompile("xleres/TechniqueLibrary/Standard/nopatches.vertex.hlsl:main:ps_*");
				StallAndRequireReady(initialSuccessfulFuture);
				StallAndRequireInvalid(initialFailedFuture);
			}

			REQUIRE(countingShaderSource->_compileFromFileCount.load() == initialCount+2);
			compilers.FlushCachedMarkers();

			// After a flush to disk, retrieving from the cache should still work

			::Assets::Services::GetAsyncMan().GetIntermediateStore()->FlushToDisk();

			{
				auto initialSuccessfulFuture = BeginShaderCompile("xleres/TechniqueLibrary/Standard/nopatches.vertex.hlsl:main:vs_*");
				auto initialFailedFuture = BeginShaderCompile("xleres/TechniqueLibrary/Standard/nopatches.vertex.hlsl:main:ps_*");
				StallAndRequireReady(initialSuccessfulFuture);
				StallAndRequireInvalid(initialFailedFuture);
			}

			REQUIRE(countingShaderSource->_compileFromFileCount.load() == initialCount+2);

			// modify the archive by compiling a 3d thing
			{
				auto additionalSuccessfulFuture = BeginShaderCompile("xleres/TechniqueLibrary/Standard/depthonly.pixel.hlsl:frameworkEntryDepthOnly:ps_*");
				StallAndRequireReady(additionalSuccessfulFuture);
			}

			REQUIRE(countingShaderSource->_compileFromFileCount.load() == initialCount+3);
			compilers.FlushCachedMarkers();
			::Assets::Services::GetAsyncMan().GetIntermediateStore()->FlushToDisk();

			{
				auto initialSuccessfulFuture = BeginShaderCompile("xleres/TechniqueLibrary/Standard/nopatches.vertex.hlsl:main:vs_*");
				auto initialFailedFuture = BeginShaderCompile("xleres/TechniqueLibrary/Standard/nopatches.vertex.hlsl:main:ps_*");
				auto additionalSuccessfulFuture = BeginShaderCompile("xleres/TechniqueLibrary/Standard/depthonly.pixel.hlsl:frameworkEntryDepthOnly:ps_*");
				StallAndRequireReady(initialSuccessfulFuture);
				StallAndRequireInvalid(initialFailedFuture);
				StallAndRequireReady(additionalSuccessfulFuture);
			}
		}

		::Assets::Services::GetAsyncMan().GetIntermediateStore()->FlushToDisk();
		compilers.DeregisterCompiler(shaderCompilerRegistration._registrationId);
		Assets::MainFileSystem::GetMountingTree()->Unmount(xleresmnt);
	}
	
}
