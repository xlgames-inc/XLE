// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../BufferUploads/IBufferUploads_Forward.h"
#include "../Math/Vector.h"
#include "../Assets/Assets.h"
#include "../Utility/Mixins.h"
#include "../Core/Types.h"

namespace RenderCore { namespace Techniques { class CameraDesc; } }

namespace SceneEngine
{
    class LightingParserContext;
    class TechniqueContext;
    class TerrainUberSurfaceInterface;
    class ITerrainFormat;
    class ISurfaceHeightsProvider;
    
    class TerrainConfig
    {
    public:
        enum Filenames { XLE, Legacy };

        ::Assets::rstring _baseDir;
        UInt2       _cellCount;
        Filenames   _filenamesMode;

        TerrainConfig(
			const ::Assets::rstring& baseDir, UInt2 cellCount,
            Filenames filenamesMode = XLE, 
            unsigned nodeDimsInElements = 32u, unsigned cellTreeDepth = 5u, unsigned nodeOverlap = 2u)
            : _baseDir(baseDir), _cellCount(cellCount), _filenamesMode(filenamesMode)
            , _nodeDimsInElements(nodeDimsInElements), _cellTreeDepth(cellTreeDepth), _nodeOverlap(nodeOverlap) {}

		TerrainConfig(const ::Assets::rstring& baseDir = ::Assets::rstring());

        struct FileType
        {
            enum Enum {
                Heightmap, 
                ShadowCoverage,
                ArchiveHeightmap    ///< Used when doing format conversions. ArchiveHeightmap is the pre-conversion format file
            };
        };
        void        GetCellFilename(char buffer[], unsigned cnt, UInt2 cellIndex, FileType::Enum) const;
        void        GetUberSurfaceFilename(char buffer[], unsigned bufferCount, FileType::Enum) const;
        void        GetTexturingCfgFilename(char buffer[], unsigned bufferCount) const;

        Float2      TerrainCoordsToCellBasedCoords(const Float2& terrainCoords) const;
        Float2      CellBasedCoordsToTerrainCoords(const Float2& cellBasedCoords) const;

        UInt2       CellDimensionsInNodes() const;
        UInt2       NodeDimensionsInElements() const;       // (ignoring overlap)
        unsigned    CellTreeDepth() const { return _cellTreeDepth; }
        unsigned    NodeOverlap() const { return _nodeOverlap; }

        void Save();

    protected:
        unsigned    _nodeDimsInElements;
        unsigned    _cellTreeDepth;
        unsigned    _nodeOverlap;
    };

    class TerrainCoordinateSystem
    {
    public:
        Float2      WorldSpaceToTerrainCoords(const Float2& worldSpacePosition) const;
        Float2      TerrainCoordsToWorldSpace(const Float2& terrainCoords) const;
        float       WorldSpaceDistanceToTerrainCoords(float distance) const;
        Float3      TerrainOffset() const;
        void        SetTerrainOffset(const Float3& newOffset);

        TerrainCoordinateSystem(
            Float3 terrainOffset = Float3(0.f, 0.f, 0.f),
            float nodeSizeMeters = 0.f,
            const TerrainConfig& config = TerrainConfig())
        : _terrainOffset(terrainOffset)
        , _nodeSizeMeters(nodeSizeMeters)
        , _config(config) {}

    protected:
        Float3 _terrainOffset;
        float _nodeSizeMeters;
        TerrainConfig _config;
    };

    class TerrainManager : public noncopyable
    {
    public:
        void Render(    RenderCore::Metal::DeviceContext* context, 
                        LightingParserContext& parserContext, 
                        unsigned techniqueIndex);

        class IntersectionResult
        {
        public:
            Float3  _intersectionPoint;
            Float2  _cellCoordinates;
            Float2  _fullTerrainCoordinates;   // coordinates 0.->1. across the entire terrain
        };

            /// <summary>Finds an intersection between a ray and the rendered terrain geometry<summary>
            ///
            /// This is an intersection test designed for visual tools and other utilities. It's not
            /// designed for physical simulation.
            ///
            /// Note that this doesn't test against the highest terrain LOD. It tests against the 
            /// geometry that's actually rendered from a given viewport. That's important for tools 
            /// where the user wants to click on some area of the terrain -- they get an intersection 
            /// point that truly matches what they see on screen. However, this would be incorrect
            /// for physical simulations -- because the results would vary as the camera moves.
            ///
            /// Normally the client will only want a single intersection, but we can find and return
            /// multiple intersections in cases where a ray enters the terrain multiple times. Note that
            /// intersections with back faces won't be returned.
            ///
            /// We need to know the globalTransformCB & techniqueContext to calculate intersections,
            /// because we're going to be testing the post-LOD geometry. If we were just testing
            /// the highest LOD, that wouldn't matter.
            ///
        unsigned CalculateIntersections(
            IntersectionResult intersections[],
            unsigned maxIntersections,
            std::pair<Float3, Float3> ray,
            RenderCore::Metal::DeviceContext* context,
            LightingParserContext& parserContext);

        TerrainUberSurfaceInterface*    GetUberSurfaceInterface();
        ISurfaceHeightsProvider*        GetHeightsProvider();

        const TerrainCoordinateSystem&  GetCoords() const;
        const TerrainConfig&            GetConfig() const;
        const std::shared_ptr<ITerrainFormat>& GetFormat() const;
        void SetWorldSpaceOrigin(const Float3& origin);

        TerrainManager( const TerrainConfig& cfg,
                        std::shared_ptr<ITerrainFormat> ioFormat, 
                        BufferUploads::IManager* bufferUploads,
                        Int2 cellMin, Int2 cellMax, // (not inclusive of cellMax)
                        Float3 worldSpaceOrigin = Float3(0.f, 0.f, -1000.f));
        ~TerrainManager();

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    float   GetTerrainHeight(ITerrainFormat& ioFormat, const TerrainConfig& cfg, const TerrainCoordinateSystem& coords, Float2 queryPosition);

    class TerrainCell;
    class TerrainCellTexture;

    template <typename Type> class TerrainUberSurface;
    typedef std::pair<uint16, uint16> ShadowSample;

        /// <summary>Interface for reading and writing terrain data</summary>
        /// Interface for reading and writing terrain data of a particular format.
        /// This allows the system to support different raw source data for terrain.
        /// But all formats must meet some certain restrictions as defined by the
        /// TerrainCell and TerrainCellCoverage types.
        /// <seealso cref="TerrainCell"/>
        /// <seealso cref="TerrainCellCoverage"/>
    class ITerrainFormat
    {
    public:
        virtual const TerrainCell& LoadHeights(const char filename[], bool skipDependsCheck = false) const = 0;
        virtual const TerrainCellTexture& LoadCoverage(const char filename[]) const = 0;
        virtual void WriteCell( 
            const char destinationFile[], TerrainUberSurface<float>& surface, 
            UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements) const = 0;
        virtual void WriteCellCoverage_Shadow(
            const char destinationFile[], TerrainUberSurface<ShadowSample>& surface, 
            UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements) const = 0;
    };

    void ExecuteTerrainConversion(
        const TerrainConfig& outputConfig, 
        std::shared_ptr<ITerrainFormat> outputIOFormat,
        const TerrainConfig& inputConfig, 
        std::shared_ptr<ITerrainFormat> inputIOFormat);
}
