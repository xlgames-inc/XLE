// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Utility/StringUtils.h"

namespace GraphLanguage
{
	// type traits
	bool IsStructType(StringSection<char> typeName);
	bool CanBeStoredInCBuffer(const StringSection<char> type);
}

