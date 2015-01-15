// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Stream.h"
#include "StreamTypes.h"
#include "FileUtils.h"
#include "../MemoryUtils.h"
#include "../StringUtils.h"
#include "../Mixins.h"
#include "../PtrUtils.h"
#include <stdio.h>
#include <wchar.h>
#include <assert.h>

namespace Utility
{

// --------------------------------------------------------------------------
// File Input
// --------------------------------------------------------------------------
class FileInputStream : public InputStream, noncopyable
{
public:
    FileInputStream(const char filename[], const char openMode[]);
    virtual int Read(void* p, int len);
    virtual bool Seek(StreamSeekType type, int64 offset);
private:
    BasicFile _file;
};

FileInputStream::FileInputStream(const char filename[], const char openMode[]) :
_file(filename, openMode)
{
}

int FileInputStream::Read(void* p, int len)
{
    return (int)_file.Read(p, 1, len);
}

bool FileInputStream::Seek(StreamSeekType type, int64 offset)
{
    switch (type) {
    case SST_CUR:
        return _file.Seek(long(offset), SEEK_CUR) != size_t(0);
    }
    return false;
}

std::unique_ptr<InputStream> OpenFileInput(const char* path, const char* mode)
{
    return std::make_unique<FileInputStream>(path, mode);
}

// --------------------------------------------------------------------------
// File Output
// --------------------------------------------------------------------------

class FileOutputStream : public OutputStream, noncopyable
{
public:
    FileOutputStream(const char filename[], const char openMode[]);

    virtual Type GetType() const { return OSTRM_FILE; }
    virtual int64 Tell();
    virtual int64 Write(const void* p, int len);

    virtual void WriteChar(utf8 ch);
    virtual void WriteChar(ucs2 ch);
    virtual void WriteChar(ucs4 ch);
    virtual void WriteString(const utf8* s);
    virtual void WriteString(const ucs2* s);
    virtual void WriteString(const ucs4* s);


    virtual void Flush();

private:
    BasicFile _file;
};

FileOutputStream::FileOutputStream(const char filename[], const char openMode[]) 
: _file(filename, openMode)
{
}

int64 FileOutputStream::Tell()
{
    return (int64)_file.TellP();
}

int64 FileOutputStream::Write(const void* p, int len)
{
    return (int64)_file.Write(p, 1, len);
}

void FileOutputStream::WriteChar(utf8 ch)
{
    _file.Write(&ch, sizeof(ch), 1);
}

void FileOutputStream::WriteChar(ucs2 ch)
{
    //      DavidJ -- note this used to (intentionally) contain a bug, as such:
    //  _file.Put(int(ch));     // (as per previous implementation)
    _file.Write(&ch, sizeof(ch), 1);
}

void FileOutputStream::WriteChar(ucs4 ch)
{
    //      DavidJ -- note this used to (intentionally) contain a bug, as such:
    //  _file.Put(ucs2(ch));     // (as per previous implementation)
    _file.Write(&ch, sizeof(ch), 1);
}

void FileOutputStream::WriteString(const utf8* s)
{
    _file.Write(s, sizeof(*s), XlStringLen(s));
}

void FileOutputStream::WriteString(const ucs2* s)
{
    _file.Write(s, sizeof(*s), XlStringLen(s));
}

void FileOutputStream::WriteString(const ucs4* s)
{
    _file.Write(s, sizeof(*s), XlStringLen(s));
}

void FileOutputStream::Flush()
{
    _file.Flush();
}

std::unique_ptr<OutputStream> OpenFileOutput(const char* path, const char* mode)
{
    return std::make_unique<FileOutputStream>(path, mode);
}

// --------------------------------------------------------------------------
// Memory Input
// --------------------------------------------------------------------------
MemoryInputStream::MemoryInputStream(const void* s, int len) :
_ptr((uint8*)s),
    _begin((uint8*)s),
    _end((uint8*)s + len)
{
}

int MemoryInputStream::Read(void* p, int len)
{
    int avail = (int)(_end - _ptr);
    if (len > avail) len = avail;
    XlCopyMemory(p, _ptr, len);
    _ptr += len;
    return len;
}

// type := naturally SEEK_CUR
bool MemoryInputStream::Seek(StreamSeekType /*type*/, int64 offset)
{
    const uint8* p = _ptr + offset;
    if (p < _begin || p >= _end)
        return false;
    _ptr = p;
    return true;
}


std::unique_ptr<InputStream> OpenMemoryInput(const void* s, int len)
{
    return std::make_unique<MemoryInputStream>(s, len);
}

MemoryOutputStream::MemoryOutputStream(void* s, int len) :
_ptr((uint8*)s),
    _begin((uint8*)s),
    _end((uint8*)s + len)
{
}

int64 MemoryOutputStream::Tell()
{
    return (int64)(_ptr - _begin);
}

int64 MemoryOutputStream::Write(const void* p, int len)
{
    int avail = (int)(_end - _ptr);
    if (len > avail) len = avail;
    XlCopyMemory(_ptr, p, len);
    _ptr += len;
    return len;
}

void MemoryOutputStream::WriteChar(utf8 ch)
{
    if (_ptr < _end)
        *_ptr++ = ch;
}

void MemoryOutputStream::WriteChar(ucs2 ch)
{
    if ((_ptr+1) < _end) {
        *(ucs2*)_ptr = ch;
        _ptr += 2;
    }
}

void MemoryOutputStream::WriteChar(ucs4 ch)
{
    if (_ptr + 3 < _end) {
        *(ucs4*)_ptr = ch;
        _ptr += 4;
    }
}

void MemoryOutputStream::WriteString(const utf8* s)
{
    while (_ptr < _end && *s)
        *_ptr++ = *s++;
}

void MemoryOutputStream::WriteString(const ucs2* s)
{
    while (_ptr + 1 < _end && *s) {
        *(ucs2*)_ptr = *s++;
        _ptr += 2;
    }
}

void MemoryOutputStream::WriteString(const ucs4* s)
{
    while (_ptr + 3 < _end && *s) {
        *(ucs4*)_ptr = *s++;
        _ptr += 4;
    }
}

std::unique_ptr<OutputStream> OpenMemoryOutput(void *s, int len)
{
    return std::make_unique<MemoryOutputStream>(s, len);
}

// #undef new
// 
// OutputStream* OpenStringOutput(char* b, int bl, char *s, int sl)
// {
//     if (bl < sizeof(StringOutputStream<char>)) {
//         return 0;
//     }
//     return new (b) StringOutputStream<char>(s, sl);
// }
// 
// #if defined(DEBUG_NEW)
//     #define new DEBUG_NEW
// #endif

}

