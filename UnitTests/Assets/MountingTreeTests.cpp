// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../Assets/MountingTree.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/NascentChunk.h"
#include "../../Assets/MemoryFile.h"
#include "../../Utility/Streams/PathUtils.h"
#include <stdexcept>
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"

using namespace Catch::literals;
namespace UnitTests
{
	static std::unordered_map<std::string, ::Assets::Blob> s_utData0 {
		std::make_pair(
			"exampleFileOne.file",
			::Assets::AsBlob(R"--(exampleFileOne-contents)--")),
		std::make_pair(
			"exampleFileTwo.file",
			::Assets::AsBlob(R"--(exampleFileTwo-contents)--")),
		std::make_pair(
			"internalFolder/exampleFileThree.file",
			::Assets::AsBlob(R"--(exampleFileThree-contents)--")),
		std::make_pair(
			"internalFolder/one/two/three/exampleFileFive.file",
			::Assets::AsBlob(R"--(exampleFileFive-contents)--"))
	};
	static std::unordered_map<std::string, ::Assets::Blob> s_utData1 {
		std::make_pair(
			"exampleFileFour.file",
			::Assets::AsBlob(R"--(exampleFileFour-contents)--")),
		std::make_pair(
			"one/two/three/exampleFileSix.file",
			::Assets::AsBlob(R"--(exampleFileSix-contents)--"))
	};

	TEST_CASE( "MountingTree-UnderlyingInterface", "[assets]" )
	{
		auto memoryFS0 = ::Assets::CreateFileSystem_Memory(s_utData0);
		auto memoryFS1 = ::Assets::CreateFileSystem_Memory(s_utData1);

		SECTION("Mount&Unmount")
		{
			auto mountingTree = std::make_shared<::Assets::MountingTree>(s_defaultFilenameRules);

			// Ensure we can mount and then unmount a FS in the tree
			auto mountID0 = mountingTree->Mount("ut-data", memoryFS0);
			REQUIRE(mountingTree->GetMountedFileSystem(mountID0) == memoryFS0.get());
			REQUIRE(mountingTree->GetMountPoint(mountID0) == "/ut-data/");
			mountingTree->Unmount(mountID0);
			REQUIRE(mountingTree->GetMountedFileSystem(mountID0) == nullptr);
			REQUIRE(mountingTree->GetMountPoint(mountID0).empty());

			// More complicated mounting points & mounting the same FS more than once
			auto mountID1 = mountingTree->Mount("/ut-data", memoryFS0);
			auto mountID2 = mountingTree->Mount("ut-data/subFolder0/", memoryFS0);
			auto mountID3 = mountingTree->Mount("/ut-data\\subFolder1\\subFolder2", memoryFS0);
			auto mountID4 = mountingTree->Mount("", memoryFS0);
			REQUIRE(mountingTree->GetMountedFileSystem(mountID1) == memoryFS0.get());
			REQUIRE(mountingTree->GetMountedFileSystem(mountID2) == memoryFS0.get());
			REQUIRE(mountingTree->GetMountedFileSystem(mountID3) == memoryFS0.get());
			REQUIRE(mountingTree->GetMountedFileSystem(mountID4) == memoryFS0.get());
			REQUIRE(mountingTree->GetMountPoint(mountID1) == "/ut-data/");
			REQUIRE(mountingTree->GetMountPoint(mountID2) == "/ut-data/subFolder0/");
			REQUIRE(mountingTree->GetMountPoint(mountID3) == "/ut-data/subFolder1/subFolder2/");
			REQUIRE(mountingTree->GetMountPoint(mountID4) == "/");
			mountingTree->Unmount(mountID1);
			mountingTree->Unmount(mountID2);
			mountingTree->Unmount(mountID3);
			mountingTree->Unmount(mountID4);
		}

		SECTION("Lookup candidate objects")
		{
			auto mountingTree = std::make_shared<::Assets::MountingTree>(s_defaultFilenameRules);
			auto mnt1 = mountingTree->Mount("ut-data", memoryFS0);

			{
				auto lookup = mountingTree->Lookup("ut-data/exampleFileOne.file");
				REQUIRE(lookup.IsGood());
				::Assets::MountingTree::CandidateObject obj;
				auto lookupResult = lookup.TryGetNext(obj);
				REQUIRE(lookupResult == ::Assets::MountingTree::EnumerableLookup::Result::Success);
				REQUIRE(obj._fileSystem == memoryFS0);
				REQUIRE(obj._mountPoint == "/ut-data/");
				lookupResult = lookup.TryGetNext(obj);
				REQUIRE(lookupResult == ::Assets::MountingTree::EnumerableLookup::Result::NoCandidates);

				lookup = mountingTree->Lookup("ut-data/missing-file.file");
				REQUIRE(lookup.IsGood());
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::NoCandidates);
			}

			auto mnt2 = mountingTree->Mount("ut-data/internalFolder", memoryFS1);

			{
				// FS name is "internalFolder/exampleFileThree.file", but mount is "ut-data"
				auto lookup = mountingTree->Lookup("ut-data/internalFolder/exampleFileThree.file");
				REQUIRE(lookup.IsGood());
				::Assets::MountingTree::CandidateObject obj;
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);

				// FS name is "exampleFileFour.file", but mount is "ut-data/internalFolder"
				lookup = mountingTree->Lookup("ut-data/internalFolder/exampleFileFour.file");
				REQUIRE(lookup.IsGood());
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);

