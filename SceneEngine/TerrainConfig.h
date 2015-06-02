// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TerrainCoverageId.h"
#include "../Math/Vector.h"
#include "../Math/Matrix.h"
#include "../Assets/Assets.h"
#include "../Utility/UTFUtils.h"
#include "../Core/Types.h"

namespace Utility { class OutputStream; }

namespace SceneEngine
{
    class ITerrainFormat;

    /// <summary>Configuration settings for terrain input assets</summary>
    /// This contains informations describing the input assets for a terrain
    /// such as the number of cells, and the size of those cells.
    class TerrainConfig
    {
    public:
        enum Filenames { XLE, Legacy };

        ::Assets::rstring _baseDir;
        ::Assets::rstring _textureCfgName;
        UInt2       _cellCount;
        Filenames   _filenamesMode;

        class CoverageLayer
        {
        public:
            std::basic_string<utf8> _name;
            TerrainCoverageId _id;
            UInt2 _dimensions;
            unsigned _format;
        };

        TerrainConfig(
			const ::Assets::ResChar baseDir[], UInt2 cellCount,
            Filenames filenamesMode = XLE, 
            unsigned nodeDimsInElements = 32u, unsigned cellTreeDepth = 5u, unsigned nodeOverlap = 2u,
            float elementSpacing = 10.f);
		TerrainConfig(const ::Assets::ResChar baseDir[] = "");

        void        GetCellFilename(::Assets::ResChar buffer[], unsigned cnt, UInt2 cellIndex, TerrainCoverageId id) const;
        UInt2x3     CellBasedToCoverage(TerrainCoverageId coverageId) const;

        static void GetUberSurfaceFilename(
            ::Assets::ResChar buffer[], unsigned bufferCount,
            const ::Assets::ResChar directory[],
            TerrainCoverageId fileType);

        UInt2       CellDimensionsInNodes() const;
        UInt2       NodeDimensionsInElements() const;       // (ignoring overlap)
        unsigned    CellTreeDepth() const { return _cellTreeDepth; }
        unsigned    NodeOverlap() const { return _nodeOverlap; }
        float       ElementSpacing() const { return _elementSpacing; }

        unsigned    GetCoverageLayerCount() const;
        const CoverageLayer& GetCoverageLayer(unsigned index) const;

        void        Save();

    protected:
        unsigned    _nodeDimsInElements;
        unsigned    _cellTreeDepth;
        unsigned    _nodeOverlap;
        float       _elementSpacing;
        std::vector<CoverageLayer> _coverageLayers;
    };

    /// <summary>Describes the position and size of terrain in world coordinates<summary>
    /// Terrain has it own native "terrain" and "cell-based" coordinate systems. However, these
    /// might not match world space coordinates exactly. Often we want to specify an extra
    /// translation and scale on the terrain to transform it into world space.
    /// This object jsut encapsulates that transformation.
    class TerrainCoordinateSystem
    {
    public:
        Float4x4    CellBasedToWorld() const;
        Float4x4    WorldToCellBased() const;

        Float3      TerrainOffset() const;
        void        SetTerrainOffset(const Float3& newOffset);

        TerrainCoordinateSystem(
            Float3 terrainOffset = Float3(0.f, 0.f, 0.f),
            float cellSizeInMeters = 0.f)
        : _terrainOffset(terrainOffset)
        , _cellSizeInMeters(cellSizeInMeters) {}

    protected:
        Float3 _terrainOffset;
        float _cellSizeInMeters;
    };

    /// <summary>Loads cached data prepared in a pre-processing step</summary>
    /// This contains extra data that is prepared from the raw input assets in
    /// a pre-processing step.
    /// A good example is the cell bounding boxes. We need all of the cell bounding
    /// boxes from the first frame in order to do top-level culling. But we don't 
    /// want to have to load each cell just to get the bounding box during startup.
    /// So, we prepare all of the bounding boxes and store them within this cached
    /// data.
    class TerrainCachedData
    {
    public:
        class Cell
        {
        public:
            UInt2 _cellIndex;
            std::pair<float, float> _heightRange;
        };
        
        std::vector<Cell> _cells;

        void Write(Utility::OutputStream& stream) const;

        TerrainCachedData();
        TerrainCachedData(const ::Assets::ResChar filename[]);
        TerrainCachedData(const TerrainConfig& cfg, ITerrainFormat& ioFormat);
        TerrainCachedData(TerrainCachedData&& moveFrom);
        TerrainCachedData& operator=(TerrainCachedData&& moveFrom);
    };

    /// Utility class used when calculating all of the cell positions defined
    /// by a terrain config.
    class PrimedCell
    {
    public:
        UInt2 _cellIndex;
        Float4x4 _cellToTerrainCoords;
        std::pair<UInt2, UInt2> _heightUber;
        std::pair<UInt2, UInt2> _coverageUber[5];
    };

    std::vector<PrimedCell> BuildPrimedCells(const TerrainConfig& cfg);

    void WriteTerrainCachedData(Utility::OutputStream& stream, const TerrainConfig& cfg, ITerrainFormat& format);
    void WriteTerrainMaterialData(Utility::OutputStream& stream, const TerrainConfig& cfg);
}

