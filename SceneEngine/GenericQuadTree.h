#pragma once

#include "../Math/Vector.h"
#include "../Math/Matrix.h"
#include "../Assets/AssetsCore.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/StringUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Core/Types.h"
#include <utility>
#include <memory>

namespace Assets { class ChunkFileContainer; }
namespace XLEMath { enum class ClipSpaceType; }

namespace SceneEngine
{
    /// <summary>Quad tree arrangement for static object</summary>
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
    class GenericQuadTree
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
            const Float4x4& cellToClipAligned, ClipSpaceType clipSpaceType,
            const BoundingBox objCellSpaceBoundingBoxes[], size_t objStride,
            unsigned visObjs[], unsigned& visObjsCount, unsigned visObjMaxCount,
            Metrics* metrics = nullptr) const;
        unsigned GetMaxResults() const;

		enum class Orientation { YUp, ZUp };

		static std::vector<uint8_t> BuildQuadTree(
            const BoundingBox objCellSpaceBoundingBoxes[], size_t objStride,
            size_t objCount, unsigned leafThreshold,
			Orientation orientation = Orientation::ZUp);

		GenericQuadTree(const ::Assets::ChunkFileContainer& chunkFile);
		GenericQuadTree(std::unique_ptr<uint8[], PODAlignedDeletor>&& dataBlock);
		GenericQuadTree();
        ~GenericQuadTree();

		GenericQuadTree(GenericQuadTree&& moveFrom);
		GenericQuadTree& operator=(GenericQuadTree&& moveFrom) never_throws;

		const ::Assets::DepValPtr& GetDependencyValidation() const { return _depVal; }
		std::vector<std::pair<Float3, Float3>> GetNodeBoundingBoxes() const;

    protected:
        class Pimpl;
		std::unique_ptr<uint8[], PODAlignedDeletor> _dataBlock;
		::Assets::DepValPtr _depVal;

		const Pimpl& GetPimpl() const;
        
        friend class GenericQuadTreeDebugger;
    };
}
