// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "IFileSystem.h"
#include "MountingTree.h"
#include "../Utility/Streams/PathUtils.h"

namespace Assets
{
	static std::shared_ptr<MountingTree> s_mainMountingTree;
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

	namespace Internal 
	{
		using LookupResult = MountingTree::EnumerableLookup::Result;

		template<typename FileType, typename CharType>
			static IFileSystem::IOReason TryOpen(FileType& result, StringSection<CharType> filename, const char openMode[], FileShareMode::BitField shareMode)
		{
			result = FileType();

			MountingTree::CandidateObject candidateObject;
			auto lookup = s_mainMountingTree->Lookup(filename);
			for (;;) {
				auto r = lookup.TryGetNext(candidateObject);
				if (r == LookupResult::Invalidated) {
                    // "Mounting point lookup was invalidated when the mounting tree changed. Do not change the mount or unmount filesystems while other threads may be accessing the same mounting tree."
                    lookup = s_mainMountingTree->Lookup(filename);
                    continue;
                }

				if (r == LookupResult::NoCandidates)
					break;

				assert(candidateObject._fileSystem);
				auto ioRes = candidateObject._fileSystem->TryOpen(result, candidateObject._marker, openMode, shareMode);
				if (ioRes != IFileSystem::IOReason::FileNotFound && ioRes != IFileSystem::IOReason::Invalid)
					return ioRes;
			}

			// attempt opening with the default file system...
			if (s_defaultFileSystem)
				return ::Assets::TryOpen(result, *s_defaultFileSystem, filename, openMode, shareMode);
			return IFileSystem::IOReason::FileNotFound;
		}

		template<typename FileType, typename CharType>
			static IFileSystem::IOReason TryOpen(FileType& result, StringSection<CharType> filename, uint64 size, const char openMode[], FileShareMode::BitField shareMode)
		{
			result = FileType();

			MountingTree::CandidateObject candidateObject;
			auto lookup = s_mainMountingTree->Lookup(filename);
			for (;;) {
                auto r = lookup.TryGetNext(candidateObject);
                if (r == LookupResult::Invalidated) {
                    // "Mounting point lookup was invalidated when the mounting tree changed. Do not change the mount or unmount filesystems while other threads may be accessing the same mounting tree."
                    lookup = s_mainMountingTree->Lookup(filename);
                    continue;
                }

				if (r == LookupResult::NoCandidates)
					break;

				assert(candidateObject._fileSystem);
				auto ioRes = candidateObject._fileSystem->TryOpen(result, candidateObject._marker, size, openMode, shareMode);
				if (ioRes != IFileSystem::IOReason::FileNotFound && ioRes != IFileSystem::IOReason::Invalid)
					return ioRes;
			}

			// attempt opening with the default file system...
			if (s_defaultFileSystem) 
				return ::Assets::TryOpen(result, *s_defaultFileSystem, filename, size, openMode, shareMode);
			return IFileSystem::IOReason::FileNotFound;
		}

		template<typename CharType>
			IFileSystem::IOReason TryMonitor(StringSection<CharType> filename, const std::shared_ptr<IFileMonitor>& evnt)
		{
			MountingTree::CandidateObject candidateObject;
			auto lookup = s_mainMountingTree->Lookup(filename);
			for (;;) {
				auto r = lookup.TryGetNext(candidateObject);
				if (r == LookupResult::Invalidated) {
                    // "Mounting point lookup was invalidated when the mounting tree changed. Do not change the mount or unmount filesystems while other threads may be accessing the same mounting tree."
                    lookup = s_mainMountingTree->Lookup(filename);
                    continue;
                }

				if (r == LookupResult::NoCandidates)
					break;

				// We must call TryMonitor for each filesystem, because the filesystems return
				// "success" even if the file doesn't exist. So if we stop early, on the first
				// filesystem will be monitored
				assert(candidateObject._fileSystem);
				auto ioRes = candidateObject._fileSystem->TryMonitor(candidateObject._marker, evnt);
				(void)ioRes;
			}

			if (s_defaultFileSystem) 
				return ::Assets::TryMonitor(*s_defaultFileSystem, filename, evnt);
			return IFileSystem::IOReason::FileNotFound;
		}

