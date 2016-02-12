// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Detail/API.h"
#include "../Core/Types.h"
#include "UTFUtils.h"
#include "PtrUtils.h"	// for AsPointer
#include <string>
#include <assert.h>

#if REDIRECT_CLIB_WITH_PREPROCESSOR
    #include <stdlib.h>     // (this must be pulled in, so we can replace _itoa_s, etc)
#endif

namespace Utility
{
    // naming convention
    //  size : byte unit
    //  length : array size based on each type without terminating 0
    //  count, dim : length + 1 (for terminating 0)

        ////////////   S T R I N G   T O O L S   ////////////
            // (note that counts are array length in number of chars or unichars)
    inline		   size_t   XlStringSize        (const char* str) { return std::strlen(str); }
    XL_UTILITY_API size_t   XlStringSizeSafe    (const char* str, const char* end);
    inline		   size_t   XlStringLen         (const char* str) { return std::strlen(str); }
    XL_UTILITY_API size_t   XlStringSize        (const utf8* str);
    XL_UTILITY_API size_t   XlStringLen         (const utf8* str);

    XL_UTILITY_API void     XlCopyString        (char* dst, size_t size, const char* src);
    XL_UTILITY_API void     XlCopyNString       (char* dst, size_t count, const char* src, size_t length);
    XL_UTILITY_API void     XlCopySafeUtf       (utf8* dst, size_t size, const utf8* src);
    XL_UTILITY_API void     XlCopySafeUtfN      (utf8* dst, size_t size, const utf8* src, const uint32 numSeq);

    XL_UTILITY_API void     XlCatString         (char* dst, size_t size, const char* src);
    XL_UTILITY_API void     XlCatNString        (char* dst, size_t size, const char* src, size_t length);
    XL_UTILITY_API void     XlCatString         (char* dst, size_t size, char src);
    XL_UTILITY_API void     XlCatSafeUtf        (utf8* dst, size_t size, const utf8* src);

    XL_UTILITY_API void     XlCombineString     (char dst[], size_t size, const char zero[], const char one[]);

    XL_UTILITY_API uint32   XlHashString        (const char* x);

    XL_UTILITY_API void     XlCopyString        (wchar_t* dst, size_t size, const wchar_t* src);
    XL_UTILITY_API void     XlCopyNString       (wchar_t* dst, size_t count, const wchar_t*src, size_t length);

    template <typename CharType>
        const CharType* XlStringEnd(const CharType nullTermStr[])
            { return &nullTermStr[XlStringSize(nullTermStr)]; }

    template <typename CharType>
        CharType* XlStringEnd(CharType nullTermStr[])
            { return &nullTermStr[XlStringSize(nullTermStr)]; }

        ////////////   S T R I N G   S E C T I O N   ////////////

    /// <summary>Pointers to the start and end of a string</summary>
    /// This object points into the interior of another object, identifying
    /// the start and end of a string.
    ///
    /// This is a 3rd common string representation:
    ///     * c-style char pointer
    ///     * stl-style std::string
    ///     * begin/end string "section"
    ///
    /// This useful for separating a part of a large string, or while serializing
    /// from an text file (when we want to identify an interior string without
    /// requiring an extra allocation).
    template<typename CharType=char>
        class StringSection
    {
    public:
        const CharType* _start;
        const CharType* _end;

        size_t Length() const                           { return size_t(_end - _start); }
        bool Empty() const                              { return _end <= _start; }
        std::basic_string<CharType> AsString() const    { return std::basic_string<CharType>(_start, _end); }

        const CharType* begin() const   { return _start; }
        const CharType* end() const     { return _end; }
		size_t size() const				{ return Length(); }

        const CharType& operator[](size_t index) const { assert(index < Length()); return _start[index]; }

        StringSection(const CharType* start, const CharType* end) : _start(start), _end(end) {}
        StringSection() : _start(nullptr), _end(nullptr) {}
        StringSection(const CharType* nullTerm) : _start(nullTerm), _end(XlStringEnd(_start)) {}
        
