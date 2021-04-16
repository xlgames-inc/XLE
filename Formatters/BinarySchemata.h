// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Assets/AssetUtils.h"
#include "../Utility/Streams/PreprocessorInterpreter.h"
#include "../Utility/Streams/ConditionalPreprocessingTokenizer.h"

namespace Formatters
{
	struct BlockDefinition
	{
		Utility::Internal::TokenDictionary _tokenDictionary;
		std::vector<unsigned> _cmdList;
		std::vector<unsigned> _templateParameterNames;
		uint32_t _templateParameterTypeField = 0;
	};

	struct Alias
	{
		std::string _aliasedType;
		Utility::Internal::TokenDictionary _tokenDictionary;
		std::vector<unsigned> _templateParameterNames;
		uint32_t _templateParameterTypeField = 0;
	};

	class BinarySchemata
	{
	public:
		BinarySchemata(
			StringSection<> inputData,
			const ::Assets::DirectorySearchRules& searchRules,
			const std::shared_ptr<::Assets::DependencyValidation>& depVal);
		BinarySchemata(
			Utility::IPreprocessorIncludeHandler::Result&& initialFile,
			Utility::IPreprocessorIncludeHandler* includeHandler);
		BinarySchemata();
		~BinarySchemata();

		using BlockDefinitionId = unsigned;
		using AliasId = unsigned;
		static constexpr BlockDefinitionId BlockDefinitionId_Invalid = ~0u;
		static constexpr AliasId AliasId_Invalid = ~0u;

		BlockDefinitionId FindBlockDefinition(StringSection<> name);
		AliasId FindAlias(StringSection<> name);

		const Alias& GetAlias(AliasId id) const { return _aliases[id].second; }
		const BlockDefinition& GetBlockDefinition(BlockDefinitionId id) const { return _blockDefinitions[id].second; }

		const std::string& GetAliasName(AliasId id) const { return _aliases[id].first; }
		const std::string& GetBlockDefinitionName(BlockDefinitionId id) const { return _blockDefinitions[id].first; }

	private:
		void ParseBlock(ConditionalProcessingTokenizer& tokenizer);
		void ParseLiterals(ConditionalProcessingTokenizer& tokenizer);
		void ParseAlias(ConditionalProcessingTokenizer& tokenizer);
		void ParseBitField(ConditionalProcessingTokenizer& tokenizer);
		std::string ParseTypeBaseName(ConditionalProcessingTokenizer& tokenizer);
		std::string ParseExpressionStr(ConditionalProcessingTokenizer& tokenizer);
		void PushExpression(BlockDefinition& workingDefinition, ConditionalProcessingTokenizer& tokenizer);
		void PushComplexType(BlockDefinition& workingDefinition, ConditionalProcessingTokenizer& tokenizer);
		void Parse(ConditionalProcessingTokenizer& tokenizer);

		std::vector<std::pair<std::string, ParameterBox>> _literals;
		std::vector<std::pair<std::string, Alias>> _aliases;
		std::vector<std::pair<std::string, BlockDefinition>> _blockDefinitions;
	};

    enum class Cmd
	{
		LookupType,
		EvaluateExpression,
		InlineIndividualMember,
		InlineArrayMember,
		IfFalseThenJump
	};

    enum class TemplateParameterType { Typename, Expression };
}

