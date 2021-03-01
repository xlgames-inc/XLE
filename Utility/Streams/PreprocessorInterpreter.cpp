// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4099) // 'Iterator': type name first seen using 'class' now seen using 'struct'
#pragma warning(disable:4180) // qualifier applied to function type has no meaning; ignored
#pragma warning(disable:4505) // 'preprocessor_operations::UndefinedOnUndefinedOperation': unreferenced local function has been removed

#include "PreprocessorInterpreter.h"
#include "../ParameterBox.h"
#include "../Threading/ThreadingUtils.h"
#include "../../Core/Exceptions.h"
#include "../../Foreign/cparse/shunting-yard.h"
#include "../../Foreign/cparse/shunting-yard-exceptions.h"

#include <cmath>
#include <atomic>
#include <map>

namespace preprocessor_operations
{
	static packToken Equal(const packToken& left, const packToken& right, evaluationData* data)
	{
		// undefined tokens (ie, those with type VAR) behave as if they are zero
		// (even in the case with two undefined tokens, oddly enough)
		if (left->type == VAR) {
			if (right->type == VAR)
				return true;
			return packToken(0) == right;
		} else if (right->type == VAR)
			return left == packToken(0);

		return left == right;
	}

	static packToken Different(const packToken& left, const packToken& right, evaluationData* data)
	{
		// undefined tokens (ie, those with type VAR) behave as if they are zero
		// (even in the case with two undefined tokens, oddly enough)
		if (left->type == VAR) {
			if (right->type == VAR)
				return false;
			return packToken(0) != right;
		} else if (right->type == VAR)
			return left != packToken(0);

		return left != right;
	}

	static packToken UnaryNumeralOperation(const packToken& left, const packToken& right, evaluationData* data)
	{
		const std::string& op = data->op;

		if (op == "+") {
			return right;
		} else if (op == "-") {
			return -right.asDouble();
		} else if (op == "!") {
			return !right.asBool();
		} else {
			throw undefined_operation(data->op, left, right);
		}
	}

	static packToken NumeralOperation_Internal(const packToken& left, const packToken& right, const std::string& op)
	{
		// Extract integer and real values of the operators:
		auto left_d = left.asDouble();
		auto left_i = left.asInt();

		auto right_d = right.asDouble();
		auto right_i = right.asInt();

		if (op == "+") {
			return left_d + right_d;
		} else if (op == "*") {
			return left_d * right_d;
		} else if (op == "-") {
			return left_d - right_d;
		} else if (op == "/") {
			return left_d / right_d;
		} else if (op == "<<") {
			return left_i << right_i;
		} else if (op == "**") {
			return pow(left_d, right_d);
		} else if (op == ">>") {
			return left_i >> right_i;
		} else if (op == "%") {
			return left_i % right_i;
		} else if (op == "<") {
			return left_d < right_d;
		} else if (op == ">") {
			return left_d > right_d;
		} else if (op == "<=") {
			return left_d <= right_d;
		} else if (op == ">=") {
			return left_d >= right_d;
		} else if (op == "&&") {
			return left_i && right_i;
		} else if (op == "||") {
			return left_i || right_i;
		} else {
			throw undefined_operation(op, left, right);
		}
	}

	static packToken NumeralOperation(const packToken& left, const packToken& right, evaluationData* data)
	{
		return NumeralOperation_Internal(left, right, data->op);
	}

	static packToken UndefinedOnNumberOperation(const packToken& left, const packToken& right, evaluationData* data)
	{
		// undefined symbols behave as if they are 0 in comparisons
		return NumeralOperation_Internal(packToken(0), right, data->op);
	}

	static packToken NumberOnUndefinedOperation(const packToken& left, const packToken& right, evaluationData* data)
	{
		// undefined symbols behave as if they are 0 in comparisons
		return NumeralOperation_Internal(left, packToken(0), data->op);
	}

	static packToken UndefinedOnUndefinedOperation(const packToken& left, const packToken& right, evaluationData* data)
	{
		// undefined symbols behave as if they are 0 in comparisons
		return NumeralOperation_Internal(packToken(0), packToken(0), data->op);
	}

