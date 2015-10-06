// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

namespace Utility
{
///////////////////////////////////////////////////////////////////////////////////////////////////
        // this implementation from stack overflow:
        //      http://stackoverflow.com/questions/25958259/how-do-i-find-out-if-a-tuple-contains-a-type
    template <typename T, typename Tuple>
    struct HasType;

    template <typename T>
    struct HasType<T, std::tuple<>> : std::false_type {};

    template <typename T, typename U, typename... Ts>
    struct HasType<T, std::tuple<U, Ts...>> : HasType<T, std::tuple<Ts...>> {};

    template <typename T, typename... Ts>
    struct HasType<T, std::tuple<T, Ts...>> : std::true_type {};
///////////////////////////////////////////////////////////////////////////////////////////////////

    namespace Internal
    {
        template <class T, std::size_t N, class... Args>
            struct IndexOfType
        {
            static const auto value = N;
        };

        template <class T, std::size_t N, class... Args>
            struct IndexOfType<T, N, T, Args...>
        {
            static const auto value = N;
        };

        template <class T, std::size_t N, class U, class... Args>
            struct IndexOfType<T, N, U, Args...>
        {
            static const auto value = IndexOfType<T, N + 1, Args...>::value;
        };
    }

    template <class T, class... Args>
        const T& GetByType(const std::tuple<Args...>& t)
    {
        return std::get<Internal::IndexOfType<T, 0, Args...>::value>(t);
    }
}

using namespace Utility;
