// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../StringUtils.h"
#include <unordered_map>

namespace Utility
{
    bool EvaluatePreprocessorExpression(
        StringSection<> input,
        const std::unordered_map<std::string, int>& definedTokens);
}

using namespace Utility;

