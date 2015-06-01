// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Terrain.h"
#include "TerrainInternal.h"
#include "../Math/Transformations.h"
#include "../RenderCore/Resource.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/Streams/FileUtils.h"
#include "../Utility/HeapUtils.h"
#include <memory>

namespace SceneEngine
{
    class TerrainNodeHeightCollision
    {
    public:
        float GetHeight(Float2 cellBasedCoord) const;

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const   { return _validationCallback; }

        TerrainNodeHeightCollision(const char cellFilename[], ITerrainFormat& ioFormat, unsigned nodeIndex);
        ~TerrainNodeHeightCollision();
    protected:
        TerrainCell::Node			_scaffoldData;
        std::unique_ptr<uint16[]>	_heightData;
        std::shared_ptr<Assets::DependencyValidation>  _validationCallback;

        float GetHeightSample(Int2 coord) const;
    };

    inline float TerrainNodeHeightCollision::GetHeightSample(Int2 coord) const
    {
        assert(coord[1] < int(_scaffoldData._widthInElements) && coord[0] < int(_scaffoldData._widthInElements));
        auto rawSample = _heightData.get()[coord[1] * _scaffoldData._widthInElements + coord[0]];
        return float(rawSample) * _scaffoldData._localToCell(2, 2) + _scaffoldData._localToCell(2, 3);
    }

    float TerrainNodeHeightCollision::GetHeight(Float2 cellBasedCoord) const
    {
        Float2 nodeCoord(
            (cellBasedCoord[0] - _scaffoldData._localToCell(0,3)) / _scaffoldData._localToCell(0,0),
            (cellBasedCoord[1] - _scaffoldData._localToCell(1,3)) / _scaffoldData._localToCell(1,1));
        nodeCoord *= float(_scaffoldData._widthInElements - _scaffoldData.GetOverlapWidth());
        Float2 A(XlFloor(nodeCoord[0]), XlFloor(nodeCoord[1]));
        Int2 baseIndex((int)A[0], (int)A[1]);
        if (   baseIndex[0] < 0 || baseIndex[1] < 0
            || (baseIndex[0]+1) >= int(_scaffoldData._widthInElements) 
            || (baseIndex[1]+1) >= int(_scaffoldData._widthInElements)) {
            assert(0);      // probably testing the wrong node. The coordinates given don't match the node transform
            return 0.f;
        }
        Float2 B = nodeCoord - A;

            //  Typical bilinear filtering -- get results with 4 taps.
            //  Note that this isn't exactly the same as the rendered result. When rendering
            //  the quad, the edge in the center may become a hill or a valley. But here, the 
            //  bilinear filter will smooth that out differently. It may result in objects
            //  hovering. If we know the direction of the center edge, we can do this test
            //  with just 3 taps.
        float h0 = GetHeightSample(baseIndex);
        float h1 = GetHeightSample(baseIndex + Int2(1,0));
        float h2 = GetHeightSample(baseIndex + Int2(0,1));
        float h3 = GetHeightSample(baseIndex + Int2(1,1));
        float w0 = (1.0f - B[0]) * (1.f - B[1]);
        float w1 = B[0] * (1.f - B[1]);
        float w2 = (1.0f - B[0]) * B[1];
        float w3 = B[0] * B[1];
        return h0 * w0 + h1 * w1 + h2 * w2 + h3 * w3;
    }

