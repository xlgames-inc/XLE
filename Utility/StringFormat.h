// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "PtrUtils.h"
#include "../Core/Exceptions.h"
#include "../Core/Types.h"
#include "Detail/API.h"
#include "MemoryUtils.h"
#include <string>
#include <stdarg.h>

namespace Utility
{

    XL_UTILITY_API int xl_sprintf(char *buf, const char *fmt, ...);
    XL_UTILITY_API int xl_vsprintf(char *buf, const char *fmt, va_list args);
    XL_UTILITY_API int xl_vsnprintf(char* buf, int size, const char* format, va_list& args);

    inline int xl_snprintf(char* buf, int count, const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        int n = xl_vsnprintf(buf, count, fmt, args);
        va_end(args);
        return n;
    }

    class OutputStream;
    XL_UTILITY_API int      PrintFormatV(OutputStream* stream, const char* fmt, va_list args);
    XL_UTILITY_API int      PrintFormat(OutputStream* stream, const char* fmt, ...);

    XL_UTILITY_API int      XlFormatStringV(char* buf, int count, const char* fmt, va_list args) never_throws;
    XL_UTILITY_API int      XlFormatString(char* buf, int count, const char* fmt, ...);

    XL_UTILITY_API std::basic_string<char>   XlDynFormatString(const char format[], ...);

    #if REDIRECT_CLIB_WITH_PREPROCESSOR

        #include <stdio.h>      // (required so we can redefined sprintf)

        #undef sprintf
        #undef vsprintf
        #define sprintf xl_sprintf
        #define vsprintf xl_vsprintf
        #undef vsprintf
        #define vsprintf xl_vsprintf
        #undef vsprintf_s
        #define vsprintf_s xl_vsnprintf

    #endif


    namespace Internal
    {
        template<int SizeInBytes>
            struct FixedMemoryBuffer : public std::streambuf 
        {
            uint8 _buffer[SizeInBytes];
            FixedMemoryBuffer(size_t charSize)
            {
                this->setp((char*)_buffer, (char*)PtrAdd(_buffer, sizeof(_buffer) - charSize));
                std::fill_n(_buffer, dimof(_buffer), 0);
            }
        };
    }

    /// <summary>Dynamic string formatting utility<summary>
    /// StringMeld provides a simple and handy method to build a string using operator<<.
    /// StringMeld never allocates. It just has a fixed size buffer. So this is a handy
    /// way to build a string in a way that is type-safe, convenient and avoids any allocations.
    ///
    /// <example>
    ///     <code>\code
    ///         window.SetTitle(StringMeld<128>() << "XLE sample [RenderCore: " << v.first << ", " << v.second << "]");
    ///     \endcode</code>
    /// </example>
    template<int Count, typename CharType=char> class StringMeld
    {
    public:
        mutable std::ostream _stream;
        
        StringMeld() : _stream(&_buffer), _buffer(sizeof(CharType))
        {
        }

        operator const CharType*() const
        {
            return (const CharType*)_buffer._buffer;
        }

        const CharType* get() const
        {
            return (const CharType*)_buffer._buffer;
        }

    protected:
        Internal::FixedMemoryBuffer<Count*sizeof(CharType)> _buffer;

        StringMeld(const StringMeld&);
        StringMeld& operator=(const StringMeld&);
    };

    namespace
    {
        template<typename Type, int Count, typename CharType> 
            const StringMeld<Count, CharType>& operator<<(const StringMeld<Count, CharType>& meld, const Type& type)
            {
                meld._stream << type;
                return meld;
            }
    }

    namespace Internal
    {
        class StreamBufInPlace : public std::streambuf
        {
        public:
            StreamBufInPlace(uint8* start, uint8* end, size_t charSize) 
            { 
                this->setp((char*)start, (char*)PtrAdd(end, -ptrdiff_t(charSize))); 
                XlSetMemory(start, 0, end-start);
            }
            ~StreamBufInPlace() {}

            StreamBufInPlace(StreamBufInPlace&& moveFrom) never_throws
            : std::streambuf(std::move(moveFrom))
            {}

            StreamBufInPlace& operator=(StreamBufInPlace&& moveFrom)
            {
                std::streambuf::operator=(moveFrom);
                return *this;
            }

            uint8* Begin() { return (uint8*)pbase(); }
        };

        /// <summary>Dynamic string formatting utility<summary>
        /// This version of StringMeld can work on an externally allocated buffer. It's useful
        /// when appending some formatted text onto an existing string (often it's an effective
        /// alternative to multiple string cats)
        ///
        /// <example>
        ///     <code>\code
        ///         char filename[MaxPath];
        ///         object.GetFilename(filename, dimof(filename));
        ///         StringMeldAppend(filename) << "-appendedtext";
        ///     \endcode</code>
        /// </example>
        template<typename CharType> class StringMeldInPlace
        {
        public:
            mutable std::ostream _stream;

            operator const CharType*() const
            {
                return (const CharType*)_buffer._buffer;
            }

            const CharType* get() const
            {
                return (const CharType*)_buffer._buffer;
            }

            StringMeldInPlace(CharType* bufferStart, CharType* bufferEnd)
            : _stream(&_buffer)
            , _buffer((uint8*)bufferStart, (uint8*)bufferEnd, sizeof(CharType))
            {
                if ((bufferEnd - bufferStart) > 0)
                    ((CharType*)_buffer.Begin())[0] = (CharType)'\0';
            }

            ~StringMeldInPlace() {}

            StringMeldInPlace(StringMeldInPlace&& moveFrom) never_throws
            : _stream(std::move(moveFrom._stream))
            , _buffer(std::move(moveFrom._buffer))
            {}

            StringMeldInPlace& operator=(StringMeldInPlace&& moveFrom)
            {
                _stream = std::move(moveFrom._stream);
                _buffer = std::move(moveFrom._buffer);
                return *this;
            }

        protected:
            StreamBufInPlace _buffer;

            StringMeldInPlace(const StringMeldInPlace&) = delete;
            StringMeldInPlace& operator=(const StringMeldInPlace&) = delete;
        };
    }

    namespace
    {
        template<typename Type, typename CharType> 
            const Internal::StringMeldInPlace<CharType>& 
                operator<<(const Internal::StringMeldInPlace<CharType>& meld, const Type& type)
            {
                meld._stream << type;
                return meld;
            }
    }
    
    template<typename CharType, int Count>
        Internal::StringMeldInPlace<CharType> StringMeldInPlace(CharType (&buffer)[Count])
    {
        return Internal::StringMeldInPlace<CharType>(buffer, &buffer[Count]);
    }

    template<typename CharType, int Count>
        Internal::StringMeldInPlace<CharType> StringMeldAppend(CharType (&buffer)[Count])
    {
        auto len = XlStringLen(buffer);
        return Internal::StringMeldInPlace<CharType>(&buffer[len], &buffer[Count-len]);
    }

    template<typename CharType>
        Internal::StringMeldInPlace<CharType> StringMeldInPlace(CharType *bufferStart, CharType* bufferEnd)
    {
        return Internal::StringMeldInPlace<CharType>(bufferStart, bufferEnd);
    }

    template<typename CharType>
        Internal::StringMeldInPlace<CharType> StringMeldAppend(CharType *bufferStart, CharType* bufferEnd)
    {
        auto len = XlStringLen(bufferStart);
        return Internal::StringMeldInPlace<CharType>(&bufferStart[len], bufferEnd);
    }

}

using namespace Utility;

