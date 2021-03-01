// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../Utility/Streams/PreprocessorInterpreter.h"
#include "../../Utility/Streams/ConditionalPreprocessingTokenizer.h"
#include "../../Utility/Streams/SerializationUtils.h"
#include <stdexcept>
#include <iostream>
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"

namespace Utility
{
	std::ostream& SerializationOperator(std::ostream& str, const RelevanceTable& table)
	{
		for (const auto& i:table._items)
			str << "[" << i.first << "] = " << i.second << std::endl;
		return str;
	}
}

using namespace Catch::literals;
namespace UnitTests
{
	TEST_CASE( "Utilities-ExpressionRelevance", "[utility]" )
	{
		const char* inputExpression0 = "(SEL0 || SEL1) && SEL2";
		auto expression0Relevance = CalculatePreprocessorExpressionRevelance(inputExpression0);
		std::cout << "Expression0 result: " << std::endl << expression0Relevance;

		const char* inputExpression0a = "(SEL0 || defined(SEL1) || SEL2<5) && (SEL3 || defined(SEL4) || SEL5>=7)";
		auto expression0aRelevance = CalculatePreprocessorExpressionRevelance(inputExpression0a);
		std::cout << "Expression0a result: " << std::endl << expression0aRelevance;

		const char* inputExpression1 = "(SEL0 || SEL1) && SEL2 && !SEL3 && (SEL4==2 || SEL5 < SEL6) || defined(SEL7)";
		auto expression1Relevance = CalculatePreprocessorExpressionRevelance(inputExpression1);
		std::cout << "Expression1 result: " << std::endl << expression1Relevance;
	}
	
	TEST_CASE( "Utilities-ConditionalPreprocessingTest", "[utility]" )
	{
		const char* input = R"--(
			Token0 Token1
			#if SELECTOR_0 || SELECTOR_1
				#if SELECTOR_2
					Token2
				#endif
				Token3
			#endif
		)--";

		ConditionalProcessingTokenizer tokenizer(input);

		REQUIRE(std::string("Token0") == tokenizer.GetNextToken()._value.AsString());
		REQUIRE(std::string("") == tokenizer._preprocessorContext.GetCurrentConditionString());

		REQUIRE(std::string("Token1") == tokenizer.GetNextToken()._value.AsString());
		REQUIRE(std::string("") == tokenizer._preprocessorContext.GetCurrentConditionString());

		REQUIRE(std::string("Token2") == tokenizer.GetNextToken()._value.AsString());
		REQUIRE(std::string("(SELECTOR_2) && (SELECTOR_0 || SELECTOR_1)") == tokenizer._preprocessorContext.GetCurrentConditionString());

		REQUIRE(std::string("Token3") == tokenizer.GetNextToken()._value.AsString());
		REQUIRE(std::string("(SELECTOR_0 || SELECTOR_1)") == tokenizer._preprocessorContext.GetCurrentConditionString());

		REQUIRE(tokenizer.PeekNextToken()._value.IsEmpty());
	}
}
