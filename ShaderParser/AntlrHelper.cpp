// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Core/Exceptions.h"
#include "Exceptions.h"
#include "AntlrHelper.h"
#include "Grammar/ShaderLexer.h"
#include "Grammar/ShaderParser.h"
#include "../Utility/FunctionUtils.h"
#include "../Utility/Conversion.h"
#include <sstream>

namespace ShaderSourceParser { namespace AntlrHelper
{

    namespace Internal
    {
        template<> void DestroyAntlrObject<>(struct ANTLR3_INPUT_STREAM_struct* object)
        {
            if (object) object->close(object);
        }
    }

    pANTLR3_BASE_TREE GetChild(pANTLR3_BASE_TREE node, ANTLR3_UINT32 childIndex)
    {
        return pANTLR3_BASE_TREE(node->getChild(node, childIndex));
    }

	unsigned GetChildCount(pANTLR3_BASE_TREE node)
	{
		return node->getChildCount(node);
	}

    pANTLR3_COMMON_TOKEN GetToken(pANTLR3_BASE_TREE node)
    {
        return node->getToken(node);
    }

    ANTLR3_UINT32 GetType(pANTLR3_COMMON_TOKEN token)
    {
        return token->getType(token);
    }

    template<typename CharType>
        std::basic_string<CharType> AsString(ANTLR3_STRING* antlrString)
    {
        auto tempString = antlrString->toUTF8(antlrString);
        auto result = std::basic_string<utf8>((utf8*)tempString->chars);
        tempString->factory->destroy(tempString->factory, tempString);
        return Conversion::Convert<std::basic_string<CharType>>(result);
    }

	void Description(std::ostream& str, pANTLR3_COMMON_TOKEN token)
	{
		ANTLR3_STRING* strng = token->toString(token);
		str << AsString<char>(strng);
		strng->factory->destroy(strng->factory, strng);
	}

	void StructureDescription(std::ostream& str, pANTLR3_BASE_TREE node, unsigned indent)
	{
		auto indentBuffer = std::make_unique<char[]>(indent+1);
		std::fill(indentBuffer.get(), &indentBuffer[indent], ' ');
		indentBuffer[indent] = '\0';

		str << indentBuffer.get();
		auto* token = GetToken(node);
		if (token) {
			Description(str, token);
		} else {
			str << "<<no token>>";
		}
		str << std::endl;

		auto childCount = GetChildCount(node);
		for (unsigned c=0; c<childCount; ++c)
			StructureDescription(str, GetChild(node, c), indent+4);
	}

	void __cdecl ExceptionSet::HandleException(
        ExceptionSet* pimpl,
        const ANTLR3_EXCEPTION* exc,
        const ANTLR3_UINT8 ** tokenNames)
    {
        assert(pimpl && exc);

        Error error;

        // We have a problem with character types here. Since we're using
        // basic_stringstream, we can only use one of the built-in C++
        // character types!
        // Since antlr has support for different character types, ideally we
        // would like to have a bit more flexibility here (such as using UTF8
        // for the error message strings)
        using CharType = decltype(error._message)::value_type;
        ANTLR3_COMMON_TOKEN* token = (ANTLR3_COMMON_TOKEN*)exc->token;
        std::string text;
		if (token) 
			text = AsString<CharType>(token->getText(token));

        error._lineStart = error._lineEnd = exc->line;
        error._charStart = error._charEnd = exc->charPositionInLine;

        std::basic_stringstream<CharType> str;
        switch (exc->type) {
        case ANTLR3_UNWANTED_TOKEN_EXCEPTION:
            str << "Extraneous input - expected (" << (tokenNames ? tokenNames[exc->expecting] : (const ANTLR3_UINT8*)"<<unknown>>") << ")";
            break;

        case ANTLR3_MISSING_TOKEN_EXCEPTION:
            str << "Missing (" << (tokenNames ? tokenNames[exc->expecting] : (const ANTLR3_UINT8*)"<<unknown>>") << ")";
            break;

        case ANTLR3_RECOGNITION_EXCEPTION:
            str << "Syntax error";
            break;

        case ANTLR3_MISMATCHED_TOKEN_EXCEPTION:
            str << "Expected (" << (tokenNames ? tokenNames[exc->expecting] : (const ANTLR3_UINT8*)"<<unknown>>") << ")";
            break;

        case ANTLR3_NO_VIABLE_ALT_EXCEPTION: str << "No viable alternative"; break;
        case ANTLR3_MISMATCHED_SET_EXCEPTION: str << "Mismatched set"; break;
        case ANTLR3_EARLY_EXIT_EXCEPTION: str << "Early exit exception"; break;
        default: str << "Syntax not recognized"; break;
        }

        // assert(recognizer->type == ANTLR3_TYPE_PARSER);
        str << ". Near token: (" << text << ")";

        str << ". Msg: " << (const char*)exc->message << " (" << exc->type << ")";

        error._message = str.str();

        pimpl->_errors.push_back(error);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	ExceptionContext::ExceptionContext()
	{
		// Antlr stuff is in 'C' -- so we have to drop back to a C way of doing things
        // these globals means we can only do a single parse at a time.
		_previousExceptionHandler = g_ShaderParserExceptionHandler;
        _previousExceptionHandlerUserData = g_ShaderParserExceptionHandlerUserData;

		g_ShaderParserExceptionHandlerUserData = &_exceptions;
        g_ShaderParserExceptionHandler = (ExceptionHandler*)&ExceptionSet::HandleException;
	}
	
	ExceptionContext::~ExceptionContext()
	{
		g_ShaderParserExceptionHandler = _previousExceptionHandler;
        g_ShaderParserExceptionHandlerUserData = _previousExceptionHandlerUserData;
	}

}}


namespace ShaderSourceParser
{
    namespace Exceptions
    {
        ParsingFailure::ParsingFailure(IteratorRange<Error*> errors) never_throws
        : _errors(errors.begin(), errors.end())
        {}
        ParsingFailure::~ParsingFailure() {}

        const char* ParsingFailure::what() const
        {
            return "Parsing failure";
        }
    }
}