		template<typename CT, typename A>
			StringSection(const std::basic_string<CharType, CT, A>& str) : _start(AsPointer(str.cbegin())), _end(AsPointer(str.cend())) {}
    };

    template<typename CharType>
        inline StringSection<CharType> MakeStringSection(const CharType* start, const CharType* end)
        {
            return StringSection<CharType>(start, end);
        }

    template<typename CharType>
        inline StringSection<CharType> MakeStringSection(const CharType* nullTerm)
        {
            return StringSection<CharType>(nullTerm);
        }

    template<typename CharType, typename CT, typename A>
        inline StringSection<CharType> MakeStringSection(const std::basic_string<CharType, CT, A>& str)
        {
            return StringSection<CharType>(AsPointer(str.cbegin()), AsPointer(str.cend()));
        }

    template<typename CharType, typename CT, typename A>
        inline StringSection<CharType> MakeStringSection(
            const typename std::basic_string<CharType, CT, A>::const_iterator& begin,
            const typename std::basic_string<CharType, CT, A>::const_iterator& end)
        {
            return StringSection<CharType>(AsPointer(begin), AsPointer(end));
        }

    template<typename CharType, typename CT, typename A>
        inline StringSection<CharType> MakeStringSection(
            const typename std::basic_string<CharType, CT, A>::iterator& begin,
            const typename std::basic_string<CharType, CT, A>::iterator& end)
        {
            return StringSection<CharType>(AsPointer(begin), AsPointer(end));
        }

        ////////////   S T R I N G   C O M P A R I S O N S   ////////////
    XL_UTILITY_API int      XlComparePrefix     (const char* x, const char* y, size_t size);
    XL_UTILITY_API int      XlComparePrefixI    (const char* x, const char* y, size_t size);
    XL_UTILITY_API int      XlCompareString     (const char* x, const char* y);
    XL_UTILITY_API int      XlCompareStringI    (const char* x, const char* y);

        ////////////   S T R I N G   S E A R C H I N G   ////////////
    XL_UTILITY_API const char*  XlFindChar          (const char* s, const char ch);
    XL_UTILITY_API char*        XlFindChar          (char* s, const char ch);
    XL_UTILITY_API const char*  XlFindAnyChar       (const char s[], const char ch[]);
    XL_UTILITY_API char*        XlFindAnyChar       (char s[], const char delims[]);
    XL_UTILITY_API const char*  XlFindNot           (const char s[], const char ch[]);
    XL_UTILITY_API char*        XlFindNot           (char s[], const char delims[]);
    XL_UTILITY_API const char*  XlFindCharReverse   (const char* s, char ch);
    XL_UTILITY_API const char*  XlFindString        (const char* s, const char* x);
    XL_UTILITY_API char*        XlFindString        (char* s, const char* x);
    XL_UTILITY_API const char*  XlFindStringI       (const char* s, const char* x);
    XL_UTILITY_API const char*  XlFindStringSafe    (const char* s, const char* x, size_t size);
    XL_UTILITY_API const char*  XlReplaceString     (char* dst, size_t size, const char* src, const char* strOld, const char* strNew);

        ////////////   T O K E N I Z E   ////////////
    XL_UTILITY_API size_t   XlTokenizeString    (char* buf, size_t count, const char* delimiters, char** tokens, size_t numMaxToken);
    XL_UTILITY_API char*    XlStrTok            (char* token, const char* delimit, char** context);
    XL_UTILITY_API bool     XlMatchWildcard     (const char* str, const char* pat, bool nocase = true);

        ////////////   C O N V E R S I O N S   ////////////
    XL_UTILITY_API int      XlExtractInt        (const char* buf, int* arr, int length);
    XL_UTILITY_API bool     XlSafeAtoi          (const char* str, int* n);
    XL_UTILITY_API bool     XlSafeAtoi64        (const char* str, int64* n);
    XL_UTILITY_API bool     XlSafeAtoui64       (const char* str, uint64* n);

