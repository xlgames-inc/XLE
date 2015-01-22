// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../FileUtils.h"
#include "../PathUtils.h"
#include "../../StringUtils.h"
#include <assert.h>

#include "../../Core/WinAPI/IncludeWindows.h"

namespace Utility 
{

    BasicFile::BasicFile(   const char filename[], const char openMode[], 
                            ShareMode::BitField shareMode)
    {
        assert(filename && filename[0]);
        assert(openMode);

        unsigned underlyingAccessMode = 0;
        unsigned underlyingShareMode = 0;
        unsigned creationDisposition = 0;
        unsigned underlyingFlags = 0;

        if (shareMode & ShareMode::Write)   { underlyingShareMode |= FILE_SHARE_WRITE; }
        if (shareMode & ShareMode::Read)    { underlyingShareMode |= FILE_SHARE_READ; }

        if (XlFindString(openMode, "w+")) {
            underlyingAccessMode = FILE_GENERIC_WRITE | FILE_GENERIC_READ;
            creationDisposition = CREATE_ALWAYS;
        } else if (XlFindString(openMode, "r+")) {
            underlyingAccessMode = FILE_GENERIC_WRITE | FILE_GENERIC_READ;
            creationDisposition = OPEN_EXISTING;
        } else if (XlFindString(openMode, "a+")) {
            assert(0); // not supported;
            ThrowException(Exceptions::IOException("Append file mode not supported"));
        } else if (XlFindChar(openMode, 'w')) {
            underlyingAccessMode = FILE_GENERIC_WRITE;
            creationDisposition = CREATE_ALWAYS;
        } else if (XlFindChar(openMode, 'r')) {
            underlyingAccessMode = FILE_GENERIC_READ;
            creationDisposition = OPEN_EXISTING;
        } else if (XlFindChar(openMode, 'a')) {
            assert(0); // not supported;
            ThrowException(Exceptions::IOException("Append file mode not supported"));
        }

        if (XlFindChar(openMode, 't')) {
            assert(0); // not supported;
            ThrowException(Exceptions::IOException("Text oriented file modes not supported"));
        }
        if (XlFindString(openMode, "ccs=")) {
            assert(0); // not supported;
            ThrowException(Exceptions::IOException("Encoded text file modes supported"));
        }

        if (XlFindChar(openMode, 'T') || XlFindChar(openMode, 'D')) {
            underlyingFlags |= FILE_ATTRIBUTE_TEMPORARY;
        }

        if (XlFindChar(openMode, 'R')) { underlyingFlags |= FILE_FLAG_RANDOM_ACCESS; }
        if (XlFindChar(openMode, 'S')) { underlyingFlags |= FILE_FLAG_SEQUENTIAL_SCAN; }

        auto handle = CreateFile(
            filename, 
            underlyingAccessMode,
            underlyingShareMode,
            nullptr, creationDisposition,
            underlyingFlags, nullptr);
        
        if (handle == INVALID_HANDLE_VALUE) {

                // use "FormatMessage" to get error code
                //  (as per msdn article: "Retrieving the Last-Error Code")
            LPVOID lpMsgBuf;
            DWORD dw = GetLastError(); 
            FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                dw,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &lpMsgBuf,
                0, NULL );
            Exceptions::IOException except(
                "Failure during file open. Probably missing file or bad privileges: (%s), openMode: (%s), error string: (%s)", 
                filename, openMode, lpMsgBuf);
            LocalFree(lpMsgBuf);

            ThrowException(except);
        }

