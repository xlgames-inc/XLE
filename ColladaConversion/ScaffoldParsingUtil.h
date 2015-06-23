// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/Streams/XmlStreamFormatter.h"
#include "../Utility/Conversion.h"

namespace ColladaConversion
{
    bool Is(const XmlInputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[]);
    bool BeginsWith(const XmlInputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[]);
    bool EndsWith(const XmlInputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[]);

    template <typename Enum, unsigned Count>
        static Enum ParseEnum(const XmlInputStreamFormatter<utf8>::InteriorSection& section, const std::pair<Enum, const utf8*> (&table)[Count])
    {
        static_assert(Count > 0, "Enum names table must have at least one entry");
        for (unsigned c=0; c<Count; ++c)
            if (Is(section, table[c].second))
                return table[c].first;

        return table[0].first;  // first one is the default
    }

    template<typename CharType> bool IsWhitespace(CharType chr);
    template<typename CharType> const CharType* FastParseElement(uint32& dst, const CharType* start, const CharType* end);
    template<typename CharType> const CharType* FastParseElement(int64& dst, const CharType* start, const CharType* end);
    template<typename CharType> const CharType* FastParseElement(uint64& dst, const CharType* start, const CharType* end);
    template<typename CharType> const CharType* FastParseElement(float& dst, const CharType* start, const CharType* end);

    template<typename Type>
        auto ParseXMLList(Type dest[], unsigned destCount, XmlInputStreamFormatter<utf8>::InteriorSection section, unsigned* outEleCount = nullptr) 
            -> decltype(XmlInputStreamFormatter<utf8>::InteriorSection::_start)
    {
        assert(destCount > 0);

        // in xml, lists are deliminated by white space.
        unsigned elementCount = 0;
        auto* eleStart = section._start;
        while (elementCount < destCount) {
            while (eleStart < section._end && IsWhitespace(*eleStart)) ++eleStart;

            auto* eleEnd = FastParseElement(dest[elementCount], eleStart, section._end);
            if (eleStart == eleEnd) {
                if (outEleCount) *outEleCount = elementCount;
                return eleEnd;
            }
            ++elementCount;
            eleStart = eleEnd;
        }

        // skip forward over any trailing whitespace (which should bring us right to the end if the array ends in whitespace)
        while (eleStart < section._end && IsWhitespace(*eleStart)) ++eleStart;

        if (outEleCount) {
            // while there are remaining elements, we must count them...
            // we will return the correct number of elements, even if they don't
            // all fit in the destination array
            auto countingIterator = eleStart;
            Type temp;
            while (elementCount < destCount) {
                while (countingIterator < section._end && IsWhitespace(*countingIterator)) ++countingIterator;
                auto* eleEnd = FastParseElement(temp, countingIterator, section._end);
                if (countingIterator == eleEnd) break;
                ++elementCount;
                countingIterator = eleEnd;
            }
            *outEleCount = elementCount;
        }
        return eleStart;
    }

    template<typename Section>
        static std::string AsString(const Section& section)
    {
        using CharType = std::remove_const<std::remove_reference<decltype(*section._start)>::type>::type;
        return Conversion::Convert<std::string>(
            std::basic_string<CharType>(section._start, section._end));
    }

    bool Equivalent(
        const XmlInputStreamFormatter<utf8>::InteriorSection& lhs, 
        const XmlInputStreamFormatter<utf8>::InteriorSection& rhs);

    template<typename Type>
        static Type Parse(const XmlInputStreamFormatter<utf8>::InteriorSection& section, const Type& def)
    {
            // ImpliedType::Parse is actually a fairly expensing parsing operation...
            // maybe we could get a faster result by just calling the standard library
            // type functions.
        // auto d = ImpliedTyping::Parse<Type>(section._start, section._end);
        // if (!d.first) return def;
        // return d.second;

        Type result;
        auto temp = FastParseElement(result, section._start, section._end);
        if (temp > section._start) return result;
        return def;
    }
}

namespace std   // adding these to std is awkward, but it's the only way to make sure easylogging++ can see them
{
    std::ostream& operator<<(std::ostream& os, const StreamLocation& loc);
    std::ostream& operator<<(std::ostream& os, XmlInputStreamFormatter<utf8>::InteriorSection section);
}
