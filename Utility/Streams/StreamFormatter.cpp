// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StreamFormatter.h"
#include "Stream.h"
#include "../BitUtils.h"
#include "../PtrUtils.h"
#include "../../Core/Exceptions.h"
#include <assert.h>

//
//  --(Element)--
//      ~~(Attribute Name)~~::(Attribute Value)::~~(Attribute Name)~~::(Attribute Value)::
//      ~~(Attribute Name)~~::(Attribute Value)::
//      --(SubElement)--
//          ~~(Attribute Name)~~::(Attribute Value)::
//          ~~(Attribute Name)~~::(Attribute Value
//  second line of attribute value)::
//      --(SubElement)--
//  --(Element)--
//      ~~(Attribute Name)~~::(Attribute Value)::
//
///////////////////////////////////////////////////////////////////////////////////////////////////
//
//  *--(Element)--
//      --(Attribute Name)--=--(Attribute Value)--
//      --(Attribute Name)--=--(Attribute Value)--
//      *--(SubElement)--
//          --(Attribute Name)--=--(Attribute Value)--
//          --(Attribute Name)--=--(Attribute Value)--
//

namespace Utility
{
    void MemoryMappedInputStream::MovePointer(ptrdiff_t offset) 
    { 
        assert(PtrAdd(_ptr, offset) <= _end && PtrAdd(_ptr, offset) >= _start);
        _ptr = PtrAdd(_ptr, offset);
    }

    void MemoryMappedInputStream::SetPointer(const void* newPtr)
    {
        assert(newPtr <= _end && newPtr >= _start);
        _ptr = newPtr;
    }

    MemoryMappedInputStream::MemoryMappedInputStream(const void* start, const void* end) 
    {
        _start = _ptr = start;
        _end = end;
    }

    MemoryMappedInputStream::~MemoryMappedInputStream() {}

    template<typename CharType>
        struct FormatterConstants 
    {
        static const CharType EndLine[];
        static const CharType Tab;

        static const CharType ElementPrefix[];
        static const CharType ElementPostfix[];
        static const CharType AttributeNamePrefix[];
        static const CharType AttributeNamePostfix[];
        static const CharType AttributeValuePrefix[];
        static const CharType AttributeValuePostfix[];
    };

    template<typename CharType>
        auto OutputStreamFormatter::BeginElementInt(const CharType* name) -> ElementId
    {
        NewLine<CharType>();
        _stream->WriteNullTerm(FormatterConstants<CharType>::ElementPrefix);
        _stream->WriteNullTerm(name);
        _stream->WriteNullTerm(FormatterConstants<CharType>::ElementPostfix);
        _hotLine = true;
        ++_currentIndentLevel;

        #if defined(STREAM_FORMATTER_CHECK_ELEMENTS)
            auto id = _nextElementId;
            _elementStack.push_back(id);
            return id;
        #else
            return 0;
        #endif
    }

    template<typename CharType>
        void OutputStreamFormatter::NewLine()
    {
        if (_hotLine) {
            _stream->WriteNullTerm(FormatterConstants<CharType>::EndLine);
            
            CharType tabBuffer[64];
            if (_currentIndentLevel > dimof(tabBuffer))
                ThrowException(::Exceptions::BasicLabel("Excessive indent level found in OutputStreamFormatter (%i)", _currentIndentLevel));
            std::fill(tabBuffer, &tabBuffer[_currentIndentLevel], FormatterConstants<CharType>::Tab);
            _stream->WriteString(tabBuffer, &tabBuffer[_currentIndentLevel]);
            _hotLine = false;
        }
    }

    template<typename CharType> 
        void OutputStreamFormatter::WriteAttributeInt(
            const CharType* nameStart, const CharType* nameEnd,
            const CharType* valueStart, const CharType* valueEnd)
    {
        NewLine<CharType>();

        _stream->WriteNullTerm(FormatterConstants<CharType>::AttributeNamePrefix);
        _stream->WriteString(nameStart, nameEnd);
        _stream->WriteNullTerm(FormatterConstants<CharType>::AttributeNamePostfix);

        _stream->WriteNullTerm(FormatterConstants<CharType>::AttributeValuePrefix);
        _stream->WriteString(valueStart, valueEnd);
        _stream->WriteNullTerm(FormatterConstants<CharType>::AttributeValuePostfix);

        _hotLine = true;
    }

