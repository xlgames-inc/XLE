// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/UTFUtils.h"			// for utf8, utf16
#include "../Utility/StringUtils.h"			// for StringSection
#include "../Utility/Streams/FileUtils.h"
#include "../Core/Types.h"
#include "../Core/Exceptions.h"
#include <string>
#include <memory>

namespace Utility { class OnChangeCallback; }

namespace Assets
{
	class FileDesc;
	class MountingTree;
	class IFileInterface;
	using IFileMonitor = Utility::OnChangeCallback;
	using Blob = std::shared_ptr<std::vector<uint8_t>>;
	using FileSystemId = unsigned;
	static const FileSystemId FileSystemId_Invalid = ~0u;

	static const FileShareMode::BitField FileShareMode_Default = FileShareMode::Read;

	/// <summary>Interface for interacting with a file</summary>
	/// A file can be a physical file on disk, or any logical object that behaves like a file.
	/// IFileInterface objects are typically returned from IFileSystem implementations as a result
	/// of an "open" operation.
	///
	/// This provides typical file system behaviour, such as reading, writing, searching and
	/// getting description information.
	class IFileInterface
	{
	public:
		virtual size_t			Write(const void * source, size_t size, size_t count = 1) never_throws = 0;
		virtual size_t			Read(void * destination, size_t size, size_t count = 1) const never_throws = 0;
		virtual ptrdiff_t		Seek(ptrdiff_t seekOffset, FileSeekAnchor = FileSeekAnchor::Start) never_throws = 0;
		virtual size_t			TellP() const never_throws = 0;

		virtual FileDesc		GetDesc() const never_throws = 0;

		virtual 			   ~IFileInterface();
	};

	/// <summary>Interface for a mountable virtual file system</summary>
	/// Provides a generic way to access different types of resources in a file-system like way.
	/// Typical implementions include things like archive files and "virtual" memory-based files.
	/// But the underlying OS filesystem is accessed via a IFileSystem, as well.
	///
	/// File systems can be mounted via a MountingTree. This works much like the *nix virtual
	/// file system (where new file systems can be mounted under any filespec prefix).
	/// 
	/// IFileSystem can be compared to the interfaces in the /fs/ tree of linux. Some of the 
	/// functions provide similar functionality. It's possible that we could build an adapter
	/// to allow filesystem implementations from linux to mounted as a IFileSystem.
	/// Howver, note that IFileSystem is intended mostly for input. So there are no functions for 
	/// things like creating or removing directories.
	class IFileSystem
	{
	public:
		using Marker = std::vector<uint8>;

		enum class TranslateResult { Success, Mounting, Invalid };
		virtual TranslateResult		TryTranslate(Marker& result, StringSection<utf8> filename) = 0;
		virtual TranslateResult		TryTranslate(Marker& result, StringSection<utf16> filename) = 0;

		using IOReason = Utility::Exceptions::IOException::Reason;

		virtual IOReason	TryOpen(std::unique_ptr<IFileInterface>& result, const Marker& marker, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default) = 0;
		virtual IOReason	TryOpen(BasicFile& result, const Marker& marker, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default) = 0;
		virtual IOReason	TryOpen(MemoryMappedFile& result, const Marker& marker, uint64 size, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default) = 0;

		virtual	IOReason	TryMonitor(const Marker& marker, const std::shared_ptr<IFileMonitor>& evnt) = 0;
		virtual	FileDesc	TryGetDesc(const Marker& marker) = 0;
		virtual				~IFileSystem();
	};

	class ISearchableFileSystem
	{
	public:
		virtual std::vector<IFileSystem::Marker> FindFiles(
			StringSection<utf8> baseDirectory,
			StringSection<utf8> regexMatchPattern) = 0;
		virtual std::vector<std::basic_string<utf8>> FindSubDirectories(
			StringSection<utf8> baseDirectory) = 0;
		virtual ~ISearchableFileSystem();
	};

	class FileSystemWalker
	{
	public:
		class DirectoryIterator 
		{
		public:
			FileSystemWalker get() const;
			FileSystemWalker operator*() const { return get(); }
			std::basic_string<utf8> Name() const;
			friend bool operator==(const DirectoryIterator& lhs, const DirectoryIterator& rhs)
			{
				assert(lhs._helper == rhs._helper);
				return lhs._idx == rhs._idx;
			}
			friend bool operator!=(const DirectoryIterator& lhs, const DirectoryIterator& rhs)
			{
				return !operator==(lhs, rhs);
			}
			void operator++() { ++_idx; }
			void operator++(int) { ++_idx; }
		private:
			DirectoryIterator(const FileSystemWalker* helper, unsigned idx);
			friend class FileSystemWalker;
			const FileSystemWalker* _helper;
			unsigned _idx;
		};

