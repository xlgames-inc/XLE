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

    class PrimedCell
    {
    public:
        UInt2 _cellIndex;
        Float4x4 _cellToTerrainCoords;
        std::pair<UInt2, UInt2> _heightUber;
        std::pair<UInt2, UInt2> _coverageUber[5];
    };

    std::vector<PrimedCell> BuildPrimedCells(const TerrainConfig& cfg);
}

