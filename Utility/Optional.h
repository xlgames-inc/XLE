// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#if (__cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) && !ANDROID
	#include <optional>
#elif __has_include(<experimental/optional>) && !ANDROID
    #if defined(_LIBCPP_WARN_ON_DEPRECATED_EXPERIMENTAL_HEADER)
        #undef _LIBCPP_WARN_ON_DEPRECATED_EXPERIMENTAL_HEADER
    #endif
    #include <experimental/optional>
    namespace std
    {
        // Expose the std::optional type. This is part of the C++17 standard, but only provided
        // as part of the "std::experimental" namespace in our version of the C++ library
        template <typename T>
            using optional = std::experimental::optional<T>;
         constexpr std::experimental::nullopt_t nullopt = std::experimental::nullopt;
    }
#else
     #include "../Foreign/optional-lite/include/nonstd/optional.hpp"
     namespace std
     {
         template <typename T>
            using optional = nonstd::optional<T>;
         constexpr nonstd::nullopt_t nullopt = nonstd::nullopt;
     }
#endif
