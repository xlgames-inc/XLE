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

typedef void ExceptionHandler(const ANTLR3_EXCEPTION* exception, const ANTLR3_UINT8**);
extern "C" ExceptionHandler* g_ShaderParserExceptionHandler;
extern "C" void* g_ShaderParserExceptionHandlerUserData;

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

    class ParserRig::Pimpl
    {
    public:
        AntlrPtr<struct ANTLR3_INPUT_STREAM_struct>         _inputStream;
        AntlrPtr<struct ShaderLexer_Ctx_struct>             _lxr;
        AntlrPtr<struct ANTLR3_COMMON_TOKEN_STREAM_struct>  _tokenStream;
        AntlrPtr<struct ShaderParser_Ctx_struct>            _psr;

        std::vector<Error> _errors;

        static void __cdecl HandleException(
            Pimpl* pimpl,
            const ANTLR3_EXCEPTION* exc,
            const ANTLR3_UINT8 ** tokenNames)
        {
            assert(pimpl && exc && tokenNames);

            Error error;

            // We have a problem with character types here. Since we're using
            // basic_stringstream, we can only use one of the built-in C++
            // character types!
            // Since antlr has support for different character types, ideally we
            // would like to have a bit more flexibility here (such as using UTF8
            // for the error message strings)
            using CharType = decltype(error._message)::value_type;
            ANTLR3_COMMON_TOKEN* token = (ANTLR3_COMMON_TOKEN*)exc->token;
            auto text = AsString<CharType>(token->getText(token));

            error._lineStart = error._lineEnd = exc->line;
            error._charStart = error._charEnd = exc->charPositionInLine;

            std::basic_stringstream<CharType> str;
            switch (exc->type) {
            case ANTLR3_UNWANTED_TOKEN_EXCEPTION:
                str << "Extraneous input - expected (" << tokenNames[exc->expecting] << ")";
                break;

            case ANTLR3_MISSING_TOKEN_EXCEPTION:
                str << "Missing (" << tokenNames[exc->expecting] << ")";
                break;

            case ANTLR3_RECOGNITION_EXCEPTION:
                str << "Syntax error";
                break;

            case ANTLR3_MISMATCHED_TOKEN_EXCEPTION:
                str << "Expected (" << tokenNames[exc->expecting] << ")";
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
    };

    ANTLR3_BASE_TREE* ParserRig::BuildAST()
    {
        auto* parser = _pimpl->_psr.get();

        // Antlr stuff is in 'C' -- so we have to drop back to a C way of doing things
        // these globals means we can only do a single parse at a time.
        auto* oldHandler = g_ShaderParserExceptionHandler;
        auto* oldHandlerUserData = g_ShaderParserExceptionHandlerUserData;
        auto cleanup = AutoCleanup(
            [oldHandler, oldHandlerUserData]() 
            {
                g_ShaderParserExceptionHandler = oldHandler;
                g_ShaderParserExceptionHandlerUserData = oldHandlerUserData;
            });

        _pimpl->_errors.clear();
        g_ShaderParserExceptionHandlerUserData = _pimpl.get();
        g_ShaderParserExceptionHandler = (ExceptionHandler*)&Pimpl::HandleException;

        auto result = parser->fx_file(parser).tree;
        if (!_pimpl->_errors.empty())
            Throw(Exceptions::ParsingFailure(MakeIteratorRange(_pimpl->_errors)));

        // if (psr.GetParser()->pParser->rec->state->errorCount > 0) {
        //     // LogWarning << "Tree: " << (const char*)abstractSyntaxTree.tree->toStringTree(abstractSyntaxTree.tree)->chars;
        //     // ConsoleRig::GetWarningStream().Flush();
        //     Throw(Exceptions::CompileError(
        //         XlDynFormatString(
        //             "The parser returned %d errors, tree walking aborted.\n", 
        //             psr.GetParser()->pParser->rec->state->errorCount).c_str()));
        // }

        return result;
    }

    ParserRig::ParserRig(const char sourceCode[], size_t sourceCodeLength)
    {
        _pimpl = std::make_unique<Pimpl>();

        _pimpl->_inputStream = antlr3StringStreamNew(
            (ANTLR3_UINT8*)sourceCode, ANTLR3_ENC_8BIT, 
            (unsigned)sourceCodeLength, (ANTLR3_UINT8*)"InputStream");
        if (!_pimpl->_inputStream)
			Throw(::Exceptions::BasicLabel("Unable to create the input stream due to malloc() failure\n"));

        _pimpl->_lxr = ShaderLexerNew(_pimpl->_inputStream);	    // CLexerNew is generated by ANTLR
        if (!_pimpl->_lxr)
			Throw(::Exceptions::BasicLabel("Unable to create the lexer due to malloc() failure\n"));

        _pimpl->_tokenStream = antlr3CommonTokenStreamSourceNew( ANTLR3_SIZE_HINT, TOKENSOURCE(_pimpl->_lxr));
        if (!_pimpl->_tokenStream)
			Throw(::Exceptions::BasicLabel("Out of memory trying to allocate token stream\n"));

        _pimpl->_psr = ShaderParserNew(_pimpl->_tokenStream);  // CParserNew is generated by ANTLR3
        if (!_pimpl->_psr)
			Throw(::Exceptions::BasicLabel("Out of memory trying to allocate parser\n"));
    }

    ParserRig::~ParserRig() {}


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
