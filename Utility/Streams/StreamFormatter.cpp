// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "StreamFormatter.h"
#include "Stream.h"
#include "../../Core/Exceptions.h"
#include <assert.h>

//  Element
//      --(Attribute Name)-- ==(Attribute Value)== --(Attribute Name)-- ==(Attribute Value)==
//      --(Attribute Name)-- ==(Attribute Value)==
//      SubElement
//          --(Attribute Name)-- ==(Attribute Value)==
//          --(Attribute Name)-- ==(Attribute Value
//  second line of attribute value)==
//      SubElement
//  Element
//      --(Attribute Name)-- ==(Attribute Value)==

namespace Utility
{
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
            const CharType* name, const CharType* valueStart, const CharType* valueEnd)
    {
        NewLine<CharType>();

        _stream->WriteNullTerm(FormatterConstants<CharType>::AttributeNamePrefix);
        _stream->WriteNullTerm(name);
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

    void OutputStreamFormatter::WriteAttribute(const utf8* name, const utf8* valueStart, const utf8* valueEnd) { return WriteAttributeInt(name, valueStart, valueEnd); }
    void OutputStreamFormatter::WriteAttribute(const ucs2* name, const ucs2* valueStart, const ucs2* valueEnd) { return WriteAttributeInt(name, valueStart, valueEnd); }
    void OutputStreamFormatter::WriteAttribute(const ucs4* name, const ucs4* valueStart, const ucs4* valueEnd) { return WriteAttributeInt(name, valueStart, valueEnd); }

    OutputStreamFormatter::OutputStreamFormatter(OutputStream& stream) 
    : _stream(&stream)
    {
        _currentIndentLevel = 0;
        _hotLine = false;
    }

    OutputStreamFormatter::~OutputStreamFormatter()
    {}


    auto InputStreamFormatter::GetNext() const -> Blob { return Blob::None; }
    bool InputStreamFormatter::TryReadBeginElement(InteriorSection& name) const { return false; }
    bool InputStreamFormatter::TryReadAttribute(InteriorSection& name, InteriorSection& value) const { return false; }


    const utf8 FormatterConstants<utf8>::EndLine[] = { (utf8)'\r', (utf8)'\n', (utf8)'\0' };
    const ucs2 FormatterConstants<ucs2>::EndLine[] = { (ucs2)'\r', (ucs2)'\n', (ucs2)'\0' };
    const ucs4 FormatterConstants<ucs4>::EndLine[] = { (ucs4)'\r', (ucs4)'\n', (ucs4)'\0' };

    const utf8 FormatterConstants<utf8>::ElementPrefix[] = { (utf8)'-', (utf8)'-', (utf8)'(' };
    const ucs2 FormatterConstants<ucs2>::ElementPrefix[] = { (ucs2)'-', (ucs2)'-', (ucs2)'(' };
    const ucs4 FormatterConstants<ucs4>::ElementPrefix[] = { (ucs4)'-', (ucs4)'-', (ucs4)'(' };

    const utf8 FormatterConstants<utf8>::ElementPostfix[] = { (utf8)')', (utf8)'-', (utf8)'-' };
    const ucs2 FormatterConstants<ucs2>::ElementPostfix[] = { (ucs2)')', (ucs2)'-', (ucs2)'-' };
    const ucs4 FormatterConstants<ucs4>::ElementPostfix[] = { (ucs4)')', (ucs4)'-', (ucs4)'-' };

    const utf8 FormatterConstants<utf8>::AttributeNamePrefix[] = { (utf8)'=', (utf8)'=', (utf8)'(' };
    const ucs2 FormatterConstants<ucs2>::AttributeNamePrefix[] = { (ucs2)'=', (ucs2)'=', (ucs2)'(' };
    const ucs4 FormatterConstants<ucs4>::AttributeNamePrefix[] = { (ucs4)'=', (ucs4)'=', (ucs4)'(' };

    const utf8 FormatterConstants<utf8>::AttributeNamePostfix[] = { (utf8)')', (utf8)'=', (utf8)'=' };
    const ucs2 FormatterConstants<ucs2>::AttributeNamePostfix[] = { (ucs2)')', (ucs2)'=', (ucs2)'=' };
    const ucs4 FormatterConstants<ucs4>::AttributeNamePostfix[] = { (ucs4)')', (ucs4)'=', (ucs4)'=' };

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

