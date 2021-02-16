// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PredefinedDescriptorSetLayout.h"
#include "PredefinedCBLayout.h"
#include "../../Assets/DepVal.h"
#include "../../Assets/AssetUtils.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/FastParseValue.h"
#include "../../Utility/BitUtils.h"
#include "../../Utility/Streams/PreprocessorInterpreter.h"
#include "../../Utility/Streams/ConditionalPreprocessingTokenizer.h"

namespace RenderCore { namespace Assets 
{
	PredefinedCBLayout::NameAndType ParseStatement(ConditionalProcessingTokenizer& streamIterator, ParameterBox& defaults);
	void AppendElement(PredefinedCBLayout& cbLayout, const PredefinedCBLayout::NameAndType& input, unsigned cbIterator[PredefinedCBLayout::AlignmentRules_Max]);

	void PredefinedDescriptorSetLayout::ParseSlot(ConditionalProcessingTokenizer& iterator, SlotType type)
	{
		PredefinedDescriptorSetLayout::ConditionalDescriptorSlot result;

		result._conditions = iterator._preprocessorContext.GetCurrentConditionString();

		auto layoutName = iterator.GetNextToken();
		if (layoutName._value.IsEmpty())
			Throw(FormatException("Expecting identifier after type keyword", layoutName._start));

		result._name = layoutName._value.AsString();
		result._type = type;

		auto token = iterator.GetNextToken();

		if (type == SlotType::Texture || type == SlotType::Sampler) {
			if (XlEqString(token._value, "[")) {
				auto countToken = iterator.GetNextToken();
				if (XlEqString(countToken._value, "]"))
					Throw(FormatException("Expecting expecting array count, but got empty array brackets", token._start));

				auto* parseEnd = FastParseValue(countToken._value, result._arrayElementCount);
				if (parseEnd != countToken._value.end())
					Throw(FormatException(StringMeld<256>() << "Expecting unsigned integer value for array count, but got " << countToken._value, token._start));

				auto closeBracket = iterator.GetNextToken();
				if (!XlEqString(closeBracket._value, "]"))
					Throw(FormatException(StringMeld<256>() << "Expecting expecting closing bracket for array, but got " << closeBracket._value, token._start));

				token = iterator.GetNextToken();
			}
		}

		if (type == SlotType::ConstantBuffer) {
			if (XlEqString(token._value, "{")) {
				auto newLayout = std::make_shared<PredefinedCBLayout>();
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

					auto parsed = ParseStatement(iterator, newLayout->_defaults);
					AppendElement(*newLayout, parsed, currentLayoutCBIterator);
				}

				for (unsigned c=0; c<dimof(newLayout->_cbSizeByLanguage); ++c)
					newLayout->_cbSizeByLanguage[c] = CeilToMultiplePow2(currentLayoutCBIterator[c], 16);

				_constantBuffers.push_back(newLayout);
				result._cbIdx = (unsigned)_constantBuffers.size() - 1;
			}
		}

		if (!XlEqString(token._value, ";"))
			Throw(FormatException(StringMeld<256>() << "Expecting ; after resource, but got " << token._value, token._start));

		_slots.push_back(result);
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
				ParseSlot(iterator, SlotType::ConstantBuffer);
			} else if (XlEqString(token._value, "Texture2D")) {
				ParseSlot(iterator, SlotType::Texture);
			} else if (XlEqString(token._value, "SamplerState")) {
				ParseSlot(iterator, SlotType::Sampler);
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

