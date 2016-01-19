// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ConfigFileContainer.h"
#include "AssetServices.h"
#include <regex>

namespace Assets
{
    template<typename DestType = unsigned, typename CharType = char>
        DestType StringToUnsigned(const StringSection<CharType> source)
    {
        auto* start = source.begin();
        auto* end = source.end();
        if (start >= end) return 0;

        auto result = DestType(0);
        for (;;) {
            if (start >= end) break;
            if (*start < '0' || *start > '9') break;
            result = (result * 10) + DestType((*start) - '0');
            ++start;
        }
        return result;
    }

    static bool IsWhitespace(char chr) { return chr == ' ' || chr == '\t'; }

    template<typename CharType>
        std::vector<TextChunk<CharType>> ReadCompoundTextDocument(StringSection<CharType> doc)
    {
            // compound text documents should begin with:
            //      "BOM?//\s*CompoundDocument:(\d)+"
        std::vector<TextChunk<CharType>> result;

        auto* i = doc.begin();

            // Check if we start with the UTF8 byte order mark... Skip over it
            // if so.
        if ((size_t(doc.end()) - size_t(i)) >= 3) {
            auto t = (unsigned char*)i;
            if (t[0] == 0xefu && t[1] == 0xbbu && t[2] == 0xbfu)
                i = (decltype(i))&t[3];
        }

        if (i == doc.end() || *i != '/') return std::move(result);
        ++i;
        if (i == doc.end() || *i != '/') return std::move(result);
        ++i;
        while (i != doc.end() && IsWhitespace(*i)) ++i;     // (whitespace but not newline)

        char markerAscii[] = "CompoundDocument:";
        auto* m = markerAscii;
        while (*m) {
            if (i == doc.end() || *i != *m) return std::move(result);
            ++i; ++m;
        }

        auto versionStart = i;
        while (i != doc.end() && XlIsDigit(*i)) ++i;
        auto version = StringToUnsigned(MakeStringSection(versionStart, i));
        if (version != 1)
            Throw(::Exceptions::BasicLabel("Compound text document version (%i) not understood", version));

        // If we get here, then the result is a compound document that we can read
        // scan through to find the chunks.
        static std::regex chunkHeader(R"--(<<Chunk:(\w+):(\w+)>>(\S+)\()--");
        std::regex_iterator<const CharType*> ri(i, doc.end(), chunkHeader);
        while (ri != std::regex_iterator<const CharType*>()) {
            const auto& match = *ri;
            if (!match.empty() && match.size() >= 4) {
                const auto& t = match[1];
                const auto& n = match[2];
                const auto& d = match[3];
                auto* ci = match[0].second;

                // We must scan forward to find a ')' followed by a duplicate of the 
                // deliminator pattern
                auto* contentStart = ci;
                while (ci != doc.end()) {
                    while (ci != doc.end() && *ci != ')') ++ci;
                    if (ci == doc.end()) break;

                    const auto* di = d.first;
                    auto ci2 = ci+1;
                    while (ci2 != doc.end() && di != d.second && *ci2 == *di) { ++ci2; ++di; }
                    if (di == d.second) break; // matching deliminator means we terminate here
                    ++ci;
                }

                if (ci == doc.end())
                    Throw(::Exceptions::BasicLabel("Hit end of file while reading chunk in compound text document"));

                result.push_back(
                    TextChunk<CharType>(
                        MakeStringSection(t.first, t.second),
                        MakeStringSection(n.first, n.second),
                        MakeStringSection(contentStart, ci)));
            }
            ++ri;
        }

        return std::move(result);
    }

    // Even though this is templated, it uses std::regex
    //  std::regex has problems with our unicode character types, so it's not
    //  working currently
    template std::vector<TextChunk<char>> ReadCompoundTextDocument(StringSection<char>);

    namespace Internal
    {
        void MarkInvalid(const ResChar initializer[], const char reason[])
        {
            if (Services::GetInvalidAssetMan())
                Services::GetInvalidAssetMan()->MarkInvalid(initializer, reason);
        }

        void MarkValid(const ResChar initializer[])
        {
            if (Services::GetInvalidAssetMan())
                Services::GetInvalidAssetMan()->MarkValid(initializer);
        }
    }
}

