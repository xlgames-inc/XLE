// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"
#include "../../Core/Types.h"
#include <vector>
#include <utility>

namespace GUILayer
{
    public ref class ObjectSet
    {
    public:
        void Add(uint64 document, uint64 id);

        typedef std::vector<std::pair<uint64, uint64>> UnderlyingArray;
        clix::auto_ptr<UnderlyingArray> _underlying;

        ObjectSet();
        ~ObjectSet();
    };
}