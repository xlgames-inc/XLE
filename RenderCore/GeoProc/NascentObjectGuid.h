// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"

namespace RenderCore { namespace Assets { namespace GeoProc
{
    class ObjectGuid
    {
    public:
        uint64  _objectId;
        uint64  _namespaceId;

        ObjectGuid() : _objectId(~0ull), _namespaceId(~0ull) {}
        ObjectGuid(uint64 objectId, uint64 fileId = 0) : _objectId(objectId), _namespaceId(fileId) {}
    };

    inline bool operator==(const ObjectGuid& lhs, const ObjectGuid& rhs)   { return (lhs._objectId == rhs._objectId) && (lhs._namespaceId == rhs._namespaceId); }
    inline bool operator<(const ObjectGuid& lhs, const ObjectGuid& rhs)    { if (lhs._namespaceId < rhs._namespaceId) return true; return lhs._objectId < rhs._objectId; }

}}}
