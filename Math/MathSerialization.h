// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Vector.h"
#include "Matrix.h"
#include "../Utility/ImpliedTyping.h"
#include <iosfwd>

namespace cml
{
    template <typename Type, int Count>
        constexpr Utility::ImpliedTyping::TypeDesc InternalTypeOf(cml::vector<Type, cml::fixed<Count>> const*)
    { 
        return Utility::ImpliedTyping::TypeDesc{Utility::ImpliedTyping::TypeOf<Type>()._type, Count, Utility::ImpliedTyping::TypeHint::Vector};
    }

    constexpr Utility::ImpliedTyping::TypeDesc InternalTypeOf(Float3x3 const*)      { return Utility::ImpliedTyping::TypeDesc{Utility::ImpliedTyping::TypeCat::Float, 9, Utility::ImpliedTyping::TypeHint::Matrix}; }
    constexpr Utility::ImpliedTyping::TypeDesc InternalTypeOf(Float3x4 const*)      { return Utility::ImpliedTyping::TypeDesc{Utility::ImpliedTyping::TypeCat::Float, 12, Utility::ImpliedTyping::TypeHint::Matrix}; }
    constexpr Utility::ImpliedTyping::TypeDesc InternalTypeOf(Float4x4 const*)      { return Utility::ImpliedTyping::TypeDesc{Utility::ImpliedTyping::TypeCat::Float, 16, Utility::ImpliedTyping::TypeHint::Matrix}; }
}

namespace XLEMath
{
    std::ostream& CompactTransformDescription(std::ostream& str, const Float4x4& transform);
}