		class FileIterator
		{
		public:
			struct Value
			{
				IFileSystem::Marker _marker;
				FileSystemId _fs;
			};
			Value get() const;
			Value operator*() const { return get(); }
			FileDesc Desc() const;
			friend bool operator==(const FileIterator& lhs, const FileIterator& rhs)
			{
				assert(lhs._helper == rhs._helper);
				return lhs._idx == rhs._idx;
			}
			friend bool operator!=(const FileIterator& lhs, const FileIterator& rhs)
			{
				return !operator==(lhs, rhs);
			}
			void operator++() { ++_idx; }
			void operator++(int) { ++_idx; }
		private:
			FileIterator(const FileSystemWalker* helper, unsigned idx);
			friend class FileSystemWalker;
			const FileSystemWalker* _helper;
			unsigned _idx;
		};
		
		DirectoryIterator begin_directories() const;
		DirectoryIterator end_directories() const;

		FileIterator begin_files() const;
		FileIterator end_files() const;

		FileSystemWalker RecurseTo(const std::basic_string<utf8>& subDirectory) const;

		FileSystemWalker();
		~FileSystemWalker();
		FileSystemWalker(FileSystemWalker&&);
		FileSystemWalker& operator=(FileSystemWalker&&);
	private:
		class Pimpl;
		std::unique_ptr<Pimpl> _pimpl;

		struct StartingFS
		{
			std::basic_string<utf8> _pendingDirectories;
			std::basic_string<utf8> _internalPoint;
			std::shared_ptr<ISearchableFileSystem> _fs;
			FileSystemId _fsId = FileSystemId_Invalid;
		};

		FileSystemWalker(std::vector<StartingFS>&& fileSystems);

		friend class MountingTree;
		friend FileSystemWalker BeginWalk(const std::shared_ptr<ISearchableFileSystem>& fs);
	};

	/// <summary>Description of a file object within a filesystem</summary>
	/// Typically files have a few basic properties that can be queried.
	/// But note the "files" in this sense can mean more than just files on disk.
	/// So some properties will not apply to all files.
	/// Also note that some filesystems can map multiple names onto the same object.
	/// (for example, a filesystem that is not case sensitive will map all case variations
	/// onto the same file).
	/// In cases like this, the "_naturalName" below represents the form closest to how
	/// the object is stored internally.
	class FileDesc
	{
	public:
		/// <summary>State of a file</summary>
		/// There are a few basic states that apply to all files. Here, DoesNotExist and
		/// Normal are clear. Mounting means that the file is not currently accessible, but
		/// is expected to be available later. This can happen if the filesystem is still
		/// mounting in a background thread, or there is some background operation that 
		/// must be completed before the resource can be used.
		/// Invalid is used in rare cases where the resource does not currently exist, and
		/// can never exist. This is typically required by filesystems that processing source
		/// resources into output resources.
		///
		/// Consider a filesystem that performs texture compression. A filesystem request may
		/// load a source texture and attempt to compile it. In this case "Mounting" will be
		/// returned while the texture is being compiled in a background thread. When the 
		/// compression is completed, "Normal" will be returned. If an appropriate source
		/// file does not exist, then "DoesNotExist" will be returned. If a source file does
		/// exist, but that source file is corrupt or invalid, then "Invalid" will be returned.
		///
		/// However, note that this is not a complete list of states for all files. Even if
		/// a file returns state "Normal", open operations may still fail. They can fail for
		/// filesystem specific problems (such as a permissions error)
		enum class State { DoesNotExist, Normal, Mounting, Invalid };

		std::basic_string<utf8>	_naturalName;
		State					_state;
		FileTime				_modificationTime;
		uint64					_size;
	};

	/// <summary>Provides access to the global mounting tree</summary>
	/// The global mounting tree is the default mounting tree used to resolve file requests made by
	/// code in this process. It can be thought of as similar to the file system namespace for the 
	/// current process in Linux.
	///
	/// File requests that can't be resolved by the mounting tree (eg, absolute paths and paths beginning
	/// with a drive name) are passed onto a default filesystem (which is typically just raw access to 
	/// the underlying OS filesystem).
	class MainFileSystem
	{
	public:
		using IOReason = IFileSystem::IOReason;

		static IOReason	TryOpen(std::unique_ptr<IFileInterface>& result, StringSection<utf8> filename, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default);
		static IOReason	TryOpen(BasicFile& result, StringSection<utf8> filename, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default);
		static IOReason	TryOpen(MemoryMappedFile& result, StringSection<utf8> filename, uint64 size, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default);
		static IOReason	TryMonitor(StringSection<utf8> filename, const std::shared_ptr<IFileMonitor>& evnt);
		static FileDesc	TryGetDesc(StringSection<utf8> filename);

