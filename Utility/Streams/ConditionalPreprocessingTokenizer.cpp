// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ConditionalPreprocessingTokenizer.h"
#include "../StringFormat.h"

namespace Utility
{
	static bool IsIdentifierOrNumericChar(char c)
    {
        return  (c >= 'a' && c <= 'z')
            ||  (c >= 'A' && c <= 'Z')
            ||  c == '_'
            ||  (c >= '0' && c <= '9')
            ;
    }

    void ConditionalProcessingTokenizer::SkipWhitespace()
    {
        while (_input.begin() != _input.end()) {
            switch (*_input.begin()) {
            case '/':
                if ((_input.begin()+1) != _input.end() && *(_input.begin()+1) == '/') {
                    // read upto, but don't swallow the next newline (line extension not supported for these comments)
                    _input._start += 2;
                    ReadUntilEndOfLine();
                    break;
                } else
                    return;

            case ' ':
            case '\t':
                ++_input._start;
                break;

            case '\r':  // (could be an independant new line, or /r/n combo)
                ++_input._start;
                if (_input.begin() != _input.end() &&  *_input.begin() == '\n')
                    ++_input._start;
                ++_lineIndex; _lineStart = _input.begin();
                _preprocValid = true;
                break;

            case '\n':  // (independant new line. A following /r will be treated as another new line)
                ++_input._start;
                ++_lineIndex; _lineStart = _input.begin();
                _preprocValid = true;
                break;

            case '#':
                if (!_preprocValid)
                    return;

                ++_input._start;
                ParsePreprocessorDirective();
                break;

            default:
                // something other than whitespace; time to get out
                // (we also get here on a newline char)
                return;
            }
        }
    }

    auto ConditionalProcessingTokenizer::GetNextToken() -> Token
    {
        SkipWhitespace();

        auto startLocation = GetLocation();

        auto startOfToken = _input.begin();
        if (_input.begin() != _input.end()) {
            if (IsIdentifierOrNumericChar(*_input.begin())) {
                while ((_input.begin() != _input.end()) && IsIdentifierOrNumericChar(*_input.begin()))
                    ++_input._start;
            } else {
                ++_input._start; // non identifier tokens are always single chars (ie, for some kind of operator)
            }
            _preprocValid = false;      // can't start a preprocessor directive if there is any non-whitespace content prior on the same line
        }

        return Token { { startOfToken, _input.begin() }, startLocation, GetLocation() };
    }

    auto ConditionalProcessingTokenizer::PeekNextToken() -> Token
    {
        // We do the destructive "SkipWhitespace" -- including parsing
        // any preprocessor directives before the next token, etc. This is a little more
        // efficient because PeekNextToken is usually followed by a GetNextToken, so we avoid having
        // to do this twice.
        SkipWhitespace();

        auto startLocation = GetLocation();

        auto startOfToken = _input.begin();
        auto iterator = startOfToken;
        if (iterator != _input.end()) {
            if (IsIdentifierOrNumericChar(*iterator)) {
                while ((iterator != _input.end()) && IsIdentifierOrNumericChar(*iterator))
                    ++iterator;
            } else {
                ++iterator; // non identifier tokens are always single chars (ie, for some kind of operator)
            }
        }

        auto endLocation = startLocation;
        endLocation._charIndex += unsigned(iterator-startOfToken);
        return Token { { startOfToken, iterator }, startLocation, endLocation };
    }

    void ConditionalProcessingTokenizer::PreProc_SkipWhitespace()
    {
        while (_input.begin() != _input.end()) {
            switch (*_input.begin()) {
            case '/':
                if ((_input.begin()+1) != _input.end() && *(_input.begin()+1) == '/') {
                    // read upto, but don't swallow the next newline (line extension not supported for these comments)s
                    _input._start += 2;
                    ReadUntilEndOfLine();
                }
                return;

            case ' ':
            case '\t':
                ++_input._start;
                continue;       // back to top of the loop

            default:
                // something other than whitespace; time to get out
                // (we also get here on a newline char)
                return;
            }
        }

    }

    auto ConditionalProcessingTokenizer::PreProc_GetNextToken() -> Token
    {
        // we need a special implementation for this for reading the preprocessor tokens themselves,
        // because the rules are slightly different (for example, we don't want to start parsing
        // preprocessor tokens recursively)
        PreProc_SkipWhitespace();

        auto startLocation = GetLocation();

        auto startOfToken = _input.begin();
        if (_input.begin() != _input.end()) {
            if (IsIdentifierOrNumericChar(*_input.begin())) {
                while ((_input.begin() != _input.end()) && IsIdentifierOrNumericChar(*_input.begin()))
                    ++_input._start;
            } else {
                if (*_input.begin() == '\r' || *_input.begin() == '\n')
                    return {};      // hit newline, no more tokens
                ++_input._start; // non identifier tokens are always single chars (ie, for some kind of operator)
            }
        }

        return Token { { startOfToken, _input.begin() }, startLocation, GetLocation() };
    }

