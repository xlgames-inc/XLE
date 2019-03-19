// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MemoryFile.h"
#include <algorithm>
#include "../Foreign/zlib/zlib.h"

namespace Assets
{
	class MemoryFile : public IFileInterface
	{
	public:
		size_t			Write(const void * source, size_t size, size_t count) never_throws;
		size_t			Read(void * destination, size_t size, size_t count) const never_throws;
		ptrdiff_t		Seek(ptrdiff_t seekOffset, FileSeekAnchor) never_throws;
		size_t			TellP() const never_throws;

		FileDesc		GetDesc() const never_throws;

		MemoryFile(const Blob& blob);
		~MemoryFile();
	private:
		Blob			_blob;
		mutable size_t	_ptr;
	};

	size_t			MemoryFile::Write(const void * source, size_t size, size_t count) never_throws
	{
		size_t finalSize = size * count;
		if (!_blob) _blob = std::make_shared<std::vector<uint8_t>>();
		_blob->insert(_blob->begin() + _ptr, (const uint8_t*)source, (const uint8_t*)PtrAdd(source, finalSize));
		_ptr += finalSize;
		return finalSize;
	}

	size_t			MemoryFile::Read(void * destination, size_t size, size_t count) const never_throws
	{
		if (!size || !count) return 0;
		if (!_blob) return 0;

		ptrdiff_t spaceLeft = _blob->size() - _ptr;
		ptrdiff_t maxCount = spaceLeft / size;
		ptrdiff_t finalCount = std::min(ptrdiff_t(count), maxCount);

		std::memcpy(
			destination,
			PtrAdd(AsPointer(_blob->begin()), _ptr),
			finalCount * size);
		_ptr += finalCount * size;
		return finalCount;
	}

	ptrdiff_t		MemoryFile::Seek(ptrdiff_t seekOffset, FileSeekAnchor anchor) never_throws
	{
		ptrdiff_t newPtr = 0;
		switch (anchor) {
		case FileSeekAnchor::Start:		newPtr = seekOffset; break;
		case FileSeekAnchor::Current:	newPtr = _ptr + seekOffset; break;
		case FileSeekAnchor::End:		newPtr = (_blob ? _blob->size() : 0) + seekOffset; break;
		default:
			assert(0);
		}
		newPtr = std::max(ptrdiff_t(0), newPtr);
		newPtr = std::min(ptrdiff_t(_blob->size()), newPtr);
		_ptr = newPtr;
		return _ptr;
	}

	size_t			MemoryFile::TellP() const never_throws
	{
		return _ptr;
	}

	FileDesc		MemoryFile::GetDesc() const never_throws
	{
		return FileDesc
			{
				u("<<in memory>>"),
				FileDesc::State::Normal,
				0, uint64_t(_blob ? _blob->size() : 0)
			};
	}

	MemoryFile::MemoryFile(const Blob& blob)
	: _blob(blob)
	, _ptr(0)
	{}

	MemoryFile::~MemoryFile()
	{
	}

	std::unique_ptr<IFileInterface> CreateMemoryFile(const Blob& blob)
	{
		return std::make_unique<MemoryFile>(blob);
	}

////////////////////////////////////////////////////////////////////////////////////////////////

	class ArchiveSubFile : public ::Assets::IFileInterface
	{
	public:
		size_t      Read(void *buffer, size_t size, size_t count) const never_throws;
		size_t      Write(const void *buffer, size_t size, size_t count) never_throws;
		ptrdiff_t	Seek(ptrdiff_t seekOffset, FileSeekAnchor) never_throws;
		size_t      TellP() const never_throws;

		::Assets::FileDesc	GetDesc() const never_throws;

		ArchiveSubFile(
			const std::shared_ptr<MemoryMappedFile>& archiveFile,
			const IteratorRange<const void*> memoryRange);
		~ArchiveSubFile();
	protected:
		std::shared_ptr<MemoryMappedFile> _archiveFile;
		const void*			_dataStart;
		const void*			_dataEnd;
		mutable const void*	_tellp;
	};

	size_t      ArchiveSubFile::Read(void *buffer, size_t size, size_t count) const never_throws 
	{ 
		auto remainingSpace = ptrdiff_t(_dataEnd) - ptrdiff_t(_tellp);
		auto objectsToRead = (size_t)std::max(ptrdiff_t(0), std::min(remainingSpace / ptrdiff_t(size), ptrdiff_t(count)));
		std::memcpy(buffer, _tellp, objectsToRead*size);
		_tellp = PtrAdd(_tellp, objectsToRead*size);
		return objectsToRead;
	}

	size_t      ArchiveSubFile::Write(const void *buffer, size_t size, size_t count) never_throws 
	{ 
		Throw(::Exceptions::BasicLabel("BSAFile::Write() unimplemented"));
	}

