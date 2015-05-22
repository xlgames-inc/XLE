// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DataSerialize.h"

namespace Utility
{
    std::unique_ptr<Data> SerializeToData(
        const char name[], 
        const std::vector<std::pair<const char*, std::string>>& table)
    {
        auto result = std::make_unique<Data>(name);
        for (auto i=table.cbegin(); i!=table.cend(); ++i) {
            result->SetAttribute(i->first, i->second.c_str());
        }
        return std::move(result);
    }
}

