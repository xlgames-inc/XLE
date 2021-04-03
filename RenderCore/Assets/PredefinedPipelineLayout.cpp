// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PredefinedPipelineLayout.h"
#include "PredefinedDescriptorSetLayout.h"
#include "PredefinedCBLayout.h"
#include "../../Assets/DepVal.h"
#include "../../Assets/AssetUtils.h"
#include "../../Assets/PreprocessorIncludeHandler.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/FastParseValue.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/Streams/PreprocessorInterpreter.h"
#include "../../Utility/Streams/ConditionalPreprocessingTokenizer.h"

namespace RenderCore { namespace Assets
{

	auto PredefinedPipelineLayout::ParsePipelineLayout(ConditionalProcessingTokenizer& iterator) -> std::shared_ptr<PipelineLayout>
	{
		auto result = std::make_shared<PipelineLayout>();
		for (;;) {
			auto next = iterator.PeekNextToken();
			if (next._value.IsEmpty())
				Throw(FormatException("Unexpected end of file while parsing layout at" , next._start));

			if (XlEqString(next._value, "}")) {
				break;
			}

			iterator.GetNextToken();	// skip over what we peeked

			if (XlEqString(next._value, "DescriptorSet")) {
				auto name = iterator.GetNextToken();
				auto semi = iterator.GetNextToken();
				if (name._value.IsEmpty() || !XlEqString(semi._value, ";"))
					Throw(FormatException("Expecting identifier name and then ;", name._start));

				// lookup this descriptor set in list of already registered descriptor sets
				auto i = _descriptorSets.find(name._value.AsString());
				if (i == _descriptorSets.end())
					Throw(FormatException(StringMeld<256>() << "Descriptor set with the name (" << name._value << ") has not been declared", name._start));

				result->_descriptorSets.push_back(std::make_pair(name._value.AsString(), i->second));
			} else if (XlEqString(next._value, "VSPushConstants") 
					|| XlEqString(next._value, "PSPushConstants") 
					|| XlEqString(next._value, "GSPushConstants")) {

				auto name = iterator.GetNextToken();
				auto openBrace = iterator.GetNextToken();
				if (name._value.IsEmpty() || !XlEqString(openBrace._value, "{"))
					Throw(FormatException("Expecting identifier name and then {", name._start));

				auto newLayout = std::make_shared<PredefinedCBLayout>(iterator, _depVal);

				if (XlEqString(next._value, "VSPushConstants")) {
					if (result->_vsPushConstants.second)
						Throw(FormatException("Multiple VS push constant buffers declared. Only one is supported", next._start));
					result->_vsPushConstants = std::make_pair(name._value.AsString(), newLayout);
				} else if (XlEqString(next._value, "PSPushConstants")) {
					if (result->_psPushConstants.second)
						Throw(FormatException("Multiple PS push constant buffers declared. Only one is supported", next._start));
					result->_psPushConstants = std::make_pair(name._value.AsString(), newLayout);
				} else {
					assert(XlEqString(next._value, "GSPushConstants"));
					if (result->_gsPushConstants.second)
						Throw(FormatException("Multiple GS push constant buffers declared. Only one is supported", next._start));
					result->_gsPushConstants = std::make_pair(name._value.AsString(), newLayout);
				}

				auto closeBrace = iterator.GetNextToken();
				auto semi = iterator.GetNextToken();
				if (!XlEqString(closeBrace._value, "}") || !XlEqString(semi._value, ";"))
					Throw(FormatException("Expecting } and then ;", closeBrace._start));
			}
		}

		return result;
	}

	void PredefinedPipelineLayout::Parse(Utility::ConditionalProcessingTokenizer& tokenizer)
	{
		for (;;) {
			auto token = tokenizer.GetNextToken();
			if (token._value.IsEmpty())
				break;

			if (XlEqString(token._value, "DescriptorSet") || XlEqString(token._value, "PipelineLayout")) {

				auto conditions = tokenizer._preprocessorContext.GetCurrentConditionString();
				if (!conditions.empty())
					Throw(FormatException("Preprocessor conditions are not supported wrapping a descriptor set or pipeline layout entry", tokenizer.GetLocation()));

				auto name = tokenizer.GetNextToken();
				auto openBrace = tokenizer.GetNextToken();
				if (name._value.IsEmpty() || !XlEqString(openBrace._value, "{"))
					Throw(FormatException("Expecting identifier name and then {", name._start));

				if (XlEqString(token._value, "DescriptorSet")) {
					auto existing = _descriptorSets.find(name._value.AsString());
					if (existing != _descriptorSets.end())
						Throw(FormatException(StringMeld<256>() << "Descriptor set with name (" << name._value << ") declared multiple times", name._start));

					auto newLayout = std::make_shared<PredefinedDescriptorSetLayout>(tokenizer, _depVal);
					_descriptorSets.insert(std::make_pair(name._value.AsString(), newLayout));
				} else {
					assert(XlEqString(token._value, "PipelineLayout"));
					auto existing = _pipelineLayouts.find(name._value.AsString());
					if (existing != _pipelineLayouts.end())
						Throw(FormatException(StringMeld<256>() << "Pipeline layout with name (" << name._value << ") declared multiple times", name._start));

					auto newLayout = ParsePipelineLayout(tokenizer);
					_pipelineLayouts.insert(std::make_pair(name._value.AsString(), newLayout));
				}

				auto closeBrace = tokenizer.GetNextToken();
				auto semi = tokenizer.GetNextToken();
				if (!XlEqString(closeBrace._value, "}") || !XlEqString(semi._value, ";"))
					Throw(FormatException("Expecting } and then ;", closeBrace._start));

			} else {
				Throw(FormatException(StringMeld<256>() << "Expecting either 'DescriptorSet' or 'PipelineLayout' keyword, but got " << token._value, token._start));
			}
		}

		if (!tokenizer.Remaining().IsEmpty())
			Throw(FormatException("Additional tokens found, expecting end of file", tokenizer.GetLocation()));
	}

	PredefinedPipelineLayout::PredefinedPipelineLayout(
		StringSection<> inputData,
		const ::Assets::DirectorySearchRules& searchRules,
		const ::Assets::DepValPtr& depVal)
	: _depVal(depVal)
	{
		ConditionalProcessingTokenizer tokenizer(inputData);
		Parse(tokenizer);
	}

	PredefinedPipelineLayout::PredefinedPipelineLayout(StringSection<> sourceFileName)
	{
		::Assets::PreprocessorIncludeHandler includeHandler;
		auto initialFile = includeHandler.OpenFile(sourceFileName, {});
		ConditionalProcessingTokenizer tokenizer(
			MakeStringSection((const char*)initialFile._fileContents.get(), (const char*)PtrAdd(initialFile._fileContents.get(), initialFile._fileContentsSize)),
			initialFile._filename,
			&includeHandler);
		Parse(tokenizer);
		_depVal = includeHandler.MakeDependencyValidation();
	}

	PredefinedPipelineLayout::PredefinedPipelineLayout() {}
	PredefinedPipelineLayout::~PredefinedPipelineLayout() {}

}}

