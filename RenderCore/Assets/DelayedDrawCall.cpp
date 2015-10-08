// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DelayedDrawCall.h"
#include "../../Utility/MemoryUtils.h"
#include <algorithm>

namespace RenderCore { namespace Assets
{
    void DelayedDrawCallSet::Reset() 
    {
        for (unsigned c=0; c<dimof(_entries); ++c)
            _entries[c].erase(_entries[c].begin(), _entries[c].end());
        _transforms.erase(_transforms.begin(), _transforms.end());
    }

    size_t DelayedDrawCallSet::GetRendererGUID() const
    {
        return _guid;
    }

    DelayedDrawCallSet::DelayedDrawCallSet(size_t rendererGuid) 
    {
        _guid = rendererGuid;
        for (unsigned c=0; c<dimof(_entries); ++c)
            _entries[c].reserve(10*1000);
    }

    bool DelayedDrawCallSet::IsEmpty() const
    {
        for (auto i:_entries)
            if (!i.empty()) return false;
        return true;
    }

    DelayedDrawCallSet::~DelayedDrawCallSet() {}

    void DelayedDrawCallSet::Filter(const Predicate& predicate)
    {
        for (unsigned c=0; c<dimof(_entries); ++c)
            _entries[c].erase(std::remove_if(_entries[c].begin(), _entries[c].end(), std::not1(predicate)), _entries[c].end());
    }

}}