	static packToken definedFunction(TokenMap scope)
	{
		auto* sym = scope.find("symbol");
		if (!sym) return false;

		// Tokens that look like identifiers, but aren't recognized by the shunting-yard library
		// are considered "variables". In effect, this means they haven't been defined beforehand.
		if (sym->token()->type == VAR)
			return false;

		return true;
	}

	struct Startup {
		Startup() {
			// Create the operator precedence map based on C++ default
			// precedence order as described on cppreference website:
			// http://en.cppreference.com/w/cpp/language/operator_precedence
			OppMap_t& opp = calculator::Default().opPrecedence;
			opp.add("*",  5); opp.add("/", 5); opp.add("%", 5);
			opp.add("+",  6); opp.add("-", 6);
			opp.add("<<", 7); opp.add(">>", 7);
			opp.add("<",  8); opp.add("<=", 8); opp.add(">=", 8); opp.add(">", 8);
			opp.add("==", 9); opp.add("!=", 9);
			opp.add("&&", 13);
			opp.add("||", 14);

			// Add unary operators:
			opp.addUnary("+",  3); opp.addUnary("-", 3); opp.addUnary("!", 3);

			// Link operations to respective operators:
			opMap_t& opMap = calculator::Default().opMap;
			opMap.add({ANY_TYPE, "==", ANY_TYPE}, &Equal);
			opMap.add({ANY_TYPE, "!=", ANY_TYPE}, &Different);

			// Note: The order is important:
			opMap.add({NUM, ANY_OP, NUM}, &NumeralOperation);
			opMap.add({UNARY, ANY_OP, NUM}, &UnaryNumeralOperation);
			opMap.add({VAR, ANY_OP, NUM}, &UndefinedOnNumberOperation);
			opMap.add({NUM, ANY_OP, VAR}, &NumberOnUndefinedOperation);
			opMap.add({VAR, ANY_OP, VAR}, &NumberOnUndefinedOperation);

			TokenMap& global = TokenMap::default_global();
			global["defined"] = CppFunction(&definedFunction, {"symbol"}, "defined()");
		}
	} __CPARSE_STARTUP;
}


namespace Utility
{
	static std::atomic_bool static_hasSetupPreprocOps { false };
	static std::atomic_bool static_setupThreadAssigned { false };

	bool EvaluatePreprocessorExpression(
		StringSection<> input,
		const std::unordered_map<std::string, int>& definedTokens)
	{
		if (!static_hasSetupPreprocOps.load()) {
			bool threadAssigned = static_setupThreadAssigned.exchange(true);
			if (!threadAssigned) {
				preprocessor_operations::Startup();
				static_hasSetupPreprocOps.store(true);
			} else {
				while (!static_hasSetupPreprocOps.load()) {
					Threading::YieldTimeSlice();
				}
			}
		}

		TokenMap vars;
		for (const auto&i:definedTokens)
			vars[i.first] = packToken(i.second);

		// symbols with no value can be defined like this: (but they aren't particularly useful in expressions, except when used with the defined() function)
		// vars["DEFINED_NO_VALUE"] = packToken(nullptr, NONE);

		return calculator::calculate(input.AsString().c_str(), &vars).asBool();

		// those that this can throw exceptions back to the caller (for example, if the input can't be parsed)
	}

