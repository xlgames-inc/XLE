// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#if (__cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
	#include <optional>
#else
     #include "../Foreign/optional-lite/include/nonstd/optional.hpp"
     namespace std
     {
         template <typename T>
            using optional = nonstd::optional<T>;
     }
#endif