    auto OutputStreamFormatter::BeginElement(const utf8* name) -> ElementId { return BeginElementInt(name); }
    auto OutputStreamFormatter::BeginElement(const ucs2* name) -> ElementId { return BeginElementInt(name); }
    auto OutputStreamFormatter::BeginElement(const ucs4* name) -> ElementId { return BeginElementInt(name); }

    void OutputStreamFormatter::EndElement(ElementId id)
    {
        if (_currentIndentLevel == 0)
            ThrowException(::Exceptions::BasicLabel("Unexpected EndElement in OutputStreamFormatter"));

        #if defined(STREAM_FORMATTER_CHECK_ELEMENTS)
            assert(_elementStack.size() == _currentIndentLevel);
            if (_elementStack[_elementStack.size()-1] != id)
                ThrowException(::Exceptions::BasicLabel("EndElement for wrong element id in OutputStreamFormatter"));
            _elementStack.erase(_elementStack.end()-1);
        #endif

        --_currentIndentLevel;
    }

    void OutputStreamFormatter::WriteAttribute(const utf8* nameStart, const utf8* nameEnd, const utf8* valueStart, const utf8* valueEnd) { return WriteAttributeInt(nameStart, nameEnd, valueStart, valueEnd); }
    void OutputStreamFormatter::WriteAttribute(const ucs2* nameStart, const ucs2* nameEnd, const ucs2* valueStart, const ucs2* valueEnd) { return WriteAttributeInt(nameStart, nameEnd, valueStart, valueEnd); }
    void OutputStreamFormatter::WriteAttribute(const ucs4* nameStart, const ucs4* nameEnd, const ucs4* valueStart, const ucs4* valueEnd) { return WriteAttributeInt(nameStart, nameEnd, valueStart, valueEnd); }

    OutputStreamFormatter::OutputStreamFormatter(OutputStream& stream) 
    : _stream(&stream)
    {
        _currentIndentLevel = 0;
        _hotLine = false;
    }

    OutputStreamFormatter::~OutputStreamFormatter()
    {}

    namespace Exceptions
    {
        class FormatException : public ::Exceptions::BasicLabel
        {
        public:
            FormatException(const char label[], unsigned lineIndex, unsigned charIndex)
                : ::Exceptions::BasicLabel("Format Exception: (%s) at line (%i), (%i)", label, lineIndex, charIndex) {}
        };
    }

    template<typename CharType, int Count>
        void Eat(MemoryMappedInputStream& stream, CharType (&pattern)[Count], unsigned lineIndex, unsigned charIndex)
    {
        if (stream.RemainingBytes() < (sizeof(CharType)*Count))
            ThrowException(Exceptions::FormatException("Blob prefix clipped", lineIndex, charIndex));

        const auto* test = (const CharType*)stream.ReadPointer();
        for (unsigned c=0; c<Count; ++c)
            if (test[c] != pattern[c])
                ThrowException(Exceptions::FormatException("Malformed blob prefix", lineIndex, charIndex));
        
        stream.MovePointer(sizeof(CharType)*Count);
    }

    template<typename CharType, int Count>
        const CharType* ReadToDeliminator(
            MemoryMappedInputStream& stream, CharType (&pattern)[Count], 
            unsigned lineIndex, unsigned charIndex)
    {
        const auto* end = ((const CharType*)stream.End()) - Count;
        const auto* ptr = (const CharType*)stream.ReadPointer();
        while (ptr <= end) {
            for (unsigned c=0; c<Count; ++c)
                if (ptr[c] != pattern[c])
                    goto advptr;

            stream.SetPointer(ptr + Count);
            return ptr;
        advptr:
            ++ptr;
        }

        ThrowException(Exceptions::FormatException("String deliminator not found", lineIndex, charIndex));
    }

