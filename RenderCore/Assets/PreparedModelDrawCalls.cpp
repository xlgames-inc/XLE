// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PreparedModelDrawCalls.h"
#include <algorithm>

namespace RenderCore { namespace Assets
{
    

    void PreparedModelDrawCalls::Reset() 
    {
        _entries.erase(_entries.begin(), _entries.end());
    }

    PreparedModelDrawCalls::PreparedModelDrawCalls() 
    {
        _entries.reserve(10*1000);
    }

    PreparedModelDrawCalls::~PreparedModelDrawCalls() {}

    void PreparedModelDrawCalls::Filter(
        const std::function<bool(const PreparedModelDrawCallEntry&)>& predicate)
    {
        _entries.erase(std::remove_if(_entries.begin(), _entries.end(), std::not1(predicate)), _entries.end());
    }

}}

