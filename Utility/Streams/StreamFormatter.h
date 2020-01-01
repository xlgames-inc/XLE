// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Detail/API.h"
#include "../UTFUtils.h"
#include "../StringUtils.h"
#include "../PtrUtils.h"
#include "../IteratorUtils.h"
#include "../../Core/Exceptions.h"
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
                return BeginElement(nameNullTerm, XlStringEnd(nameNullTerm));
            }

        template<typename CharType> 
            ElementId BeginElement(const std::basic_string<CharType>& name)
            {
                return BeginElement(AsPointer(name.cbegin()), AsPointer(name.cend()));
            }

        template<typename CharType> 
            void WriteAttribute(const CharType* nameNullTerm, const CharType* valueNullTerm)
            {
                WriteAttribute(
                    nameNullTerm, XlStringEnd(nameNullTerm),
                    valueNullTerm, XlStringEnd(valueNullTerm));
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
        bool            _pendingHeader;

        #if defined(STREAM_FORMATTER_CHECK_ELEMENTS)
            std::vector<ElementId> _elementStack;
            unsigned _nextElementId;
        #endif

        template<typename CharType> void DoNewLine();
    };

    class MemoryMappedInputStream
    {
    public:
        const void* ReadPointer() const { return _ptr; }
        ptrdiff_t RemainingBytes() const { return ptrdiff_t(_end) - ptrdiff_t(_ptr); }
        void AdvancePointer(ptrdiff_t offset);
        void SetPointer(const void* newPtr);
        const void* Start() const { return _start; }
        const void* End() const { return _end; }

        MemoryMappedInputStream(const void* start, const void* end);
        ~MemoryMappedInputStream();

		template<typename Type>
			MemoryMappedInputStream(IteratorRange<Type*> range)
				: MemoryMappedInputStream(range.begin(), range.end()) {}

		MemoryMappedInputStream(const char* nullTerminatedStr);
    protected:
        const void* _start;
        const void* _end;
        const void* _ptr;
    };

    class StreamLocation { public: unsigned _charIndex, _lineIndex; };
    class FormatException : public ::Exceptions::BasicLabel
    {
    public:
        FormatException(const char message[], StreamLocation location);
    };

    template<typename CharType=char>
        class XL_UTILITY_API InputStreamFormatter
    {
    public:
        enum class Blob 
        {
            BeginElement, EndElement, 
            AttributeName, AttributeValue, 
			CharacterData,
			None 
        };
        Blob PeekNext();

        using InteriorSection = StringSection<CharType>;

        bool TryBeginElement(InteriorSection& name);
        bool TryEndElement();
        bool TryAttribute(InteriorSection& name, InteriorSection& value);
		bool TryCharacterData(InteriorSection&);

        void SkipElement();

        StreamLocation GetLocation() const;

        using value_type = CharType;

        InputStreamFormatter(const MemoryMappedInputStream& stream);
		InputStreamFormatter(StringSection<CharType> inputData) : InputStreamFormatter(MemoryMappedInputStream{inputData.begin(), inputData.end()}) {}
        ~InputStreamFormatter();

		InputStreamFormatter();
		InputStreamFormatter(const InputStreamFormatter& cloneFrom);
		InputStreamFormatter& operator=(const InputStreamFormatter& cloneFrom);
    protected:
        MemoryMappedInputStream _stream;
        Blob _primed;
        signed _activeLineSpaces;
        signed _parentBaseLine;

        signed _baseLineStack[32];
        unsigned _baseLineStackPtr;

        unsigned _lineIndex;
        const void* _lineStart;

        bool _protectedStringMode;

        unsigned _format;
        unsigned _tabWidth;
        bool _pendingHeader;

        void ReadHeader();
    };


    inline void MemoryMappedInputStream::AdvancePointer(ptrdiff_t offset) 
    { 
        assert(PtrAdd(_ptr, offset) <= _end && PtrAdd(_ptr, offset) >= _start);
        _ptr = PtrAdd(_ptr, offset);
    }

    inline void MemoryMappedInputStream::SetPointer(const void* newPtr)
    {
        assert(newPtr <= _end && newPtr >= _start);
        _ptr = newPtr;
    }
}

using namespace Utility;
