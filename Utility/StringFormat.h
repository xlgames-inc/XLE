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
            FixedMemoryBuffer() 
            {
                this->setp((char*)_buffer, (char*)PtrAdd(_buffer, sizeof(_buffer) - 1));
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
        
        StringMeld() : _stream(&_buffer)
        {
            ((CharType*)_buffer._buffer)[0] = (CharType)'\0';
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

}

using namespace Utility;