    XL_UTILITY_API void     XlCompactString     (char* dst, size_t size, const char* src);
    XL_UTILITY_API const char*  XlLowerCase     (char* str);
    XL_UTILITY_API const char*  XlUpperCase     (char* str);

        ////////////   U T F 8   O V E R R I D E S   ////////////
    XL_UTILITY_API void     XlCopyString        (utf8* dst, size_t size, const utf8* src);
    XL_UTILITY_API void     XlCopyNString       (utf8* dst, size_t count, const utf8*src, size_t length);
	XL_UTILITY_API void     XlCatString			(utf8* dst, size_t size, const utf8* src);
    XL_UTILITY_API int      XlComparePrefix     (const utf8* x, const utf8* y, size_t size);
    XL_UTILITY_API int      XlComparePrefixI    (const utf8* x, const utf8* y, size_t size);
    XL_UTILITY_API int      XlCompareString     (const utf8* x, const utf8* y);
    XL_UTILITY_API int      XlCompareStringI    (const utf8* x, const utf8* y);

		////////////   U T F 1 6   O V E R R I D E S   ////////////
	// XL_UTILITY_API void     XlCopyString(utf16* dst, size_t size, const utf16* src);
	// XL_UTILITY_API void     XlCopyNString(utf16* dst, size_t count, const utf16*src, size_t length);
	// XL_UTILITY_API size_t   XlStringSize(const utf16* str);

        ////////////   U C S 2   O V E R R I D E S   ////////////
    XL_UTILITY_API void     XlCopyString        (ucs2* dst, size_t size, const ucs2* src);
    XL_UTILITY_API void     XlCopyNString       (ucs2* dst, size_t count, const ucs2*src, size_t length);
    XL_UTILITY_API void     XlCatString         (ucs2* dst, size_t size, const ucs2* src);
    XL_UTILITY_API void     XlCatString         (ucs2* dst, size_t size, ucs2 src);
    XL_UTILITY_API void     XlCatNString        (ucs2* dst, size_t size, const ucs2* src, size_t length);
    XL_UTILITY_API size_t   XlStringSize        (const ucs2* str);
    XL_UTILITY_API size_t   XlStringLen         (const ucs2* str);

    XL_UTILITY_API int      XlComparePrefix     (const ucs2* x, const ucs2* y, size_t len);
    XL_UTILITY_API int      XlComparePrefixI    (const ucs2* x, const ucs2* y, size_t len);
    XL_UTILITY_API size_t   XlCompareString     (const ucs2* x, const ucs2* y);
    XL_UTILITY_API size_t   XlCompareStringI    (const ucs2* x, const ucs2* y);

    XL_UTILITY_API size_t   XlTokenizeString    (ucs2* buf, size_t count, const ucs2* delimiters, ucs2** tokens, size_t numMaxToken);

        ////////////   U C S 4   O V E R R I D E S   ////////////
    XL_UTILITY_API size_t   XlStringSize        (const ucs4* str);
    XL_UTILITY_API size_t   XlStringSizeSafe    (const ucs4* str, const ucs4* end);
    XL_UTILITY_API size_t   XlStringLen         (const ucs4* str);
    XL_UTILITY_API void     XlCopyString        (ucs4* dst, size_t count, const ucs4* src);
    XL_UTILITY_API void     XlCopyNString       (ucs4* dst, size_t count, const ucs4*src, size_t length);
    XL_UTILITY_API void     XlCatString         (ucs4* dst, size_t count, const ucs4* src);
    XL_UTILITY_API void     XlCatString         (ucs4* dst, size_t count, ucs4 src);
    XL_UTILITY_API uint32   XlHashString        (const ucs4* x);

    XL_UTILITY_API int      XlComparePrefix     (const ucs4* x, const ucs4* y, size_t len);
    XL_UTILITY_API int      XlComparePrefixI    (const ucs4* x, const ucs4* y, size_t len);
    XL_UTILITY_API int      XlCompareString     (const ucs4* x, const ucs4* y);
    XL_UTILITY_API int      XlCompareStringI    (const ucs4* x, const ucs4* y);

