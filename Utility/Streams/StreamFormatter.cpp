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
#include <algorithm>

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
// Shadows < Id=2; Dims={32, 32}; Format=35 >
// Material < Id=1000; Dims={128, 128}; Format=62 >

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

    static const unsigned TabWidth = 4;
    
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

        static const CharType SimpleAttributes_Begin[];
        static const CharType SimpleAttributes_End[];
    };

    template<typename CharType, int Count> 
        static void WriteConst(OutputStream& stream, const CharType (&cnst)[Count], unsigned& lineLength)
    {
        stream.WriteString(cnst, &cnst[Count]);
        lineLength += Count;
    }

    template<typename CharType> 
        static bool UnsafeForSimpleAttribute(CharType c)
    {
        return c==';' || c=='>' || c=='=' || c=='\r' || c=='\n' || c == 0x0;
    }

    template<typename CharType> 
        static bool UnsafeStarterForSimpleAttribute(CharType c)
    {
        return c==' ' || c=='\t' || c==0x0B || c==0x0C || c==0x85 || c==0xA0;
    }

    template<typename CharType> 
        static bool UnsafeForSimpleElementName(CharType c)
    {
        return c==';' || c=='<' || c=='>' || c=='=' || c=='\r' || c=='\n' || c==' ' || c=='\t' || c==0x0B || c==0x0C || c==0x85 || c==0xA0 || c==0x0;
    }

    template<typename CharType>
        auto OutputStreamFormatter::BeginElement(const CharType* nameStart, const CharType* nameEnd) -> ElementId
    {
        DoNewLine<CharType>();

        assert(nameEnd > nameStart);

            // in simple cases, we just write the name without extra formatting --
        if (std::find_if(nameStart, nameEnd, UnsafeForSimpleAttribute<CharType>) == nameEnd && *nameStart != '-') {
            _stream->WriteString(nameStart, nameEnd);
        } else {
            WriteConst(*_stream, FormatterConstants<CharType>::ElementPrefix, _currentLineLength);
            _stream->WriteString(nameStart, nameEnd);
            WriteConst(*_stream, FormatterConstants<CharType>::ElementPostfix, _currentLineLength);
        }

        _hotLine = true;
        _currentLineLength += unsigned(nameEnd - nameStart);
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
        void OutputStreamFormatter::DoNewLine()
    {
        if (_hotLine) {
            if (_writingSimpleAttributes != ~unsigned(0x0)) {
                WriteConst(*_stream, FormatterConstants<CharType>::SimpleAttributes_End, _currentLineLength);
                _writingSimpleAttributes = ~unsigned(0x0);
            }

            WriteConst(*_stream, FormatterConstants<CharType>::EndLine, _currentLineLength);
            
            CharType tabBuffer[64];
            if (_currentIndentLevel > dimof(tabBuffer))
                ThrowException(::Exceptions::BasicLabel("Excessive indent level found in OutputStreamFormatter (%i)", _currentIndentLevel));
            std::fill(tabBuffer, &tabBuffer[_currentIndentLevel], FormatterConstants<CharType>::Tab);
            _stream->WriteString(tabBuffer, &tabBuffer[_currentIndentLevel]);
            _hotLine = false;
            _currentLineLength = _currentIndentLevel * TabWidth;
        }
    }

    template<typename CharType> 
        void OutputStreamFormatter::WriteAttribute(
            const CharType* nameStart, const CharType* nameEnd,
            const CharType* valueStart, const CharType* valueEnd)
    {
        if (    std::find_if(nameStart, nameEnd, UnsafeForSimpleAttribute<CharType>) == nameEnd
            &&  std::find_if(valueStart, valueEnd, UnsafeForSimpleAttribute<CharType>) == valueEnd
            &&  (nameEnd == nameStart) || !UnsafeStarterForSimpleAttribute(*nameStart)
            &&  (valueEnd == valueStart) || !UnsafeStarterForSimpleAttribute(*valueStart)) {

            const unsigned idealLineLength = 100;
            bool forceNewLine = 
                    (_writingSimpleAttributes != ~unsigned(0x0) && _writingSimpleAttributes != _currentIndentLevel)
                ||  (_currentLineLength + (valueEnd - valueStart) + (nameEnd - nameStart) + 3) > idealLineLength;

            if (forceNewLine) DoNewLine<CharType>();

            if (_writingSimpleAttributes != _currentIndentLevel) {
                if (_hotLine) { _stream->WriteChar((CharType)' '); ++_currentLineLength; }
                WriteConst(*_stream, FormatterConstants<CharType>::SimpleAttributes_Begin, _currentLineLength);
            } else {
                _stream->WriteChar((CharType)';'); ++_currentLineLength;
                _stream->WriteChar((CharType)' '); ++_currentLineLength;
            }
            _stream->WriteString(nameStart, nameEnd);
            _stream->WriteChar((CharType)'=');
            _stream->WriteString(valueStart, valueEnd);

            _writingSimpleAttributes = _currentIndentLevel;
            _currentLineLength += unsigned((valueEnd - valueStart) + (nameEnd - nameStart) + 1);
            _hotLine = true;
            return;
        }
        
        DoNewLine<CharType>();

        WriteConst(*_stream, FormatterConstants<CharType>::AttributeNamePrefix, _currentLineLength);
        _stream->WriteString(nameStart, nameEnd);
        WriteConst(*_stream, FormatterConstants<CharType>::AttributeNamePostfix, _currentLineLength);

        WriteConst(*_stream, FormatterConstants<CharType>::AttributeValuePrefix, _currentLineLength);
        _stream->WriteString(valueStart, valueEnd);
        WriteConst(*_stream, FormatterConstants<CharType>::AttributeValuePostfix, _currentLineLength);

        _currentLineLength += unsigned((valueEnd - valueStart) + (nameEnd - nameStart));
        _hotLine = true;
    }

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

    void OutputStreamFormatter::Flush()
    {
        DoNewLine<utf8>();    // end in a new line to make sure the last ">" gets written
        assert(_currentIndentLevel == 0);
    }

    void OutputStreamFormatter::NewLine()
    {
        DoNewLine<utf8>();
    }

    OutputStreamFormatter::OutputStreamFormatter(OutputStream& stream) 
    : _stream(&stream)
    {
        _currentIndentLevel = 0;
        _hotLine = false;
        _currentLineLength = 0;
        _writingSimpleAttributes = ~unsigned(0x0);
    }

    OutputStreamFormatter::~OutputStreamFormatter()
    {}

    template<> auto OutputStreamFormatter::BeginElement(const char* nameStart, const char* nameEnd) -> ElementId
    {
        return BeginElement((const utf8*)nameStart, (const utf8*)nameEnd);
    }

    template<> void OutputStreamFormatter::WriteAttribute(const char* nameStart, const char* nameEnd, const char* valueStart, const char* valueEnd)
    {
        WriteAttribute(
            (const utf8*)nameStart, (const utf8*)nameEnd,
            (const utf8*)valueStart, (const utf8*)valueEnd);
    }

    template auto OutputStreamFormatter::BeginElement(const utf8* nameStart, const utf8* nameEnd) -> ElementId;
    template auto OutputStreamFormatter::BeginElement(const ucs2* nameStart, const ucs2* nameEnd) -> ElementId;
    template auto OutputStreamFormatter::BeginElement(const ucs4* nameStart, const ucs4* nameEnd) -> ElementId;
    
    template void OutputStreamFormatter::WriteAttribute(const utf8* nameStart, const utf8* nameEnd, const utf8* valueStart, const utf8* valueEnd);
    template void OutputStreamFormatter::WriteAttribute(const ucs2* nameStart, const ucs2* nameEnd, const ucs2* valueStart, const ucs2* valueEnd);
    template void OutputStreamFormatter::WriteAttribute(const ucs4* nameStart, const ucs4* nameEnd, const ucs4* valueStart, const ucs4* valueEnd);

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
        const CharType* ReadToSimpleAttributeDeliminator(
            MemoryMappedInputStream& stream,
            unsigned lineIndex, unsigned charIndex)
    {
        const auto* end = (const CharType*)stream.End();
        const auto* ptr = (const CharType*)stream.ReadPointer();
        while (ptr < end && (*ptr != ';' && *ptr != '>' && *ptr != '=')) ++ptr;
        stream.SetPointer(ptr);
        return ptr;
    }

    template<typename CharType>
        auto InputStreamFormatter<CharType>::PeekNext() -> Blob
    {
        if (_primed != Blob::None) return _primed;

        using Consts = FormatterConstants<CharType>;

        while (_stream.RemainingBytes() > sizeof(CharType)) {
            const auto* next = (const CharType*)_stream.ReadPointer();

            switch (*next)
            {
            case '\t':
                _stream.MovePointer(sizeof(CharType));
                _activeLineSpaces = CeilToMultiple(_activeLineSpaces+1, TabWidth);
                continue;
            case ' ': 
                _stream.MovePointer(sizeof(CharType));
                ++_activeLineSpaces; 
                continue;

                // throw exception when using an extended unicode whitespace character
                // let's just stick to the ascii whitespace characters for simplicity
            case 0x0B:  // (line tabulation)
            case 0x0C:  // (form feed)
            case 0x85:  // (next line)
            case 0xA0:  // (no-break space)
                ThrowException(Exceptions::FormatException("Unsupported white space character", _lineIndex, CharIndex()));
            }

            if (!_simpleAttributeMode) {

                switch (*next)
                {
                case '\r':  // (could be an independant new line, or /r/n combo)
                    _stream.MovePointer(sizeof(CharType));
                    if (    _stream.RemainingBytes() > sizeof(CharType)
                        &&  *(const CharType*)_stream.ReadPointer() == '\n')
                        _stream.MovePointer(sizeof(CharType));

                        // don't adjust _expected line spaces here -- we want to be sure 
                        // that lines with just whitespace don't affect _activeLineSpaces
                    _activeLineSpaces = 0;
                    ++_lineIndex; _lineStart = _stream.ReadPointer();
                    break;

                case '\n':  // (independant new line. A following /r will be treated as another new line)
                    _stream.MovePointer(sizeof(CharType));
                    _activeLineSpaces = 0;
                    ++_lineIndex; _lineStart = _stream.ReadPointer();
                    break;

                case '-': // Consts::ElementPrefix[0]:
                case '~': // Consts::AttributeNamePrefix[0]:
                case ':': // Consts::AttributeValuePrefix[0]:
                case '<': // Consts::SimpleAttributes_Begin[0]
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
                            Eat(_stream, Consts::ElementPrefix, _lineIndex, CharIndex());
                            _simpleElementNameMode = 0;
                            return _primed = Blob::BeginElement;
                        }

                        if (*next == Consts::AttributeNamePrefix[0]) {
                            Eat(_stream, Consts::AttributeNamePrefix, _lineIndex, CharIndex());
                            return _primed = Blob::AttributeName;
                        }

                        if (*next == Consts::AttributeValuePrefix[0]) {
                            Eat(_stream, Consts::AttributeValuePrefix, _lineIndex, CharIndex());
                            return _primed = Blob::AttributeValue;
                        }

                        if (*next == Consts::SimpleAttributes_Begin[0]) {
                            Eat(_stream, Consts::SimpleAttributes_Begin, _lineIndex, CharIndex());
                            _simpleAttributeMode = 1;
                            return _primed = Blob::AttributeName;
                        }

                        assert(0);
                    }
                    break;

                case '>': // Consts::SimpleAttributes_End[0]
                    ThrowException(Exceptions::FormatException("Expected '>' outside of simple attribute mode", _lineIndex, CharIndex()));
                    break;

                default:
                    // ThrowException(Exceptions::FormatException("Expected element or attribute", _lineIndex, CharIndex()));

                        // unexpected characters are treated as the start of a simple element name
                    if (_activeLineSpaces <= _parentBaseLine) {
                        return _primed = Blob::EndElement;
                    }

                    _simpleElementNameMode = 1;
                    return _primed = Blob::BeginElement;
                }

            } else {

                switch (*next)
                {
                case '\r':
                case '\n':
                    ThrowException(Exceptions::FormatException("Newline in simple attribute block", _lineIndex, CharIndex()));

                case '>': // Consts::SimpleAttributes_End[0]
                    Eat(_stream, Consts::SimpleAttributes_End, _lineIndex, CharIndex());
                    _simpleAttributeMode = 0;
                    break;

                case '=':
                    _simpleAttributeMode = 2;
                    _stream.MovePointer(sizeof(CharType));
                    break;

                case ';':
                    _simpleAttributeMode = 1;
                    _stream.MovePointer(sizeof(CharType));
                    break;

                default:
                    return (_simpleAttributeMode==1)?_primed = Blob::AttributeName:_primed = Blob::AttributeValue;
                }

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

        name._start = (const CharType*)_stream.ReadPointer();

        if (_simpleElementNameMode == 0) {
            name._end = ReadToDeliminator(_stream, FormatterConstants<CharType>::ElementPostfix, _lineIndex, CharIndex());
        } else {
            const auto* end = (const CharType*)_stream.End();
            name._end = (const CharType*)_stream.ReadPointer();
            while (name._end <= end && !UnsafeForSimpleElementName(*name._end)) ++name._end;
            _stream.SetPointer(name._end);
        }

        // the new "parent base line" should be the indentation level of the line this element started on
        if ((_baseLineStackPtr+1) > dimof(_baseLineStack))
            ThrowException(Exceptions::FormatException(
                "Excessive indentation format in input stream formatter", _lineIndex, CharIndex()));

        _baseLineStack[_baseLineStackPtr++] = _activeLineSpaces;
        _parentBaseLine = _activeLineSpaces;
        _primed = Blob::None;
        _simpleElementNameMode = 0;
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
        void InputStreamFormatter<CharType>::SkipElement()
    {
        unsigned subtreeEle = 0;
        InteriorSection dummy0, dummy1;
        switch(PeekNext()) {
        case Blob::BeginElement:
            if (!TryReadBeginElement(dummy0))
                ThrowException(Exceptions::FormatException(
                    "Malformed begin element while skipping forward", _lineIndex, CharIndex()));
            ++subtreeEle;
            break;

        case Blob::EndElement:
            if (!subtreeEle) return;    // end now, while the EndElement is primed

            if (!TryReadEndElement())
                ThrowException(Exceptions::FormatException(
                    "Malformed end element while skipping forward", _lineIndex, CharIndex()));
            --subtreeEle;
            break;

        case Blob::AttributeName:
            if (!TryReadAttribute(dummy0, dummy1))
                ThrowException(Exceptions::FormatException(
                    "Malformed attribute while skipping forward", _lineIndex, CharIndex()));
            break;

        default:
            ThrowException(Exceptions::FormatException(
                "Unexpected blob or end of stream hit while skipping forward", _lineIndex, CharIndex()));
        }
    }

    template<typename CharType>
        bool InputStreamFormatter<CharType>::TryReadAttribute(InteriorSection& name, InteriorSection& value)
    {
        if (PeekNext() != Blob::AttributeName) return false;

        if (_simpleAttributeMode) {
            name._start = (const CharType*)_stream.ReadPointer();
            name._end = ReadToSimpleAttributeDeliminator<CharType>(_stream, _lineIndex, CharIndex());
            _primed = Blob::None;

            if (PeekNext() == Blob::AttributeValue) {
                value._start = (const CharType*)_stream.ReadPointer();
                value._end = ReadToSimpleAttributeDeliminator<CharType>(_stream, _lineIndex, CharIndex());
            } else {
                value._start = value._end = nullptr;
            }
            _primed = Blob::None;

        } else {
            name._start = (const CharType*)_stream.ReadPointer();
            name._end = ReadToDeliminator(
                _stream, FormatterConstants<CharType>::AttributeNamePostfix, 
                _lineIndex, CharIndex());
            _primed = Blob::None;

            if (PeekNext() == Blob::AttributeValue) {
                value._start = (const CharType*)_stream.ReadPointer();
                value._end = ReadToDeliminator(
                    _stream, FormatterConstants<CharType>::AttributeValuePostfix, 
                    _lineIndex, CharIndex());
            } else {
                value._start = value._end = nullptr;
            }
            _primed = Blob::None;
        }

        return true;
    }

    template<typename CharType>
        unsigned InputStreamFormatter<CharType>::CharIndex() const
    {
        return unsigned((size_t(_stream.ReadPointer()) - size_t(_lineStart)) / sizeof(CharType));
    }

    template<typename CharType>
        InputStreamFormatter<CharType>::InputStreamFormatter(MemoryMappedInputStream& stream) : _stream(stream)
    {
        _primed = Blob::None;
        _activeLineSpaces = 0;
        _parentBaseLine = -1;
        _baseLineStackPtr = 0;
        _lineIndex = 0;
        _lineStart = stream.ReadPointer();
        _simpleAttributeMode = 0;
        _simpleElementNameMode = 0;
    }

    template<typename CharType>
        InputStreamFormatter<CharType>::~InputStreamFormatter()
    {}

    template class InputStreamFormatter<utf8>;
    template class InputStreamFormatter<ucs4>;
    template class InputStreamFormatter<ucs2>;

    const utf8 FormatterConstants<utf8>::EndLine[] = { (utf8)'\r', (utf8)'\n' };
    const ucs2 FormatterConstants<ucs2>::EndLine[] = { (ucs2)'\r', (ucs2)'\n' };
    const ucs4 FormatterConstants<ucs4>::EndLine[] = { (ucs4)'\r', (ucs4)'\n' };

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

    const utf8 FormatterConstants<utf8>::SimpleAttributes_Begin[] = { (utf8)'<' };
    const ucs2 FormatterConstants<ucs2>::SimpleAttributes_Begin[] = { (ucs2)'<' };
    const ucs4 FormatterConstants<ucs4>::SimpleAttributes_Begin[] = { (ucs4)'<' };

    const utf8 FormatterConstants<utf8>::SimpleAttributes_End[] = { (utf8)'>' };
    const ucs2 FormatterConstants<ucs2>::SimpleAttributes_End[] = { (ucs2)'>' };
    const ucs4 FormatterConstants<ucs4>::SimpleAttributes_End[] = { (ucs4)'>' };

    const utf8 FormatterConstants<utf8>::Tab = (utf8)'\t';
    const ucs2 FormatterConstants<ucs2>::Tab = (ucs2)'\t';
    const ucs4 FormatterConstants<ucs4>::Tab = (ucs4)'\t';
}