	bool EvaluatePreprocessorExpression(
		StringSection<> input,
		IteratorRange<const ParameterBox**> definedTokens)
	{
		if (!static_hasSetupPreprocOps.load()) {
			bool threadAssigned = static_setupThreadAssigned.exchange(true);
			if (!threadAssigned) {
				preprocessor_operations::Startup();
				static_hasSetupPreprocOps.store(true);
			} else {
				while (!static_hasSetupPreprocOps.load()) {
					Threading::YieldTimeSlice();
				}
			}
		}

		TokenMap vars;
		for (const auto&b:definedTokens) {
			for (const auto&i:*b) {
				auto name = i.Name().AsString();
				auto type = i.Type();

				// For simple scalar types, attempt conversion to something
				// we can construct a packToken with
				if (type._arrayCount <= 1) {
					if (type._type == ImpliedTyping::TypeCat::Bool) {
						vars[name] = packToken(*(bool*)i.RawValue().begin());
						continue;
					} else if (type._type == ImpliedTyping::TypeCat::Int8
							|| type._type == ImpliedTyping::TypeCat::UInt8
							|| type._type == ImpliedTyping::TypeCat::Int16
							|| type._type == ImpliedTyping::TypeCat::UInt16
							|| type._type == ImpliedTyping::TypeCat::Int32) {
						int dest;
						ImpliedTyping::Cast(
							MakeOpaqueIteratorRange(dest), ImpliedTyping::TypeDesc{ImpliedTyping::TypeCat::Int32},
							i.RawValue(), ImpliedTyping::TypeDesc{type._type});
						vars[name] = packToken(dest);
						continue;
					} else if (type._type == ImpliedTyping::TypeCat::UInt32
							|| type._type == ImpliedTyping::TypeCat::Int64
							|| type._type == ImpliedTyping::TypeCat::UInt64) {
						int64_t dest;
						ImpliedTyping::Cast(
							MakeOpaqueIteratorRange(dest), ImpliedTyping::TypeDesc{ImpliedTyping::TypeCat::Int64},
							i.RawValue(), ImpliedTyping::TypeDesc{type._type});
						vars[name] = packToken(dest);
						continue;
					} else if (type._type == ImpliedTyping::TypeCat::Float) {
						vars[name] = packToken(*(float*)i.RawValue().begin());
						continue;
					} else if (type._type == ImpliedTyping::TypeCat::Double) {
						vars[name] = packToken(*(double*)i.RawValue().begin());
						continue;
					}
				}

				// If we didn't get a match with one of the above types, just 
				// treat it as a string
				vars[name] = packToken(i.ValueAsString());
			}
		}

		return calculator::calculate(input.AsString().c_str(), &vars).asBool();
	}

	class AbstractExpression
	{
	public:
		enum class TokenType { UnaryMarker, Literal, Variable, IsDefinedTest, Operation };
		struct Token
		{
			TokenType _type;
			std::string _value;
		};
		std::vector<Token> _tokens;
		std::vector<unsigned> _reversePolishOrdering;		// we use this indirection here because we're expecting tokens (particular variables) to be frequently reused
	};

	using AbstractSubExpression = std::vector<unsigned>;

	static bool operator==(const AbstractExpression::Token& lhs, const AbstractExpression::Token& rhs)
	{
		return lhs._type == rhs._type && lhs._value == rhs._value;
	}

	static bool operator<(const AbstractExpression::Token& lhs, const AbstractExpression::Token& rhs)
	{
		if (lhs._type < rhs._type) return true;
		if (lhs._type > rhs._type) return false;
		return lhs._value < rhs._value;
	}

	static void PushBackToken(AbstractExpression& expr, AbstractExpression::TokenType type, const std::string& value = {});

	using WorkingRelevanceTable = std::map<unsigned, AbstractSubExpression>;

	static WorkingRelevanceTable MergeRelevanceTables(
		const WorkingRelevanceTable& lhs, const AbstractSubExpression& lhsCondition,
		const WorkingRelevanceTable& rhs, const AbstractSubExpression& rhsCondition);

	static AbstractSubExpression InvertExpression(const AbstractSubExpression& expr);
	static AbstractSubExpression AddExpression(const AbstractSubExpression& lhs, const AbstractSubExpression& rhs);
	static AbstractSubExpression OrExpression(const AbstractSubExpression& lhs, const AbstractSubExpression& rhs);

	static RelevanceTable AsRelevanceTable(class AbstractExpression& tokenTable, const WorkingRelevanceTable& input);

	static const unsigned s_fixedTokenFalse = 0;
	static const unsigned s_fixedTokenTrue = 1;
	static const unsigned s_fixedTokenLogicalAnd = 2;
	static const unsigned s_fixedTokenLogicalOr = 3;
	static const unsigned s_fixedTokenNot = 4;
	static const unsigned s_fixedTokenUnaryMarker = 5;

