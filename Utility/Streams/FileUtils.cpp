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
