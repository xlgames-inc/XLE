// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PreprocessorInterpreter.h"
#include "../../Core/Exceptions.h"
#include "../../Foreign/cparse/shunting-yard.h"
#include "../../Foreign/cparse/shunting-yard-exceptions.h"

#include <cmath>

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
    bool EvaluatePreprocessorExpression(
        StringSection<> input,
        const std::unordered_map<std::string, int>& definedTokens)
    {
        static bool hasSetupPreprocOps = false;
        if (!hasSetupPreprocOps) {
            preprocessor_operations::Startup();
            hasSetupPreprocOps = true;
        }

        TokenMap vars;
        for (const auto&i:definedTokens)
            vars[i.first] = packToken(i.second);

        // symbols with no value can be defined like this: (but they aren't particularly useful in expressions, except when used with the defined() function)
        // vars["DEFINED_NO_VALUE"] = packToken(nullptr, NONE);

        return calculator::calculate(input.AsString().c_str(), &vars).asBool();

        // those that this can throw exceptions back to the caller (for example, if the input can't be parsed)
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

