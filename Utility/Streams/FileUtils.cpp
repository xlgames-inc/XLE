// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FileUtils.h"
#include "../PtrUtils.h"
#include <stdio.h>

namespace Utility
{
	namespace Exceptions
	{
		IOException::IOException(Reason reason, const char format[], ...) never_throws
			: _reason(reason)
		{
			va_list args;
			va_start(args, format);
            std::vsnprintf(_buffer, dimof(_buffer), format, args);
			va_end(args);
		}
	}

    namespace RawFS
	{
		std::unique_ptr<uint8[]> TryLoadFileAsMemoryBlock(const char sourceFileName[], size_t* sizeResult)
		{
			// Implemented to avoid throwing an exception on a missing file
			// However, there may be some exception cases where we do actually want to throw an exception
			//      -- for example, a permissions error or something more complex. In those cases, we
			//      may want the calling code to take some action. Suppressing all exceptions here
			//      is too generous!
			BasicFile file;
			if (file.TryOpen((const utf8*)sourceFileName, "rb", FileShareMode::Read) == Exceptions::IOException::Reason::Success) {
				file.Seek(0, FileSeekAnchor::End);
				size_t size = file.TellP();
				file.Seek(0);
				if (size) {
					std::unique_ptr<uint8[]> result(new uint8[size+1]);
					file.Read(result.get(), 1, size);
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
	}

	MemoryMappedFile::MemoryMappedFile(void* begin, void* end, CloseFn&& close)
	: _begin(begin), _end(end), _closeFn(std::move(close)) {}

    MemoryMappedFile::~MemoryMappedFile()
    {
		if (_closeFn)
			_closeFn(_begin, _end);
    }

    MemoryMappedFile::MemoryMappedFile()
    {
		_begin = _end = nullptr;
    }

    MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& moveFrom) never_throws
    {
		_begin = moveFrom._begin;
		_end = moveFrom._end;
		moveFrom._begin = moveFrom._end = nullptr;
		_closeFn = std::move(moveFrom._closeFn);
		moveFrom._closeFn = nullptr;
    }

    MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& moveFrom) never_throws
    {
		if (_closeFn)
			_closeFn(_begin, _end);

		_begin = moveFrom._begin;
		_end = moveFrom._end;
		moveFrom._begin = moveFrom._end = nullptr;
		_closeFn = std::move(moveFrom._closeFn);
		moveFrom._closeFn = nullptr;
        return *this;
    }
}
