// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TerrainCoverageId.h"
#include "../RenderCore/Metal/Forward.h"
#include "../Math/Vector.h"
#include "../Math/Matrix.h"
#include "../Assets/Assets.h"

namespace RenderCore { namespace Techniques { class CameraDesc; } }
namespace Utility { class OutputStream; }

namespace SceneEngine
{
    class LightingParserContext;
    class TechniqueContext;
    class HeightsUberSurfaceInterface;
    class CoverageUberSurfaceInterface;
    class ITerrainFormat;
    class ISurfaceHeightsProvider;
    
    class TerrainConfig;

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

    class TerrainManager
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

        HeightsUberSurfaceInterface*    GetHeightsInterface();
        CoverageUberSurfaceInterface*   GetCoverageInterface(TerrainCoverageId id);
        ISurfaceHeightsProvider*        GetHeightsProvider();

        const TerrainCoordinateSystem&  GetCoords() const;
        const TerrainConfig&            GetConfig() const;
        const std::shared_ptr<ITerrainFormat>& GetFormat() const;
        void SetWorldSpaceOrigin(const Float3& origin);

        /// <summary>Loads a new terrain, removing the old one</summary>
        /// Note that "cellMax" is non-inclusive. That is,
        ///   Load(cfg, Int2(0,0), Int2(1,1));
        /// will only load a single cell (not 4)
        ///
        /// The previous terrain (if any) will be removed. However, if
        /// any cached textures or data can be retained, they will be.
        void Load(const TerrainConfig& cfg, UInt2 cellMin, UInt2 cellMax, bool allowModification);
        void LoadUberSurface(const ::Assets::ResChar uberSurfaceDir[]);
        void Reset();

        TerrainManager(std::shared_ptr<ITerrainFormat> ioFormat);
        ~TerrainManager();

        TerrainManager(const TerrainManager&) = delete;
        TerrainManager& operator=(const TerrainManager&) = delete;

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
        virtual void WriteCell(
            const char destinationFile[], TerrainUberSurface<ShadowSample>& surface, 
            UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements) const = 0;
        virtual void WriteCell(
            const char destinationFile[], TerrainUberSurface<uint8>& surface, 
            UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements) const = 0;
        virtual ~ITerrainFormat();
    };

    void ExecuteTerrainConversion(
        const ::Assets::ResChar destinationUberSurfaceDirectory[],
        const TerrainConfig& outputConfig, 
        const TerrainConfig& inputConfig, 
        std::shared_ptr<ITerrainFormat> inputIOFormat);
    void GenerateMissingUberSurfaceFiles(
        const TerrainConfig& outputConfig, 
        std::shared_ptr<ITerrainFormat> outputIOFormat,
        const ::Assets::ResChar uberSurfaceDir[]);
    void GenerateMissingCellFiles(
        const TerrainConfig& outputConfig, 
        std::shared_ptr<ITerrainFormat> outputIOFormat,
        const ::Assets::ResChar uberSurfaceDir[]);

    void WriteTerrainCachedData(
        Utility::OutputStream& stream,
        const TerrainConfig& cfg, 
        ITerrainFormat& format);

    void WriteTerrainMaterialData(
        Utility::OutputStream& stream,
        const TerrainConfig& cfg);
}