    TerrainNodeHeightCollision::TerrainNodeHeightCollision(const char cellFilename[], ITerrainFormat& ioFormat, unsigned nodeIndex)
        : _scaffoldData(Identity<Float4x4>(), 0, 0, 0)
    {
        auto& cell = ioFormat.LoadHeights(cellFilename);
        if (nodeIndex >= cell._nodes.size()) {
            throw ::Exceptions::BasicLabel("Bad node index in TerrainNodeHeightCollision");
        }

            //  We need to reopen the file, and a load the raw height map data 
            //  from the file. This will be stored CPU-side so that we can
            //  do physics queries on it. We'll store the height map data
            //  in the same format it exist on disk: 16 bit values within
            //  a coordinate space defined by a single precision floating 
            //  point transform.
        auto& node = *cell._nodes[nodeIndex];
        auto heightData = std::make_unique<uint16[]>(node._heightMapFileSize/2);
        {
            BasicFile file(cellFilename, "rb");
            file.Seek(node._heightMapFileOffset, SEEK_SET);
            file.Read(heightData.get(), 1, node._heightMapFileSize);
        }

        auto validCallback = std::make_shared<Assets::DependencyValidation>();
        ::Assets::RegisterAssetDependency(validCallback, cell.GetDependencyValidation());
        ::Assets::RegisterFileDependency(validCallback, cellFilename);

        _heightData = std::move(heightData);
        _validationCallback = std::move(validCallback);
        _scaffoldData = node;
    }

    TerrainNodeHeightCollision::~TerrainNodeHeightCollision()
    {}

    extern Int2 TerrainOffset;

    float GetTerrainHeight(ITerrainFormat& ioFormat, const TerrainConfig& cfg, const TerrainCoordinateSystem& coords, Float2 queryPosition)
    {
        TRY
        {
                //
                //  Find the cell and node that contains this position.
                //  Once we've found it, we need to find a cached TerrainNodeHeightCollision
                //  for the given node, and get the height data from that.
                //
                //  We're going to make some assumptions to make this faster. 
                //      * We'll assume that the cells are arranged in a grid, so we can find the cell quickly
                //      * we'll also make similar assumptions about the arrangement of nodes within
                //          the cell, so we can find the node index directly (within loading the cell node)
                //  

            auto worldToCell = coords.WorldToCellBased();
            auto cellBasedCoord = Truncate(
                TransformPoint(worldToCell, Expand(queryPosition, 0.f)));

            Float2 cellIndex(XlFloor(cellBasedCoord[0]), XlFloor(cellBasedCoord[1]));

            if (    cellIndex[0] < 0.f || cellIndex[0] >= float(cfg._cellCount[0])
                ||  cellIndex[1] < 0.f || cellIndex[1] >= float(cfg._cellCount[1])) {
                return 0.f;
            }

            Float2 cellFrac(cellBasedCoord[0] - cellIndex[0], cellBasedCoord[1] - cellIndex[1]);
        
            auto cellDimsInNodes = cfg.CellDimensionsInNodes();
            float nodeX = XlFloor(cellFrac[0] * float(cellDimsInNodes[0]));
            float nodeY = XlFloor(cellFrac[1] * float(cellDimsInNodes[1]));
            unsigned nodeIndex = 85 + unsigned(nodeY) * cellDimsInNodes[0] + unsigned(nodeX);

            uint64 cellHash = (uint64(nodeIndex) << 32ull) | (uint64(cellIndex[1]) << 6ull) | uint64(cellIndex[0]);

                // (simple cache for recently used terrain files -- so we don't have to continually re-load every frame)
                //      -- \todo -- this cache should be in a manager object! todo many statics in functions!
            static LRUCache<TerrainNodeHeightCollision> CollisionCache(8);
            auto collisionObject = CollisionCache.Get(cellHash);
            if (!collisionObject) {
                char cellFilename[MaxPath];
                cfg.GetCellFilename(cellFilename, dimof(cellFilename), UInt2(unsigned(cellIndex[0]), unsigned(cellIndex[1])), CoverageId_Heights);
                collisionObject = std::make_shared<TerrainNodeHeightCollision>(cellFilename, ioFormat, nodeIndex);
                CollisionCache.Insert(cellHash, collisionObject);
            }

            assert(collisionObject);
            return collisionObject->GetHeight(cellFrac) + coords.TerrainOffset()[2];

        } CATCH(const ::Assets::Exceptions::PendingResource&) {
        } CATCH(const std::exception&) {
            // we can sometimes get missing files. Just return a default height
            LogWarning << "Error when querying terrain height at " << queryPosition[0] << ", " << queryPosition[1];
        } CATCH_END

        return 0.f;
    }

}