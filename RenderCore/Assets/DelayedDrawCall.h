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
    class DelayedDrawCall
    {
    public:
            // The "variation hash" is the primary sorting
            // parameter. It should represent the type of shader
            // to use. It becomes a critical factor when sorting
            // lists of draw calls.
        unsigned        _shaderVariationHash;

            // 
        const void*     _renderer;
        const void*     _subMesh;
        unsigned        _drawCallIndex;
        unsigned        _meshToWorld;       // index into "_transforms" in the DelayedDrawCallSet

            // 
        unsigned        _indexCount, _firstIndex, _firstVertex;
        unsigned        _topology;  // (Metal::Topology::Enum)
    };

    enum class DelayStep : unsigned
    {
        OpaqueRender,
        PostDeferred,       // forward rendering mode after the deferred render step (where alpha blending can be used)
        SortedBlending,     // blending step with pixel-accurate depth sorting
        Max
    };

    /// <summary>Holds a collection of draw calls that have been delayed for later rendering<summary>
    /// If we want to sort the draw calls from multiple objects, we need to first prepare
    /// a large list of all the draw calls (at the sorting granularity). When we will sort
    /// them together, and finally renderer them in the new order. 
    ///
    /// So the steps are:
    /// <list>
    ///  <item>"Prepare" list of draw calls
    ///  <item>Sort
    ///  <item>"Commit" -- in which the true render is performed
    /// </list>
    ///
    /// In this way, draw calls from multiple objects can be mixed and sorted together.
    ///
    /// This object holds lists of draw calls. It is associated with a single renderer
    /// type (for example, ModelRenderer). This means it can only sort draw calls 
    /// associated with that single type. But it means that the "commit" step is 
    /// very efficient. 
    ///
    /// The transforms are kept in a separate array to save memory in cases where
    /// the same transform is used for multiple draw calls (eg, a single mesh with
    /// many materials.
    class DelayedDrawCallSet
    {
    public:
        std::vector<DelayedDrawCall>    _entries[(unsigned)DelayStep::Max];
        std::vector<Float4x4>           _transforms;

        using Predicate = std::function<bool(const DelayedDrawCall&)>;
        
        void        Reset();
        void        Filter(const Predicate& predicate);
        size_t      GetRendererGUID() const;
            
        DelayedDrawCallSet(size_t rendererGuid);
        ~DelayedDrawCallSet();
    protected:
        size_t _guid;
    };
}}

