// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PredefinedDescriptorSetLayout.h"
#include "PredefinedCBLayout.h"
#include "../../Assets/DepVal.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/BitUtils.h"
#include "../../Utility/Streams/PreprocessorInterpreter.h"
#include "../../Utility/Streams/ConditionalPreprocessingTokenizer.h"

namespace RenderCore { namespace Assets 
{
	PredefinedCBLayout::NameAndType ParseStatement(ConditionalProcessingTokenizer& streamIterator, ParameterBox& defaults);
	void AppendElement(PredefinedCBLayout& cbLayout, const PredefinedCBLayout::NameAndType& input, unsigned cbIterator[PredefinedCBLayout::AlignmentRules_Max]);

	auto PredefinedDescriptorSetLayout::ParseCBLayout(ConditionalProcessingTokenizer& iterator) -> ConstantBuffer
	{
		PredefinedDescriptorSetLayout::ConstantBuffer result;

		result._conditions = iterator._preprocessorContext.GetCurrentConditionString();

        auto layoutName = iterator.GetNextToken();
        if (layoutName._value.IsEmpty())
            Throw(FormatException("Expecting identifier after struct keyword", layoutName._start));

        result._name = layoutName._value.AsString();

        auto token = iterator.GetNextToken();
        if (!XlEqString(token._value, "{"))
            Throw(FormatException(StringMeld<256>() << "Expecting '{', but got " << token._value, token._start));

		result._layout = std::make_shared<PredefinedCBLayout>();
		unsigned currentLayoutCBIterator[PredefinedCBLayout::AlignmentRules_Max] = { 0, 0, 0 };

		for (;;) {
            auto next = iterator.PeekNextToken();
            if (next._value.IsEmpty())
                Throw(FormatException(StringMeld<256>() << "Unexpected end of file while parsing layout for (" << result._name << ") at " << token._value, token._start));

			if (XlEqString(next._value, "}")) {
                iterator.GetNextToken();		// (advance over the })
                token = iterator.GetNextToken();
                if (!XlEqString(token._value, ";"))
                    Throw(FormatException(StringMeld<256>() << "Expecting ; after }, but got " << token._value, token._start));
				break;
			}

			auto parsed = ParseStatement(iterator, result._layout->_defaults);
            AppendElement(*result._layout, parsed, currentLayoutCBIterator);
		}

		for (unsigned c=0; c<dimof(result._layout->_cbSizeByLanguage); ++c)
			result._layout->_cbSizeByLanguage[c] = CeilToMultiplePow2(currentLayoutCBIterator[c], 16);

		return result;
	}

	auto PredefinedDescriptorSetLayout::ParseResource(ConditionalProcessingTokenizer& iterator) -> Resource
	{
		PredefinedDescriptorSetLayout::Resource result;

		result._conditions = iterator._preprocessorContext.GetCurrentConditionString();

		auto name = iterator.GetNextToken();
        if (name._value.IsEmpty())
            Throw(FormatException("Expecting identifier after resource keyword", name._start));

        result._name = name._value.AsString();

		auto token = iterator.GetNextToken();
        if (!XlEqString(token._value, ";"))
            Throw(FormatException(StringMeld<256>() << "Expecting ; after resource, but got " << token._value, token._start));

		return result;
	}

	auto PredefinedDescriptorSetLayout::ParseSampler(ConditionalProcessingTokenizer& iterator) -> Sampler
	{
		PredefinedDescriptorSetLayout::Sampler result;

		result._conditions = iterator._preprocessorContext.GetCurrentConditionString();

		auto name = iterator.GetNextToken();
        if (name._value.IsEmpty())
            Throw(FormatException("Expecting identifier after sampler keyword", name._start));

        result._name = name._value.AsString();

		auto token = iterator.GetNextToken();
        if (!XlEqString(token._value, ";"))
            Throw(FormatException(StringMeld<256>() << "Expecting ; after sampler, but got " << token._value, token._start));

		return result;
	}

	PredefinedDescriptorSetLayout::PredefinedDescriptorSetLayout(
        StringSection<> inputData,
        const ::Assets::DirectorySearchRules& searchRules,
        const ::Assets::DepValPtr& depVal)
	: _depVal(depVal)
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

        ConditionalProcessingTokenizer iterator(inputData);

        for (;;) {
            auto token = iterator.GetNextToken();
            if (token._value.IsEmpty())
                break;

            if (XlEqString(token._value, "struct")) {
				_constantBuffers.emplace_back(ParseCBLayout(iterator));
			} else if (XlEqString(token._value, "Texture2D")) {
				_resources.emplace_back(ParseResource(iterator));
			} else if (XlEqString(token._value, "SamplerState")) {
				_samplers.emplace_back(ParseSampler(iterator));
			} else {
                Throw(FormatException(StringMeld<256>() << "Expecting 'struct' keyword, but got " << token._value, token._start));
			}
        }
    }

	PredefinedDescriptorSetLayout::PredefinedDescriptorSetLayout() {}

    PredefinedDescriptorSetLayout::~PredefinedDescriptorSetLayout()
    {
    }

}}

