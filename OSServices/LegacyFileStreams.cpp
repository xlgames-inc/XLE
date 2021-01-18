// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LegacyFileStreams.h"
#include "RawFS.h"
#include "../Utility/Streams/Stream.h"

namespace OSServices { namespace Legacy 
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


}}

