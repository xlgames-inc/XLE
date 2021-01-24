// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../UnitTestHelper.h"
#include "../../Assets/CompileAndAsyncManager.h"
#include "../../Assets/ICompileOperation.h"
#include "../../Assets/IArtifact.h"
#include "../../Assets/IntermediateCompilers.h"
#include "../../Assets/IntermediateAssets.h"
#include "../../Assets/MemoryFile.h"
#include "../../Assets/ChunkFile.h"
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

		virtual std::vector<OperationResult>	SerializeTarget(unsigned idx) override
        {
            assert(idx == 0);

            // Blobs written here will become chunks in the output file
            std::vector<OperationResult> result;
            {
                OperationResult opRes;
                opRes._type = Type_UnitTestArtifact;
                opRes._version = 1;
                opRes._name = "unitary-artifact";
                opRes._data = ::Assets::AsBlob("This is file data from TestCompileOperation for " + _initializer);
                result.emplace_back(std::move(opRes));
            }
            {
                OperationResult opRes;
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
                OperationResult opRes;
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

    TEST_CASE( "AssetCompilers-CompilerRegistration", "[assets]" )
    {
        // note -- consider decoupling Assets::IntermediatesStore from the main file system, so
        // we don't have to boot this here
        /*auto mountingTree = std::make_shared<::Assets::MountingTree>(s_defaultFilenameRules);
        // _utDataMount = _mountingTree->Mount("ut-data", ::Assets::CreateFileSystem_Memory(s_utData));
        ::Assets::MainFileSystem::Init(mountingTree, nullptr);*/

        UnitTest_SetWorkingDirectory();
		auto globalServices = ConsoleRig::MakeAttachablePtr<ConsoleRig::GlobalServices>(GetStartupConfig());

        auto tempDirPath = std::filesystem::temp_directory_path() / "xle-unit-tests";
        std::filesystem::create_directories(tempDirPath);

        auto intermediateStore = std::make_shared<::Assets::IntermediatesStore>(
            tempDirPath.string().c_str(),
            ConsoleRig::GetLibVersionDesc()._versionString,
            GetConfigString());
        auto compilers = std::make_shared<::Assets::IntermediateCompilers>(intermediateStore);

        SECTION("Register/Deregister")
        {
            uint64_t outputTypes[] = { Type_UnitTestArtifact };
            auto registration = compilers->RegisterCompiler(
                "unit-test-asset-.*",
                MakeIteratorRange(outputTypes),
                "UnitTestCompiler",
                ConsoleRig::GetLibVersionDesc(),
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
            auto artifacts = compile->GetArtifacts();
            REQUIRE(artifacts.size() == 1);

            {
                auto blobOfData = artifacts[0].second->GetBlob();
                auto memoryFile = ::Assets::CreateMemoryFile(blobOfData);
                auto chunkTable = ::Assets::ChunkFile::LoadChunkTable(*memoryFile);
                REQUIRE(chunkTable.size() == 2);

                auto mainChunkHdr = chunkTable[0];
                auto dataInside = StringSection<>(
                    (const char*)PtrAdd(blobOfData->data(), mainChunkHdr._fileOffset),
                    (const char*)PtrAdd(blobOfData->data(), mainChunkHdr._fileOffset+mainChunkHdr._size));
                REQUIRE(dataInside.AsString() == "This is file data from TestCompileOperation for unit-test-asset-one");

                auto extraChunkHdr = chunkTable[1];
                auto extraDataInside = StringSection<>(
                    (const char*)PtrAdd(blobOfData->data(), extraChunkHdr._fileOffset),
                    (const char*)PtrAdd(blobOfData->data(), extraChunkHdr._fileOffset+extraChunkHdr._size));
                REQUIRE(extraDataInside.AsString() == "This is extra file data");
            }

            compilers->DeregisterCompiler(registration._registrationId);    
        }

        // ::Assets::MainFileSystem::Shutdown();
    }
}

