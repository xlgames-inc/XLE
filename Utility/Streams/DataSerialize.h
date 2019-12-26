// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Data.h"
#include "../Conversion.h"
#include <vector>
#include <string>
#include <utility>

namespace Utility
{
    template<typename Type> Type Deserialize(const Data* source, const char name[], const Type& def)
        {
            return source ? Deserialize(source->ChildWithValue(name), def) : def;
        }

    template<typename Type> Type Deserialize(const Data* source, const Type& def)
        {
            return (source && source->child) ? Conversion::Convert<Type>(source->child->value) : def;
        }

    template<typename FirstType, typename SecondType>
        std::pair<FirstType, SecondType> Deserialize(const Data* source, const std::pair<FirstType, SecondType>& def)
        {
            auto result = def;
            if (source) {
                if (auto* child0 = source->ChildAt(0))
                    result.first = Conversion::Convert<FirstType>(child0->value);
                if (auto* child1 = source->ChildAt(1))
                    result.second = Conversion::Convert<SecondType>(child1->value);
            }
            return result;
        }

    std::unique_ptr<Data> SerializeToData(
        const char name[], 
        const std::vector<std::pair<const utf8*, std::string>>& table);
}

using namespace Utility;

