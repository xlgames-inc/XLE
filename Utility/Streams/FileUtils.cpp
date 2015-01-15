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
                ThrowException(Exceptions::BasicLabel("Failure during file open. Probably missing file or bad privileges: (%s), openMode: (%s)", filename, openMode));
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

    std::unique_ptr<uint8[]> LoadFileAsMemoryBlock(const char sourceFileName[], size_t* sizeResult)
    {
        TRY {
            BasicFile file(sourceFileName, "rb");

            file.Seek(0, SEEK_END);
            size_t size = file.TellP();
            file.Seek(0, SEEK_SET);

            auto result = std::make_unique<uint8[]>(size+1);
            file.Read(result.get(), 1, size);
            result[size] = '\0';
            if (sizeResult) {
                *sizeResult = size;
            }
            return result;
        } CATCH(const std::exception& ) {
            if (sizeResult) { *sizeResult = 0; }
            return nullptr;
        } CATCH_END
    }
}