    void ConditionalProcessingTokenizer::ParsePreprocessorDirective()
    {
        //
        // There are only a few options here, after the hash:
        //      1) define, undef, include, line, error, pragma
        //      2) if, ifdef, ifndef
        //      3) elif, else, endif
        //
        //  (not case sensitive)
        //

        auto directive = PreProc_GetNextToken();
        if (    XlEqStringI(directive._value, "define") || XlEqStringI(directive._value, "undef") || XlEqStringI(directive._value, "include")
            ||  XlEqStringI(directive._value, "line") || XlEqStringI(directive._value, "error") || XlEqStringI(directive._value, "pragma")) {

            Throw(FormatException(StringMeld<256>() << "Unexpected preprocessor directive: " << directive._value << ". This directive is not supported.", directive._start));

        } else if (XlEqStringI(directive._value, "if")) {

            // the rest of the line should be some preprocessor condition expression
            PreProc_SkipWhitespace();
            auto remainingLine = ReadUntilEndOfLine();
            _preprocessorContext._conditionsStack.push_back({remainingLine._value.AsString()});

        } else if (XlEqStringI(directive._value, "ifdef")) {

            auto symbol = PreProc_GetNextToken();
            if (symbol._value.IsEmpty())
                Throw(FormatException("Expected token in #ifdef", directive._start));

            _preprocessorContext._conditionsStack.push_back({"defined(" + symbol._value.AsString() + ")"});

        } else if (XlEqStringI(directive._value, "ifndef")) {

            auto symbol = PreProc_GetNextToken();
            if (symbol._value.IsEmpty())
                Throw(FormatException("Expected token in #ifndef", directive._start));

            _preprocessorContext._conditionsStack.push_back({"!defined(" + symbol._value.AsString() + ")"});

        } else if (XlEqStringI(directive._value, "elif") || XlEqStringI(directive._value, "else") || XlEqStringI(directive._value, "endif")) {

            if (_preprocessorContext._conditionsStack.empty())
                Throw(FormatException(
                    StringMeld<256>() << "endif/else/elif when there has been no if",
                    directive._start));

            auto prevCondition = *(_preprocessorContext._conditionsStack.end()-1);
            _preprocessorContext._conditionsStack.erase(_preprocessorContext._conditionsStack.end()-1);

            if (XlEqStringI(directive._value, "elif")) {
                auto negCondition = prevCondition._positiveCond;
                if (!prevCondition._negativeCond.empty())
                    negCondition = "(" + negCondition + ") && (" + prevCondition._negativeCond +  ")";
                _preprocessorContext._conditionsStack.push_back({Remaining().AsString(), negCondition});
            } else if (XlEqStringI(directive._value, "else")) {
                auto negCondition = prevCondition._positiveCond;
                if (!prevCondition._negativeCond.empty())
                    negCondition = "(" + negCondition + ") && (" + prevCondition._negativeCond +  ")";
                _preprocessorContext._conditionsStack.push_back({"1", prevCondition._positiveCond});
            }

        } else {

            Throw(FormatException(
                StringMeld<256>() << "Unknown preprocessor directive: " << directive._value,
                directive._start));

        }

    }

    auto ConditionalProcessingTokenizer::ReadUntilEndOfLine() -> Token
    {
        auto startLocation = GetLocation();
        auto startOfToken = _input.begin();

        while (_input.begin() != _input.end() && *_input.begin() != '\r' && *_input.begin() != '\n') {
            ++_input._start;
        }

        return Token { { startOfToken, _input.begin() }, startLocation, GetLocation() };
    }

    StreamLocation ConditionalProcessingTokenizer::GetLocation() const
    {
        StreamLocation result;
        result._charIndex = 1 + unsigned(size_t(_input.begin()) - size_t(_lineStart));
        result._lineIndex = 1 + _lineIndex;
        return result;
    }

    ConditionalProcessingTokenizer::ConditionalProcessingTokenizer(
        StringSection<> input,
        unsigned lineIndex, const void* lineStart)
    {
        _input = input;
        _lineIndex = lineIndex;
        _lineStart = lineStart;
        if (!lineStart)
            _lineStart = input.begin();
    }

    ConditionalProcessingTokenizer::~ConditionalProcessingTokenizer()
    {
    }

    auto ConditionalProcessingTokenizer::PreprocessorParseContext::GetCurrentConditionString() -> Expression
    {
        Expression result;
        for (auto i=_conditionsStack.rbegin(); i!=_conditionsStack.rend(); ++i) {
            if (!i->_negativeCond.empty()) {
                if (!result.empty()) result += " && ";
                result += "!(" + i->_negativeCond + ")";
            }
            if (!i->_positiveCond.empty()) {
                if (!result.empty()) result += " && ";
                result += "(" + i->_positiveCond + ")";
            }
        }
        return result;
    }

    ConditionalProcessingTokenizer::PreprocessorParseContext::PreprocessorParseContext() {}
    ConditionalProcessingTokenizer::PreprocessorParseContext::~PreprocessorParseContext() {}
}
