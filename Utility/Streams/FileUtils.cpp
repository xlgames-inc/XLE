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
			_vsnprintf_s(_buffer, _TRUNCATE, format, args);
			va_end(args);
		}
	}

        //  here is an older implementation of BasicFile. It's implemented in terms of
        //  generic functions, but it has some limitations
    #if 0
        BasicFile::BasicFile(const char filename[], const char openMode[], ShareMode::BitField shareMode)
        {
            assert(shareMode==0);       // (share mode not supported by this implementation)
            #if _MSC_VER >= 1400 // MSVC 8.0 / 2005
	            fopen_s(&_file, filename, openMode);
            #else
	            _file = fopen(filename, openMode);
            #endif
            if (!_file) {
                Throw(Exceptions::BasicLabel("Failure during file open. Probably missing file or bad privileges: (%s), openMode: (%s)", filename, openMode));
            }
        }

        BasicFile::BasicFile(BasicFile&& moveFrom) never_throws
        {
            _file = moveFrom._file;
            moveFrom._file = nullptr;
        }

        BasicFile& BasicFile::operator=(BasicFile&& moveFrom) never_throws
        {
            if (_file != nullptr) {
                fclose(_file);
            }
            _file = moveFrom._file;
            moveFrom._file = nullptr;
            return *this;
        }

        BasicFile::BasicFile()
        {
            _file = nullptr;
        }

        BasicFile::~BasicFile()
        {
            if (_file != nullptr) {
                fclose(_file);
            }
        }

        size_t   BasicFile::Read(void *buffer, size_t size, size_t count) const never_throws
        {
            return fread(buffer, size, count, _file);
        }
    
        size_t   BasicFile::Write(const void *buffer, size_t size, size_t count) never_throws
        {
            return fwrite(buffer, size, count, _file);
        }

        size_t   BasicFile::Seek(size_t offset, int origin) never_throws
        {
            return fseek(_file, long(offset), origin);
        }

        size_t   BasicFile::TellP() const never_throws
        {
            return ftell(_file);
        }

        void    BasicFile::Flush() const never_throws
        {
            return fflush(_file);
        }

        uint64      BasicFile::GetSize() never_throws
        {
            if (!_file) return 0;

            auto originalPosition = TellP();
            Seek(0, SEEK_END);
            size_t size = TellP();
            Seek(originalPosition, SEEK_SET);

            return uint64(size);
        }
    #endif

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
