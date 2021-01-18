// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Stream.h"
#include "StreamTypes.h"
#include "../../OSServices/RawFS.h"
#include "../MemoryUtils.h"
#include "../StringUtils.h"
#include "../PtrUtils.h"
#include "../Conversion.h"
#include <stdio.h>
#include <wchar.h>
#include <assert.h>
#include <sstream>

namespace Utility
{

// --------------------------------------------------------------------------
// File Output
// --------------------------------------------------------------------------

namespace Legacy
{

class FileOutputStream : public OutputStream
{
public:
    virtual size_type Tell();
    virtual void Write(const void* p, size_type len);
    virtual void WriteChar(char ch);
    virtual void Write(StringSection<utf8> s);

    virtual void Flush();

    FileOutputStream(const char filename[], const char openMode[]);
    FileOutputStream(OSServices::BasicFile&& moveFrom);

    FileOutputStream(FileOutputStream&&) = default;
    FileOutputStream& operator=(FileOutputStream&&) = default;
private:
    OSServices::BasicFile _file;
};

FileOutputStream::FileOutputStream(const char filename[], const char openMode[]) 
: _file(OSServices::BasicFile((const utf8*)filename, openMode, OSServices::FileShareMode::Read))
{}

FileOutputStream::FileOutputStream(OSServices::BasicFile&& moveFrom)
: _file(std::move(moveFrom))
{}

auto FileOutputStream::Tell() -> size_type
{
    return (size_type)_file.TellP();
}

void FileOutputStream::Write(const void* p, size_type len)
{
    _file.Write(p, 1, len);
}

void FileOutputStream::WriteChar(char ch)
{
    _file.Write(&ch, sizeof(ch), 1);
}
void FileOutputStream::Write(StringSection<utf8> s)
{
    _file.Write(s.begin(), sizeof(*s.begin()), s.Length());
}

void FileOutputStream::Flush()
{
    _file.Flush();
}

std::unique_ptr<OutputStream> OpenFileOutput(const char* path, const char* mode)
{
    return std::make_unique<FileOutputStream>(path, mode);
}

std::unique_ptr<OutputStream> OpenFileOutput(OSServices::BasicFile&& moveFrom)
{
    return std::make_unique<FileOutputStream>(std::move(moveFrom));
}

}


template<typename BufferType>
    auto StreamBuf<BufferType>::Tell() -> size_type
{
    return (size_type)_buffer.pubseekoff(0, std::ios_base::cur, std::ios_base::out);
}

template<typename BufferType>
    void StreamBuf<BufferType>::Write(const void* p, size_type len)
{
    assert((len % sizeof(typename BufferType::char_type)) == 0);
    _buffer.sputn((const typename BufferType::char_type*)p, len / sizeof(typename BufferType::char_type));
}

template<typename BufferType>
    void StreamBuf<BufferType>::WriteChar(char ch)
{
    _buffer.sputc((typename BufferType::char_type)ch);
}
template<typename OutChar, typename InChar> struct CompatibleCharTypes { static const bool compatible = false; };
template<typename CharType> struct CompatibleCharTypes<CharType, CharType> { static const bool compatible = true; };
template<> struct CompatibleCharTypes<utf8, char> { static const bool compatible = true; };
// template<> struct CompatibleCharTypes<char, utf8> { static const bool compatible = true; };
template<> struct CompatibleCharTypes<wchar_t, ucs2> { static const bool compatible = true; };
template<> struct CompatibleCharTypes<ucs2, wchar_t> { static const bool compatible = true; };

template<typename OutChar, typename InChar, typename std::enable_if<CompatibleCharTypes<OutChar, InChar>::compatible>::type* = nullptr>
    void PushString(
        std::basic_streambuf<OutChar>& stream,
        StringSection<InChar> input)
    {
        stream.sputn((const OutChar*)input.begin(), input.Length());
    }

template<typename OutChar, typename InChar, typename std::enable_if<!CompatibleCharTypes<OutChar, InChar>::compatible>::type* = nullptr>
    void PushString(
        std::basic_streambuf<OutChar>& stream,
        StringSection<InChar> input)
    {
            //  String conversion process results in several redundant allocations. It's not perfectly
            //  efficient
        using OutputString = std::basic_string<OutChar>;
        auto converted = Conversion::Convert<OutputString>(input.AsString());
        stream.sputn(AsPointer(converted.begin()), converted.size());    
    }

template<typename BufferType>
    void StreamBuf<BufferType>::Write(StringSection<utf8> s)
{
    PushString(_buffer, s);
}

template<typename BufferType>
    void StreamBuf<BufferType>::Flush()
{}

template<typename BufferType> StreamBuf<BufferType>::StreamBuf() {}

template<typename BufferType> StreamBuf<BufferType>::~StreamBuf() {}

template class StreamBuf<Internal::ResizeableMemoryBuffer<utf8>>;
template class StreamBuf<Internal::FixedMemoryBuffer2<utf8>>;
template class StreamBuf<Internal::ResizeableMemoryBuffer<utf16>>;
template class StreamBuf<Internal::FixedMemoryBuffer2<utf16>>;

}

