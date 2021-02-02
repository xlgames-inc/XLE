// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Detail/API.h"
#include "../UTFUtils.h"
#include "../StringUtils.h"
#include <vector>
#include <assert.h>

namespace Utility
{
	class OutputStream;

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
}
