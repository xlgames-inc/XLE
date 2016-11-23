// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "IFileSystem.h"
#include "MountingTree.h"
#include "../Utility/Streams/PathUtils.h"

namespace Assets
{
	static std::shared_ptr<MountingTree> s_mainMountingTree = std::make_shared<MountingTree>(s_defaultFilenameRules);
	static std::shared_ptr<IFileSystem> s_defaultFileSystem;

	static IFileSystem::IOReason AsIOReason(IFileSystem::TranslateResult transResult)
	{
		switch (transResult) {
		case IFileSystem::TranslateResult::Mounting: return IFileSystem::IOReason::Mounting;
		case IFileSystem::TranslateResult::Invalid: return IFileSystem::IOReason::Invalid;
		default: return IFileSystem::IOReason::Invalid;
		}
	}

	static FileDesc::State AsFileState(IFileSystem::TranslateResult transResult)
	{
		switch (transResult) {
		case IFileSystem::TranslateResult::Mounting: return FileDesc::State::Mounting;
		case IFileSystem::TranslateResult::Invalid: return FileDesc::State::Invalid;
		default: return FileDesc::State::Invalid;
		}
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename ResultFile>
		IFileSystem::IOReason TryOpen(ResultFile& result, MountingTree::EnumerableLookup& lookup, const char openMode[], FileShareMode::BitField shareMode)
	{
		// note -- exception throw is an issue here.
		// When we have overlapping mount points, we continue until we get a successful open.
		// But if all possibilities fail, what exception do we throw? Should we stop searching through
		// mount points for some errors while openning -- and if so, which ones?

		result = ResultFile();

		using LookupResult = MountingTree::EnumerableLookup::Result;
		for (;;) {
			MountingTree::CandidateObject object;
			auto r = lookup.TryGetNext(object);
			if (r == LookupResult::Invalidated)
				Throw(std::logic_error("Mounting point lookup was invalidated when the mounting tree changed. Do not change the mount or unmount filesystems while other threads may be accessing the same mounting tree."));

			if (r == LookupResult::NoCandidates)
				break;	// could not find an object to match this request

			assert(r == LookupResult::Success && object._fileSystem);
			return object._fileSystem->TryOpen(result, object._marker, openMode, shareMode);
		}

		// We just get the error information from the last open attempt (which might not be the most
		// interesting attempt from the point of view of the caller)
		return IFileSystem::IOReason::FileNotFound;
	}

	template<typename ResultFile>
		IFileSystem::IOReason TryOpen(ResultFile& result, MountingTree::EnumerableLookup& lookup, uint64 size, const char openMode[], FileShareMode::BitField shareMode)
	{
		// note -- exception throw is an issue here.
		// When we have overlapping mount points, we continue until we get a successful open.
		// But if all possibilities fail, what exception do we throw? Should we stop searching through
		// mount points for some errors while openning -- and if so, which ones?

		result = ResultFile();

		using LookupResult = MountingTree::EnumerableLookup::Result;
		for (;;) {
			MountingTree::CandidateObject object;
			auto r = lookup.TryGetNext(object);
			if (r == LookupResult::Invalidated)
				Throw(std::logic_error("Mounting point lookup was invalidated when the mounting tree changed. Do not change the mount or unmount filesystems while other threads may be accessing the same mounting tree."));

			if (r == LookupResult::NoCandidates)
				break;	// could not find an object to match this request

			assert(r == LookupResult::Success && object._fileSystem);
			return object._fileSystem->TryOpen(result, object._marker, size, openMode, shareMode);
		}

		// We just get the error information from the last open attempt (which might not be the most
		// interesting attempt from the point of view of the caller)
		return IFileSystem::IOReason::FileNotFound;
	}

	IFileSystem::IOReason TryMonitor(MountingTree::EnumerableLookup& lookup, const std::shared_ptr<IFileMonitor>& evnt)
	{
		auto lastIOReason = IFileSystem::IOReason::FileNotFound;
		using LookupResult = MountingTree::EnumerableLookup::Result;
		for (;;) {
			MountingTree::CandidateObject object;
			auto r = lookup.TryGetNext(object);
			if (r == LookupResult::Invalidated)
				Throw(std::logic_error("Mounting point lookup was invalidated when the mounting tree changed. Do not change the mount or unmount filesystems while other threads may be accessing the same mounting tree."));

			if (r == LookupResult::NoCandidates)
				break;	// could not find an object to match this request

			assert(r == LookupResult::Success && object._fileSystem);
			std::unique_ptr<IFileInterface> fileInterface;
			lastIOReason = object._fileSystem->TryMonitor(object._marker, evnt);
			if (lastIOReason == IFileSystem::IOReason::Success)
				return IFileSystem::IOReason::Success;
		}

		return lastIOReason;
	}

	FileDesc TryGetDesc(MountingTree::EnumerableLookup& lookup)
	{
		using LookupResult = MountingTree::EnumerableLookup::Result;
		for (;;) {
			MountingTree::CandidateObject object;
			auto r = lookup.TryGetNext(object);
			if (r == LookupResult::Invalidated)
				Throw(std::logic_error("Mounting point lookup was invalidated when the mounting tree changed. Do not change the mount or unmount filesystems while other threads may be accessing the same mounting tree."));

			if (r == LookupResult::NoCandidates)
				break;	// could not find an object to match this request

			assert(r == LookupResult::Success && object._fileSystem);
			std::unique_ptr<IFileInterface> fileInterface;
			auto desc = object._fileSystem->TryGetDesc(object._marker);
			if (desc._state != FileDesc::State::DoesNotExist)
				return desc;
		}

		return FileDesc{ std::basic_string<utf8>(), FileDesc::State::DoesNotExist };
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	//
	// note -- the UTF8 and UTF16 versions of these functions are identical... They could be implemented
	//			with a template. But the C++ method resolution works better when they are explicitly separated
	//			like this.
	//		eg, because 
	//			MainFileSystem::FileOpen(u("SomeFile.txt"),...);
	//		relies on automatic conversion for StringSection<utf8>, it works in this case, but not in the
	//		template case.
	//

	auto MainFileSystem::TryOpen(std::unique_ptr<IFileInterface>& result, StringSection<utf8> filename, const char openMode[], FileShareMode::BitField shareMode) -> IOReason
	{
		result = nullptr;

		auto lookup = s_mainMountingTree->Lookup(filename);
		if (lookup.IsGood())
			return ::Assets::TryOpen(result, lookup, openMode, shareMode);

		// attempt opening with the default file system...
		if (s_defaultFileSystem) {
			return ::Assets::TryOpen(result, *s_defaultFileSystem, filename, openMode, shareMode);
		} else
			return IOReason::FileNotFound;
	}

	auto MainFileSystem::TryOpen(BasicFile& result, StringSection<utf8> filename, const char openMode[], FileShareMode::BitField shareMode) -> IOReason
	{
		result = BasicFile();

		auto lookup = s_mainMountingTree->Lookup(filename);
		if (lookup.IsGood())
			return ::Assets::TryOpen(result, lookup, openMode, shareMode);

		// attempt opening with the default file system...
		if (s_defaultFileSystem) {
			return ::Assets::TryOpen(result, *s_defaultFileSystem, filename, openMode, shareMode);
		} else
			return IOReason::FileNotFound;
	}

	auto MainFileSystem::TryOpen(MemoryMappedFile& result, StringSection<utf8> filename, uint64 size, const char openMode[], FileShareMode::BitField shareMode) -> IOReason
	{
		result = MemoryMappedFile();

		auto lookup = s_mainMountingTree->Lookup(filename);
		if (lookup.IsGood())
			return ::Assets::TryOpen(result, lookup, size, openMode, shareMode);

		// attempt opening with the default file system...
		if (s_defaultFileSystem) {
			return ::Assets::TryOpen(result, *s_defaultFileSystem, filename, size, openMode, shareMode);
		} else
			return IOReason::FileNotFound;
	}

	IFileSystem::IOReason MainFileSystem::TryMonitor(StringSection<utf8> filename, const std::shared_ptr<IFileMonitor>& evnt)
	{
		auto lookup = s_mainMountingTree->Lookup(filename);
		if (lookup.IsGood())
			return ::Assets::TryMonitor(lookup, evnt);

		if (s_defaultFileSystem) 
			return ::Assets::TryMonitor(*s_defaultFileSystem, filename, evnt);
		return IFileSystem::IOReason::FileNotFound;
	}

	FileDesc MainFileSystem::TryGetDesc(StringSection<utf8> filename)
	{
		auto lookup = s_mainMountingTree->Lookup(filename);
		if (lookup.IsGood())
			return ::Assets::TryGetDesc(lookup);

		if (s_defaultFileSystem)
			return ::Assets::TryGetDesc(*s_defaultFileSystem, filename);

		return FileDesc{ std::basic_string<utf8>(), FileDesc::State::DoesNotExist };
	}

	auto MainFileSystem::TryOpen(std::unique_ptr<IFileInterface>& result, StringSection<utf16> filename, const char openMode[], FileShareMode::BitField shareMode) -> IOReason
	{
		result = nullptr;

		auto lookup = s_mainMountingTree->Lookup(filename);
		if (lookup.IsGood())
			return ::Assets::TryOpen(result, lookup, openMode, shareMode);

		// attempt opening with the default file system...
		if (s_defaultFileSystem) {
			return ::Assets::TryOpen(result, *s_defaultFileSystem, filename, openMode, shareMode);
		} else
			return IOReason::FileNotFound;
	}

	IFileSystem::IOReason MainFileSystem::TryMonitor(StringSection<utf16> filename, const std::shared_ptr<IFileMonitor>& evnt)
	{
		auto lookup = s_mainMountingTree->Lookup(filename);
		if (lookup.IsGood())
			return ::Assets::TryMonitor(lookup, evnt);

		if (s_defaultFileSystem)
			return ::Assets::TryMonitor(*s_defaultFileSystem, filename, evnt);
		return IFileSystem::IOReason::FileNotFound;
	}

	FileDesc MainFileSystem::TryGetDesc(StringSection<utf16> filename)
	{
		auto lookup = s_mainMountingTree->Lookup(filename);
		if (lookup.IsGood())
			return ::Assets::TryGetDesc(lookup);

		if (s_defaultFileSystem)
			return ::Assets::TryGetDesc(*s_defaultFileSystem, filename);

		return FileDesc{ std::basic_string<utf8>(), FileDesc::State::DoesNotExist };
	}

	BasicFile MainFileSystem::OpenBasicFile(StringSection<utf8> filename, const char openMode[], FileShareMode::BitField shareMode)
	{
		BasicFile result;
		auto ioRes = TryOpen(result, filename, openMode, shareMode);
		if (ioRes != IOReason::Success)
			Throw(Utility::Exceptions::IOException(ioRes, "Failure while opening file (%s) in mode (%s)", std::string((const char*)filename.begin(), (const char*)filename.end()).c_str(), openMode));
		return std::move(result);
	}

	MemoryMappedFile MainFileSystem::OpenMemoryMappedFile(StringSection<utf8> filename, uint64 size, const char openMode[], FileShareMode::BitField shareMode)
	{
		MemoryMappedFile result;
		auto ioRes = TryOpen(result, filename, size, openMode, shareMode);
		if (ioRes != IOReason::Success)
			Throw(Utility::Exceptions::IOException(ioRes, "Failure while opening file (%s) in mode (%s)", std::string((const char*)filename.begin(), (const char*)filename.end()).c_str(), openMode));
		return std::move(result);
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	const std::shared_ptr<MountingTree>& MainFileSystem::GetMountingTree() { return s_mainMountingTree; }
	void MainFileSystem::SetDefaultFileSystem(std::shared_ptr<IFileSystem> fs)
	{
		s_defaultFileSystem = std::move(fs);
	}

	T2(CharType, FileObject) IFileSystem::IOReason TryOpen(FileObject& result, IFileSystem& fs, StringSection<CharType> fn, const char openMode[], FileShareMode::BitField shareMode)
	{
		result = FileObject();

		IFileSystem::Marker marker;
		auto transResult = fs.TryTranslate(marker, fn);
		if (transResult == IFileSystem::TranslateResult::Success)
			return fs.TryOpen(result, marker, openMode);

		return AsIOReason(transResult);
	}

	T2(CharType, FileObject) IFileSystem::IOReason TryOpen(FileObject& result, IFileSystem& fs, StringSection<CharType> fn, uint64 size, const char openMode[], FileShareMode::BitField shareMode)
	{
		result = FileObject();

		IFileSystem::Marker marker;
		auto transResult = fs.TryTranslate(marker, fn);
		if (transResult == IFileSystem::TranslateResult::Success)
			return fs.TryOpen(result, marker, size, openMode);

		return AsIOReason(transResult);
	}

	T1(CharType) IFileSystem::IOReason TryMonitor(IFileSystem& fs, StringSection<CharType> fn, const std::shared_ptr<IFileMonitor>& evnt)
	{
		IFileSystem::Marker marker;
		auto transResult = fs.TryTranslate(marker, fn);
		if (transResult == IFileSystem::TranslateResult::Success)
			return fs.TryMonitor(marker, evnt);
		return AsIOReason(transResult);
	}

	T1(CharType) FileDesc TryGetDesc(IFileSystem& fs, StringSection<CharType> fn)
	{
		IFileSystem::Marker marker;
		auto transResult = fs.TryTranslate(marker, fn);
		if (transResult == IFileSystem::TranslateResult::Success)
			return fs.TryGetDesc(marker);
		return FileDesc{std::basic_string<utf8>(), AsFileState(transResult)};
	}

	template IFileSystem::IOReason TryOpen<utf8, std::unique_ptr<IFileInterface>>(std::unique_ptr<IFileInterface>& result, IFileSystem& fs, StringSection<utf8> fn, const char openMode[], FileShareMode::BitField shareMode);
	template IFileSystem::IOReason TryOpen<utf8, BasicFile>(BasicFile& result, IFileSystem& fs, StringSection<utf8> fn, const char openMode[], FileShareMode::BitField shareMode);
	template IFileSystem::IOReason TryOpen<utf8, MemoryMappedFile>(MemoryMappedFile& result, IFileSystem& fs, StringSection<utf8> fn, uint64 size, const char openMode[], FileShareMode::BitField shareMode);
	template IFileSystem::IOReason TryMonitor<utf8>(IFileSystem& fs, StringSection<utf8> fn, const std::shared_ptr<IFileMonitor>& evnt);
	template FileDesc TryGetDesc<utf8>(IFileSystem& fs, StringSection<utf8> fn);
	template IFileSystem::IOReason TryOpen<utf16, std::unique_ptr<IFileInterface>>(std::unique_ptr<IFileInterface>& result, IFileSystem& fs, StringSection<utf16> fn, const char openMode[], FileShareMode::BitField shareMode);
	template IFileSystem::IOReason TryOpen<utf16, BasicFile>(BasicFile& result, IFileSystem& fs, StringSection<utf16> fn, const char openMode[], FileShareMode::BitField shareMode);
	template IFileSystem::IOReason TryOpen<utf16, MemoryMappedFile>(MemoryMappedFile& result, IFileSystem& fs, StringSection<utf16> fn, uint64 size, const char openMode[], FileShareMode::BitField shareMode);
	template IFileSystem::IOReason TryMonitor<utf16>(IFileSystem& fs, StringSection<utf16> fn, const std::shared_ptr<IFileMonitor>& evnt);
	template FileDesc TryGetDesc<utf16>(IFileSystem& fs, StringSection<utf16> fn);

	std::unique_ptr<uint8[]> TryLoadFileAsMemoryBlock(const char sourceFileName[], size_t* sizeResult)
	{
		std::unique_ptr<IFileInterface> file;
		if (MainFileSystem::TryOpen(file, sourceFileName, "rb", FileShareMode::Read) == IFileSystem::IOReason::Success) {
			file->Seek(0, FileSeekAnchor::End);
			size_t size = file->TellP();
			file->Seek(0);
			if (size) {
				auto result = std::make_unique<uint8[]>(size+1);
				file->Read(result.get(), 1, size);
				result[size] = '\0';
				if (sizeResult) {
					*sizeResult = size;
				}
				return result;
			}
		}

		// on missing file (or failed load), we return the equivalent of an empty file
		if (sizeResult) { *sizeResult = 0; }
		return nullptr;
	}

	IFileInterface::~IFileInterface() {}
	IFileSystem::~IFileSystem() {}
}