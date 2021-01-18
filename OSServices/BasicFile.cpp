// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "BasicFile.h"
#include "../Utility/PtrUtils.h"
#include <stdio.h>

namespace OSServices
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

	MemoryMappedFile::MemoryMappedFile(IteratorRange<void*> data, CloseFn&& close)
	: _data(data), _closeFn(std::move(close)) {}

    MemoryMappedFile::~MemoryMappedFile()
    {
		if (_closeFn)
			_closeFn(_data);
    }

    MemoryMappedFile::MemoryMappedFile()
    {
    }

    MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& moveFrom) never_throws
    {
		_data = moveFrom._data;
		moveFrom._data = {};
		_closeFn = std::move(moveFrom._closeFn);
		moveFrom._closeFn = nullptr;
    }

    MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& moveFrom) never_throws
    {
		if (_closeFn)
			_closeFn(_data);

		_data = moveFrom._data;
		moveFrom._data = {};
		_closeFn = std::move(moveFrom._closeFn);
		moveFrom._closeFn = nullptr;
        return *this;
    }
}