    XL_UTILITY_API const ucs4* XlFindString      (const ucs4* s, const ucs4* x);
    XL_UTILITY_API const ucs4* XlFindStringI     (const ucs4* s, const ucs4* x);
    XL_UTILITY_API const ucs4* XlFindStringSafe  (const ucs4* s, const ucs4* x, size_t len);

    XL_UTILITY_API void     XlCompactString     (ucs4* dst, size_t size, const ucs4* src);        // add - johnryou (compare two string ignoring whitespace characters)
    XL_UTILITY_API const ucs4* XlLowerCase   (ucs4* str);
    XL_UTILITY_API const ucs4* XlUpperCase   (ucs4* str);

        ////////////   C H A R A C T E R   C O N V E R S I O N S   ////////////
    XL_UTILITY_API ucs4     XlGetChar(const char* str, size_t* size);
    XL_UTILITY_API void     XlGetChar(char* output, size_t count, const ucs4* uniStr, size_t* size);
    

#if 0   //  see also UTFUtils.h
            // utf8 <-> ucs4
    XL_UTILITY_API size_t   XlMultiToWide(ucs4* dst, size_t count, const utf8* src);
    XL_UTILITY_API size_t   XlWideToMulti(utf8* dst, size_t size, const ucs4* src);

            // utf8 <-> utf16
    XL_UTILITY_API size_t   XlUtf8To16(ucs2* utf16, size_t count, const utf8* utf8);
    XL_UTILITY_API size_t   XlUtf16To8(utf8* utf8, size_t count, const ucs2* utf16);

            // utf8 <-> acp(active code page)
    XL_UTILITY_API size_t   XlMultiToAcp(char* dst, size_t count, const char* src);
    XL_UTILITY_API size_t   XlAcpToMulti(char* dst, size_t count, const char* src);

            // ucs4 -> acp
    XL_UTILITY_API size_t   XlWideToAcp(char* dst, size_t count, const unsigned* src);
    XL_UTILITY_API size_t   XlMultiToWide2(wchar_t* dst, size_t count, const char* src);
    XL_UTILITY_API size_t   XlWide2ToAcp(char* dst, size_t count, const wchar_t* src);
    XL_UTILITY_API size_t   XlAcpToWide2(wchar_t* dst, size_t count, const char* src);

    XL_UTILITY_API size_t   XlMultiToCp(unsigned int codePage, char* dst, size_t count, const char* src);
    XL_UTILITY_API size_t   XlCpToMulti(unsigned int codePage, char* dst, size_t count, const char* src);
#endif

            // url encoded -> non encoded
    XL_UTILITY_API size_t   XlDecodeUrl(char* dst, size_t count, const char* encoded);

        ////////////   L O C A L E   ////////////

    //intialize
    namespace Locale
    {
        enum Enum
        {
            KO = 0,
            ZH_CN,
            EN_US,
            JA,
            ZH_TW,
            RU,
            DE,
            FR,
            Max
        };
    }

    XL_UTILITY_API void             XlInitStringUtil(const char locale[]);
    XL_UTILITY_API Locale::Enum     XlGetLocale();
    XL_UTILITY_API const char*      XlGetLocaleString(Locale::Enum locale);
    XL_UTILITY_API int              XlGetLocaleCodePage(Locale::Enum locale);

        ////////////   U T F   H E L P E R S   ////////////
    #define UTF8_BUFSIZE(count)         ((count) * 4)       // this covers current valid unicode range.
    #define UTF16_BUFSIZE(count)        ((count) * 4)       // in bytes
    #define WCHAR_BUFSIZE(count)        (UTF16_BUFSIZE(count) / sizeof(wchar_t))

    XL_UTILITY_API bool     XlIsValidAscii(const char* str);
    XL_UTILITY_API bool     XlIsValidUtf8(const utf8* str, size_t count = -1);
    XL_UTILITY_API bool     XlHasUtf8Bom(const utf8* str);
    XL_UTILITY_API size_t   XlGetOffset(const char* str, size_t index);              // return byte offset for index's character.
    inline int              XlCountUtfSequence(uint8 c);