    template<typename CharType>
        auto InputStreamFormatter<CharType>::PeekNext() -> Blob
    {
        if (_primed != Blob::None) return _primed;

        using Consts = FormatterConstants<CharType>;
        const unsigned tabWidth = 4;

        while (_stream->RemainingBytes() > sizeof(CharType)) {
            const auto* next = (const CharType*)_stream->ReadPointer();

            switch (*next)
            {
            case '\t':
                _stream->MovePointer(sizeof(CharType));
                _activeLineSpaces = CeilToMultiple(_activeLineSpaces+1, tabWidth);
                break;
            case ' ': 
                _stream->MovePointer(sizeof(CharType));
                ++_activeLineSpaces; 
                break;

                // throw exception when using an extended unicode whitespace character
                // let's just stick to the ascii whitespace characters for simplicity
            case 0x0B:  // (line tabulation)
            case 0x0C:  // (form feed)
            case 0x85:  // (next line)
            case 0xA0:  // (no-break space)
                ThrowException(Exceptions::FormatException("Unsupported white space character", _lineIndex, CharIndex()));

            case '\r':  // (could be an independant new line, or /r/n combo)
                _stream->MovePointer(sizeof(CharType));
                if (    _stream->RemainingBytes() > sizeof(CharType)
                    &&  *(const CharType*)_stream->ReadPointer() == '\n')
                    _stream->MovePointer(sizeof(CharType));

                    // don't adjust _expected line spaces here -- we want to be sure 
                    // that lines with just whitespace don't affect _activeLineSpaces
                _activeLineSpaces = 0;
                ++_lineIndex; _lineStart = _stream->ReadPointer();
                break;

            case '\n':  // (independant new line. A following /r will be treated as another new line)
                _stream->MovePointer(sizeof(CharType));
                _activeLineSpaces = 0;
                ++_lineIndex; _lineStart = _stream->ReadPointer();
                break;

            case '-': // Consts::ElementPrefix[0]:
            case '~': // Consts::AttributeNamePrefix[0]:
            case ':': // Consts::AttributeValuePrefix[0]:
                {
                    // first, if our spacing has decreased, then we must consider it an "end element"
                    // caller must follow with "TryReadEndElement" until _expectedLineSpaces matches _activeLineSpaces
                    if (_activeLineSpaces <= _parentBaseLine) {
                        return _primed = Blob::EndElement;
                    }

                        // now, _activeLineSpaces must be larger than _parentBaseLine. Anything that is 
                        // more indented than it's parent will become it's child
                        // let's see if there's a fully formed blob here
                    if (*next == Consts::ElementPrefix[0]) {
                        Eat(*_stream, Consts::ElementPrefix, _lineIndex, CharIndex());
                        return _primed = Blob::BeginElement;
                    }

                    if (*next == Consts::AttributeNamePrefix[0]) {
                        Eat(*_stream, Consts::AttributeNamePrefix, _lineIndex, CharIndex());
                        return _primed = Blob::AttributeName;
                    }

                    if (*next == Consts::AttributeValuePrefix[0]) {
                        Eat(*_stream, Consts::AttributeValuePrefix, _lineIndex, CharIndex());
                        return _primed = Blob::AttributeValue;
                    }
                }
                continue;

            default:
                ThrowException(Exceptions::FormatException("Expected element or attribute", _lineIndex, CharIndex()));
            }
        }

            // we've reached the end of the stream...
            // while there are elements on our stack, we need to end them
        if (_baseLineStackPtr > 0) return _primed = Blob::EndElement;
        return Blob::None;
    }

    template<typename CharType>
        bool InputStreamFormatter<CharType>::TryReadBeginElement(InteriorSection& name)
    {
        if (PeekNext() != Blob::BeginElement) return false;

        name._start = (const CharType*)_stream->ReadPointer();
        name._end = ReadToDeliminator(*_stream, FormatterConstants<CharType>::ElementPostfix, _lineIndex, CharIndex());

        // the new "parent base line" should be the indentation level of the line this element started on
        if ((_baseLineStackPtr+1) > dimof(_baseLineStack))
            ThrowException(Exceptions::FormatException(
                "Excessive indentation format in input stream formatter", _lineIndex, CharIndex()));

        _baseLineStack[_baseLineStackPtr++] = _activeLineSpaces;
        _parentBaseLine = _activeLineSpaces;
        _primed = Blob::None;
        return true;
    }

    template<typename CharType>
        bool InputStreamFormatter<CharType>::TryReadEndElement()
    {
        if (PeekNext() != Blob::EndElement) return false;

        if (_baseLineStackPtr != 0) {
            _parentBaseLine = (_baseLineStackPtr > 1) ? _baseLineStack[_baseLineStackPtr-2] : -1;
            --_baseLineStackPtr;
        }

        _primed = Blob::None;
        return true;
    }

    template<typename CharType>
        bool InputStreamFormatter<CharType>::TryReadAttribute(InteriorSection& name, InteriorSection& value)
    {
        if (PeekNext() != Blob::AttributeName) return false;

        name._start = (const CharType*)_stream->ReadPointer();
        name._end = ReadToDeliminator(*_stream, FormatterConstants<CharType>::AttributeNamePostfix, _lineIndex, CharIndex());
        _primed = Blob::None;

        if (PeekNext() == Blob::AttributeValue) {
            value._start = (const CharType*)_stream->ReadPointer();
            value._end = ReadToDeliminator(*_stream, FormatterConstants<CharType>::AttributeValuePostfix, _lineIndex, CharIndex());
        } else {
            value._start = value._end = nullptr;
        }

        _primed = Blob::None;
        return true;
    }

