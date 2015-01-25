// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "OutputStream.h"
#include "Console.h"
#include "../../Utility/Streams/Stream.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/StringFormat.h"
#include <assert.h>
#include <algorithm>

#if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS
    extern "C" dll_import void __stdcall OutputDebugStringA(const char lpOutputString[]);
    extern "C" dll_import void __stdcall OutputDebugStringW(const wchar_t lpOutputString[]);
#endif

namespace ConsoleRig
{

        ////    B U F F E R E D   O U T P U T   S T R E A M   ////

    class BufferedOutputStream : public Utility::OutputStream
    {
    public:
        virtual Type    GetType() const;
        virtual int64   Tell();
        virtual int64   Write(const void* p, int len);
        virtual void    WriteChar(utf8 ch);
        virtual void    WriteChar(ucs2 ch);
        virtual void    WriteChar(ucs4 ch);

        virtual void    WriteString(const utf8* s);
        virtual void    WriteString(const ucs2* s);
        virtual void    WriteString(const ucs4* s);

        virtual void    Flush();

        BufferedOutputStream(std::shared_ptr<Utility::OutputStream> nextStream);
        virtual ~BufferedOutputStream();

    private:
        uint8       _buffer[4096];
        unsigned    _bufferPosition;
        std::shared_ptr<Utility::OutputStream>      _nextStream;
    };

    auto    BufferedOutputStream::GetType() const -> Type   { return OSTRM_MEM; }
    int64   BufferedOutputStream::Tell()                    { return ~int64(0x0); }

    int64   BufferedOutputStream::Write(const void* p, int len)
    {
        if ((_bufferPosition + len) <= dimof(_buffer)) {

            XlCopyMemory(PtrAdd(_buffer, _bufferPosition), p, len);
            _bufferPosition += len;

        } else if (len >= dimof(_buffer)) {

                //      Very long input. Just flush the buffer and send this down to
                //      the next stream
            Flush();
            if (_nextStream) {
                _nextStream->Write(p, len);
            }

        } else {

                //      Copy as much as possible, flush and then 
                //      restart the buffer
            unsigned firstCopyLength = dimof(_buffer) - _bufferPosition;
            if (firstCopyLength) {
                XlCopyMemory(PtrAdd(_buffer, _bufferPosition), p, firstCopyLength);
                _bufferPosition += firstCopyLength;
            }
            Flush();

            unsigned secondCopyLength = len-firstCopyLength;
            if (secondCopyLength) {
                XlCopyMemory(_buffer, PtrAdd(p, firstCopyLength), secondCopyLength);
            }
            _bufferPosition = len-firstCopyLength;

        }

        return len;
    }

    void    BufferedOutputStream::WriteChar(utf8 ch)             { Write(&ch, sizeof(ch)); }
    void    BufferedOutputStream::WriteChar(ucs2 ch)             { Write(&ch, sizeof(ch)); }
    void    BufferedOutputStream::WriteChar(ucs4 ch)             { Write(&ch, sizeof(ch)); }

    void    BufferedOutputStream::WriteString(const utf8* s)     { Write(s, int(XlStringLen(s) * sizeof(*s))); }
    void    BufferedOutputStream::WriteString(const ucs2* s)     { Write(s, int(XlStringLen(s) * sizeof(*s))); }
    void    BufferedOutputStream::WriteString(const ucs4* s)     { Write(s, int(XlStringLen(s) * sizeof(*s))); }

    void    BufferedOutputStream::Flush()
    {
        if (_bufferPosition && _nextStream) {
            _nextStream->Write(_buffer, _bufferPosition);
        }
        _bufferPosition = 0;
    }

    BufferedOutputStream::BufferedOutputStream(std::shared_ptr<Utility::OutputStream> nextStream)
    :       _nextStream(std::move(nextStream))
    ,       _bufferPosition(0)
    {
    }

    BufferedOutputStream::~BufferedOutputStream()
    {
        Flush();
    }

        ////    F O R K   O U T P U T   S T R E A M   ////

    class ForkOutputStream : public Utility::OutputStream
    {
    public:
        virtual Type    GetType() const;
        virtual int64   Tell();
        virtual int64   Write(const void* p, int len);
        virtual void    WriteChar(utf8 ch);
        virtual void    WriteChar(ucs2 ch);
        virtual void    WriteChar(ucs4 ch);