        ////////////   C H A R A C T E R   C L A S S I F I C A T I O N   A N D   C O N V E R S I O N   ////////////
    XL_UTILITY_API bool     XlIsAlnum(char c);
    XL_UTILITY_API bool     XlIsEngAlpha(char c);
    XL_UTILITY_API bool     XlIsAlNumSpace(char c);

    XL_UTILITY_API bool     XlIsDigit(char c);
    XL_UTILITY_API bool     XlIsDigit(utf8 c);
    XL_UTILITY_API bool     XlIsDigit(ucs4 c);

    XL_UTILITY_API bool     XlIsHex(char c);
    XL_UTILITY_API bool     XlIsLower(char c);
    XL_UTILITY_API bool     XlIsPrint(char c);
    XL_UTILITY_API bool     XlIsSpace(char c);
    XL_UTILITY_API bool     XlIsUpper(char c);

    XL_UTILITY_API int      XlFromHex(char c);
    XL_UTILITY_API char     XlToHex(int n);

    XL_UTILITY_API char     XlToLower(char c);
    XL_UTILITY_API char     XlToUpper(char c);
    XL_UTILITY_API wchar_t  XlToLower(wchar_t c);
    XL_UTILITY_API wchar_t  XlToUpper(wchar_t c);

    inline utf8     XlToLower(utf8 c) { return (utf8)XlToLower((char)c); }
    inline utf8     XlToUpper(utf8 c) { return (utf8)XlToLower((char)c); }

    XL_UTILITY_API ucs2     XlToLower(ucs2 c);
    XL_UTILITY_API ucs2     XlToUpper(ucs2 c);

    XL_UTILITY_API bool     XlIsAlpha(ucs4 c);
    XL_UTILITY_API bool     XlIsEngAlpha(ucs4 c);
    XL_UTILITY_API bool     XlIsSpace(ucs4 c);
    XL_UTILITY_API bool     XlIsUpper(ucs4 c);
    XL_UTILITY_API bool     XlIsLower(ucs4 c);

    XL_UTILITY_API ucs4     XlToLower(ucs4 c);
    XL_UTILITY_API ucs4     XlToUpper(ucs4 c);

    XL_UTILITY_API bool     XlIsUpper(ucs4 c);
    XL_UTILITY_API bool     XlIsLower(ucs4 c);

    XL_UTILITY_API char*    XlTrimRight(char* str);
    XL_UTILITY_API char*    XlRemoveAllSpace(char* str);

    // <string <==> numeric> conversion
    XL_UTILITY_API bool     XlAtoBool(const char* str, const char** end_ptr = 0);
    XL_UTILITY_API int32    XlAtoI32 (const char* str, const char** end_ptr = 0, int radix = 10);
    XL_UTILITY_API int64    XlAtoI64 (const char* str, const char** end_ptr = 0, int radix = 10);
    XL_UTILITY_API uint32   XlAtoUI32(const char* str, const char** end_ptr = 0, int radix = 10);
    XL_UTILITY_API uint64   XlAtoUI64(const char* str, const char** end_ptr = 0, int radix = 10);
    XL_UTILITY_API f32      XlAtoF32 (const char* str, const char** end_ptr = 0);
    XL_UTILITY_API f64      XlAtoF64 (const char* str, const char** end_ptr = 0);

    XL_UTILITY_API char*    XlI32toA(int32 value, char* buffer, size_t dim, int radix);
    XL_UTILITY_API char*    XlI64toA(int64 value, char* buffer, size_t dim, int radix);
    XL_UTILITY_API char*    XlUI32toA(uint32 value, char* buffer, size_t dim, int radix);
    XL_UTILITY_API char*    XlUI64toA(uint64 value, char* buffer, size_t dim, int radix);