	AbstractExpression AsAbstractExpression(TokenQueue_t&& input)
	{
		AbstractExpression result;
		result._tokens.push_back({AbstractExpression::TokenType::Literal, "0"});		// s_fixedTokenFalse
		result._tokens.push_back({AbstractExpression::TokenType::Literal, "1"});		// s_fixedTokenTrue
		result._tokens.push_back({AbstractExpression::TokenType::Operation, "&&"});		// s_fixedTokenLogicalAnd
		result._tokens.push_back({AbstractExpression::TokenType::Operation, "||"});		// s_fixedTokenLogicalOr
		result._tokens.push_back({AbstractExpression::TokenType::Operation, "!"});		// s_fixedTokenNot
		result._tokens.push_back({AbstractExpression::TokenType::UnaryMarker});			// s_fixedTokenUnaryMarker
		result._reversePolishOrdering.reserve(input.size());

		while (!input.empty()) {
			TokenBase& base  = *input.front();
			
			if (base.type == OP) {
				auto op = static_cast<Token<std::string>*>(&base)->val;

				if (op == "()") {
					Throw(std::runtime_error("Only defined() is supported in relevance checks. Other functions are not supported"));
				} else {
					PushBackToken(result, AbstractExpression::TokenType::Operation, op);
				}

			} else if (base.type == UNARY) {

				PushBackToken(result, AbstractExpression::TokenType::UnaryMarker);
			
			} else if (base.type == VAR) {

				std::string key = static_cast<Token<std::string>*>(&base)->val;
				PushBackToken(result, AbstractExpression::TokenType::Variable, key);

			} else if (base.type & REF) {

				// This will appear when calling the "defined" pseudo-function
				// We want to transform the pattern
				//		<REF "&Function defined()"> <VARIABLE var> <Op "()">
				// to be just 
				//		<IsDefinedTest var>

				auto* resolvedRef = static_cast<RefToken*>(&base)->resolve();
				if (!resolvedRef || static_cast<CppFunction*>(resolvedRef)->name() != "defined()")
					Throw(std::runtime_error("Only defined() is supported in relevance checks. Other functions are not supported"));

				input.pop();
				if (input.empty())
					Throw(std::runtime_error("Missing parameters to defined() function in token stream"));
				TokenBase& varToTest  = *input.front();
				if (varToTest.type != VAR)
					Throw(std::runtime_error("Missing parameters to defined() function in token stream"));
				std::string key = static_cast<Token<std::string>*>(&varToTest)->val;
				input.pop();
				if (input.empty())
					Throw(std::runtime_error("Missing parameters to defined() function in token stream"));
				TokenBase& callOp  = *input.front();
				if (callOp.type != OP || static_cast<Token<std::string>*>(&callOp)->val != "()")
					Throw(std::runtime_error("Missing call token for defined() function in token stream"));
				// (final pop still happens below)

				PushBackToken(result, AbstractExpression::TokenType::IsDefinedTest, key);
				
			} else {
				
				std::string literal = packToken::str(&base);
				PushBackToken(result, AbstractExpression::TokenType::Literal, literal);

			}

			input.pop();
		}

		return result;
	}

	AbstractExpression AsAbstractExpression(StringSection<> input)
	{
		TokenMap vars;
		auto rpn = calculator::toRPN(input.AsString().c_str(), vars);
		return AsAbstractExpression(std::move(rpn));
	}

