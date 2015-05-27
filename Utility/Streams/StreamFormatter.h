// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Detail/API.h"
#include "../UTFUtils.h"
#include <vector>

namespace Utility
{
    class OutputStream;
    class InputStream;

    #define STREAM_FORMATTER_CHECK_ELEMENTS

    class XL_UTILITY_API OutputStreamFormatter
    {
    public:
        typedef unsigned ElementId;

        ElementId BeginElement(const utf8* name);
        ElementId BeginElement(const ucs2* name);
        ElementId BeginElement(const ucs4* name);
        void EndElement(ElementId);

        void WriteAttribute(const utf8* name, const utf8* valueStart, const utf8* valueEnd);
        void WriteAttribute(const ucs2* name, const ucs2* valueStart, const ucs2* valueEnd);
        void WriteAttribute(const ucs4* name, const ucs4* valueStart, const ucs4* valueEnd);

        OutputStreamFormatter(OutputStream& stream);
        ~OutputStreamFormatter();
    protected:
        OutputStream*   _stream;
        unsigned        _currentIndentLevel;
        bool            _hotLine;

        #if defined(STREAM_FORMATTER_CHECK_ELEMENTS)
            std::vector<ElementId> _elementStack;
            unsigned _nextElementId;
        #endif

        template<typename CharType> void NewLine();
        template<typename CharType> ElementId BeginElementInt(const CharType* name);
        template<typename CharType> void WriteAttributeInt(const CharType* name, const CharType* valueStart, const CharType* valueEnd);
    };

    class XL_UTILITY_API InputStreamFormatter
    {
    public:
        enum class Blob { BeginElement, EndElement, Attribute, None };
        Blob GetNext() const;

        class InteriorSection
        {
        public:
            const void* _start;
            const void* _end;
        };
        bool TryReadBeginElement(InteriorSection& name) const;
        bool TryReadAttribute(InteriorSection& name, InteriorSection& value) const;
    };

}

using namespace Utility;