	    // ms secure version compatible
    XL_UTILITY_API int      XlI32toA_s(int32 value, char* buffer, size_t dim, int radix);
    XL_UTILITY_API int      XlI64toA_s(int64 value, char* buffer, size_t dim, int radix);
    XL_UTILITY_API int      XlUI32toA_s(uint32 value, char* buffer, size_t dim, int radix);
    XL_UTILITY_API int      XlUI64toA_s(uint64 value, char* buffer, size_t dim, int radix);

	    // non-secure version
    XL_UTILITY_API char*    XlI32toA_ns(int32 value, char* buffer, int radix);
    XL_UTILITY_API char*    XlI64toA_ns(int64 value, char* buffer, int radix);
    XL_UTILITY_API char*    XlUI32toA_ns(uint32 value, char* buffer, int radix);
    XL_UTILITY_API char*    XlUI64toA_ns(uint64 value, char* buffer, int radix);

    XL_UTILITY_API bool     XlToHexStr(const char* x, size_t xlen, char* y, size_t ylen);
    XL_UTILITY_API bool     XlHexStrToBin(const char* x, char* y);

        ////////////   H E L P E R S   ////////////
    template <int Count, typename CharType>
        void XlCopyString(CharType (&destination)[Count], const CharType source[])
        {
            XlCopyString(destination, Count, source);
        }

    template <int Count, typename CharType>
        void XlCopyNString(CharType (&destination)[Count], const CharType source[], size_t length)
        {
            XlCopyNString(destination, Count, source, length);
        }

    template <int Count, typename CharType>
        void XlCopyString(CharType (&destination)[Count], const StringSection<CharType>& source)
        {
            XlCopyNString(destination, Count, source._start, source.Length());
        }

	template <typename CharType>
		void XlCopyString(CharType destination[], size_t destinationCount, const StringSection<CharType>& source)
		{
			XlCopyNString(destination, destinationCount, source._start, source.Length());
		}

    template <int Count, typename CharType>
        void XlCopyString(CharType (&destination)[Count], const std::basic_string<CharType>& source)
        {
            XlCopyNString(destination, Count, AsPointer(source.cbegin()), source.size());
        }

    template <typename CharType>
        void XlCatString(CharType destination[], size_t size, const StringSection<CharType>& source)
        {
            XlCatNString(destination, size, source.begin(), source.Length());
        }

    template <int Count, typename CharType>
        void XlCatString(CharType (&destination)[Count], const CharType source[])
        {
            XlCatString(destination, Count, source);
        }

    template <int Count, typename CharType>
        void XlCatString(CharType (&destination)[Count], const StringSection<CharType>& source)
        {
            XlCatNString(destination, Count, source.begin(), source.Length());
        }

    template <int Count, typename CharType>
        void XlCatString(CharType (&destination)[Count], const std::basic_string<CharType>& source)
        {
            XlCatNString(destination, Count, AsPointer(source.cbegin()), source.size());
        }

    template<typename T>
        bool XlEqString(const T* a, const T* b)
        {
            return XlCompareString(a, b) == 0;
        }

    template<typename T>
        bool XlEqStringI(const T* a, const T* b)
        {
            return XlCompareStringI(a, b) == 0;
        }

    template<typename T>
        bool XlEqStringI(const std::basic_string<T>& a, const std::basic_string<T>& b)
        {
            auto sz = a.size();
            if (b.size() != sz) return false;
            for (size_t i = 0; i < sz; ++i)
                if (XlToLower(a[i]) != XlToLower(b[i]))
                    return false;
            return true;
        }

    template<typename T>
        bool XlEqStringI(const std::basic_string<T>& a, const T* b)
        {
            auto sz = a.size();
            size_t i = 0;
            for (; i < sz; ++i)
                if (!b[i] || XlToLower(a[i]) != XlToLower(b[i]))
                    return false;
            return b[i] == 0;
        }

