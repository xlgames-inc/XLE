// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Math/Vector.h"
#include "../Math/Matrix.h"
#include <utility>
#include <memory>


namespace SceneEngine
{
    /// <summary>Quad tree arrangement for static placements</summary>
    /// Given a set of objects (identified by cell-space bounding boxes)
    /// calculate a balanced quad tree. This can be used to optimise camera
    /// frustum.
    ///
    /// Use "CalculateVisibleObjects" to perform camera frustum tests
    /// using the quad tree information.
    ///
    /// Note that all object culling is done using bounding boxes axially
    /// aligned in cell-space (not object local space). This can be a little
    /// less accurate than object space. But it avoids an expensive matrix
    /// multiply. If the world space bounding box straddles the edge of the
    /// frustum, the caller may wish to perform a local space bounding
    /// box test to further improve the result.
    class PlacementsQuadTree
    {
    public:
        typedef std::pair<Float3, Float3> BoundingBox;

        class Metrics
        {
        public:
            unsigned _nodeAabbTestCount;
            unsigned _payloadAabbTestCount;

            Metrics() : _nodeAabbTestCount(0), _payloadAabbTestCount(0) {}
        };

        bool CalculateVisibleObjects(
            const Float4x4& cellToClipAligned,
            const BoundingBox objCellSpaceBoundingBoxes[], size_t objStride,
            unsigned visObjs[], unsigned& visObjsCount, unsigned visObjMaxCount,
            Metrics* metrics = nullptr) const;

        unsigned GetMaxResults() const;

        PlacementsQuadTree(
            const BoundingBox objCellSpaceBoundingBoxes[], size_t objStride,
            size_t objCount);
        ~PlacementsQuadTree();

    protected:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
        
        friend class PlacementsQuadTreeDebugger;
    };
}