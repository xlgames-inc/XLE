// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Detail/API.h"
#include "../UTFUtils.h"
#include <vector>
#include <assert.h>

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

        void WriteAttribute(const utf8* nameStart, const utf8* nameEnd, const utf8* valueStart, const utf8* valueEnd);
        void WriteAttribute(const ucs2* nameStart, const ucs2* nameEnd, const ucs2* valueStart, const ucs2* valueEnd);
        void WriteAttribute(const ucs4* nameStart, const ucs4* nameEnd, const ucs4* valueStart, const ucs4* valueEnd);

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
        template<typename CharType> void WriteAttributeInt(
            const CharType* nameStart, const CharType* nameEnd,
            const CharType* valueStart, const CharType* valueEnd);
    };

    class MemoryMappedInputStream
    {
    public:
        const void* ReadPointer() const { assert(_ptr != _end); return _ptr; }
        ptrdiff_t RemainingBytes() const { return ptrdiff_t(_end) - ptrdiff_t(_ptr); }
        void MovePointer(ptrdiff_t offset);
        void SetPointer(const void* newPtr);
        const void* Start() const { return _start; }
        const void* End() const { return _end; }

        MemoryMappedInputStream(const void* start, const void* end);
        ~MemoryMappedInputStream();
    protected:
        const void* _start;
        const void* _end;
        const void* _ptr;
    };

    template<typename CharType>
        class XL_UTILITY_API InputStreamFormatter
    {
    public:
        enum class Blob { BeginElement, EndElement, AttributeName, AttributeValue, None };
        Blob PeekNext();

        class InteriorSection
        {
        public:
            const CharType* _start;
            const CharType* _end;
        };

        bool TryReadBeginElement(InteriorSection& name);
        bool TryReadEndElement();
        bool TryReadAttribute(InteriorSection& name, InteriorSection& value);

        InputStreamFormatter(MemoryMappedInputStream& stream);
        ~InputStreamFormatter();
    protected:
        MemoryMappedInputStream* _stream;
        Blob _primed;
        signed _activeLineSpaces;
        signed _parentBaseLine;

        signed _baseLineStack[32];
        unsigned _baseLineStackPtr;

        unsigned _lineIndex;
        const void* _lineStart;

        unsigned CharIndex() const;
    };

}

using namespace Utility;
