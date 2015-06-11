// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ParsingUtil.h"

namespace ColladaConversion
{
    bool Is(const XmlInputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[])
    {
        const auto* a = section._start;
        const auto* b = match;
        for (;;) {
            if (a == section._end)
                return !(*b);   // success if both strings have terminated at the same time
            if (*b != *a) return false;
            assert(*b); // potentially hit this assert if there are null characters in "section"... that isn't supported
            ++b; ++a;
        }
    }

    bool BeginsWith(const XmlInputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[])
    {
        auto matchLen = XlStringLen(match);
        if ((section._end - section._start) < ptrdiff_t(matchLen)) return false;
        return Is(XmlInputStreamFormatter<utf8>::InteriorSection(section._start, section._start + matchLen), match);
    }

    bool EndsWith(const XmlInputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[])
    {
        auto matchLen = XlStringLen(match);
        if ((section._end - section._start) < ptrdiff_t(matchLen)) return false;
        return Is(XmlInputStreamFormatter<utf8>::InteriorSection(section._end - matchLen, section._end), match);
    }

    bool Equivalent(
        const XmlInputStreamFormatter<utf8>::InteriorSection& lhs, 
        const XmlInputStreamFormatter<utf8>::InteriorSection& rhs)
    {
        if ((lhs._end - lhs._start) != (rhs._end - rhs._start)) return false;
        if (lhs._start == rhs._start) return true;

        const auto* a = lhs._start;
        const auto* b = rhs._start;
        for (;;) {
            if (a == lhs._end) return true;
            if (*b != *a) return false;
            ++b; ++a;
        }
    }


    template<typename CharType>
        bool IsWhitespace(CharType chr)
    {
        return chr == 0x20 || chr == 0x9 || chr == 0xD || chr == 0xA;
    }

    template<typename CharType>
        const CharType* FastParseElement(int64& dst, const CharType* start, const CharType* end)
    {
        bool positive = true;
        dst = 0;

        if (start >= end) return start;
        if (*start == '-') { positive = false; ++start; }
        else if (*start == '+') ++start;

        uint64 result = 0;
        for (;;) {
            if (start >= end) break;
            if (*start < '0' || *start > '9') break;

            result = (result * 10ull) + uint64((*start) - '0');
            ++start;
        }
        dst = positive ? result : -int64(result);
        return start;
    }

    template<typename CharType>
        const CharType* FastParseElement(uint64& dst, const CharType* start, const CharType* end)
    {
        uint64 result = 0;
        for (;;) {
            if (start >= end) break;
            if (*start < '0' || *start > '9') break;

            result = (result * 10ull) + uint64((*start) - '0');
            ++start;
        }
        dst = result;
        return start;
    }

    template<typename CharType>
        const CharType* FastParseElement(uint32& dst, const CharType* start, const CharType* end)
    {
        uint32 result = 0;
        for (;;) {
            if (start >= end) break;
            if (*start < '0' || *start > '9') break;

            result = (result * 10u) + uint32((*start) - '0');
            ++start;
        }
        dst = result;
        return start;
    }

    template<typename CharType>
        const CharType* FastParseElement(float& dst, const CharType* start, const CharType* end)
    {
        // this code found on stack exchange...
        //      (http://stackoverflow.com/questions/98586/where-can-i-find-the-worlds-fastest-atof-implementation)
        // But there are some problems!
        // Most importantly:
        //      Sub-normal numbers are not handled properly. Subnormal numbers happen when the exponent
        //      is the smallest is can be. In this case, values in the mantissa are evenly spaced around
        //      zero. 
        //
        // It does other things right. But I don't think it's reliable enough to use. It's a pity because
        // the standard library functions require null terminated strings, and seems that it may be possible
        // to get a big performance improvement loss of few features.

            // to avoid making a copy, we're going do a hack and 
            // We're assuming that "end" is writable memory. This will be the case when parsing
            // values from XML. But in other cases, it may not be reliable.
            // Also, consider that there might be threading implications in some cases!
        CharType replaced = *end;
        *const_cast<CharType*>(end) = '\0';
        char* newEnd = nullptr;
        dst = std::strtof((const char*)start, &newEnd);
        *const_cast<CharType*>(end) = replaced;

        return (const CharType*)newEnd;
    }

    template bool IsWhitespace(utf8 chr);
    template const utf8* FastParseElement(int64& dst, const utf8* start, const utf8* end);
    template const utf8* FastParseElement(uint64& dst, const utf8* start, const utf8* end);
    template const utf8* FastParseElement(uint32& dst, const utf8* start, const utf8* end);
    template const utf8* FastParseElement(float& dst, const utf8* start, const utf8* end);
}

