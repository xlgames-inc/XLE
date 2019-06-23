// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PredefinedCBLayout.h"
#include "../RenderUtils.h"
#include "../ShaderLangUtil.h"
#include "../Format.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/DepVal.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/BitUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/Conversion.h"

namespace RenderCore { namespace Techniques
{
    class Tokenizer
    {
    public:
        struct Token
        {
            StringSection<> _value;
            StreamLocation _start, _end;
        };

        Token GetNextToken();
        Token PeekNextToken();

        StreamLocation GetLocation() const;
        StringSection<> Remaining() const { return _input; }

        class PreprocessorParseContext
        {
        public:
            using Expression = std::string;
            struct Cond
            {
                Expression _positiveCond;
                Expression _negativeCond;
            };
            std::vector<Cond> _conditionsStack;

            Expression GetCurrentConditionString();

            PreprocessorParseContext();
            ~PreprocessorParseContext();
        };
        PreprocessorParseContext _preprocessorContext;

        Tokenizer(
            StringSection<> input,
            unsigned lineIndex = 0, const void* lineStart = nullptr);
        ~Tokenizer();

    private:
        StringSection<>     _input;
        unsigned            _lineIndex;
        const void*         _lineStart;
        bool                _preprocValid = true;

        void SkipWhitespace();
        Token ReadUntilEndOfLine();

        void ParsePreprocessorDirective();
        void PreProc_SkipWhitespace();
        auto PreProc_GetNextToken() -> Token;
    };

    static bool IsIdentifierOrNumericChar(char c)
    {
        return  (c >= 'a' && c <= 'z')
            ||  (c >= 'A' && c <= 'Z')
            ||  c == '_'
            ||  (c >= '0' && c <= '9')
            ;
    }

    void Tokenizer::SkipWhitespace()
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

    auto Tokenizer::GetNextToken() -> Token
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

    auto Tokenizer::PeekNextToken() -> Token
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
        endLocation._charIndex += iterator-startOfToken;
        return Token { { startOfToken, iterator }, startLocation, endLocation };
    }

    void Tokenizer::PreProc_SkipWhitespace()
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

    auto Tokenizer::PreProc_GetNextToken() -> Token
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

    void Tokenizer::ParsePreprocessorDirective()
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

    auto Tokenizer::ReadUntilEndOfLine() -> Token
    {
        auto startLocation = GetLocation();
        auto startOfToken = _input.begin();

        while (_input.begin() != _input.end() && *_input.begin() != '\r' && *_input.begin() != '\n') {
            ++_input._start;
        }

        return Token { { startOfToken, _input.begin() }, startLocation, GetLocation() };
    }

    StreamLocation Tokenizer::GetLocation() const
    {
        StreamLocation result;
        result._charIndex = 1 + unsigned(size_t(_input.begin()) - size_t(_lineStart));
        result._lineIndex = 1 + _lineIndex;
        return result;
    }

    Tokenizer::Tokenizer(
        StringSection<> input,
        unsigned lineIndex, const void* lineStart)
    {
        _input = input;
        _lineIndex = lineIndex;
        _lineStart = lineStart;
        if (!lineStart)
            _lineStart = input.begin();
    }

    Tokenizer::~Tokenizer()
    {
    }

    auto Tokenizer::PreprocessorParseContext::GetCurrentConditionString() -> Expression
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