	RelevanceTable CalculatePreprocessorExpressionRevelance(StringSection<> input)
	{
		// For the given expression, we want to figure out how variables are used, and under what conditions
		// they impact the result of the evaluation

		auto abstractInput = AsAbstractExpression(input);

		struct PartialExpression
		{
			WorkingRelevanceTable _relevance;
			std::vector<unsigned> _subExpression;
		};

		std::stack<PartialExpression> evaluation;
		for (auto tokenIdx:abstractInput._reversePolishOrdering) {
			const auto& token = abstractInput._tokens[tokenIdx];

			if (token._type == AbstractExpression::TokenType::Operation) {

				PartialExpression r_token = std::move(evaluation.top()); evaluation.pop();
				PartialExpression l_token = std::move(evaluation.top()); evaluation.pop();

				// For logical operations, we need to carefully consider the left and right
				// relevance tables. For defined(), we will simplify the relevance to show
				// that we only care whether the symbol is defined or now.
				// For other operations, we will basically just merge together the relevance tables for both left and right

				PartialExpression newPartialExpression;
				if (token._value == "()")
					Throw(std::runtime_error("Only defined() is supported in relevance checks. Other functions are not supported"));
				
				if (token._value == "&&") {
					// lhs variables relevant when rhs expression is true
					// rhs variables relevant when lhs expression is true
					newPartialExpression._relevance = MergeRelevanceTables(
						l_token._relevance, r_token._subExpression,
						r_token._relevance, l_token._subExpression);
				} else if (token._value == "||") {
					// lhs variables relevant when rhs expression is false
					// rhs variables relevant when lhs expression is false
					newPartialExpression._relevance = MergeRelevanceTables(
						l_token._relevance, InvertExpression(r_token._subExpression),
						r_token._relevance, InvertExpression(l_token._subExpression));
				} else {
					newPartialExpression._relevance = MergeRelevanceTables(l_token._relevance, {}, r_token._relevance, {});
				}

				newPartialExpression._subExpression.reserve(l_token._subExpression.size() + r_token._subExpression.size() + 1);
				newPartialExpression._subExpression.insert(
					newPartialExpression._subExpression.end(),
					l_token._subExpression.begin(), l_token._subExpression.end());
				newPartialExpression._subExpression.insert(
					newPartialExpression._subExpression.end(),
					r_token._subExpression.begin(), r_token._subExpression.end());
				newPartialExpression._subExpression.push_back(tokenIdx);

				evaluation.push(std::move(newPartialExpression));

			} else if (token._type == AbstractExpression::TokenType::Variable) {

				PartialExpression newPartialExpression;
				newPartialExpression._relevance.insert(std::make_pair(tokenIdx, AbstractSubExpression{s_fixedTokenTrue}));
				newPartialExpression._subExpression = {tokenIdx};
				evaluation.push(std::move(newPartialExpression));

			} else if (token._type == AbstractExpression::TokenType::IsDefinedTest) {

				PartialExpression newPartialExpression;
				newPartialExpression._relevance.insert(std::make_pair(tokenIdx, AbstractSubExpression{s_fixedTokenTrue}));
				newPartialExpression._subExpression = {tokenIdx};
				evaluation.push(std::move(newPartialExpression));

			} else {

				PartialExpression newPartialExpression;
				newPartialExpression._subExpression = {tokenIdx};
				evaluation.push(std::move(newPartialExpression));
			
			}
		}

		assert(evaluation.size() == 1);
		return AsRelevanceTable(abstractInput, evaluation.top()._relevance);
	}

	static std::string Concatenate(StringSection<> zero, StringSection<> one)
	{
		std::string result;
		result.reserve(zero.size() + one.size());
		result.insert(result.end(), zero.begin(), zero.end());
		result.insert(result.end(), one.begin(), one.end());
		return result;
	}
	
	static std::string Concatenate(StringSection<> zero, StringSection<> one, StringSection<> two)
	{
		std::string result;
		result.reserve(zero.size() + one.size() + two.size());
		result.insert(result.end(), zero.begin(), zero.end());
		result.insert(result.end(), one.begin(), one.end());
		result.insert(result.end(), two.begin(), two.end());
		return result;
	}

	static std::string Concatenate(StringSection<> zero, StringSection<> one, StringSection<> two, StringSection<> three)
	{
		std::string result;
		result.reserve(zero.size() + one.size() + two.size() + three.size());
		result.insert(result.end(), zero.begin(), zero.end());
		result.insert(result.end(), one.begin(), one.end());
		result.insert(result.end(), two.begin(), two.end());
		result.insert(result.end(), three.begin(), three.end());
		return result;
	}
	
	static std::string Concatenate(StringSection<> zero, StringSection<> one, StringSection<> two, StringSection<> three, StringSection<> four)
	{
		std::string result;
		result.reserve(zero.size() + one.size() + two.size() + three.size() + four.size());
		result.insert(result.end(), zero.begin(), zero.end());
		result.insert(result.end(), one.begin(), one.end());
		result.insert(result.end(), two.begin(), two.end());
		result.insert(result.end(), three.begin(), three.end());
		result.insert(result.end(), four.begin(), four.end());
		return result;
	}

