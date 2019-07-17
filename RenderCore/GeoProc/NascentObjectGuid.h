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
        uint64_t  _objectId;
        uint64_t  _namespaceId;

        NascentObjectGuid() : _objectId(~0ull), _namespaceId(~0ull) {}
        NascentObjectGuid(uint64_t objectId, uint64_t fileId = 0) : _objectId(objectId), _namespaceId(fileId) {}
    };

    inline bool operator==(const NascentObjectGuid& lhs, const NascentObjectGuid& rhs)   { return (lhs._objectId == rhs._objectId) && (lhs._namespaceId == rhs._namespaceId); }
	inline bool operator!=(const NascentObjectGuid& lhs, const NascentObjectGuid& rhs)   { return !operator==(lhs, rhs); }
    inline bool operator<(const NascentObjectGuid& lhs, const NascentObjectGuid& rhs)    { if (lhs._namespaceId < rhs._namespaceId) return true; return lhs._objectId < rhs._objectId; }

}}}
