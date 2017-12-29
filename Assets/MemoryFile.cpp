// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "MemoryFile.h"
#include <algorithm>

namespace Assets
{
	class MemoryFile
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
		mutable size_t	_readPtr;
	};

	size_t			MemoryFile::Write(const void * source, size_t size, size_t count) never_throws
	{
		assert(0);
		return 0;
	}

	size_t			MemoryFile::Read(void * destination, size_t size, size_t count) const never_throws
	{
		if (!size || !count) return 0;
		if (!_blob) return 0;

		ptrdiff_t spaceLeft = _blob->size() - _readPtr;
		ptrdiff_t maxCount = spaceLeft / size;
		ptrdiff_t finalCount = std::min(ptrdiff_t(count), maxCount);

		std::memcpy(
			destination,
			PtrAdd(AsPointer(_blob->begin()), _readPtr),
			finalCount * size);
		_readPtr += finalCount * size;
		return finalCount;
	}

	ptrdiff_t		MemoryFile::Seek(ptrdiff_t seekOffset, FileSeekAnchor anchor) never_throws
	{
		ptrdiff_t newPtr = 0;
		switch (anchor) {
		case FileSeekAnchor::Start:		newPtr = seekOffset; break;
		case FileSeekAnchor::Current:	newPtr = _readPtr + seekOffset; break;
		case FileSeekAnchor::End:		newPtr = (_blob ? _blob->size() : 0) + seekOffset; break;
		default:
			assert(0);
		}
		newPtr = std::max(ptrdiff_t(0), newPtr);
		newPtr = std::min(ptrdiff_t(_blob->size()), newPtr);
		_readPtr = newPtr;
		return _readPtr;
	}

	size_t			MemoryFile::TellP() const never_throws
	{
		return _readPtr;
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
	, _readPtr(0)
	{}

	MemoryFile::~MemoryFile()
	{
	}
}

