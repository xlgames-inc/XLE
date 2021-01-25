// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../UnitTestHelper.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/ICompileOperation.h"
#include "../../Assets/IArtifact.h"
#include "../../Assets/IntermediateCompilers.h"
#include "../../Assets/IntermediatesStore.h"
#include "../../Assets/MemoryFile.h"
#include "../../Assets/ChunkFile.h"
#include "../../Assets/AssetTraits.h"
#include "../../ConsoleRig/GlobalServices.h"
#include "../../ConsoleRig/AttachablePtr.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/StringUtils.h"
#include <stdexcept>
#include <filesystem>
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"

#include "../../Assets/IFileSystem.h"
#include "../../Assets/MountingTree.h"

using namespace Catch::literals;
namespace UnitTests
{
	static uint64_t Type_UnitTestArtifact = ConstHash64<'unit', 'test', 'arti', 'fact'>::Value;
	static uint64_t Type_UnitTestExtraArtifact = ConstHash64<'unit', 'test', 'extr', 'a'>::Value;

	class TestCompileOperation : public ::Assets::ICompileOperation
	{
	public:
		std::string _initializer;
		virtual std::vector<TargetDesc>			GetTargets() const override
		{
			return {
				TargetDesc { Type_UnitTestArtifact, "unitary-artifact" }
			};
		}

		static unsigned s_serializeTargetCount;

		virtual std::vector<SerializedArtifact>	SerializeTarget(unsigned idx) override
		{
			assert(idx == 0);
			++s_serializeTargetCount;

			// Blobs written here will become chunks in the output file
			std::vector<SerializedArtifact> result;
			{
				SerializedArtifact opRes;
				opRes._type = Type_UnitTestArtifact;
				opRes._version = 1;
				opRes._name = "unitary-artifact";
				opRes._data = ::Assets::AsBlob("This is file data from TestCompileOperation for " + _initializer);
				result.emplace_back(std::move(opRes));
			}
			{
				SerializedArtifact opRes;
				opRes._type = Type_UnitTestExtraArtifact;
				opRes._version = 1;
				opRes._name = "unitary-artifact-extra";
				opRes._data = ::Assets::AsBlob("This is extra file data");
				result.emplace_back(std::move(opRes));
			}

			// we can optionally write a "metrics" chunk. This doesn't actually get returned to the
			// client that initiated the compile. It's just gets written to the intermediate assets
			// store and is mostly used for debugging purposes
			{
				SerializedArtifact opRes;
				opRes._type = ConstHash64<'Metr', 'ics'>::Value;
				opRes._version = 1;
				opRes._name = "unitary-artifact-metrics";
				opRes._data = ::Assets::AsBlob("This is file data from TestCompileOperation for " + _initializer);
				result.emplace_back(std::move(opRes));
			}
			return result;
		}

		virtual std::vector<::Assets::DependentFileState> GetDependencies() const override
		{
			return {};
		}

		TestCompileOperation(StringSection<> initializer) : _initializer(initializer.AsString()) {}
	};

	unsigned TestCompileOperation::s_serializeTargetCount = 0;

	class TestChunkRequestsAsset
	{
	public:
		static const ::Assets::ArtifactRequest ChunkRequests[2];

		std::string _data0, _data1;
		TestChunkRequestsAsset(
			IteratorRange<::Assets::ArtifactRequestResult*> chunks, 
			const ::Assets::DepValPtr& depVal)
		{
			_data0 = ::Assets::AsString(chunks[0]._sharedBlob);
			_data1 = ::Assets::AsString(chunks[1]._sharedBlob);
		}
	};

	const ::Assets::ArtifactRequest TestChunkRequestsAsset::ChunkRequests[]
    {
        ::Assets::ArtifactRequest { "unitary-artifact", Type_UnitTestArtifact, 1, ::Assets::ArtifactRequest::DataType::SharedBlob },
		::Assets::ArtifactRequest { "unitary-artifact-extra", Type_UnitTestExtraArtifact, 1, ::Assets::ArtifactRequest::DataType::SharedBlob },
    };

	static const char* GetConfigString()
	{
		#if defined(_DEBUG)
			#if TARGET_64BIT
				return "d64";
			#else
				return "d";
			#endif
		#else
			#if TARGET_64BIT
				return "r64";
			#else
				return "r";
			#endif
		#endif
	}

	TEST_CASE( "AssetCompilers-BasicCompilers", "[assets]" )
	{
		//
		// IntermediateCompilers provides a mechanism for running pre-processing operations on
		// data files in order to prepare them for the final format 
		// 
		UnitTest_SetWorkingDirectory();
		auto globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());
		
		auto compilers = std::make_shared<::Assets::IntermediateCompilers>(nullptr);