    template<typename CharType>
        unsigned InputStreamFormatter<CharType>::CharIndex() const
    {
        return unsigned((size_t(_stream->ReadPointer()) - size_t(_lineStart)) / sizeof(CharType));
    }

    template<typename CharType>
        InputStreamFormatter<CharType>::InputStreamFormatter(MemoryMappedInputStream& stream) : _stream(&stream)
    {
        _primed = Blob::None;
        _activeLineSpaces = 0;
        _parentBaseLine = -1;
        _baseLineStackPtr = 0;
        _lineIndex = 0;
        _lineStart = stream.ReadPointer();
    }

    template<typename CharType>
        InputStreamFormatter<CharType>::~InputStreamFormatter()
    {}

    template class InputStreamFormatter<utf8>;
    template class InputStreamFormatter<ucs4>;
    template class InputStreamFormatter<ucs2>;

    const utf8 FormatterConstants<utf8>::EndLine[] = { (utf8)'\r', (utf8)'\n', (utf8)'\0' };
    const ucs2 FormatterConstants<ucs2>::EndLine[] = { (ucs2)'\r', (ucs2)'\n', (ucs2)'\0' };
    const ucs4 FormatterConstants<ucs4>::EndLine[] = { (ucs4)'\r', (ucs4)'\n', (ucs4)'\0' };

    const utf8 FormatterConstants<utf8>::ElementPrefix[] = { (utf8)'-', (utf8)'-', (utf8)'(' };
    const ucs2 FormatterConstants<ucs2>::ElementPrefix[] = { (ucs2)'-', (ucs2)'-', (ucs2)'(' };
    const ucs4 FormatterConstants<ucs4>::ElementPrefix[] = { (ucs4)'-', (ucs4)'-', (ucs4)'(' };

    const utf8 FormatterConstants<utf8>::ElementPostfix[] = { (utf8)')', (utf8)'-', (utf8)'-' };
    const ucs2 FormatterConstants<ucs2>::ElementPostfix[] = { (ucs2)')', (ucs2)'-', (ucs2)'-' };
    const ucs4 FormatterConstants<ucs4>::ElementPostfix[] = { (ucs4)')', (ucs4)'-', (ucs4)'-' };

    const utf8 FormatterConstants<utf8>::AttributeNamePrefix[] = { (utf8)'~', (utf8)'~', (utf8)'(' };
    const ucs2 FormatterConstants<ucs2>::AttributeNamePrefix[] = { (ucs2)'~', (ucs2)'~', (ucs2)'(' };
    const ucs4 FormatterConstants<ucs4>::AttributeNamePrefix[] = { (ucs4)'~', (ucs4)'~', (ucs4)'(' };

    const utf8 FormatterConstants<utf8>::AttributeNamePostfix[] = { (utf8)')', (utf8)'~', (utf8)'~' };
    const ucs2 FormatterConstants<ucs2>::AttributeNamePostfix[] = { (ucs2)')', (ucs2)'~', (ucs2)'~' };
    const ucs4 FormatterConstants<ucs4>::AttributeNamePostfix[] = { (ucs4)')', (ucs4)'~', (ucs4)'~' };

    const utf8 FormatterConstants<utf8>::AttributeValuePrefix[] = { (utf8)':', (utf8)':', (utf8)'(' };
    const ucs2 FormatterConstants<ucs2>::AttributeValuePrefix[] = { (ucs2)':', (ucs2)':', (ucs2)'(' };
    const ucs4 FormatterConstants<ucs4>::AttributeValuePrefix[] = { (ucs4)':', (ucs4)':', (ucs4)'(' };

    const utf8 FormatterConstants<utf8>::AttributeValuePostfix[] = { (utf8)')', (utf8)':', (utf8)':' };
    const ucs2 FormatterConstants<ucs2>::AttributeValuePostfix[] = { (ucs2)')', (ucs2)':', (ucs2)':' };
    const ucs4 FormatterConstants<ucs4>::AttributeValuePostfix[] = { (ucs4)')', (ucs4)':', (ucs4)':' };

    const utf8 FormatterConstants<utf8>::Tab = (utf8)'\t';
    const ucs2 FormatterConstants<ucs2>::Tab = (ucs2)'\t';
    const ucs4 FormatterConstants<ucs4>::Tab = (ucs4)'\t';
}