    template<typename T>
        bool XlEqString(const std::basic_string<T>& a, const T* b)
        {
            auto sz = a.size();
            size_t i = 0;
            for (; i < sz; ++i)
                if (!b[i] || a[i] != b[i])
                    return false;
            return b[i] == 0;
        }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename T>
        bool XlEqStringI(const StringSection<T>& a, const StringSection<T>& b)
        {
            auto sz = a.Length();
            if (b.Length() != sz) return false;
            for (size_t i = 0; i < sz; ++i)
                if (XlToLower(a._start[i]) != XlToLower(b._start[i]))
                    return false;
            return true;
        }

    template<typename T>
        bool XlEqString(const StringSection<T>& a, const StringSection<T>& b)
        {
            auto sz = a.Length();
            if (b.Length() != sz) return false;
            for (size_t i = 0; i < sz; ++i)
                if (a._start[i] != b._start[i])
                    return false;
            return true;
        }

    template<typename T>
        bool XlEqStringI(const StringSection<T>& a, const T* b)
        {
            auto sz = a.Length();
            size_t i = 0;
            for (; i < sz; ++i)
                if (!b[i] || XlToLower(a._start[i]) != XlToLower(b[i]))
                    return false;
            return b[i] == 0;
        }

    template<typename T>
        bool XlEqString(const StringSection<T>& a, const T* b)
        {
            auto sz = a.Length();
            size_t i = 0;
            for (; i < sz; ++i)
                if (!b[i] || a._start[i] != b[i])
                    return false;
            return b[i] == 0;
        }

    template<typename T>
        bool XlEqStringI(const StringSection<T>& a, const std::basic_string<T>& b)
        {
            return XlEqStringI(a, MakeStringSection(b));
        }

    template<typename T>
        bool XlEqString(const StringSection<T>& a, const std::basic_string<T>& b)
        {
            return XlEqString(a, MakeStringSection(b));
        }

    template<typename T>
        int XlCompareString(const StringSection<T>& a, const T* b)
        {
            auto alen = a.Length();
            auto blen = XlStringLen(b);
            auto cmp = XlComparePrefix(a.begin(), b, std::min(alen, blen));
            if (cmp != 0) return cmp;

                // Initial prefix is identical.
                // If the length of both strings are identical, then they
                // are identical. Otherwise we should emulate the result we 
                // would get from expression "int(*x) - int(*y)" on two 
                // null terminated strings
            if (alen == blen) return 0;
            if (alen < blen) return -int(b[alen]);
            return a[blen];
        }

    template<typename T>
        int XlCompareString(const T* a, const StringSection<T>& b)
        {
            auto alen = XlStringLen(a);
            auto blen = b.Length();
            auto cmp = XlComparePrefix(a, b.begin(), std::min(alen, blen));
            if (cmp != 0) return cmp;

                // Initial prefix is identical.
                // If the length of both strings are identical, then they
                // are identical. Otherwise we should emulate the result we 
                // would get from expression "int(*x) - int(*y)" on two 
                // null terminated strings
            if (alen == blen) return 0;
            if (alen < blen) return -int(b[alen]);
            return a[blen];
        }

    template<typename T>
        int XlCompareString(const StringSection<T>& a, const StringSection<T>& b)
        {
            auto alen = a.Length(), blen = b.Length();
            auto cmp = XlComparePrefix(a.begin(), b.begin(), std::min(alen, blen));
            if (cmp != 0) return cmp;

                // Initial prefix is identical.
                // If the length of both strings are identical, then they
                // are identical. Otherwise we should emulate the result we 
                // would get from expression "int(*x) - int(*y)" on two 
                // null terminated strings
            if (alen == blen) return 0;
            if (alen < blen) return -int(b[alen]);
            return a[blen];
        }

    template<typename T>
        int XlCompareStringI(const StringSection<T>& a, const T* b)
        {
            auto alen = a.Length();
            auto blen = XlStringLen(b);
            auto cmp = XlComparePrefixI(a.begin(), b, std::min(alen, blen));
            if (cmp != 0) return cmp;

                // Initial prefix is identical.
                // If the length of both strings are identical, then they
                // are identical. Otherwise we should emulate the result we 
                // would get from expression "int(*x) - int(*y)" on two 
                // null terminated strings
            if (alen == blen) return 0;
            if (alen < blen) return -int(XlToLower(b[alen]));
            return XlToLower(a[blen]);
        }