		SECTION("Register/Deregister")
		{
			uint64_t outputTypes[] = { Type_UnitTestArtifact };
			auto registration = compilers->RegisterCompiler(
				"unit-test-asset-.*",
				MakeIteratorRange(outputTypes),
				"UnitTestCompiler",
				ConsoleRig::GetLibVersionDesc(),
				nullptr,
				[](StringSection<> initializer) {
					return std::make_shared<TestCompileOperation>(initializer);
				});

			StringSection<> initializers[] = { "unit-test-asset-one" };
			auto marker = compilers->Prepare(Type_UnitTestArtifact, initializers, dimof(initializers));
			REQUIRE(marker != nullptr);
			REQUIRE(marker->GetExistingAsset() == nullptr);
			REQUIRE(marker->Initializer().AsString() == "unit-test-asset-one");

			auto compile = marker->InvokeCompile();
			REQUIRE(compile != nullptr);

			compile->StallWhilePending();
			REQUIRE(compile->GetAssetState() == ::Assets::AssetState::Ready);

			SECTION("SuccessfulResolveRequests")
			{
				//
				// Resolve artifacts via an explicit call to ResolveRequests
				//
				::Assets::ArtifactRequest requests[] {
					::Assets::ArtifactRequest {
						"unitary-artifact", Type_UnitTestArtifact, 1,
						::Assets::ArtifactRequest::DataType::SharedBlob
					},
					::Assets::ArtifactRequest {
						"unitary-artifact-extra", Type_UnitTestExtraArtifact, 1,
						::Assets::ArtifactRequest::DataType::SharedBlob
					}
				};
				auto artifacts = compile->GetArtifactCollection()->ResolveRequests(MakeIteratorRange(requests));
				REQUIRE(artifacts.size() == 2);
				REQUIRE(::Assets::AsString(artifacts[0]._sharedBlob) == "This is file data from TestCompileOperation for unit-test-asset-one");
				REQUIRE(::Assets::AsString(artifacts[1]._sharedBlob) == "This is extra file data");

				//
				// Resolve artifacts implicitly via calling ::Assets::AutoConstructAsset
				// The ChunkRequests array within TestChunkRequestsAsset is used to bind input artifacts
				//
				auto implicitlyConstructed = ::Assets::AutoConstructAsset<TestChunkRequestsAsset>(*compile->GetArtifactCollection());
				REQUIRE(implicitlyConstructed->_data0 == "This is file data from TestCompileOperation for unit-test-asset-one");
				REQUIRE(implicitlyConstructed->_data1 == "This is extra file data");
			}

			SECTION("FailedResolveRequests")
			{
				REQUIRE_THROWS([&]() {
					// Fails because the version number requested doesn't match what's provided
					// (this is case requested version number is higher)
					::Assets::ArtifactRequest requests[] {
						::Assets::ArtifactRequest {
							"unitary-artifact", Type_UnitTestArtifact, 2,
							::Assets::ArtifactRequest::DataType::SharedBlob
						}
					};
					compile->GetArtifactCollection()->ResolveRequests(MakeIteratorRange(requests));
				}());
				REQUIRE_THROWS([&]() {
					// Fails because the type code requested doesn't match (note name ignored)
					::Assets::ArtifactRequest requests[] {
						::Assets::ArtifactRequest {
							"unitary-artifact", Type_UnitTestArtifact + 5, 1,
							::Assets::ArtifactRequest::DataType::SharedBlob
						}
					};
					compile->GetArtifactCollection()->ResolveRequests(MakeIteratorRange(requests));
				}());
				REQUIRE_THROWS([&]() {
					// Fails because the same type code is repeated multiple times in the request
					::Assets::ArtifactRequest requests[] {
						::Assets::ArtifactRequest {
							"unitary-artifact", Type_UnitTestArtifact, 1,
							::Assets::ArtifactRequest::DataType::SharedBlob
						},
						::Assets::ArtifactRequest {
							"unitary-artifact-two", Type_UnitTestArtifact, 1,
							::Assets::ArtifactRequest::DataType::SharedBlob
						}
					};
					compile->GetArtifactCollection()->ResolveRequests(MakeIteratorRange(requests));
				}());
			}

			compilers->DeregisterCompiler(registration._registrationId);
		}

