// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Detail/API.h"
#include "../Core/Prefix.h"
#include "../Core/Types.h"
#include <stdarg.h>

namespace Utility
{

    // preferred types := utf8, ucs2, ucs4
    typedef uint8  utf8_t, utf8;
    typedef uint16 ucs2_t, ucs2, utf16;
    typedef uint32 ucs4_t, ucs4, utf32;

    // nchar_t := os preferred path encoding type
    #define __L(x)      L ## x

    #if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS

        typedef ucs2 nchar;
        #define a2n(x) (const ucs2*)__L(x)
        #define n2w(x) (const wchar_t*)x

    #elif (PLATFORMOS_ACTIVE == PLATFORMOS_OSX) || (PLATFORMOS_ACTIVE == PLATFORMOS_ANDROID)

        typedef utf8 nchar;
        #define a2n(x) x
        #define n2w(x) x
        // #error 'not implemented'

    #else

        typedef ucs4 nchar;
        #error 'not implemented'

    #endif

    #define a2w(x) __L(x)

    typedef nchar nchar_t;


    #define UTF_REPLACEMENT_CHAR    (ucs4)0x0000FFFD
    #define UTF_MAX_BMP             (ucs4)0x0000FFFF
    #define UTF_MAX_UTF16           (ucs4)0x0010FFFF
    #define UTF_MAX_UTF32           (ucs4)0x7FFFFFFF
    #define UTF_MAX_LEGAL_UTF32     (ucs4)0x0010FFFF

    enum ucs_conv_error {
        UCE_OK = 0,
        UCE_SRC_EXHAUSTED = -1,
        UCE_DST_EXHAUSTED = -2,
        UCE_ILLEGAL = -2
    };

    // convert UTF-8 data to ucs4
    // return value := >= 0 if valid or 
    XL_UTILITY_API int utf8_2_ucs4(const utf8* src, size_t sl, ucs4* dst, size_t dl);
    XL_UTILITY_API int ucs4_2_utf8(const ucs4* src, size_t sl, utf8* dst, size_t dl);
    XL_UTILITY_API int utf8_2_ucs2(const utf8* src, size_t sl, ucs2* dst, size_t dl);
    XL_UTILITY_API int ucs2_2_utf8(const ucs2* src, size_t sl, utf8* dst, size_t dl);
    XL_UTILITY_API int ucs4_2_ucs2(const ucs4* src, size_t sl, ucs2* dst, size_t dl);
    XL_UTILITY_API int ucs2_2_ucs4(const ucs2* src, size_t sl, ucs4* dst, size_t dl);

    // aliases
    #define utf8_2_utf32    utf8_2_ucs4
    #define utf32_2_utf8    ucs4_2_utf8
    #define utf8_2_utf16    utf8_2_ucs2
    #define utf16_2_utf8    ucs2_2_utf8
    #define utf32_2_utf16   ucs4_2_ucs2
    #define utf16_2_utf32   ucs2_2_ucs4

    #if PLATFORMOS_ACTIVE == PLATFORMOS_WINDOWS
        #define nchar_2_utf8    ucs2_2_utf8
        #define nchar_2_ucs2(x) x
        #define nchar_2_ucs4    ucs2_2_utf8
    #elif (PLATFORMOS_ACTIVE == PLATFORMOS_OSX) || (PLATFORMOS_ACTIVE == PLATFORMOS_ANDROID)
        #define nchar_2_utf8(x) x
        #define nchar_2_ucs2    utf8_2_ucs2
        #define nchar_2_ucs4    utf8_2_ucs4
    #else
        #error 'not implemented!'
    #endif

    // refer ConvertUTF.h in libtransmission
    // remark_todo("need some safe conversion") 

    // single character conversion
    XL_UTILITY_API int ucs2_2_utf8(ucs2 ch, utf8* dst);
    XL_UTILITY_API int ucs4_2_utf8(ucs4 ch, utf8* dst);

    // find byte offset of n-th character
    XL_UTILITY_API int utf8_ord_offset(const utf8* s, int N);
    // find ordinal at the given byte offset
    XL_UTILITY_API int utf8_offset_ord(const utf8* s, int offset);


    // return next character, updating an index variable
    // i := byte offset
    XL_UTILITY_API ucs4 utf8_nextchar(const utf8* s, int* i);

    // count the number of characters in a UTF-8 string
    XL_UTILITY_API size_t utf8_strlen(const utf8* s);

    // move to next character
    XL_UTILITY_API void utf8_inc(const utf8* s, int* i);
    // move to previous character
    XL_UTILITY_API void utf8_dec(const utf8* s, int*i);

    // next utf-8 sequence length
    XL_UTILITY_API int utf8_seqlen(const utf8* s);

    XL_UTILITY_API int utf8_read_escape_sequence(char* src, uint32* dst);

    XL_UTILITY_API size_t utf8_escape_wchar(utf8* buf, size_t dim, uint32 ch);

    // ASCII <--> UTF8 escaping
    XL_UTILITY_API size_t utf8_unescape(utf8* buf, size_t dim, char *src);
    XL_UTILITY_API size_t utf8_escape(utf8* buf, size_t dim, char *src, int escape_quotes);

    // strchr with ordinal
    XL_UTILITY_API const utf8* utf8_strchr(const utf8* s, ucs4 ch, int& ord);

    // memchr with ordinal
    // n := buffer length
    XL_UTILITY_API const utf8* utf8_memchr(const utf8* s, ucs4 ch, size_t n, int& ord);

    XL_UTILITY_API int utf8_is_locale_utf8(char *locale);

    XL_UTILITY_API size_t utf8_vprintf(const char* fmt, va_list ap);
    XL_UTILITY_API size_t utf8_printf(const char* fmt, ...);

    inline const utf8* u(const char input[]) { return (const utf8*)input; }
}

using namespace Utility;