        virtual void    WriteString(const utf8* s);
        virtual void    WriteString(const ucs2* s);
        virtual void    WriteString(const ucs4* s);

        virtual void    Flush();

        ForkOutputStream(std::shared_ptr<Utility::OutputStream> first, std::shared_ptr<Utility::OutputStream> second);
        virtual ~ForkOutputStream();

    private:
        std::shared_ptr<Utility::OutputStream>      _first;
        std::shared_ptr<Utility::OutputStream>      _second;
    };

    auto    ForkOutputStream::GetType() const -> Type   { return _first->GetType(); }
    int64   ForkOutputStream::Tell()                    { return _first->Tell(); }

    int64   ForkOutputStream::Write(const void* p, int len)
    {
        return std::min(
            _first->Write(p, len),
            _second->Write(p, len));
    }

    void    ForkOutputStream::WriteChar(utf8 ch)             { _first->Write(&ch, sizeof(ch)); _second->Write(&ch, sizeof(ch)); }
    void    ForkOutputStream::WriteChar(ucs2 ch)             { _first->Write(&ch, sizeof(ch)); _second->Write(&ch, sizeof(ch)); }
    void    ForkOutputStream::WriteChar(ucs4 ch)             { _first->Write(&ch, sizeof(ch)); _second->Write(&ch, sizeof(ch)); }

    void    ForkOutputStream::WriteString(const utf8* s)     { size_t len = XlStringLen(s); _first->Write(s, int(len * sizeof(*s))); _second->Write(s, int(len * sizeof(*s))); }
    void    ForkOutputStream::WriteString(const ucs2* s)     { size_t len = XlStringLen(s); _first->Write(s, int(len * sizeof(*s))); _second->Write(s, int(len * sizeof(*s))); }
    void    ForkOutputStream::WriteString(const ucs4* s)     { size_t len = XlStringLen(s); _first->Write(s, int(len * sizeof(*s))); _second->Write(s, int(len * sizeof(*s))); }

    void    ForkOutputStream::Flush()
    {
        _first->Flush();
        _second->Flush();
    }

    ForkOutputStream::ForkOutputStream(std::shared_ptr<Utility::OutputStream> first, std::shared_ptr<Utility::OutputStream> second)
    :       _first(first)
    ,       _second(second)
    {}

    ForkOutputStream::~ForkOutputStream()
    {
    }

            ////    C O N S O L E   O U T P U T   S T R E A M   ////

    class ConsoleOutputStream : public Utility::OutputStream
    {
    public:
        virtual Type    GetType() const;
        virtual int64   Tell();
        virtual int64   Write(const void* p, int len);
        virtual void    WriteChar(utf8 ch);
        virtual void    WriteChar(ucs2 ch);
        virtual void    WriteChar(ucs4 ch);

        virtual void    WriteString(const utf8* s);
        virtual void    WriteString(const ucs2* s);
        virtual void    WriteString(const ucs4* s);

        virtual void    Flush();

        ConsoleOutputStream();
        virtual ~ConsoleOutputStream();
    };

    auto    ConsoleOutputStream::GetType() const -> Type   { return OSTRM_MEM; }
    int64   ConsoleOutputStream::Tell()                    { return ~int64(0x0); }

    int64   ConsoleOutputStream::Write(const void* p, int len)
    {
        Console::GetInstance().Print(std::string((char*)p, len));
        return len;
    }

        //      Character type conversions not handled correctly!
    void    ConsoleOutputStream::WriteChar(utf8 ch)             { Console::GetInstance().Print(std::string((char*)&ch, 1)); }
    void    ConsoleOutputStream::WriteChar(ucs2 ch)             { Console::GetInstance().Print(std::u16string((char16_t*)&ch, 1)); }
    void    ConsoleOutputStream::WriteChar(ucs4 ch)             { assert(0); }

    void    ConsoleOutputStream::WriteString(const utf8* s)     { Console::GetInstance().Print((const char*)s); }
    void    ConsoleOutputStream::WriteString(const ucs2* s)     { Console::GetInstance().Print((char16_t*)s); }
    void    ConsoleOutputStream::WriteString(const ucs4* s)     { assert(0); }

    void    ConsoleOutputStream::Flush(){}

    ConsoleOutputStream::ConsoleOutputStream()
    {}

    ConsoleOutputStream::~ConsoleOutputStream()
    {}


