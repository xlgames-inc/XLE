// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StringUtils.h"        // (for StringSection)

namespace Utility
{
    /*
        FastParseValue does essentially the same thing as std::from_chars, and should be replaced with
        that standard library function where it's available.
        However, it's not fully supported everywhere yet; so we need something to get us by

        The floating point versions are particularly important, but also particularly lagging in
        support. They also seem surprisingly difficult to implement with all of the edge cases.
        So the implementation here isn't actually 100%... but it works in at least some cases, and
        is quick. Just be careful with it -- because it's not guaranteed to be correct
    */
    template<typename CharType> const CharType* FastParseValue(StringSection<CharType> input, int32_t& dst);
    template<typename CharType> const CharType* FastParseValue(StringSection<CharType> input, uint32_t& dst);
    template<typename CharType> const CharType* FastParseValue(StringSection<CharType> input, int64_t& dst);
    template<typename CharType> const CharType* FastParseValue(StringSection<CharType> input, uint64_t& dst);
    template<typename CharType> const CharType* FastParseValue(StringSection<CharType> input, int32_t& dst, unsigned radix);
    template<typename CharType> const CharType* FastParseValue(StringSection<CharType> input, uint32_t& dst, unsigned radix);
    template<typename CharType> const CharType* FastParseValue(StringSection<CharType> input, int64_t& dst, unsigned radix);
    template<typename CharType> const CharType* FastParseValue(StringSection<CharType> input, uint64_t& dst, unsigned radix);
    template<typename CharType> const CharType* FastParseValue(StringSection<CharType> input, float& dst);
    template<typename CharType> const CharType* FastParseValue(StringSection<CharType> input, double& dst);
}

using namespace Utility;
