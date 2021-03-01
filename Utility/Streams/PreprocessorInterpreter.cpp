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

	using WorkingRelevanceTable = std::map<std::string, std::string>;

	static WorkingRelevanceTable MergeRelevanceTables(
		const WorkingRelevanceTable& lhs, const std::string& lhsCondition,
		const WorkingRelevanceTable& rhs, const std::string& rhsCondition);

	static std::string InvertExpression(const std::string& expr);
	static std::string AddExpression(const std::string& lhs, const std::string& rhs);
	static std::string OrExpression(const std::string& lhs, const std::string& rhs);

	static RelevanceTable AsRelevanceTable(const WorkingRelevanceTable& input);

	static std::string s_trueRelevance = "1";

	RelevanceTable CalculatePreprocessorExpressionRevelance(StringSection<> input)
	{
		// For the given expression, we want to figure out how variables are used, and under what conditions
		// they impact the result of the evaluation

		TokenMap vars;
		auto rpn = calculator::toRPN(input.AsString().c_str(), vars);

		struct PartialExpression
		{
			WorkingRelevanceTable _relevance;
			std::unique_ptr<TokenBase> _token;
			std::string _expandedExpression;
		};

		std::stack<PartialExpression> evaluation;
		while (!rpn.empty()) {
			std::unique_ptr<TokenBase> base { rpn.front()->clone() };
			rpn.pop();

			// Operator:
			if (base->type == OP) {
				auto op = static_cast<Token<std::string>*>(base.get())->val;

				PartialExpression r_token = std::move(evaluation.top()); evaluation.pop();
				PartialExpression l_token = std::move(evaluation.top()); evaluation.pop();

				// For logical operations, we need to carefully consider the left and right
				// relevance tables. For defined(), we will simplify the relevance to show
				// that we only care whether the symbol is defined or now.
				// For other operations, we will basically just merge together the relevance tables for both left and right

				PartialExpression newPartialExpression;
				if (op == "()") {
					// Function calls. Here we only care about "defined()"
					// We're expecting the r_token to have only one entry, and for that to have a "=1" relevance

					assert(l_token._token && (l_token._token->type & REF));
					auto* resolvedRef = static_cast<RefToken*>(l_token._token.get())->resolve();

					if (!resolvedRef || static_cast<CppFunction*>(resolvedRef)->name() != "defined()")
						Throw(std::runtime_error("Only defined() is supported in relevance checks. Other functions are not supported"));
					if (r_token._relevance.size() != 1 || r_token._relevance.begin()->second != s_trueRelevance)
						Throw(std::runtime_error("Relevance table is unexpected while evaluating an expression. Could there have been an expression inside of a defined() check?"));
					
					std::string definedExpr = "defined(" + r_token._expandedExpression + ")";
					newPartialExpression._relevance.insert(std::make_pair(definedExpr, s_trueRelevance));
					newPartialExpression._expandedExpression = definedExpr;
				} else {
					if (op == "&&") {
						// lhs variables relevant when rhs expression is true
						// rhs variables relevant when lhs expression is true
						newPartialExpression._relevance = MergeRelevanceTables(
							l_token._relevance, r_token._expandedExpression,
							r_token._relevance, l_token._expandedExpression);
					} else if (op == "||") {
						// lhs variables relevant when rhs expression is false
						// rhs variables relevant when lhs expression is false
						newPartialExpression._relevance = MergeRelevanceTables(
							l_token._relevance, InvertExpression(r_token._expandedExpression),
							r_token._relevance, InvertExpression(l_token._expandedExpression));
					} else {
						newPartialExpression._relevance = MergeRelevanceTables(l_token._relevance, {}, r_token._relevance, {});
					}

					if (l_token._token && l_token._token->type == UNARY) {
						newPartialExpression._expandedExpression = op + " (" + r_token._expandedExpression + ")";
					} else {
						newPartialExpression._expandedExpression = "(" + l_token._expandedExpression + ") " + op + " (" + r_token._expandedExpression + ")";
					}
				}

				evaluation.push(std::move(newPartialExpression));

			} else if (base->type == UNARY) {

				PartialExpression newPartialExpression;
				newPartialExpression._token = std::move(base);
				evaluation.push(std::move(newPartialExpression));
			
			} else if (base->type == VAR) {
				std::string key = static_cast<Token<std::string>*>(base.get())->val;

				PartialExpression newPartialExpression;
				newPartialExpression._relevance.insert(std::make_pair(key, s_trueRelevance));
				newPartialExpression._expandedExpression = key;
				newPartialExpression._token = std::move(base);
				evaluation.push(std::move(newPartialExpression));
				
			} else {
				
				PartialExpression newPartialExpression;
				newPartialExpression._expandedExpression = packToken::str(base.get());
				newPartialExpression._token = std::move(base);
				evaluation.push(std::move(newPartialExpression));

			}
		}

		assert(evaluation.size() == 1);
		return AsRelevanceTable(evaluation.top()._relevance);
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

	static std::string AndExpression(const std::string& lhs, const std::string& rhs)
	{
		if (!lhs.empty() && !rhs.empty())
			return Concatenate("(", lhs, ") && (", rhs, ")");
		else if (!lhs.empty())
			return lhs;
		else
			return rhs;
	}

	static std::string OrExpression(const std::string& lhs, const std::string& rhs)
	{
		if (!lhs.empty() && !rhs.empty())
			return Concatenate("(", lhs, ") || (", rhs, ")");
		else if (!lhs.empty())
			return lhs;
		else
			return rhs;
	}

	static std::string InvertExpression(const std::string& expr)
	{
		return Concatenate("!(", expr, ")");
	}

	static WorkingRelevanceTable MergeRelevanceTables(
		const WorkingRelevanceTable& lhs, const std::string& lhsCondition,
		const WorkingRelevanceTable& rhs, const std::string& rhsCondition)
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

	RelevanceTable AsRelevanceTable(const WorkingRelevanceTable& input)
	{
		RelevanceTable result;
		for (const auto&e:input)
			result._items.insert(e);
		return result;
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

