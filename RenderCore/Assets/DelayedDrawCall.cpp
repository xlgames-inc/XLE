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

    const GUID& DelayedDrawCallSet::GetRendererGUID() const
    {
        return *(const GUID*)&_guid;
    }

    DelayedDrawCallSet::DelayedDrawCallSet(const GUID& rendererGuid) 
    {
        XlCopyMemory(_guid, &rendererGuid, sizeof(_guid));
        for (unsigned c=0; c<dimof(_entries); ++c)
            _entries[c].reserve(10*1000);
    }

    DelayedDrawCallSet::~DelayedDrawCallSet() {}

    void DelayedDrawCallSet::Filter(const Predicate& predicate)
    {
        for (unsigned c=0; c<dimof(_entries); ++c)
            _entries[c].erase(std::remove_if(_entries[c].begin(), _entries[c].end(), std::not1(predicate)), _entries[c].end());
    }

}}

