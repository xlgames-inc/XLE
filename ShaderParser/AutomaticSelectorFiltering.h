// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/AssetsCore.h"
#include "../Assets/IntermediateCompilers.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/PreprocessorInterpreter.h"
#include "../Utility/MemoryUtils.h"
#include <memory>

namespace Utility { class OutputStreamFormatter; }
namespace Assets { class ICompileOperation; class DirectorySearchRules; }

namespace ShaderSourceParser
{
	class SelectorFilteringRules
	{
	public:
		Utility::Internal::TokenDictionary _tokenDictionary;
		std::map<unsigned, Utility::Internal::ExpressionTokenList> _relevanceTable;
		std::map<unsigned, Utility::Internal::ExpressionTokenList> _defaultSets;

		uint64_t GetHash() const;

		bool IsRelevant(
			StringSection<> symbol, StringSection<> value = {},
			IteratorRange<const ParameterBox**> environment = {}) const;

		void MergeIn(const SelectorFilteringRules& source);

		static const auto CompileProcessType = ConstHash64<'Filt', 'erRu', 'les'>::Value;
		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }

		friend void SerializationOperator(
			Utility::OutputStreamFormatter& formatter,
			const SelectorFilteringRules& input);

		SelectorFilteringRules(
			InputStreamFormatter<utf8>& formatter, 
			const ::Assets::DirectorySearchRules&,
			const ::Assets::DepValPtr& depVal);
		SelectorFilteringRules(const std::unordered_map<std::string, std::string>& relevanceStrings);
		SelectorFilteringRules();
		~SelectorFilteringRules();

	private:
		::Assets::DepValPtr _depVal;
		void RecalculateHash();
	};

	::Assets::IntermediateCompilers::CompilerRegistration RegisterShaderSelectorFilteringCompiler(
		::Assets::IntermediateCompilers& intermediateCompilers);

	SelectorFilteringRules GenerateSelectorFilteringRules(StringSection<> sourceCode);

}

