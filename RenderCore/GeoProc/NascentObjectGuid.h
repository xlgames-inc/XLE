// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <stdint.h>

namespace RenderCore { namespace Assets { namespace GeoProc
{
    class NascentObjectGuid
    {
    public:
        uint64_t  _objectId = ~0ull;
        uint64_t  _namespaceId = ~0ull;
    };

    inline bool operator==(const NascentObjectGuid& lhs, const NascentObjectGuid& rhs)   { return (lhs._objectId == rhs._objectId) && (lhs._namespaceId == rhs._namespaceId); }
	inline bool operator!=(const NascentObjectGuid& lhs, const NascentObjectGuid& rhs)   { return !operator==(lhs, rhs); }
    inline bool operator<(const NascentObjectGuid& lhs, const NascentObjectGuid& rhs)    { if (lhs._namespaceId < rhs._namespaceId) return true; return lhs._objectId < rhs._objectId; }

}}}
