// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ScaffoldParsingUtil.h"
#include "../Utility/ArithmeticUtils.h"
#include "../Math/XLEMath.h"

namespace ColladaConversion
{
    bool BeginsWith(const XmlInputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[])
    {
        auto matchLen = XlStringSize(match);
        if ((section._end - section._start) < ptrdiff_t(matchLen)) return false;
        return Is(XmlInputStreamFormatter<utf8>::InteriorSection(section._start, section._start + matchLen), match);
    }

    bool EndsWith(const XmlInputStreamFormatter<utf8>::InteriorSection& section, const utf8 match[])
    {
        auto matchLen = XlStringSize(match);
        if ((section._end - section._start) < ptrdiff_t(matchLen)) return false;
        return Is(XmlInputStreamFormatter<utf8>::InteriorSection(section._end - matchLen, section._end), match);
    }

    template<typename CharType>
        __forceinline bool IsWhitespace(CharType chr)
    {
        return chr == 0x20 || chr == 0x9 || chr == 0xD || chr == 0xA;
    }
}


namespace std   // adding these to std is awkward, but it's the only way to make sure easylogging++ can see them
{
    std::ostream& operator<<(std::ostream& os, const StreamLocation& loc) 
    {
        os << "Line: " << loc._lineIndex << ", Char: " << loc._charIndex;
        return os;
    }

    std::ostream& operator<<(std::ostream& os, XmlInputStreamFormatter<utf8>::InteriorSection section)
    {
        os << ColladaConversion::AsString(section);
        return os;
    }
}
