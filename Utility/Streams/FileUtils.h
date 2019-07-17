// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Detail/API.h"
#include "../../Core/Exceptions.h"
#include "../../Core/Types.h"
#include "../StringUtils.h" // for StringSection
#include "../Optional.h"
#include "../IteratorUtils.h"
#include <memory>       // for std::unique_ptr

#include <vector>
#include <string>
#include <functional>

namespace Utility 
{
    namespace Exceptions
    {
        class IOException : public ::Exceptions::BasicLabel
        {
        public:
            /// <summary>A incomplete list of a few common file related errors</summary>
            /// Openning a file can result in a wide variety of possible errors. However, there
            /// are a few particularly common ones (like file not found, etc). This enum provides
            /// a way to quickly identify some of the common error types.
            enum class Reason
            {
                Success, FileNotFound,
                AccessDenied, WriteProtect,
				Mounting, Invalid,
				ExclusiveLock,				// locked for read and/or write exclusively by another process (ie, this is called "sharing violation" in the Win32 error codes)
                Complex
            };

			Reason GetReason() const { return _reason; }

            IOException(Reason reason, const char format[], ...) never_throws;
        private:
            Reason _reason;
        };
    }

	struct FileShareMode
	{
		enum { Read = 1<<0, Write = 1<<1 };
		typedef unsigned BitField;
	};

	using FileTime = uint64;

	enum class FileSeekAnchor { Start = SEEK_SET, Current = SEEK_CUR, End = SEEK_END };

        //
        //  "BasicFile" --  C++ wrapper for file interactions.
        //                  Prefer using BasicFile, instead of C-style interface functions 
        //                  (XlFileOpen, etc)
        //
        //      Cannot be copied, but can be moved.
        //      Constructor will throw on file-system errors
        //      But other methods should not throw on errors
        //
    class BasicFile
    {
    public:
        size_t      Read(void *buffer, size_t size, size_t count) const never_throws;
        size_t      Write(const void *buffer, size_t size, size_t count) never_throws;
        size_t      Seek(size_t offset, FileSeekAnchor anchor = FileSeekAnchor::Start) never_throws;
        size_t      TellP() const never_throws;
        void        Flush() const never_throws;

        uint64      GetSize() const never_throws;
		bool		IsGood() const never_throws;
		FileTime	GetFileTime() const never_throws;

        BasicFile(BasicFile&& moveFrom) never_throws;
        BasicFile& operator=(BasicFile&& moveFrom) never_throws;
        BasicFile(const BasicFile& copyFrom) never_throws;
        BasicFile& operator=(const BasicFile& copyFrom) never_throws;
        BasicFile();
        ~BasicFile();

    protected:
        void* _file;
    };

    class MemoryMappedFile
    {
    public:
        IteratorRange<void*>		GetData()		{ return _data; }
		IteratorRange<const void*>	GetData() const	{ return _data; }
        bool            IsGood() const				{ return !_data.empty(); }
        size_t          GetSize() const				{ return _data.size(); }

		using CloseFn = std::function<void(IteratorRange<const void*>)>;

        MemoryMappedFile();
		MemoryMappedFile(IteratorRange<void*> data, CloseFn&& close);
        MemoryMappedFile(MemoryMappedFile&& moveFrom) never_throws;
        MemoryMappedFile& operator=(MemoryMappedFile&& moveFrom) never_throws;
        ~MemoryMappedFile();

	protected:
        IteratorRange<void*> _data;
		CloseFn _closeFn;
    };

	namespace RawFS
	{
		class BasicFile : public ::Utility::BasicFile
		{
		public:
			Exceptions::IOException::Reason TryOpen(const utf8 filename[], const char openMode[], FileShareMode::BitField shareMode) never_throws;
			Exceptions::IOException::Reason TryOpen(const utf16 filename[], const char openMode[], FileShareMode::BitField shareMode) never_throws;
			BasicFile(const utf8 filename[], const char openMode[], FileShareMode::BitField shareMode);
			BasicFile(const utf16 filename[], const char openMode[], FileShareMode::BitField shareMode);
			BasicFile();
		};

		class MemoryMappedFile : public ::Utility::MemoryMappedFile
		{
		public:
			Exceptions::IOException::Reason TryOpen(const utf8 filename[], uint64 size, const char openMode[], FileShareMode::BitField shareMode) never_throws;
			Exceptions::IOException::Reason TryOpen(const utf16 filename[], uint64 size, const char openMode[], FileShareMode::BitField shareMode) never_throws;
			MemoryMappedFile(
				const utf8 filename[], uint64 size, 
				const char openMode[],
				FileShareMode::BitField shareMode);
			MemoryMappedFile(
				const utf16 filename[], uint64 size, 
				const char openMode[],
				FileShareMode::BitField shareMode);
			MemoryMappedFile();
		};

		class FileAttributes
		{
		public:
			uint64_t _size;
			uint64_t _lastWriteTime;
			uint64_t _lastAccessTime;
		};

		XL_UTILITY_API bool DoesFileExist(StringSection<char> filename);
		XL_UTILITY_API bool DoesDirectoryExist(StringSection<char> filename);

        XL_UTILITY_API std::optional<FileAttributes> TryGetFileAttributes(const utf8 filename[]);
        XL_UTILITY_API std::optional<FileAttributes> TryGetFileAttributes(const utf16 filename[]);

		XL_UTILITY_API void CreateDirectoryRecursive(StringSection<char> filename);
		XL_UTILITY_API void CreateDirectoryRecursive(StringSection<utf8> filename);
		XL_UTILITY_API void CreateDirectoryRecursive(StringSection<utf16> filename);

		namespace FindFilesFilter
		{
			enum Enum 
			{
				File = 1<<0,
				Directory = 1<<1,
				All =  0xfffffffful
			};
			typedef unsigned BitField;
		}
		XL_UTILITY_API std::vector<std::string> FindFiles(const std::string& searchPath, FindFilesFilter::BitField filter = FindFilesFilter::All);
		XL_UTILITY_API std::vector<std::string> FindFilesHierarchical(const std::string& rootDirectory, const std::string& filePattern, FindFilesFilter::BitField filter = FindFilesFilter::All);
	}
}

using namespace Utility;