	static std::string Concatenate(StringSection<> zero, StringSection<> one, StringSection<> two, StringSection<> three, StringSection<> four, StringSection<> five)
	{
		std::string result;
		result.reserve(zero.size() + one.size() + two.size() + three.size() + four.size() + five.size());
		result.insert(result.end(), zero.begin(), zero.end());
		result.insert(result.end(), one.begin(), one.end());
		result.insert(result.end(), two.begin(), two.end());
		result.insert(result.end(), three.begin(), three.end());
		result.insert(result.end(), four.begin(), four.end());
		result.insert(result.end(), five.begin(), five.end());
		return result;
	}
	
	static std::string Concatenate(StringSection<> zero, StringSection<> one, StringSection<> two, StringSection<> three, StringSection<> four, StringSection<> five, StringSection<> six)
	{
		std::string result;
		result.reserve(zero.size() + one.size() + two.size() + three.size() + four.size() + five.size() + six.size());
		result.insert(result.end(), zero.begin(), zero.end());
		result.insert(result.end(), one.begin(), one.end());
		result.insert(result.end(), two.begin(), two.end());
		result.insert(result.end(), three.begin(), three.end());
		result.insert(result.end(), four.begin(), four.end());
		result.insert(result.end(), five.begin(), five.end());
		result.insert(result.end(), six.begin(), six.end());
		return result;
	}

	static AbstractSubExpression AndExpression(const AbstractSubExpression& lhs, const AbstractSubExpression& rhs)
	{
		if (lhs.empty()) return rhs;
		if (rhs.empty()) return lhs;

		if (lhs.size() == 1) {
			if (lhs[0] == s_fixedTokenTrue) return rhs;
			if (lhs[0] == s_fixedTokenFalse) return {s_fixedTokenFalse};
		}

		if (rhs.size() == 1) {
			if (rhs[0] == s_fixedTokenTrue) return lhs;
			if (rhs[0] == s_fixedTokenFalse) return {s_fixedTokenFalse};
		}

		AbstractSubExpression result;
		result.reserve(lhs.size() + rhs.size() + 1);
		result.insert(result.end(), lhs.begin(), lhs.end());
		result.insert(result.end(), rhs.begin(), rhs.end());
		result.push_back(s_fixedTokenLogicalAnd);
		return result;
	}

	static AbstractSubExpression OrExpression(const AbstractSubExpression& lhs, const AbstractSubExpression& rhs)
	{
		if (lhs.empty()) return rhs;
		if (rhs.empty()) return lhs;

		if (lhs.size() == 1) {
			if (lhs[0] == s_fixedTokenTrue) return {s_fixedTokenTrue};
			if (lhs[0] == s_fixedTokenFalse) return rhs;
		}

		if (rhs.size() == 1) {
			if (rhs[0] == s_fixedTokenTrue) return {s_fixedTokenTrue};
			if (rhs[0] == s_fixedTokenFalse) return lhs;
		}

		AbstractSubExpression result;
		result.reserve(lhs.size() + rhs.size() + 1);
		result.insert(result.end(), lhs.begin(), lhs.end());
		result.insert(result.end(), rhs.begin(), rhs.end());
		result.push_back(s_fixedTokenLogicalOr);
		return result;
	}

	static AbstractSubExpression InvertExpression(const AbstractSubExpression& expr)
	{
		if (expr.size() == 1) {
			if (expr[0] == s_fixedTokenTrue) return {s_fixedTokenFalse};
			if (expr[0] == s_fixedTokenFalse) return {s_fixedTokenTrue};
		}

		AbstractSubExpression result;
		result.reserve(expr.size() + 2);
		result.push_back(s_fixedTokenUnaryMarker);
		result.insert(result.end(), expr.begin(), expr.end());
		result.push_back(s_fixedTokenNot);
		return result;
	}