    template<typename T>
        int XlCompareStringI(const T* a, const StringSection<T>& b)
        {
            auto alen = XlStringLen(a);
            auto blen = b.Length();
            auto cmp = XlComparePrefixI(a, b.begin(), std::min(alen, blen));
            if (cmp != 0) return cmp;

                // Initial prefix is identical.
                // If the length of both strings are identical, then they
                // are identical. Otherwise we should emulate the result we 
                // would get from expression "int(*x) - int(*y)" on two 
                // null terminated strings
            if (alen == blen) return 0;
            if (alen < blen) return -int(XlToLower(b[alen]));
            return XlToLower(a[blen]);
        }

    template<typename T>
        int XlCompareStringI(const StringSection<T>& a, const StringSection<T>& b)
        {
            auto alen = a.Length(), blen = b.Length();
            auto cmp = XlComparePrefixI(a.begin(), b.begin(), std::min(alen, blen));
            if (cmp != 0) return cmp;

                // Initial prefix is identical.
                // If the length of both strings are identical, then they
                // are identical. Otherwise we should emulate the result we 
                // would get from expression "int(*x) - int(*y)" on two 
                // null terminated strings
            if (alen == blen) return 0;
            if (alen < blen) return -int(XlToLower(b[alen]));
            return XlToLower(a[blen]);
        }

    template<typename T>
        int XlCompareString(const std::basic_string<T>& a, const std::basic_string<T>& b)
        {
            return XlCompareString(MakeStringSection(a), MakeStringSection(b));
        }

    template<typename T>
        int XlCompareStringI(const std::basic_string<T>& a, const std::basic_string<T>& b)
        {
            return XlCompareStringI(MakeStringSection(a), MakeStringSection(b));
        }

    template<typename T>
        bool XlBeginsWith(const StringSection<T>& a, const StringSection<T>& b)
        {
            return 
                a.Length() >= b.Length()
                && XlEqString(StringSection<T>(a.begin(), a.begin() + b.Length()), b);
        }

    template<typename T>
        bool XlBeginsWithI(const StringSection<T>& a, const StringSection<T>& b)
        {
            return 
                a.Length() >= b.Length()
                && XlEqStringI(StringSection<T>(a.begin(), a.begin() + b.Length()), b);
        }

///////////////////////////////////////////////////////////////////////////////////////////////////

    #if REDIRECT_CLIB_WITH_PREPROCESSOR

        #undef itoa
        #undef ltoa
        #undef ultoa
        #undef _ui64toa
        #undef _i64toa
        #undef _itoa_s
        #undef _i64toa_s
        #undef _ui64toa_s
        #undef _atoi64

        #define  itoa(v, b, r)    XlI32toA_ns((int32)v, b, r)
        #define  ltoa(v, b, r)    XlI32toA_ns((int32)v, b, r)
        #define ultoa(v, b, r)    XlUI32toA_ns((uint32)v, b, r)
        #define _ui64toa(v, b, r) XlUI64toA_ns(v, b, r)
        #define _i64toa(v, b, r)  XlI64toA_ns(v, b, r)
        #define _itoa_s           XlI32toA_s
        #define _i64toa_s         XlI64toA_s
        #define _ui64toa_s        XlUI64toA_s
        #define _atoi64			  XlAtoI64
        #define _strtoui64		  XlAtoUI64	

        #undef _strlwr
        #undef _strupr

        #define _strlwr XlLowerCase
        #define _strupr XlUpperCase

    #endif

    inline int XlCountUtfSequence(uint8 c)
    {
        if (c < 0x80) {
            return 1;
        } else if (c < 0xc2) {
            // second, third, fourth byte or overlong encoding
            return 0;
        } else if (c < 0xe0) {
            return 2;
        } else if (c < 0xf0) {
            return 3;
        } else if (c < 0xf5) {
            return 4;
        } else {
            // illegal
            return 0;
        }
    }

}

using namespace Utility;
