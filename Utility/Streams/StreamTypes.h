// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Core/Types.h"
#include "Stream.h"
#include <streambuf>

namespace Utility
{

    // --------------------------------------------------------------------------
    // Memory Input
    // --------------------------------------------------------------------------

    class XL_UTILITY_API MemoryInputStream : public InputStream 
    {
    public:
        const uint8* _ptr;
        const uint8* _begin;
        const uint8* _end;

        MemoryInputStream(const void* s, int len);
        virtual int Read(void* p, int len);
        virtual bool Seek(StreamSeekType type, int64 offset);
    };

    // --------------------------------------------------------------------------
    // Memory Output
    // --------------------------------------------------------------------------

    template<typename BufferType>
        class XL_UTILITY_API StreamBuf : public OutputStream 
    {
    public:
        virtual size_type Tell();
        virtual void Write(const void* p, size_type len);

        virtual void WriteChar(utf8 ch);
        virtual void WriteChar(ucs2 ch);
        virtual void WriteChar(ucs4 ch);
        virtual void Write(StringSection<utf8>);
        virtual void Write(StringSection<ucs2>);
        virtual void Write(StringSection<ucs4>);

        virtual void Flush();

    private:
        template <typename C> static unsigned StrTest(decltype(&C::str)*);
        template <typename C> static char StrTest(...);
        template <typename C> static unsigned IsFullTest(decltype(&C::str)*);
        template <typename C> static char IsFullTest(...);

        template<typename U, void (U::*)(typename U::char_type*, size_t) const> struct FunctionSignature {};
        template <typename C> static unsigned ConstructorTest(FunctionSignature<C, &C::C>*);
        template <typename C> static char ConstructorTest(...);
    public:

            //  If the "BufferType" type has a method called str(), then we
            //  should have an AsString() method that calls str(). 
            //  if str() is missing, then AsString() is also missing.
            //  (likewise for IsFull)
        
        template<
            typename Buffer = BufferType,
            typename std::enable_if<(sizeof(StrTest<Buffer>(0)) > 1)>::type* = nullptr>
        auto AsString() const -> decltype(((Buffer*)nullptr)->str()) { return _buffer.str(); }

        template<
            typename Buffer = BufferType,
            typename std::enable_if<(sizeof(IsFullTest<Buffer>(0)) > 1)>::type* = nullptr>
        bool IsFull() const { return _buffer.IsFull(); }

        const BufferType& GetBuffer() const { return _buffer; }

        using CharType = typename BufferType::char_type;

        StreamBuf();
        ~StreamBuf();

        template<
            typename Buffer = BufferType,
            typename std::enable_if<(sizeof(ConstructorTest<Buffer>(0)) > 1)>::type* = nullptr>
            StreamBuf(CharType* buffer, size_t bufferCharCount)
            : _buffer(buffer, bufferCharCount) {}

    protected:
        BufferType _buffer;
    };

    namespace Internal
    {
        template<typename CharType>
            struct FixedMemoryBuffer2 : public std::basic_streambuf<CharType>
        {
            typedef typename std::basic_streambuf<CharType>::char_type char_type;

            bool IsFull() const { return pptr() >= epptr(); }
            unsigned Length() const { return unsigned(pptr() - pbase()); }

            FixedMemoryBuffer2(CharType buffer[], size_t bufferCharCount) 
            {
                this->setp(buffer, &buffer[bufferCharCount-1]);
                for (unsigned c=0; c<bufferCharCount; ++c) buffer[c] = 0;
            }
            FixedMemoryBuffer2() {}
            ~FixedMemoryBuffer2() {}
        };

        template<typename CharType>
            struct ResizeableMemoryBuffer : public std::basic_stringbuf<CharType>
        {
            typedef typename std::basic_stringbuf<CharType>::char_type char_type;

            CharType* Begin() const { return pbase(); }
            CharType* End() const   { return pptr(); }

            ResizeableMemoryBuffer() {}
            ~ResizeableMemoryBuffer() {}
        };
    }

    template<typename CharType = char>
        using MemoryOutputStream = StreamBuf<Internal::ResizeableMemoryBuffer<CharType>>;

    template<typename CharType = char>
        using FixedMemoryOutputStream = StreamBuf<Internal::FixedMemoryBuffer2<CharType>>;
}

using namespace Utility;