	ptrdiff_t	ArchiveSubFile::Seek(ptrdiff_t seekOffset, FileSeekAnchor anchor) never_throws 
	{
		ptrdiff_t result = ptrdiff_t(_tellp) - ptrdiff_t(_dataStart);
		switch (anchor) {
		case FileSeekAnchor::Start: _tellp = PtrAdd(_dataStart, seekOffset); break;
		case FileSeekAnchor::Current: _tellp = PtrAdd(_tellp, seekOffset); break;
		case FileSeekAnchor::End: _tellp = PtrAdd(_dataEnd, -ptrdiff_t(seekOffset)); break;
		default:
			Throw(::Exceptions::BasicLabel("Unknown seek anchor in BSAFile::Seek(). Only Start/Current/End supported"));
		}
		return result;
	}

	size_t      ArchiveSubFile::TellP() const never_throws 
	{ 
		return ptrdiff_t(_tellp) - ptrdiff_t(_dataStart);
	}

	::Assets::FileDesc ArchiveSubFile::GetDesc() const never_throws
	{
		Throw(::Exceptions::BasicLabel("BSAFile::GetDesc() unimplemented"));
	}

	ArchiveSubFile::ArchiveSubFile(const std::shared_ptr<MemoryMappedFile>& archiveFile, const IteratorRange<const void*> memoryRange)
	: _archiveFile(archiveFile)
	{
		_dataStart = memoryRange.begin();
		_dataEnd = memoryRange.end();
		_tellp = _dataStart;
	}

	ArchiveSubFile::~ArchiveSubFile() {}

	std::unique_ptr<IFileInterface> CreateSubFile(
		const std::shared_ptr<MemoryMappedFile>& archiveFile,
		const IteratorRange<const void*> memoryRange)
	{
		return std::make_unique<ArchiveSubFile>(archiveFile, memoryRange);
	}

////////////////////////////////////////////////////////////////////////////////////////////////

	class FileDecompressOnRead : public ::Assets::IFileInterface
	{
	public:
		size_t      Read(void *buffer, size_t size, size_t count) const never_throws;
		size_t      Write(const void *buffer, size_t size, size_t count) never_throws;
		ptrdiff_t	Seek(ptrdiff_t seekOffset, FileSeekAnchor) never_throws;
		size_t      TellP() const never_throws;

		::Assets::FileDesc	GetDesc() const never_throws;

		FileDecompressOnRead(
			const std::shared_ptr<MemoryMappedFile>& archiveFile,
			const IteratorRange<const void*> memoryRange,
			size_t decompressedSize);
		~FileDecompressOnRead();
	protected:
		std::shared_ptr<MemoryMappedFile> _archiveFile;
		mutable z_stream	_stream;
		size_t				_decompressedSize;
	};

	size_t      FileDecompressOnRead::Write(const void *buffer, size_t size, size_t count) never_throws
	{
		Throw(::Exceptions::BasicLabel("BSAFileDecompressOnRead::Seek() unimplemented"));
	}

	ptrdiff_t	FileDecompressOnRead::Seek(ptrdiff_t seekOffset, FileSeekAnchor) never_throws
	{
		// We can't easily seek, because the underlying stream is compressed. Seeking would require
		// decompressing the buffer as we go along.
		Throw(::Exceptions::BasicLabel("BSAFileDecompressOnRead::Seek() unimplemented"));
	}

	size_t      FileDecompressOnRead::TellP() const never_throws
	{
		Throw(::Exceptions::BasicLabel("BSAFileDecompressOnRead::TellP() unimplemented"));
	}

	::Assets::FileDesc	FileDecompressOnRead::GetDesc() const never_throws
	{
		// Throw(::Exceptions::BasicLabel("BSAFileDecompressOnRead::GetDesc() unimplemented"));
		return {
			{}, FileDesc::State::Normal, 0,
			_decompressedSize
		};
	}

	size_t      FileDecompressOnRead::Read(void *buffer, size_t size, size_t count) const never_throws
	{
		_stream.next_out = (Bytef*)buffer;
		_stream.avail_out = (uInt)(size*count);
		auto err = inflate(&_stream, Z_SYNC_FLUSH);
		assert(err >= 0); (void) err;
		return _stream.total_out / size;
	}

	FileDecompressOnRead::FileDecompressOnRead(
		const std::shared_ptr<MemoryMappedFile>& archiveFile,
		const IteratorRange<const void*> memoryRange,
		size_t decompressedSize)
	: _archiveFile(archiveFile)
	, _decompressedSize(decompressedSize)
	{
		_stream.next_in = (z_const Bytef *)memoryRange.begin();
		_stream.avail_in = (uInt)memoryRange.size();

		_stream.next_out = nullptr;
		_stream.avail_out = 0;

		_stream.zalloc = (alloc_func)0;
		_stream.zfree = (free_func)0;

		auto err = inflateInit(&_stream);
		assert(err == Z_OK); (void) err;
	}

	FileDecompressOnRead::~FileDecompressOnRead() 
	{
		auto err = inflateEnd(&_stream);
		assert(err == Z_OK); (void) err;
	}

	std::unique_ptr<IFileInterface> CreateDecompressOnReadFile(
		const std::shared_ptr<MemoryMappedFile>& archiveFile,
		const IteratorRange<const void*> memoryRange,
		size_t decompressedSize)
	{
		return std::make_unique<FileDecompressOnRead>(archiveFile, memoryRange, decompressedSize);
	}
}

