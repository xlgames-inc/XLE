// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Exceptions.h"
#include <ostream>

namespace Utility
{
    template<typename CharType, typename CharTraits>
        std::basic_ostream<CharType, CharTraits>& operator<<(
            std::basic_ostream<CharType, CharTraits>& stream,
            const ::Exceptions::BasicLabel& exception)
        {
            stream << exception.what();
            return stream;
        }

    template<typename CharType, typename CharTraits>
        std::basic_ostream<CharType, CharTraits>& operator<<(
            std::basic_ostream<CharType, CharTraits>& stream,
            const std::exception& exception)
        {
            stream << exception.what();
            return stream;
        }
}

using namespace Utility;
