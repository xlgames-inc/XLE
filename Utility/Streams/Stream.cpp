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
#include "../Conversion.h"
#include <stdio.h>
#include <wchar.h>
#include <assert.h>
#include <sstream>

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

// --------------------------------------------------------------------------
// File Output
// --------------------------------------------------------------------------

class FileOutputStream : public OutputStream, noncopyable
{
public:
    FileOutputStream(const char filename[], const char openMode[]);

    virtual int64 Tell();
    virtual int64 Write(const void* p, int len);

    virtual void WriteChar(utf8 ch);
    virtual void WriteChar(ucs2 ch);
    virtual void WriteChar(ucs4 ch);
    virtual void WriteString(const utf8* s, const utf8* e);
    virtual void WriteString(const ucs2* s, const ucs2* e);
    virtual void WriteString(const ucs4* s, const ucs4* e);


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

void FileOutputStream::WriteString(const utf8* s, const utf8* e)
{
    _file.Write(s, sizeof(*s), e-s);
}

void FileOutputStream::WriteString(const ucs2* s, const ucs2* e)
{
    _file.Write(s, sizeof(*s), e-s);
}

void FileOutputStream::WriteString(const ucs4* s, const ucs4* e)
{
    _file.Write(s, sizeof(*s), e-s);
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

template<typename BufferType>
    int64 StreamBuf<BufferType>::Tell()
{
    return _buffer.pubseekoff(0, std::ios_base::cur, std::ios_base::out);
}

template<typename BufferType>
    int64 StreamBuf<BufferType>::Write(const void* p, int len)
{
    assert((len % sizeof(typename BufferType::char_type)) == 0);
    _buffer.sputn((const typename BufferType::char_type*)p, len / sizeof(BufferType::char_type));
    return len;
}

template<typename BufferType>
    void StreamBuf<BufferType>::WriteChar(utf8 ch)
{
    BufferType::char_type buffer[4];
    auto count = Conversion::Convert(buffer, ch);
    if (count < 0) _buffer.sputc((BufferType::char_type)"?");
    for (int c=0; c<count; ++c) _buffer.sputc(buffer[c]);
}

template<typename BufferType>
    void StreamBuf<BufferType>::WriteChar(ucs2 ch)
{
    BufferType::char_type buffer[4];
    auto count = Conversion::Convert(buffer, ch);
    if (count < 0) _buffer.sputc((BufferType::char_type)"?");
    for (int c=0; c<count; ++c) _buffer.sputc(buffer[c]);
}

template<typename BufferType>
    void StreamBuf<BufferType>::WriteChar(ucs4 ch)
{
    BufferType::char_type buffer[4];
    auto count = Conversion::Convert(buffer, ch);
    if (count < 0) _buffer.sputc((BufferType::char_type)"?");
    for (int c=0; c<count; ++c) _buffer.sputc(buffer[c]);
}

template<typename OutChar, typename InChar> struct CompatibleCharTypes { static const bool compatible = false; };
template<typename CharType> struct CompatibleCharTypes<CharType, CharType> { static const bool compatible = true; };
template<> struct CompatibleCharTypes<utf8, char> { static const bool compatible = true; };
template<> struct CompatibleCharTypes<char, utf8> { static const bool compatible = true; };
template<> struct CompatibleCharTypes<wchar_t, ucs2> { static const bool compatible = true; };
template<> struct CompatibleCharTypes<ucs2, wchar_t> { static const bool compatible = true; };

template<typename OutChar, typename InChar, typename std::enable_if<CompatibleCharTypes<OutChar, InChar>::compatible>::type* = nullptr>
    void PushString(
        std::basic_streambuf<OutChar>& stream,
        const InChar inputStart[], const InChar inputEnd[])
    {
        stream.sputn((const OutChar*)inputStart, inputEnd - inputStart);
    }

template<typename OutChar, typename InChar, typename typename std::enable_if<!CompatibleCharTypes<OutChar, InChar>::compatible>::type* = nullptr>
    void PushString(
        std::basic_streambuf<OutChar>& stream,
        const InChar inputStart[], const InChar inputEnd[])
    {
            //  String conversion process results in several redundant allocations. It's not perfectly
            //  efficient
        using InputString = std::basic_string<InChar>;
        using OutputString = std::basic_string<OutChar>;
        auto converted = Conversion::Convert<OutputString>(InputString(inputStart, inputEnd));
        stream.sputn(AsPointer(converted.begin()), converted.size());    
    }

template<typename BufferType>
    void StreamBuf<BufferType>::WriteString(const utf8* s, const utf8* e)
{
    PushString(_buffer, s, e);
}
template<typename BufferType>
    void StreamBuf<BufferType>::WriteString(const ucs2* s, const ucs2* e)
{
    PushString(_buffer, s, e);
}

template<typename BufferType>
    void StreamBuf<BufferType>::WriteString(const ucs4* s, const ucs4* e)
{
    PushString(_buffer, s, e);
}

template<typename BufferType>
    void StreamBuf<BufferType>::Flush()
{}

template<typename BufferType> StreamBuf<BufferType>::StreamBuf() {}

template<typename BufferType> StreamBuf<BufferType>::~StreamBuf() {}

template class StreamBuf<Internal::ResizeableMemoryBuffer<char>>;
template class StreamBuf<Internal::ResizeableMemoryBuffer<wchar_t>>;
template class StreamBuf<Internal::ResizeableMemoryBuffer<utf8>>;
template class StreamBuf<Internal::ResizeableMemoryBuffer<ucs2>>;
template class StreamBuf<Internal::ResizeableMemoryBuffer<ucs4>>;

template class StreamBuf<Internal::FixedMemoryBuffer2<char>>;
template class StreamBuf<Internal::FixedMemoryBuffer2<wchar_t>>;
template class StreamBuf<Internal::FixedMemoryBuffer2<utf8>>;
template class StreamBuf<Internal::FixedMemoryBuffer2<ucs2>>;
template class StreamBuf<Internal::FixedMemoryBuffer2<ucs4>>;

}

