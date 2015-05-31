// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Detail/API.h"
#include "../UTFUtils.h"
#include "../StringUtils.h"
#include "../PtrUtils.h"
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

        template<typename CharType> 
            ElementId BeginElement(const CharType* nameStart, const CharType* nameEnd);
        void EndElement(ElementId);
        
        template<typename CharType> 
            void WriteAttribute(
                const CharType* nameStart, const CharType* nameEnd,
                const CharType* valueStart, const CharType* valueEnd);

        template<typename CharType> 
            ElementId BeginElement(const CharType* nameNullTerm)
            {
                return BeginElement(nameNullTerm, &nameNullTerm[XlStringLen(nameNullTerm)]);
            }

        template<typename CharType> 
            void WriteAttribute(const CharType* nameNullTerm, const CharType* valueNullTerm)
            {
                WriteAttribute(
                    nameNullTerm, &nameNullTerm[XlStringLen(nameNullTerm)],
                    valueNullTerm, &valueNullTerm[XlStringLen(valueNullTerm)]);
            }

        template<typename CharType> 
            void WriteAttribute(const CharType* nameNullTerm, const std::basic_string<CharType>& value)
            {
                WriteAttribute(
                    nameNullTerm, &nameNullTerm[XlStringLen(nameNullTerm)],
                    AsPointer(value.cbegin()), AsPointer(value.cend()));
            }

        void Flush();
        void NewLine();

        OutputStreamFormatter(OutputStream& stream);
        ~OutputStreamFormatter();
    protected:
        OutputStream*   _stream;
        unsigned        _currentIndentLevel;
        bool            _hotLine;
        unsigned        _currentLineLength;
        unsigned        _writingSimpleAttributes;

        #if defined(STREAM_FORMATTER_CHECK_ELEMENTS)
            std::vector<ElementId> _elementStack;
            unsigned _nextElementId;
        #endif

        template<typename CharType> void DoNewLine();
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

        void SkipElement();

        InputStreamFormatter(MemoryMappedInputStream& stream);
        ~InputStreamFormatter();
    protected:
        MemoryMappedInputStream _stream;
        Blob _primed;
        signed _activeLineSpaces;
        signed _parentBaseLine;

        signed _baseLineStack[32];
        unsigned _baseLineStackPtr;

        unsigned _lineIndex;
        const void* _lineStart;

        unsigned _simpleAttributeMode;
        unsigned _simpleElementNameMode;

        unsigned CharIndex() const;
    };

}

using namespace Utility;