    Tokenizer::PreprocessorParseContext::PreprocessorParseContext() {}
    Tokenizer::PreprocessorParseContext::~PreprocessorParseContext() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    PredefinedCBLayout::PredefinedCBLayout(StringSection<::Assets::ResChar> initializer)
    {
		_validationCallback = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(_validationCallback, initializer);

		TRY {
			// Here, we will read a simple configuration file that will define the layout
			// of a constant buffer. Sometimes we need to get the layout of a constant 
			// buffer without compiling any shader code, or really touching the HLSL at all.
			size_t size;
			auto file = ::Assets::TryLoadFileAsMemoryBlock(initializer, &size);
			StringSection<char> configSection((const char*)file.get(), (const char*)PtrAdd(file.get(), size));

			// if it's a compound document, we're only going to extra the cb layout part
			auto compoundDoc = ::Assets::ReadCompoundTextDocument(configSection);
			if (!compoundDoc.empty()) {
				auto i = std::find_if(
					compoundDoc.cbegin(), compoundDoc.cend(),
					[](const ::Assets::TextChunk<char>& chunk)
					{ return XlEqString(chunk._type, "CBLayout"); });
				if (i != compoundDoc.cend())
					configSection = i->_content;
			}

			Parse(configSection);
		} CATCH(const ::Assets::Exceptions::ConstructionError& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, _validationCallback));
		} CATCH (const std::exception& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, _validationCallback));
		} CATCH_END
	}

    PredefinedCBLayout::PredefinedCBLayout(StringSection<char> source, bool)
    {
        Parse(source);
        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
    }

    static bool ParseStatement(PredefinedCBLayout& cbLayout, Tokenizer& streamIterator, unsigned& cbIterator)
    {
        auto type = streamIterator.GetNextToken();
        if (type._value.IsEmpty())
            return false;

        auto cond = streamIterator._preprocessorContext.GetCurrentConditionString();

        auto name = streamIterator.GetNextToken();
        if (name._value.IsEmpty())
            Throw(FormatException("Expecting variable name", name._start));

        StringSection<> arrayCount;
        if (XlEqString(streamIterator.PeekNextToken()._value, "[")) {
            streamIterator.GetNextToken();      // skip the one we peeked

            auto count = streamIterator.GetNextToken();
            if (count._value.IsEmpty() || XlEqString(count._value, "]"))
                Throw(FormatException("Expecting array count", count._start));

            arrayCount = count._value;
            auto token = streamIterator.GetNextToken();
            if (!XlEqString(token._value, "]"))
                Throw(FormatException("Expecting closing array brace", token._start));
        }

        PredefinedCBLayout::Element e;
        e._name = name._value.AsString();
        e._hash = ParameterBox::MakeParameterNameHash(e._name);
        e._hash64 = Hash64(AsPointer(e._name.begin()), AsPointer(e._name.end()));
        e._type = ShaderLangTypeNameAsTypeDesc(type._value);
        e._conditions = cond;

        auto size = e._type.GetSize();
        if (!size) {
            Throw(FormatException(
                StringMeld<256>() << "Problem parsing type (" << type._value << ") in PredefinedCBLayout. Type size is: " << size,
                type._start));
        }

            // HLSL adds padding so that vectors don't straddle 16 byte boundaries!
            // let's detect that case, and add padding as necessary
        if (FloorToMultiplePow2(cbIterator, 16) != FloorToMultiplePow2(cbIterator + std::min(16u, e._type.GetSize()) - 1, 16)) {
            cbIterator = CeilToMultiplePow2(cbIterator, 16);
        }

        unsigned arrayElementCount = 1;
        if (!arrayCount.IsEmpty()) {
            arrayElementCount = Conversion::Convert<unsigned>(arrayCount);
        }

        e._offset = cbIterator;
        e._arrayElementCount = arrayElementCount;
        e._arrayElementStride = (arrayElementCount>1) ? CeilToMultiplePow2(size, 16) : size;
        if (arrayElementCount != 0)
            cbIterator += (arrayElementCount-1) * e._arrayElementStride + size;
        cbLayout._elements.push_back(e);

        if (XlEqString(streamIterator.PeekNextToken()._value, "=")) {
            auto equalsToken = streamIterator.GetNextToken();      // skip the one we peeked

            if (!arrayCount.IsEmpty())
                Throw(FormatException(
                    "Attempting to provide an default for an array type in PredefinedCBLayout (this isn't supported)",
                    equalsToken._start));

            // We should read until we hit a ';'
            // (note that the whitespace between tokens will be removed by this method)
            auto startLocation = streamIterator.GetLocation();
            std::string value;
            for (;;) {
                auto nextToken = streamIterator.GetNextToken();
                if (nextToken._value.IsEmpty() || XlEqString(nextToken._value, ";"))
                    break;
                value.insert(value.end(), nextToken._value.begin(), nextToken._value.end());
            }

            uint8 buffer0[256], buffer1[256];
            auto defaultType = ImpliedTyping::Parse(
                AsPointer(value.begin()), AsPointer(value.end()),
                buffer0, dimof(buffer0));

            if (!(defaultType == e._type)) {
                    //  The initialiser isn't exactly the same type as the
                    //  defined variable. Let's try a casting operation.
                    //  Sometimes we can get int defaults for floats variables, etc.
                bool castSuccess = ImpliedTyping::Cast(
                    buffer1, dimof(buffer1), e._type,
                    buffer0, defaultType);
                if (castSuccess) {
                    cbLayout._defaults.SetParameter((const utf8*)e._name.c_str(), buffer1, e._type);
                } else {
                    Throw(FormatException(
                        "Default initialiser can't be cast to same type as variable in PredefinedCBLayout: ",
                        startLocation));
                }
            } else {
                cbLayout._defaults.SetParameter((const utf8*)e._name.c_str(), buffer0, defaultType);
            }
        } else {
            auto token = streamIterator.GetNextToken();
            if (!XlEqString(token._value, ";"))
                Throw(FormatException("Expecting ';' to finish statement", token._start));
        }

        return true;
    }

    void PredefinedCBLayout::Parse(StringSection<char> source)
    {
        Tokenizer si { source };
        unsigned cbIterator = 0;
        while (ParseStatement(*this, si, cbIterator)) {
            /**/
        }

        _cbSize = cbIterator;
        _cbSize = CeilToMultiplePow2(_cbSize, 16);
    }

    void PredefinedCBLayout::WriteBuffer(void* dst, const ParameterBox& parameters) const
    {
        for (auto c=_elements.cbegin(); c!=_elements.cend(); ++c) {
            for (auto e=0; e<c->_arrayElementCount; e++) {
                bool gotValue = parameters.GetParameter(
                    c->_hash + e, PtrAdd(dst, c->_offset + e * c->_arrayElementStride),
                    c->_type);

                if (!gotValue)
                    _defaults.GetParameter(c->_hash + e, PtrAdd(dst, c->_offset), c->_type);
            }
        }
    }

    std::vector<uint8> PredefinedCBLayout::BuildCBDataAsVector(const ParameterBox& parameters) const
    {
        std::vector<uint8> cbData(_cbSize, uint8(0));
        WriteBuffer(AsPointer(cbData.begin()), parameters);
        return std::move(cbData);
    }

    SharedPkt PredefinedCBLayout::BuildCBDataAsPkt(const ParameterBox& parameters) const
    {
        SharedPkt result = MakeSharedPktSize(_cbSize);
        std::memset(result.begin(), 0, _cbSize);
        WriteBuffer(result.begin(), parameters);
        return std::move(result);
    }
    
    uint64_t PredefinedCBLayout::CalculateHash() const
    {
        return HashCombine(Hash64(AsPointer(_elements.begin()), AsPointer(_elements.end())), _defaults.GetHash());
    }
    
    auto PredefinedCBLayout::MakeConstantBufferElements() const -> std::vector<ConstantBufferElementDesc>
    {
        std::vector<ConstantBufferElementDesc> result;
        result.reserve(_elements.size());
        for (auto i=_elements.begin(); i!=_elements.end(); ++i) {
            result.push_back(ConstantBufferElementDesc {
                i->_hash64, AsFormat(i->_type),
                i->_offset, i->_arrayElementCount });
        }
        return result;
    }

    PredefinedCBLayout::PredefinedCBLayout() : _cbSize(0) {}
    PredefinedCBLayout::~PredefinedCBLayout() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    PredefinedCBLayoutFile::PredefinedCBLayoutFile(
        StringSection<> inputData,
        const ::Assets::DirectorySearchRules& searchRules,
        const ::Assets::DepValPtr& depVal)
    {
        //
        //  Parse through thes input data line by line.
        //  If we find lines beginning with preprocessor command, we should pass them through
        //  the PreprocessorParseContext
        //
        //  Note that we don't support line appending syntax (eg, back-slash and then a newline)
        //      -- that just requires a bunch of special cases, and doesn't seem like it's
        //      worth the hassle.
        //  Also preprocessor symbols must be at the start of the line, or at least preceeded only
        //  by whitespace (same as C/CPP)
        //

        Tokenizer iterator(inputData);

        std::string currentLayoutName;
        std::shared_ptr<PredefinedCBLayout> currentLayout;
        unsigned currentLayoutCBIterator = 0;

        for (;;) {
            auto next = iterator.PeekNextToken();
            if (next._value.IsEmpty())
                break;

            if (currentLayoutName.empty()) {
                auto token = iterator.GetNextToken();
                if (!XlEqString(token._value, "struct"))
                    Throw(FormatException(StringMeld<256>() << "Expecting 'struct' keyword, but got " << token._value, token._start));

                auto layoutName = iterator.GetNextToken();
                if (layoutName._value.IsEmpty())
                    Throw(FormatException("Expecting identifier after struct keyword", token._start));

                currentLayoutName = layoutName._value.AsString();

                token = iterator.GetNextToken();
                if (!XlEqString(token._value, "{"))
                    Throw(FormatException(StringMeld<256>() << "Expecting '{', but got " << token._value, token._start));
            }

            if (XlEqString(next._value, "}")) {
                iterator.GetNextToken();
                auto token = iterator.GetNextToken();
                if (!XlEqString(token._value, ";"))
                    Throw(FormatException(StringMeld<256>() << "Expecting ; after }, but got " << token._value, token._start));

                currentLayout->_cbSize = CeilToMultiplePow2(currentLayoutCBIterator, 16);
                _layouts.insert(std::make_pair(currentLayoutName, std::move(currentLayout)));
                currentLayoutName = {};
                currentLayout = nullptr;
                currentLayoutCBIterator = 0;
                continue;
            }

            if (!currentLayout)
                currentLayout = std::make_shared<PredefinedCBLayout>();
            ParseStatement(*currentLayout, iterator, currentLayoutCBIterator);
        }

        if (currentLayout) {
            currentLayout->_cbSize = CeilToMultiplePow2(currentLayoutCBIterator, 16);
            _layouts.insert(std::make_pair(currentLayoutName, std::move(currentLayout)));
        }
    }

    PredefinedCBLayoutFile::~PredefinedCBLayoutFile()
    {
    }

////////////////////////////////////////////////////////////////////////////////////////////////////

}}
