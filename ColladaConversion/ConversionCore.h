// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"

namespace RenderCore { namespace ColladaConversion
{
    class ObjectGuid
    {
    public:
        uint64  _objectId;
        uint64  _fileId;

        ObjectGuid() : _objectId(~0ull), _fileId(~0ull) {}
        ObjectGuid(uint64 objectId, uint64 fileId = 0) : _objectId(objectId), _fileId(fileId) {}
    };

    inline bool operator==(const ObjectGuid& lhs, const ObjectGuid& rhs)   { return (lhs._objectId == rhs._objectId) && (lhs._fileId == rhs._fileId); }
    inline bool operator<(const ObjectGuid& lhs, const ObjectGuid& rhs)    { if (lhs._fileId < rhs._fileId) return true; return lhs._objectId < rhs._objectId; }

}}