// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Data.h"
#include "../Conversion.h"
#include "../../Math/Vector.h"
#include <vector>
#include <string>

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

    template<typename Type, int Count>
        cml::vector<Type, cml::fixed<Count>> Deserialize(const Data* source, const cml::vector<Type, cml::fixed<Count>>& def)
        {
            auto result = def;
            if (source) {
                for (unsigned c=0; c<Count; ++c)
                    if (auto* child = source->ChildAt(c)) { 
                        result[c] = Conversion::Convert<Type>(child->value);
                    }
            }
            return result;
        }

    std::unique_ptr<Data> SerializeToData(
        const char name[], 
        const std::vector<std::pair<const char*, std::string>>& table);
}

using namespace Utility;

