// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../StringUtils.h"
#include "../IteratorUtils.h"
#include <unordered_map>

namespace Utility
{
	class ParameterBox;

    bool EvaluatePreprocessorExpression(
        StringSection<> input,
        const std::unordered_map<std::string, int>& definedTokens);

	bool EvaluatePreprocessorExpression(
        StringSection<> input,
        IteratorRange<const ParameterBox**> definedTokens);
}

using namespace Utility;