        _file = (void*)handle;
    }

    BasicFile::BasicFile(BasicFile&& moveFrom) never_throws
    {
        _file = moveFrom._file;
        moveFrom._file = INVALID_HANDLE_VALUE;
    }

    BasicFile& BasicFile::operator=(BasicFile&& moveFrom) never_throws
    {
        if (_file != INVALID_HANDLE_VALUE) {
            CloseHandle(_file);
        }
        _file = moveFrom._file;
        moveFrom._file = INVALID_HANDLE_VALUE;
        return *this;
    }

    BasicFile::BasicFile()
    {
        _file = INVALID_HANDLE_VALUE;
    }

    BasicFile::~BasicFile()
    {
        if (_file != INVALID_HANDLE_VALUE) {
            CloseHandle(_file);
        }
    }

    size_t   BasicFile::Read(void *buffer, size_t size, size_t count) const never_throws
    {
        if (!(size * count)) return 0;
        DWORD bytesRead = 0;
        auto result = ReadFile(_file, buffer, size * count, &bytesRead, nullptr);
        auto errorCode = GetLastError();
        assert((bytesRead%size)==0); (void)errorCode;
        return result?(bytesRead/size):0;
    }
    
    size_t   BasicFile::Write(const void *buffer, size_t size, size_t count) never_throws
    {
        if (!(size * count)) return 0;
        DWORD bytesWritten = 0;
        auto result = WriteFile(_file, buffer, size * count, &bytesWritten, nullptr);
        assert((bytesWritten%size)==0);
        return result?(bytesWritten/size):0;
    }

    size_t   BasicFile::Seek(size_t offset, int origin) never_throws
    {
        unsigned underlingMoveMethod = 0;
        switch (origin) {
        case SEEK_SET: underlingMoveMethod = FILE_BEGIN; break;
        case SEEK_CUR: underlingMoveMethod = FILE_CURRENT; break;
        case SEEK_END: underlingMoveMethod = FILE_END; break;
        default: assert(0);
        }
        return SetFilePointer(_file, offset, nullptr, underlingMoveMethod);
    }

    size_t   BasicFile::TellP() const never_throws
    {
        return SetFilePointer(_file, 0, nullptr, FILE_CURRENT);
    }

    void    BasicFile::Flush() const never_throws
    {
        FlushFileBuffers(_file);
    }

    uint64      BasicFile::GetSize() never_throws
    {
        if (_file == INVALID_HANDLE_VALUE) return 0;
        DWORD highWord = 0;
        auto lowWord = ::GetFileSize(_file, &highWord);
        return (uint64(highWord)<<32ull) | uint64(lowWord);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    bool DoesFileExist(const char filename[])
    {
        DWORD dwAttrib = GetFileAttributes(filename);
        return dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
    }

    void CreateDirectoryRecursive(const char filename[])
    {
        const char delims[] = "/\\";

        char filenameCopy[MaxPath];
        XlCopyString(filenameCopy, filename);
        
        char* start = XlFindNot(filenameCopy, delims);
        char* i = start;
        while (i) {
            char* t = XlFindAnyChar(i, delims);

            if (t) {
                char q = 0;
                std::swap(q, *t);
                CreateDirectory(filenameCopy, nullptr);
                std::swap(q, *t);
                i = XlFindNot(t, delims);
            } else {
                CreateDirectory(filenameCopy, nullptr);
                i = t;
            }
        }
    }

    uint64 GetFileModificationTime(const char filename[])
    {
        WIN32_FILE_ATTRIBUTE_DATA attribData;
        auto result = GetFileAttributesEx(filename, GetFileExInfoStandard, &attribData);
        if (!result) return 0ull;
        return (uint64(attribData.ftLastWriteTime.dwHighDateTime) << 32ull) | uint64(attribData.ftLastWriteTime.dwLowDateTime);
    }

    uint64 GetFileSize(const char filename[])
    {
        WIN32_FILE_ATTRIBUTE_DATA attribData;
        auto result = GetFileAttributesEx(filename, GetFileExInfoStandard, &attribData);
        if (!result) return 0ull;
        return (uint64(attribData.nFileSizeHigh) << 32ull) | uint64(attribData.nFileSizeLow);
    }

    std::vector<std::string> FindFiles(const std::string& searchPath, FindFilesFilter::BitField filter)
    {
        std::vector<std::string> result;

        char buffer[256];
        XlDirname(buffer, dimof(buffer), searchPath.c_str());
        std::string basePath = buffer;
        if (!basePath.empty() && basePath[basePath.size()-1]!='/') {
            basePath += "/";
        }
        WIN32_FIND_DATAA findData;
        memset(&findData, 0, sizeof(findData));
        HANDLE findHandle = FindFirstFileA(searchPath.c_str(), &findData);
        if (findHandle != INVALID_HANDLE_VALUE) {
            do {
                bool isDir = !!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
                if (filter & (1<<unsigned(isDir))) {
                    result.push_back(basePath + findData.cFileName);
                }
            } while (FindNextFileA(findHandle, &findData));
            FindClose(findHandle);
        }

        return std::move(result);
    }

    MemoryMappedFile::MemoryMappedFile(const char filename[], uint64 size, Access::BitField access)
    {
        _mapping = INVALID_HANDLE_VALUE;
        _fileHandle = INVALID_HANDLE_VALUE;
        _mappedData = nullptr;

        unsigned underlyingAccess = 0;
        if (access & Access::Read)  underlyingAccess |= GENERIC_READ;
        if (access & Access::Write) underlyingAccess |= GENERIC_WRITE|GENERIC_READ;

        unsigned creationDisposition = OPEN_EXISTING;
        if (access & Access::Write && (!(access & Access::Read))) {
            creationDisposition = CREATE_ALWAYS;
        } else if (access & Access::OpenAlways) {
            creationDisposition = OPEN_ALWAYS;
        }

        auto fileHandle = CreateFile(
            filename, underlyingAccess, 0, nullptr, creationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (fileHandle == INVALID_HANDLE_VALUE) {
            return;
        }

        unsigned pageAccessMode = (access & Access::Write) ? PAGE_READWRITE : PAGE_READONLY;
        auto mapping = CreateFileMapping(
            fileHandle, nullptr, pageAccessMode, DWORD(size>>32), DWORD(size), nullptr);
        if (!mapping || mapping == INVALID_HANDLE_VALUE) {
            CloseHandle(fileHandle);
            return;
        }

        unsigned mapAccess = (access & Access::Write) ? FILE_MAP_WRITE : FILE_MAP_READ;
        auto mappingStart = MapViewOfFile(mapping, mapAccess, 0, 0, 0);
        if (!mappingStart) {
            CloseHandle(mapping);
            CloseHandle(fileHandle);
            return;
        }

        _mappedData = mappingStart;
        _mapping = mapping;
        _fileHandle = fileHandle;
    }

    MemoryMappedFile::~MemoryMappedFile()
    {
        if (_mappedData != nullptr) {
            UnmapViewOfFile(_mappedData);
        }
        CloseHandle(_mapping);
        CloseHandle(_fileHandle);
    }
}

