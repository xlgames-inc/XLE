// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../Core/Types.h"
#include <vector>
#include <functional>

namespace RenderCore { namespace Assets
{
    class ModelRenderer;

    class PreparedModelDrawCallEntry
    {
    public:
        unsigned        _shaderVariationHash;

        ModelRenderer*  _renderer;
        unsigned        _drawCallIndex;
        Float4x4        _meshToWorld;

        unsigned        _indexCount, _firstIndex, _firstVertex;
        unsigned        _topology;  // (Metal::Topology::Enum)

        uint64          _materialGuid;
        
        const void*     _mesh;
    };

    class PreparedModelDrawCalls
    {
    public:
        std::vector<PreparedModelDrawCallEntry> _entries;

        using Predicate = std::function<bool(const PreparedModelDrawCallEntry&)>;
        
        void Reset();
        void Filter(const Predicate& predicate);
            
        PreparedModelDrawCalls();
        ~PreparedModelDrawCalls();
    };
}}