		template<typename CharType>
			FileDesc TryGetDesc(StringSection<CharType> filename)
		{
			MountingTree::CandidateObject candidateObject;
			auto lookup = s_mainMountingTree->Lookup(filename);
			for (;;) {
				auto r = lookup.TryGetNext(candidateObject);
				if (r == LookupResult::Invalidated) {
                    // "Mounting point lookup was invalidated when the mounting tree changed. Do not change the mount or unmount filesystems while other threads may be accessing the same mounting tree."
                    lookup = s_mainMountingTree->Lookup(filename);
                    continue;
                }

				if (r == LookupResult::NoCandidates) 
					break;

				assert(candidateObject._fileSystem);
				auto res = candidateObject._fileSystem->TryGetDesc(candidateObject._marker);
				if (res._state != FileDesc::State::DoesNotExist)
					return res;
			}

			if (s_defaultFileSystem)
				return ::Assets::TryGetDesc(*s_defaultFileSystem, filename);
			return FileDesc{ std::basic_string<utf8>(), FileDesc::State::DoesNotExist };
		}
	}

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
		return Internal::TryOpen(result, filename, openMode, shareMode);
	}

	auto MainFileSystem::TryOpen(BasicFile& result, StringSection<utf8> filename, const char openMode[], FileShareMode::BitField shareMode) -> IOReason
	{
		return Internal::TryOpen(result, filename, openMode, shareMode);
	}

	auto MainFileSystem::TryOpen(MemoryMappedFile& result, StringSection<utf8> filename, uint64 size, const char openMode[], FileShareMode::BitField shareMode) -> IOReason
	{
		return Internal::TryOpen(result, filename, size, openMode, shareMode);
	}

	IFileSystem::IOReason MainFileSystem::TryMonitor(StringSection<utf8> filename, const std::shared_ptr<IFileMonitor>& evnt)
	{
		return Internal::TryMonitor(filename, evnt);
	}

	FileDesc MainFileSystem::TryGetDesc(StringSection<utf8> filename)
	{
		return Internal::TryGetDesc(filename);
	}

	auto MainFileSystem::TryOpen(std::unique_ptr<IFileInterface>& result, StringSection<utf16> filename, const char openMode[], FileShareMode::BitField shareMode) -> IOReason
	{
		return Internal::TryOpen(result, filename, openMode, shareMode);
	}

	auto MainFileSystem::TryOpen(BasicFile& result, StringSection<utf16> filename, const char openMode[], FileShareMode::BitField shareMode) -> IOReason
	{
		return Internal::TryOpen(result, filename, openMode, shareMode);
	}

	auto MainFileSystem::TryOpen(MemoryMappedFile& result, StringSection<utf16> filename, uint64 size, const char openMode[], FileShareMode::BitField shareMode) -> IOReason
	{
		return Internal::TryOpen(result, filename, size, openMode, shareMode);
	}

	IFileSystem::IOReason MainFileSystem::TryMonitor(StringSection<utf16> filename, const std::shared_ptr<IFileMonitor>& evnt)
	{
		return Internal::TryMonitor(filename, evnt);
	}

	FileDesc MainFileSystem::TryGetDesc(StringSection<utf16> filename)
	{
		return Internal::TryGetDesc(filename);
	}

	BasicFile MainFileSystem::OpenBasicFile(StringSection<utf8> filename, const char openMode[], FileShareMode::BitField shareMode)
	{
		BasicFile result;
		auto ioRes = TryOpen(result, filename, openMode, shareMode);
		if (ioRes != IOReason::Success)
			Throw(Utility::Exceptions::IOException(ioRes, "Failure while opening file (%s) in mode (%s)", std::string((const char*)filename.begin(), (const char*)filename.end()).c_str(), openMode));
		return result;
	}

	MemoryMappedFile MainFileSystem::OpenMemoryMappedFile(StringSection<utf8> filename, uint64 size, const char openMode[], FileShareMode::BitField shareMode)
	{
		MemoryMappedFile result;
		auto ioRes = TryOpen(result, filename, size, openMode, shareMode);
		if (ioRes != IOReason::Success)
			Throw(Utility::Exceptions::IOException(ioRes, "Failure while opening file (%s) in mode (%s)", std::string((const char*)filename.begin(), (const char*)filename.end()).c_str(), openMode));
		return result;
	}

	std::unique_ptr<IFileInterface> MainFileSystem::OpenFileInterface(StringSection<utf8> filename, const char openMode[], FileShareMode::BitField shareMode)
	{
		std::unique_ptr<IFileInterface> result;
		auto ioRes = TryOpen(result, filename, openMode, shareMode);
		if (ioRes != IOReason::Success)
			Throw(Utility::Exceptions::IOException(ioRes, "Failure while opening file (%s) in mode (%s)", std::string((const char*)filename.begin(), (const char*)filename.end()).c_str(), openMode));
		return result;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////

	const std::shared_ptr<MountingTree>& MainFileSystem::GetMountingTree() { return s_mainMountingTree; }
	void MainFileSystem::Init(const std::shared_ptr<MountingTree>& mountingTree, const std::shared_ptr<IFileSystem>& defaultFileSystem)
	{
		s_mainMountingTree = mountingTree;
		s_defaultFileSystem = defaultFileSystem;
	}

    void MainFileSystem::Shutdown()
    {
        Init(nullptr, nullptr);
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

	std::unique_ptr<uint8[]> TryLoadFileAsMemoryBlock(StringSection<char> sourceFileName, size_t* sizeResult)
	{
		std::unique_ptr<IFileInterface> file;
		if (MainFileSystem::TryOpen(file, sourceFileName, "rb", FileShareMode::Read) == IFileSystem::IOReason::Success) {
			file->Seek(0, FileSeekAnchor::End);
			size_t size = file->TellP();
			file->Seek(0);
			if (size) {
				auto result = std::make_unique<uint8[]>(size);
				file->Read(result.get(), 1, size);
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

	Blob TryLoadFileAsBlob(StringSection<char> sourceFileName)
	{
		std::unique_ptr<IFileInterface> file;
		if (MainFileSystem::TryOpen(file, sourceFileName, "rb", FileShareMode::Read) == IFileSystem::IOReason::Success) {
			file->Seek(0, FileSeekAnchor::End);
			size_t size = file->TellP();
			file->Seek(0);
			if (size) {
				auto result = std::make_shared<std::vector<uint8_t>>(size);
				file->Read(result->data(), 1, size);
				return result;
			}
		}

		return nullptr;
	}

	IFileInterface::~IFileInterface() {}
	IFileSystem::~IFileSystem() {}
}