				// a little deeper down in the filesystem tree
				lookup = mountingTree->Lookup("ut-data/internalFolder/one/two/three/exampleFileFive.file");
				REQUIRE(lookup.IsGood());
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);
				REQUIRE(obj._mountPoint == "/ut-data/");

				lookup = mountingTree->Lookup("ut-data/internalFolder/one/two/three/exampleFileSix.file");
				REQUIRE(lookup.IsGood());
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);
				REQUIRE(obj._mountPoint == "/ut-data/internalFolder/");
			}

			mountingTree->Unmount(mnt1);

			{
				// lookup should fail after unmount
				auto lookup = mountingTree->Lookup("ut-data/exampleFileOne.file");
				REQUIRE(lookup.IsGood());
				::Assets::MountingTree::CandidateObject obj;
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::NoCandidates);
			}

			mnt1 = mountingTree->Mount("", memoryFS0);

			{
				// mounting at root should also work
				auto lookup = mountingTree->Lookup("exampleFileOne.file");
				REQUIRE(lookup.IsGood());
				::Assets::MountingTree::CandidateObject obj;
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);
			}

			mountingTree->Unmount(mnt1);
			mountingTree->Unmount(mnt2);
		}

		SECTION("Lookup awkward paths")
		{
			auto mountingTree = std::make_shared<::Assets::MountingTree>(s_defaultFilenameRules);
			auto mnt1 = mountingTree->Mount("ut-data", memoryFS0);
			auto mnt2 = mountingTree->Mount("ut-data/internalFolder", memoryFS1);

			::Assets::MountingTree::CandidateObject obj;
			SECTION("utf8") {
				// "./" at the end of the of the mounting path
				auto lookup = mountingTree->Lookup("ut-data/././internalFolder/exampleFileThree.file");
				REQUIRE(lookup.IsGood());
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);
				REQUIRE(obj._mountPoint == "/ut-data/");

				// "./" at the very start
				lookup = mountingTree->Lookup("./ut-data/internalFolder/exampleFileThree.file");
				REQUIRE(lookup.IsGood());
				REQUIRE(!lookup.IsAbsolutePath());
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);
				REQUIRE(obj._mountPoint == "/ut-data/");

				// "./" in the middle of the mounting name
				lookup = mountingTree->Lookup("ut-data/./internalFolder/exampleFileFour.file");
				REQUIRE(lookup.IsGood());
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);
				REQUIRE(obj._mountPoint == "/ut-data/internalFolder/");

				// "../" to drop back to an earlier directory (effectively ignoring the "internalFolder/" part)
				lookup = mountingTree->Lookup("ut-data/internalFolder/../exampleFileOne.file");
				REQUIRE(lookup.IsGood());
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);
				REQUIRE(obj._mountPoint == "/ut-data/");

				// "./" or "../" within the mounted filesystem part may or may not work depending on the 
				// filesystem implementation

				// When the mounting tree "absolute path mode" is set to MountingTree, the absolute
				// root is the root of the mounting tree (not the root of the raw FS)
				lookup = mountingTree->Lookup("/ut-data/internalFolder/exampleFileThree.file");
				REQUIRE(lookup.IsGood());
				REQUIRE(lookup.IsAbsolutePath());
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);
				REQUIRE(obj._mountPoint == "/ut-data/");
			}

			SECTION("utf16") {
				// "./" at the end of the of the mounting path
				auto lookup = mountingTree->Lookup(u"ut-data/././internalFolder/exampleFileThree.file");
				REQUIRE(lookup.IsGood());
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);
				REQUIRE(obj._mountPoint == "/ut-data/");

				// "./" at the very start
				lookup = mountingTree->Lookup(u"./ut-data/internalFolder/exampleFileThree.file");
				REQUIRE(lookup.IsGood());
				REQUIRE(!lookup.IsAbsolutePath());
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);
				REQUIRE(obj._mountPoint == "/ut-data/");

				// "./" in the middle of the mounting name
				lookup = mountingTree->Lookup(u"ut-data/./internalFolder/exampleFileFour.file");
				REQUIRE(lookup.IsGood());
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);
				REQUIRE(obj._mountPoint == "/ut-data/internalFolder/");

				// "../" to drop back to an earlier directory (effectively ignoring the "internalFolder/" part)
				lookup = mountingTree->Lookup(u"ut-data/internalFolder/../exampleFileOne.file");
				REQUIRE(lookup.IsGood());
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);
				REQUIRE(obj._mountPoint == "/ut-data/");

				// "./" or "../" within the mounted filesystem part may or may not work depending on the 
				// filesystem implementation

				// When the mounting tree "absolute path mode" is set to MountingTree, the absolute
				// root is the root of the mounting tree (not the root of the raw FS)
				lookup = mountingTree->Lookup(u"/ut-data/internalFolder/exampleFileThree.file");
				REQUIRE(lookup.IsGood());
				REQUIRE(lookup.IsAbsolutePath());
				REQUIRE(lookup.TryGetNext(obj) == ::Assets::MountingTree::EnumerableLookup::Result::Success);
				REQUIRE(obj._mountPoint == "/ut-data/");
			}

			mountingTree->Unmount(mnt2);
			mountingTree->Unmount(mnt1);
		}
	}
}
