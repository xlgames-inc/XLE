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
            ElementId BeginElement(StringSection<CharType> name);
        void EndElement(ElementId);

        template<typename CharType> 
            void WriteAttribute(
                StringSection<CharType> name,
                StringSection<CharType> value);
        
        void Flush();
        void NewLine();

        OutputStreamFormatter(OutputStream& stream);
        ~OutputStreamFormatter();

        ///////////////////////////////////////////////////////////////////////////////////
        //    Deprecated interface follows

        template<typename CharType> 
            DEPRECATED_ATTRIBUTE ElementId BeginElement(const CharType* nameStart, const CharType* nameEnd)
        {
            return BeginElement(StringSection<CharType>{nameStart, nameEnd});
        }

        template<typename CharType> 
            DEPRECATED_ATTRIBUTE void WriteAttribute(
                const CharType* nameStart, const CharType* nameEnd,
                const CharType* valueStart, const CharType* valueEnd)
        {
            WriteAttribute(StringSection<CharType>{nameStart, nameEnd}, StringSection<CharType>{valueStart, valueEnd});
        }

        template<typename CharType> 
            DEPRECATED_ATTRIBUTE ElementId BeginElement(const CharType* nameNullTerm)
            {
                return BeginElement(nameNullTerm, XlStringEnd(nameNullTerm));
            }

        template<typename CharType> 
            DEPRECATED_ATTRIBUTE ElementId BeginElement(const std::basic_string<CharType>& name)
            {
                return BeginElement(AsPointer(name.cbegin()), AsPointer(name.cend()));
            }

        template<typename CharType> 
            DEPRECATED_ATTRIBUTE void WriteAttribute(const CharType* nameNullTerm, const CharType* valueNullTerm)
            {
                WriteAttribute(
                    nameNullTerm, XlStringEnd(nameNullTerm),
                    valueNullTerm, XlStringEnd(valueNullTerm));
            }

        template<typename CharType> 
            DEPRECATED_ATTRIBUTE void WriteAttribute(const CharType* nameNullTerm, const std::basic_string<CharType>& value)
            {
                WriteAttribute(
                    nameNullTerm, XlStringEnd(nameNullTerm),
                    AsPointer(value.cbegin()), AsPointer(value.cend()));
            }

    protected:
        OutputStream*   _stream;
        unsigned        _currentIndentLevel;
		unsigned		_indentLevelAtStartOfLine;
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

    enum class FormatterBlob
    {
        MappedItem,
        Value,
        BeginElement,
        EndElement,
        CharacterData,
        None 
    };

    template<typename CharType=char>
        class XL_UTILITY_API InputStreamFormatter
    {
    public:
        FormatterBlob PeekNext();

        bool TryBeginElement();
        bool TryEndElement();
        bool TryMappedItem(StringSection<CharType>& name);
        bool TryValue(StringSection<CharType>& value);
		bool TryCharacterData(StringSection<CharType>&);

        StreamLocation GetLocation() const;

        using value_type = CharType;
        using InteriorSection = StringSection<CharType>;
        using Blob = FormatterBlob;

        InputStreamFormatter(const MemoryMappedInputStream& stream);
		InputStreamFormatter(StringSection<CharType> inputData) : InputStreamFormatter(MemoryMappedInputStream{inputData.begin(), inputData.end()}) {}
        ~InputStreamFormatter();

		InputStreamFormatter();
		InputStreamFormatter(const InputStreamFormatter& cloneFrom);
		InputStreamFormatter& operator=(const InputStreamFormatter& cloneFrom);
    protected:
        MemoryMappedInputStream _stream;
        FormatterBlob _primed;
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

	namespace Internal
    {
        template<typename T> struct HasSerializeMethod
        {
            template<typename U, void (U::*)(OutputStreamFormatter&) const> struct FunctionSignature {};
            template<typename U> static std::true_type Test1(FunctionSignature<U, &U::SerializeMethod>*);
            template<typename U> static std::false_type Test1(...);
            static const bool Result = decltype(Test1<T>(0))::value;
        };
	}

	template<typename Type, typename std::enable_if<Internal::HasSerializeMethod<Type>::Result>::type* =nullptr>
		inline void SerializationOperator(OutputStreamFormatter& formatter, const Type& input)
	{
		input.SerializeMethod(formatter);
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Formatter>
        void SkipElement(Formatter& formatter)
    {
        unsigned subtreeEle = 0;
        typename Formatter::InteriorSection dummy0;
        for (;;) {
            switch(formatter.PeekNext()) {
            case FormatterBlob::BeginElement:
                if (!formatter.TryBeginElement())
                    Throw(FormatException(
                        "Malformed begin element while skipping forward", formatter.GetLocation()));
                ++subtreeEle;
                break;

            case FormatterBlob::EndElement:
                if (!subtreeEle) return;    // end now, while the EndElement is primed

                if (!formatter.TryEndElement())
                    Throw(FormatException(
                        "Malformed end element while skipping forward", formatter.GetLocation()));
                --subtreeEle;
                break;

            case FormatterBlob::MappedItem:
                if (!formatter.TryMappedItem(dummy0))
                    Throw(FormatException(
                        "Malformed mapped item while skipping forward", formatter.GetLocation()));
                break;

            case FormatterBlob::Value:
                if (!formatter.TryValue(dummy0))
                    Throw(FormatException(
                        "Malformed mapped item while skipping forward", formatter.GetLocation()));
                break;

            default:
                Throw(FormatException(
                    "Unexpected blob or end of stream hit while skipping forward", formatter.GetLocation()));
            }
        }
    }

    template<typename Formatter>
        void RequireBeginElement(Formatter& formatter)
    {
        if (!formatter.TryBeginElement())
            Throw(Utility::FormatException("Expecting begin element", formatter.GetLocation()));
    }

    template<typename Formatter>
        void RequireEndElement(Formatter& formatter)
    {
        if (!formatter.TryEndElement())
            Throw(Utility::FormatException("Expecting end element", formatter.GetLocation()));
    }

    template<typename Formatter>
        typename Formatter::InteriorSection RequireMappedItem(Formatter& formatter)
    {
        typename Formatter::InteriorSection name;
        if (!formatter.TryMappedItem(name))
            Throw(Utility::FormatException("Expecting mapped item", formatter.GetLocation()));
        return name;
    }

    template<typename Formatter>
        typename Formatter::InteriorSection RequireValue(Formatter& formatter)
    {
        typename Formatter::InteriorSection value;
        if (!formatter.TryValue(value))
            Throw(Utility::FormatException("Expecting value", formatter.GetLocation()));
        return value;
    }
}

using namespace Utility;
