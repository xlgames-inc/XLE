// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../StringUtils.h"
#include "../IteratorUtils.h"
#include <unordered_map>
#include <map>

namespace Utility
{
	class ParameterBox;

	bool EvaluatePreprocessorExpression(
		StringSection<> input,
		const std::unordered_map<std::string, int>& definedTokens);

	bool EvaluatePreprocessorExpression(
		StringSection<> input,
		IteratorRange<const ParameterBox**> definedTokens);

	namespace Internal
	{
		using ExpressionTokenList = std::vector<unsigned>;

		class TokenDictionary
		{
		public:
			enum class TokenType { UnaryMarker, Literal, Variable, IsDefinedTest, Operation };
			struct Token
			{
				TokenType _type;
				std::string _value;
			};
			std::vector<Token> _tokenDefinitions;

			void PushBack(
				ExpressionTokenList& tokenList,
				TokenType type, const std::string& value = {});

			ExpressionTokenList Translate(
				const TokenDictionary& otherDictionary,
				const ExpressionTokenList& tokenListForOtherDictionary);
			unsigned Translate(
				const TokenDictionary& otherDictionary,
				unsigned tokenForOtherDictionary);

			unsigned GetToken(TokenType type, const std::string& value = {});
			std::optional<unsigned> TryGetToken(TokenType type, StringSection<> value) const;

			int EvaluateExpression(
				IteratorRange<const unsigned*> tokenList,
				IteratorRange<ParameterBox const*const*> environment) const;
			std::string AsString(IteratorRange<const unsigned*> tokenList) const;
			void Simplify(ExpressionTokenList&);

			uint64_t CalculateHash() const;

			TokenDictionary();
			~TokenDictionary();
		};

		const char* AsString(TokenDictionary::TokenType);
		TokenDictionary::TokenType AsTokenType(StringSection<>);

		using WorkingRelevanceTable = std::map<unsigned, ExpressionTokenList>;

		WorkingRelevanceTable MergeRelevanceTables(
			const WorkingRelevanceTable& lhs, const ExpressionTokenList& lhsCondition,
			const WorkingRelevanceTable& rhs, const ExpressionTokenList& rhsCondition);

		ExpressionTokenList InvertExpression(const ExpressionTokenList& expr);
		ExpressionTokenList AndExpression(const ExpressionTokenList& lhs, const ExpressionTokenList& rhs);
		ExpressionTokenList OrExpression(const ExpressionTokenList& lhs, const ExpressionTokenList& rhs);
		ExpressionTokenList AndNotExpression(const ExpressionTokenList& lhs, const ExpressionTokenList& rhs);

		struct PreprocessorSubstitutions
		{
			TokenDictionary _dictionary;
			enum class Type { Define, Undefine, DefaultDefine };
			struct ConditionalSubstitutions
			{	
				std::string _symbol;
				Type _type;
				ExpressionTokenList _condition;
				ExpressionTokenList _substitution;
			};
			std::vector<ConditionalSubstitutions> _substitutions;
		};

		ExpressionTokenList AsExpressionTokenList(
			TokenDictionary& dictionary,
			StringSection<> input,
			const PreprocessorSubstitutions& substitutions = {});
		
		WorkingRelevanceTable CalculatePreprocessorExpressionRevelance(
			TokenDictionary& dictionary,
			const ExpressionTokenList& input);
	}

	class PreprocessorAnalysis
    {
    public:
        Internal::TokenDictionary _tokenDictionary;
        std::map<unsigned, Internal::ExpressionTokenList> _relevanceTable;
        Internal::PreprocessorSubstitutions _sideEffects;
    };

	class IPreprocessorIncludeHandler;

    PreprocessorAnalysis GeneratePreprocessorAnalysisFromString(
		StringSection<> input,
		StringSection<> filenameForRelativeIncludeSearch = {},
		IPreprocessorIncludeHandler* includeHandler = nullptr);

	PreprocessorAnalysis GeneratePreprocessorAnalysisFromFile(
		StringSection<> inputFilename,
		IPreprocessorIncludeHandler* includeHandler = nullptr);

	class IPreprocessorIncludeHandler
	{
	public:
		struct Result 
		{ 
			std::string _filename; 
			std::unique_ptr<uint8[]> _fileContents;
			size_t _fileContentsSize;
		};

		virtual Result OpenFile(
			StringSection<> requestString,
			StringSection<> fileIncludedFrom) = 0;
		virtual ~IPreprocessorIncludeHandler();
	};
}

using namespace Utility;