		SECTION("Compiler marker management")
		{
			uint64_t outputTypes[] = { Type_UnitTestArtifact };
			auto registration = compilers->RegisterCompiler(
				"unit-test-asset-.*",
				MakeIteratorRange(outputTypes),
				"UnitTestCompiler",
				ConsoleRig::GetLibVersionDesc(),
				nullptr,
				[](StringSection<> initializer) {
					return std::make_shared<TestCompileOperation>(initializer);
				});

			StringSection<> initializers0[] = { "unit-test-asset-one" };
			StringSection<> initializers1[] = { "unit-test-asset-two" };
			auto marker0 = compilers->Prepare(Type_UnitTestArtifact, initializers0, dimof(initializers0));
			auto marker1 = compilers->Prepare(Type_UnitTestArtifact, initializers0, dimof(initializers0));
			auto marker2 = compilers->Prepare(Type_UnitTestArtifact, initializers1, dimof(initializers1));
			REQUIRE(marker0 == marker1);
			REQUIRE(marker0 != marker2);

			compilers->DeregisterCompiler(registration._registrationId);

		}
	}

	TEST_CASE( "AssetCompilers-IntermediatesStore", "[assets]" )
	{
		// note -- consider decoupling Assets::IntermediatesStore from the main file system, so
		// we don't have to boot this here
		/*auto mountingTree = std::make_shared<::Assets::MountingTree>(s_defaultFilenameRules);
		// _utDataMount = _mountingTree->Mount("ut-data", ::Assets::CreateFileSystem_Memory(s_utData));
		::Assets::MainFileSystem::Init(mountingTree, nullptr);*/

		UnitTest_SetWorkingDirectory();
		auto globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());

		auto tempDirPath = std::filesystem::temp_directory_path() / "xle-unit-tests";
		std::filesystem::remove_all(tempDirPath);	// ensure we're starting from an empty temporary directory
		std::filesystem::create_directories(tempDirPath);

		auto intermediateStore = std::make_shared<::Assets::IntermediatesStore>(
			tempDirPath.string().c_str(),
			ConsoleRig::GetLibVersionDesc()._versionString,
			GetConfigString());
		auto compilers = std::make_shared<::Assets::IntermediateCompilers>(intermediateStore);

		uint64_t outputTypes[] = { Type_UnitTestArtifact };
		auto registration = compilers->RegisterCompiler(
			"unit-test-asset-.*",
			MakeIteratorRange(outputTypes),
			"UnitTestCompiler",
			ConsoleRig::GetLibVersionDesc(),
			nullptr,
			[](StringSection<> initializer) {
				return std::make_shared<TestCompileOperation>(initializer);
			});

		::Assets::ArtifactRequest requests[] {
			::Assets::ArtifactRequest {
				"unitary-artifact", Type_UnitTestArtifact, 1,
				::Assets::ArtifactRequest::DataType::SharedBlob
			},
			::Assets::ArtifactRequest {
				"unitary-artifact-extra", Type_UnitTestExtraArtifact, 1,
				::Assets::ArtifactRequest::DataType::SharedBlob
			}
		};

		SECTION("Cache compile result")
		{
			unsigned initialSerializeTargetCount = TestCompileOperation::s_serializeTargetCount;
				
			StringSection<> initializers[] = { "unit-test-asset-one" };
			auto marker = compilers->Prepare(Type_UnitTestArtifact, initializers, dimof(initializers));
			REQUIRE(marker != nullptr);
			REQUIRE(marker->GetExistingAsset() == nullptr);

			auto compile = marker->InvokeCompile();
			REQUIRE(compile != nullptr);

			compile->StallWhilePending();
			REQUIRE(compile->GetAssetState() == ::Assets::AssetState::Ready);
			
			auto artifacts = compile->GetArtifactCollection()->ResolveRequests(MakeIteratorRange(requests));
			REQUIRE(artifacts.size() == 2);
			REQUIRE(::Assets::AsString(artifacts[0]._sharedBlob) == "This is file data from TestCompileOperation for unit-test-asset-one");
			REQUIRE(::Assets::AsString(artifacts[1]._sharedBlob) == "This is extra file data");
			REQUIRE(TestCompileOperation::s_serializeTargetCount == initialSerializeTargetCount+1);

			// Now GetExistingAsset() on the same marker should give us something immediately
			auto existingAsset = marker->GetExistingAsset();
			REQUIRE(existingAsset != nullptr);
			REQUIRE(existingAsset->GetDependencyValidation()->GetValidationIndex() == 0);	// still clean
			artifacts = existingAsset->ResolveRequests(MakeIteratorRange(requests));
			REQUIRE(::Assets::AsString(artifacts[0]._sharedBlob) == "This is file data from TestCompileOperation for unit-test-asset-one");
			REQUIRE(::Assets::AsString(artifacts[1]._sharedBlob) == "This is extra file data");
			REQUIRE(TestCompileOperation::s_serializeTargetCount == initialSerializeTargetCount+1);

			// We can also go all the way back to the Prepare() function and expect an existing asset this time
			compile = nullptr;
			marker = nullptr;
			marker = compilers->Prepare(Type_UnitTestArtifact, initializers, dimof(initializers));
			REQUIRE(marker != nullptr);
			existingAsset = marker->GetExistingAsset();
			REQUIRE(existingAsset != nullptr);
			REQUIRE(existingAsset->GetDependencyValidation()->GetValidationIndex() == 0);	// still clean
			artifacts = existingAsset->ResolveRequests(MakeIteratorRange(requests));
			REQUIRE(::Assets::AsString(artifacts[0]._sharedBlob) == "This is file data from TestCompileOperation for unit-test-asset-one");
			REQUIRE(::Assets::AsString(artifacts[1]._sharedBlob) == "This is extra file data");
			REQUIRE(TestCompileOperation::s_serializeTargetCount == initialSerializeTargetCount+1);
		}

		compilers->DeregisterCompiler(registration._registrationId);
		::Assets::MainFileSystem::Shutdown();
	}
}