		static BasicFile OpenBasicFile(StringSection<utf8> filename, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default);
		static MemoryMappedFile OpenMemoryMappedFile(StringSection<utf8> filename, uint64 size, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default);
		static std::unique_ptr<IFileInterface> OpenFileInterface(StringSection<utf8> filename, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default);

		static IOReason	TryOpen(std::unique_ptr<IFileInterface>& result, StringSection<utf16> filename, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default);
		static IOReason	TryOpen(BasicFile& result, StringSection<utf16> filename, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default);
		static IOReason	TryOpen(MemoryMappedFile& result, StringSection<utf16> filename, uint64 size, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default);
		static IOReason	TryMonitor(StringSection<utf16> filename, const std::shared_ptr<IFileMonitor>& evnt);
		static FileDesc	TryGetDesc(StringSection<utf16> filename);

		static IFileSystem* GetFileSystem(FileSystemId id);

		static FileSystemWalker BeginWalk(StringSection<utf8> initialSubDirectory = u(""));

		static const std::shared_ptr<MountingTree>& GetMountingTree();
		static void Init(const std::shared_ptr<MountingTree>& mountingTree, const std::shared_ptr<IFileSystem>& defaultFileSystem);
        static void Shutdown();

		static IOReason	TryOpen(std::unique_ptr<IFileInterface>& result, StringSection<char> filename, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default)
			{ return TryOpen(result, MakeStringSection((const utf8*)filename.begin(), (const utf8*)filename.end()), openMode, shareMode); }
		static IOReason	TryOpen(BasicFile& result, StringSection<char> filename, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default)
			{ return TryOpen(result, MakeStringSection((const utf8*)filename.begin(), (const utf8*)filename.end()), openMode, shareMode); }
		static IOReason	TryOpen(MemoryMappedFile& result, StringSection<char> filename, uint64 size, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default)
			{ return TryOpen(result, MakeStringSection((const utf8*)filename.begin(), (const utf8*)filename.end()), size, openMode, shareMode); }
		static IOReason	TryMonitor(StringSection<char> filename, const std::shared_ptr<IFileMonitor>& evnt)
			{ return TryMonitor(MakeStringSection((const utf8*)filename.begin(), (const utf8*)filename.end()), evnt); }
		static FileDesc	TryGetDesc(StringSection<char> filename)
			{ return TryGetDesc(MakeStringSection((const utf8*)filename.begin(), (const utf8*)filename.end())); }

		static BasicFile OpenBasicFile(StringSection<char> filename, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default)
			{ return OpenBasicFile(MakeStringSection((const utf8*)filename.begin(), (const utf8*)filename.end()), openMode, shareMode); }
		static MemoryMappedFile OpenMemoryMappedFile(StringSection<char> filename, uint64 size, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default)
			{ return OpenMemoryMappedFile(MakeStringSection((const utf8*)filename.begin(), (const utf8*)filename.end()), size, openMode, shareMode); }
		static std::unique_ptr<IFileInterface> OpenFileInterface(StringSection<char> filename, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default)
			{ return OpenFileInterface(MakeStringSection((const utf8*)filename.begin(), (const utf8*)filename.end()), openMode, shareMode); }
	};

	T2(CharType, FileObject) IFileSystem::IOReason TryOpen(FileObject& result, IFileSystem& fs, StringSection<CharType> fn, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default);
	T2(CharType, FileObject) IFileSystem::IOReason TryOpen(FileObject& result, IFileSystem& fs, StringSection<CharType> fn, uint64 size, const char openMode[], FileShareMode::BitField shareMode=FileShareMode_Default);
	T1(CharType) IFileSystem::IOReason TryMonitor(IFileSystem& fs, StringSection<CharType> fn, const std::shared_ptr<IFileMonitor>& evnt);
	T1(CharType) FileDesc TryGetDesc(IFileSystem& fs, StringSection<CharType> fn);
	FileSystemWalker BeginWalk(const std::shared_ptr<ISearchableFileSystem>& fs);

	std::unique_ptr<uint8[]> TryLoadFileAsMemoryBlock(StringSection<char> sourceFileName, size_t* sizeResult = nullptr);
	Blob TryLoadFileAsBlob(StringSection<char> sourceFileName);

	std::unique_ptr<uint8[]> TryLoadFileAsMemoryBlock_TolerateSharingErrors(StringSection<char> sourceFileName, size_t* sizeResult);
	Blob TryLoadFileAsBlob_TolerateSharingErrors(StringSection<char> sourceFileName);

}