    #if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS

            ////    D E B U G   C O N S O L E   O U T P U T   ////

        class DebuggerConsoleOutput : public Utility::OutputStream
        {
        public:
            virtual Type    GetType() const;
            virtual int64   Tell();
            virtual int64   Write(const void* p, int len);
            virtual void    WriteChar(utf8 ch);
            virtual void    WriteChar(ucs2 ch);
            virtual void    WriteChar(ucs4 ch);

            virtual void    WriteString(const utf8* s);
            virtual void    WriteString(const ucs2* s);
            virtual void    WriteString(const ucs4* s);

            virtual void    Flush();

            DebuggerConsoleOutput();
            virtual ~DebuggerConsoleOutput();
        };


        auto    DebuggerConsoleOutput::GetType() const -> Type              { return OSTRM_MEM; }
        int64   DebuggerConsoleOutput::Tell()                               { return ~int64(0x0); }

        int64   DebuggerConsoleOutput::Write(const void* p, int len)
        {
                //
                //              Input is treated as UTF-8. If we're using BufferedOutputStream, we may
                //              have forgotten the input type on the way through...
                //
            uint8 buffer[4096];
            while (len) {

                    //
                    //          Have to append the null terminator!
                    //
                size_t copyAmount = std::min(dimof(buffer) - 1, size_t(len));
                XlCopyMemory(buffer, p, copyAmount);
                buffer[copyAmount]      = '\0';

                OutputDebugStringA((const char*)buffer);

                p = PtrAdd(p, copyAmount);
                len -= (int)copyAmount;
            }
            return 0;
        }

        void    DebuggerConsoleOutput::WriteChar(utf8 ch)
        {
            utf8 buffer[2];
            buffer[0] = ch;
            buffer[1] = (utf8)'\0';
            OutputDebugStringA((const char*)buffer);
        }

        void    DebuggerConsoleOutput::WriteChar(ucs2 ch)
        {
            ucs2 buffer[2];
            buffer[0] = ch;
            buffer[1] = '\0';
            OutputDebugStringW((const wchar_t*)buffer);
        }

        void    DebuggerConsoleOutput::WriteChar(ucs4 ch)
        {
            assert(0);
        }

        void    DebuggerConsoleOutput::WriteString(const utf8* s)
        {
            OutputDebugStringA((const char*)s);
        }

        void    DebuggerConsoleOutput::WriteString(const ucs2* s)
        {
            OutputDebugStringW((const wchar_t*)s);
        }

        void    DebuggerConsoleOutput::WriteString(const ucs4* s)
        {
            assert(0);
        }

        void    DebuggerConsoleOutput::Flush()          {}
        DebuggerConsoleOutput::DebuggerConsoleOutput()  {}
        DebuggerConsoleOutput::~DebuggerConsoleOutput() {}

    #endif



    static std::shared_ptr<Utility::OutputStream>      GetSharedDebuggerWarningStream()
    {
        static auto result = std::make_shared<BufferedOutputStream>(std::make_shared<DebuggerConsoleOutput>());
        return result;
    }

    Utility::OutputStream&      GetWarningStream()
    {
        static ForkOutputStream stream(
            GetSharedDebuggerWarningStream(),
            std::make_shared<ConsoleOutputStream>());
        return stream;
    }

    Utility::OutputStream&      GetDebuggerWarningStream()
    {
        return *GetSharedDebuggerWarningStream();
    }

    void xleWarning(const char format[], va_list args)
    {
        ConsoleRig::GetWarningStream().WriteString((utf8*)"{Color:ff7f7f}");
        PrintFormatV(&ConsoleRig::GetWarningStream(), format, args);
        ConsoleRig::GetDebuggerWarningStream().Flush();
    }

    void xleWarning(const char format[], ...)
    {
        va_list args;
        va_start(args, format);
        xleWarning(format, args);
        va_end(args);
    }
    
    #if defined(_DEBUG)
        void xleWarningDebugOnly(const char format[], ...)
        {
            va_list args;
            va_start(args, format);
            xleWarning(format, args);
            va_end(args);
        }
    #endif


    void DebuggerOnlyWarning(const char format[], ...)
    {
        va_list args;
        va_start(args, format);
        PrintFormatV(&ConsoleRig::GetDebuggerWarningStream(), format, args);
        va_end(args);
    }

}