	static WorkingRelevanceTable MergeRelevanceTables(
		const WorkingRelevanceTable& lhs, const AbstractSubExpression& lhsCondition,
		const WorkingRelevanceTable& rhs, const AbstractSubExpression& rhsCondition)
	{
		WorkingRelevanceTable result;

		// note that we have to use an "ordered" map here to make the merging
		// efficient. Using an std::unordered_map here would probably result
		// in a significant amount of re-hashing

		auto lhsi = lhs.begin();
		auto rhsi = rhs.begin();
		for (;;) {
			if (lhsi == lhs.end() && rhsi == rhs.end()) break;
			while (lhsi != lhs.end() && (rhsi == rhs.end() || lhsi->first < rhsi->first)) {
				result.insert(std::make_pair(lhsi->first, AndExpression(lhsi->second, lhsCondition)));
				++lhsi;
			}
			while (rhsi != rhs.end() && (lhsi == lhs.end() || rhsi->first < lhsi->first)) {
				result.insert(std::make_pair(rhsi->first, AndExpression(rhsi->second, rhsCondition)));
				++rhsi;
			}
			if (lhsi != lhs.end() && rhsi != rhs.end() && lhsi->first == rhsi->first) {
				auto lhsPart = AndExpression(lhsi->second, lhsCondition);
				auto rhsPart = AndExpression(rhsi->second, rhsCondition);
				result.insert(std::make_pair(lhsi->first, OrExpression(lhsPart, rhsPart)));
				++lhsi;
				++rhsi;
			}
		}

		return result;
	}

	static std::string AsString(class AbstractExpression& tokenTable, const AbstractSubExpression& subExpression)
	{
		std::stack<std::string> evaluation;
		for (auto tokenIdx:subExpression) {
			const auto& token = tokenTable._tokens[tokenIdx];

			if (token._type == AbstractExpression::TokenType::Operation) {
				auto r_token = std::move(evaluation.top()); evaluation.pop();
				auto l_token = std::move(evaluation.top()); evaluation.pop();
				if (l_token.empty()) {	// we get an empty string for the unary marker
					evaluation.push(Concatenate("(", token._value, r_token, ")"));
				} else {
					evaluation.push(Concatenate("(", l_token, " ", token._value, " ", r_token, ")"));
				}
			} else if (token._type == AbstractExpression::TokenType::UnaryMarker) {
				evaluation.push({});
			} else if (token._type == AbstractExpression::TokenType::IsDefinedTest) {
				evaluation.push(Concatenate("defined(", token._value, ")"));
			} else {
				evaluation.push(token._value);
			}
		}
		assert(evaluation.size() == 1);
		return evaluation.top();
	}

	RelevanceTable AsRelevanceTable(class AbstractExpression& tokenTable, const WorkingRelevanceTable& input)
	{
		RelevanceTable result;
		for (const auto&e:input) {
			auto& varToken = tokenTable._tokens[e.first];
			if (varToken._type == AbstractExpression::TokenType::Variable) {
				result._items.insert(std::make_pair(varToken._value, AsString(tokenTable, e.second)));
			} else if (varToken._type == AbstractExpression::TokenType::IsDefinedTest) {
				result._items.insert(std::make_pair(Concatenate("defined(", varToken._value, ")"), AsString(tokenTable, e.second)));
			} else {
				assert(0);
			}
		}
		return result;
	}

	void PushBackToken(AbstractExpression& expr, AbstractExpression::TokenType type, const std::string& value)
	{
		AbstractExpression::Token token { type, value };
		auto existing = std::find(expr._tokens.begin(), expr._tokens.end(), token);
		if (existing == expr._tokens.end()) {
			expr._reversePolishOrdering.push_back((unsigned)expr._tokens.size());
			expr._tokens.push_back(token);
		} else {
			expr._reversePolishOrdering.push_back((unsigned)std::distance(expr._tokens.begin(), existing));
		}
	}

	/* TokenMap vars;
	vars["__VERSION__"] = 300;
	vars["DEFINED_NO_VALUE"] = packToken(nullptr, NONE);        // tokens with no value can't be part of expressions
	bool undefinedIsDefined = calculator::calculate("defined(NOT_DEFINED)", &vars).asBool();
	bool noValueIsDefined = calculator::calculate("defined(DEFINED_NO_VALUE)", &vars).asBool();
	bool noValueIsZero = calculator::calculate("NOT_DEFINED == 0", &vars).asBool();
	bool res = calculator::calculate("defined(__VERSION__) && defined(DEFINED_NO_VALUE) && !defined(NOT_DEFINED) && (__VERSION__ >= 300)", &vars).asBool();

	(void)res; (void)undefinedIsDefined; (void)noValueIsDefined; (void)noValueIsZero;
	return res;*/
}

