// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ConditionalPreprocessingTokenizer.h"
#include "../StringFormat.h"
#include <iostream>

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
        // Note -- not supporting block comments or line extensions
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
        // Note -- not supporting block comments or line extensions
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

            ReadUntilEndOfLine();       // skip rest of line

        } else if (XlEqStringI(directive._value, "ifndef")) {

            auto symbol = PreProc_GetNextToken();
            if (symbol._value.IsEmpty())
                Throw(FormatException("Expected token in #ifndef", directive._start));

            _preprocessorContext._conditionsStack.push_back({"!defined(" + symbol._value.AsString() + ")"});

            ReadUntilEndOfLine();       // skip rest of line

        } else if (XlEqStringI(directive._value, "elif") || XlEqStringI(directive._value, "else") || XlEqStringI(directive._value, "endif")) {

            if (_preprocessorContext._conditionsStack.empty())
                Throw(FormatException(
                    StringMeld<256>() << "endif/else/elif when there has been no if",
                    directive._start));

            auto prevCondition = *(_preprocessorContext._conditionsStack.end()-1);
            _preprocessorContext._conditionsStack.erase(_preprocessorContext._conditionsStack.end()-1);

            auto negCondition = prevCondition._positiveCond;
            if (!prevCondition._negativeCond.empty())
                negCondition = "(" + negCondition + ") && (" + prevCondition._negativeCond +  ")";

            if (XlEqStringI(directive._value, "elif")) {
                auto remainingLine = ReadUntilEndOfLine();
                _preprocessorContext._conditionsStack.push_back({remainingLine._value.AsString(), negCondition});
            } else if (XlEqStringI(directive._value, "else")) {
                _preprocessorContext._conditionsStack.push_back({"1", negCondition});

                ReadUntilEndOfLine();       // skip rest of line
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

    struct ParserHelper
    {
        StringSection<>     _input;
        unsigned            _lineIndex;
        const void*         _lineStart;

        void SkipUntilNextPreproc()
        {
            bool preprocValid = true;
            // Note -- not supporting block comments or line extensions
            while (_input.begin() != _input.end()) {
                switch (*_input.begin()) {
                case '/':
                    if ((_input.begin()+1) != _input.end() && *(_input.begin()+1) == '/') {
                        // read upto, but don't swallow the next newline (line extension not supported for these comments)
                        _input._start += 2;
                        ReadUntilEndOfLine();
                        break;
                    } else {
                        ++_input._start;
                        break;
                    }

                case '\r':  // (could be an independant new line, or /r/n combo)
                    ++_input._start;
                    if (_input.begin() != _input.end() &&  *_input.begin() == '\n')
                        ++_input._start;
                    ++_lineIndex; _lineStart = _input.begin();
                    preprocValid = true;
                    break;

                case '\n':  // (independant new line. A following /r will be treated as another new line)
                    ++_input._start;
                    ++_lineIndex; _lineStart = _input.begin();
                    preprocValid = true;
                    break;

                case '#':
                    if (preprocValid) {
                        ++_input._start;
                        return;
                    }
                    // else intentional fall-through
                default:
                    if (*_input.begin() != '\t' && *_input.begin() != ' ')
                        preprocValid = false;
                    ++_input._start;
                    break;
                }
            }
        }

        struct Token
        {
            StringSection<> _value;
            StreamLocation _start, _end;
        };

        StreamLocation GetLocation() const
        {
            StreamLocation result;
            result._charIndex = 1 + unsigned(size_t(_input.begin()) - size_t(_lineStart));
            result._lineIndex = 1 + _lineIndex;
            return result;
        }

        auto ReadUntilEndOfLine() -> Token
        {
            auto startLocation = GetLocation();
            auto startOfToken = _input.begin();

            while (_input.begin() != _input.end() && *_input.begin() != '\r' && *_input.begin() != '\n') {
                // Also terminate if we hit a "//" style comment
                if (*_input._start == '/' && (_input.begin()+1) != _input.end() && *(_input.begin()+1) == '/')
                    break;
                ++_input._start;
            }

            return Token { { startOfToken, _input.begin() }, startLocation, GetLocation() };
        }

        void PreProc_SkipWhitespace()
        {
            // Note -- not supporting block comments or line extensions
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

        auto PreProc_GetNextToken() -> Token
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

        auto PreProc_GetBrackettedToken() -> Token
        {
            // we need a special implementation for this for reading the preprocessor tokens themselves,
            // because the rules are slightly different (for example, we don't want to start parsing
            // preprocessor tokens recursively)
            PreProc_SkipWhitespace();

            auto startLocation = GetLocation();
            if (_input.begin() == _input.end())
                return Token { { _input.begin(), _input.begin() }, startLocation, GetLocation() };

            char expectingTerminator = '"';
            if (*_input.begin() == '"') {
                ++_input._start;
            } else if (*_input.begin() == '<') {
                expectingTerminator = '>';
                ++_input._start;
            } else {
                return Token { { _input.begin(), _input.begin() }, startLocation, GetLocation() };
            }

            auto startOfToken = _input.begin();
            for (;;) {
                if (_input.begin() == _input.end() || *_input.begin() == '\r' || *_input.begin() == '\n' || *_input.begin() == expectingTerminator)
                    break;
                ++_input._start;
            }

            auto endOfToken = _input.begin();
            if (_input.begin() != _input.end() && *_input.begin() == expectingTerminator)
                ++_input._start;

            return Token { { startOfToken, endOfToken }, startLocation, GetLocation() };
        }
    };

    struct Cond
    {
        Internal::ExpressionTokenList _positiveCond;
        Internal::ExpressionTokenList _negativeCond;
    };

    Internal::ExpressionTokenList GetCurrentCondition(IteratorRange<const Cond*> conditionStack)
    {
        Internal::ExpressionTokenList result;
        result.push_back(1);    // start with "true"

        for (const auto&c:conditionStack) {
            // The "_negativeCond" must be false and the _positiveCond must be true
            if (!c._negativeCond.empty())
                result = Internal::AndNotExpression(result, c._negativeCond);
            if (!c._positiveCond.empty())
                result = Internal::AndExpression(result, c._positiveCond);
        }

        return result;
    }
    
    PreprocessorAnalysis GeneratePreprocessorAnalysis(StringSection<> input, StringSection<> filenameForRelativeIncludeSearch, IPreprocessorIncludeHandler& includeHandler)
    {
        // Walk through the input string, extracting all preprocessor operations
        // We need to consider "//" comments, but we don't support block comments or line extensions

        ParserHelper helper;
        helper._input = input;
        helper._lineIndex = 0;
        helper._lineStart = input.begin();

        Internal::TokenDictionary tokenDictionary;
        std::vector<Cond> conditionsStack;

        Internal::WorkingRelevanceTable relevanceTable;
        Internal::PreprocessorSubstitutions activeSubstitutions;

        while (true) {
            helper.SkipUntilNextPreproc();
            auto directive = helper.PreProc_GetNextToken();
            if (directive._value.IsEmpty()) break;

            if (XlEqStringI(directive._value, "if")) {

                // the rest of the line should be some preprocessor condition expression
                helper.PreProc_SkipWhitespace();
                auto remainingLine = helper.ReadUntilEndOfLine();

                auto expr = Internal::AsExpressionTokenList(
                    tokenDictionary, remainingLine._value, activeSubstitutions);

                auto exprRelevance = CalculatePreprocessorExpressionRevelance(
                    tokenDictionary, expr);

                relevanceTable = Internal::MergeRelevanceTables(
                    relevanceTable, {},
                    exprRelevance, GetCurrentCondition(conditionsStack));

                conditionsStack.push_back({expr});

            } else if (XlEqStringI(directive._value, "ifdef")) {

                auto symbol = helper.PreProc_GetNextToken();
                if (symbol._value.IsEmpty())
                    Throw(FormatException("Expected token in #ifdef", directive._start));

                Internal::ExpressionTokenList expr;
                tokenDictionary.PushBack(expr, Internal::TokenDictionary::TokenType::IsDefinedTest, symbol._value.AsString());

                auto exprRelevance = CalculatePreprocessorExpressionRevelance(
                    tokenDictionary, expr);

                relevanceTable = Internal::MergeRelevanceTables(
                    relevanceTable, {},
                    exprRelevance, GetCurrentCondition(conditionsStack));

                conditionsStack.push_back({expr});
                helper.ReadUntilEndOfLine();       // skip rest of line

            } else if (XlEqStringI(directive._value, "ifndef")) {

                auto symbol = helper.PreProc_GetNextToken();
                if (symbol._value.IsEmpty())
                    Throw(FormatException("Expected token in #ifndef", directive._start));

                Internal::ExpressionTokenList expr;
                tokenDictionary.PushBack(expr, Internal::TokenDictionary::TokenType::IsDefinedTest, symbol._value.AsString());
                expr = Internal::InvertExpression(expr);

                auto exprRelevance = CalculatePreprocessorExpressionRevelance(
                    tokenDictionary, expr);

                relevanceTable = Internal::MergeRelevanceTables(
                    relevanceTable, {},
                    exprRelevance, GetCurrentCondition(conditionsStack));

                conditionsStack.push_back({expr});
                helper.ReadUntilEndOfLine();       // skip rest of line

            } else if (XlEqStringI(directive._value, "elif") || XlEqStringI(directive._value, "else") || XlEqStringI(directive._value, "endif")) {

                if (conditionsStack.empty())
                    Throw(FormatException(
                        StringMeld<256>() << "endif/else/elif when there has been no if",
                        directive._start));

                // Since we're at the same layer of nesting as the #if that we're trailing,
                // we wil actually modify the top condition on the stack, rather than pushing
                // and going to a deaper level
                auto prevCondition = *(conditionsStack.end()-1);
                conditionsStack.erase(conditionsStack.end()-1);

                // The "_negativeCond" must be false for this section to be value
                auto negCondition = Internal::AndExpression(prevCondition._positiveCond, prevCondition._negativeCond);

                if (XlEqStringI(directive._value, "elif")) {

                    auto remainingLine = helper.ReadUntilEndOfLine();

                    auto expr = Internal::AsExpressionTokenList(
                        tokenDictionary, remainingLine._value, activeSubstitutions);

                    auto exprRelevance = CalculatePreprocessorExpressionRevelance(
                        tokenDictionary, expr);

                    auto conditions = Internal::AndNotExpression(GetCurrentCondition(conditionsStack), negCondition);
                    relevanceTable = Internal::MergeRelevanceTables(
                        relevanceTable, {},
                        exprRelevance, conditions);

                    conditionsStack.push_back({expr, negCondition});
                } else if (XlEqStringI(directive._value, "else")) {
                    conditionsStack.push_back({{}, negCondition});
                    helper.ReadUntilEndOfLine();       // skip rest of line
                }

            } else if (XlEqStringI(directive._value, "define") || XlEqStringI(directive._value, "undef")) {

                auto symbol = helper.PreProc_GetNextToken();
                if (symbol._value.IsEmpty())
                    Throw(FormatException("Expected token in #define", directive._start));

                auto remainingLine = helper.ReadUntilEndOfLine();
                bool foundNonWhitespaceChar = false;
                for (auto c:remainingLine._value)
                    if (c!=' ' && c!='\t') {
                        foundNonWhitespaceChar = true;
                        break;
                    }

                bool unconditionalSet = true;
                bool gotNotDefinedCheck = false;
                for (const auto& condition:conditionsStack) {
                    if (!condition._negativeCond.empty()) {
                        unconditionalSet = false;
                        break;
                    }
                    // We're looking for "just true" or "just !defined(X)"
                    if (condition._positiveCond.size() == 1) {
                        if (condition._positiveCond[0] != 1) {
                            unconditionalSet = false;
                            break;
                        }
                    } else if (condition._positiveCond.size() == 3) {
                        auto& token0 = tokenDictionary._tokenDefinitions[condition._positiveCond[0]];
                        auto& token1 = tokenDictionary._tokenDefinitions[condition._positiveCond[1]];
                        auto& token2 = tokenDictionary._tokenDefinitions[condition._positiveCond[2]];
                        bool matchingUndefinedCheck =
                            token0._type == Internal::TokenDictionary::TokenType::UnaryMarker
                            && token1._type == Internal::TokenDictionary::TokenType::IsDefinedTest && XlEqString(symbol._value, token1._value)
                            && token2._type == Internal::TokenDictionary::TokenType::Operation && XlEqString(token2._value, "!");
                        if (!matchingUndefinedCheck) {
                            unconditionalSet = false;
                            break;
                        }
                        gotNotDefinedCheck = true;
                    } else {
                        break;
                    }
                }

                if (unconditionalSet) {
                    // If we're defining something with no value, and the only condition on the stack is a check to see
                    // if that same symbol defined; let's assume this is a header guard type pattern, and just wipe out
                    // the condition on the stack
                    auto headerGuardDetection = !foundNonWhitespaceChar && gotNotDefinedCheck && conditionsStack.size()==1 && XlEqStringI(directive._value, "define");
                    if (!headerGuardDetection) {
                        // It could be an unconditional substitution, or it could be set in different ways
                        // depending on the conditions on the stack.
                        // Conditional sets add some complexity -- we can't actually think of it as just a straight
                        // substitution anymore; but instead just something that pulls in an extra relevance table
                        if (XlEqStringI(directive._value, "define")) {
                            if (foundNonWhitespaceChar) {
                                try {
                                    auto expr = Internal::AsExpressionTokenList(
                                        activeSubstitutions._dictionary, remainingLine._value, activeSubstitutions);

                                    // Note that we don't want to calculate any relevance information yet. We will do
                                    // that if the substitution is used, however
                                    if (!gotNotDefinedCheck) {
                                        activeSubstitutions._items.insert(std::make_pair(symbol._value.AsString(), expr));
                                    } else {
                                        activeSubstitutions._defaultSets.insert(std::make_pair(symbol._value.AsString(), expr));
                                    }
                                } catch (const std::exception& e) {
                                    std::cout << "Substitution for " << symbol._value << " is not an expression" << std::endl;
                                }
                            } else {
                                if (!gotNotDefinedCheck) {
                                    activeSubstitutions._items.insert(std::make_pair(symbol._value.AsString(), Internal::ExpressionTokenList{}));
                                } else {
                                    activeSubstitutions._defaultSets.insert(std::make_pair(symbol._value.AsString(), Internal::ExpressionTokenList{}));
                                }
                            }
                        } else {
                            auto subs = activeSubstitutions._items.find(symbol._value.AsString());
                            if (subs != activeSubstitutions._items.end())
                                activeSubstitutions._items.erase(subs);
                        }
                    } else {
                        auto tableEntry = tokenDictionary.GetToken(Internal::TokenDictionary::TokenType::IsDefinedTest, symbol._value.AsString());
                        auto i = relevanceTable.find(tableEntry);
                        if (i != relevanceTable.end())
                            relevanceTable.erase(i);

                        // By the check above, any size 3 positive conditions are just !defined(X).
                        // We will just clear them out here, because we won't want them to appear in the
                        // relevance conditions
                        for (auto& condition:conditionsStack)
                            if (condition._positiveCond.size() == 3)
                                conditionsStack[0]._positiveCond = {1};
                    }
                } else {
                    // The state of this variable will vary based on
                    // We can still support this, but it requires building a relevance table for
                    // the symbol here; and then any expression that uses this symbol should then
                    // merge in the relevance information for it
                    std::cout << "Conditional substitution for " << symbol._value << " ignored." << std::endl;
                }

            } else if (XlEqStringI(directive._value, "include")) {

                auto symbol = helper.PreProc_GetBrackettedToken();
                if (symbol._value.IsEmpty())
                    Throw(FormatException("Expected file to include after #include directive", directive._start));

                // todo -- do we need any #pragma once type functionality to prevent infinite recursion
                // or just searching through too many files
                auto includedAnalysis = includeHandler.GeneratePreprocessorAnalysis(symbol._value, filenameForRelativeIncludeSearch);

                // merge in the results we got from this included file
                std::map<unsigned, Internal::ExpressionTokenList> translatedRelevanceTable;
                for (const auto& relevance:includedAnalysis._relevanceTable) {
                    translatedRelevanceTable.insert(std::make_pair(
                        tokenDictionary.Translate(includedAnalysis._tokenDictionary, relevance.first),
                        tokenDictionary.Translate(includedAnalysis._tokenDictionary, relevance.second)));
                }
                relevanceTable = Internal::MergeRelevanceTables(
                    relevanceTable, {},
                    translatedRelevanceTable, GetCurrentCondition(conditionsStack));

                for (const auto& sideEffect:includedAnalysis._substitutionSideEffects._items) {
                    activeSubstitutions._items.insert(std::make_pair(
                        sideEffect.first,
                        activeSubstitutions._dictionary.Translate(includedAnalysis._substitutionSideEffects._dictionary, sideEffect.second)));
                }

                for (const auto& sideEffect:includedAnalysis._substitutionSideEffects._defaultSets) {
                    if (    activeSubstitutions._items.find(sideEffect.first) != activeSubstitutions._items.end()
                        ||  activeSubstitutions._defaultSets.find(sideEffect.first) != activeSubstitutions._defaultSets.end())
                        continue;
                    activeSubstitutions._defaultSets.insert(std::make_pair(
                        sideEffect.first,
                        activeSubstitutions._dictionary.Translate(includedAnalysis._substitutionSideEffects._dictionary, sideEffect.second)));
                }

            } else if (XlEqStringI(directive._value, "line") || XlEqStringI(directive._value, "error") || XlEqStringI(directive._value, "pragma")) {

                // These don't have any effects relevant to us. We can just go ahead and skip them
                helper.SkipUntilNextPreproc();

            } else {

                Throw(FormatException(
                    StringMeld<256>() << "Unknown preprocessor directive: " << directive._value,
                    directive._start));

            }
        }

        PreprocessorAnalysis result;
        result._tokenDictionary = std::move(tokenDictionary);
        result._relevanceTable = std::move(relevanceTable);
        result._substitutionSideEffects = std::move(activeSubstitutions);
		return result;
    }
}
