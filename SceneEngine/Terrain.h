// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "TerrainCoverageId.h"
#include "../RenderCore/Metal/Forward.h"    // (for RenderCore::Metal::DeviceContext)
#include "../Math/Vector.h"
#include "../Assets/AssetsCore.h"

namespace RenderCore { namespace Techniques { class CameraDesc; } }
namespace Utility { class OutputStream; }
namespace ConsoleRig { class IProgress; }

namespace SceneEngine
{
    class LightingParserContext;
    class TechniqueContext;
    class PreparedScene;
    class HeightsUberSurfaceInterface;
    class CoverageUberSurfaceInterface;
    class ITerrainFormat;
    class ISurfaceHeightsProvider;
    
    class TerrainConfig;
    class TerrainCoordinateSystem;
    class TerrainMaterialConfig;
    class GradientFlagsSettings;

    /// <summary>Top-level manager for terrain assets</summary>
    /// Internally, the manager coordinates many class that perform the rendering,
    /// streaming, intersection tests and so forth.
    class TerrainManager
    {
    public:
        void Prepare(
            RenderCore::Metal::DeviceContext* context,
            LightingParserContext& parserContext,
            PreparedScene& preparedPackets);
        void Render(
            RenderCore::Metal::DeviceContext* context,
            LightingParserContext& parserContext,
            PreparedScene& preparedPackets,
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
        std::shared_ptr<ISurfaceHeightsProvider>    GetHeightsProvider();

        const TerrainCoordinateSystem&  GetCoords() const;
        const TerrainConfig&            GetConfig() const;
        const TerrainMaterialConfig&    GetMaterialConfig() const;
        const std::shared_ptr<ITerrainFormat>& GetFormat() const;
        void SetWorldSpaceOrigin(const Float3& origin);
        void SetShortCircuitSettings(const GradientFlagsSettings& gradientFlagsSettings);

        /// <summary>Loads a new terrain, removing the old one</summary>
        ///
        /// We can restrict the amount loaded to a rectangle of cells using
        /// cellMin and cellMax. This is especially useful when writing a
        /// test-bed app that must startup quickly.
        ///
        /// Note that "cellMax" is non-inclusive. That is,
        ///   Load(cfg, Int2(0,0), Int2(1,1));
        /// will only load a single cell (not 4)
        ///
        /// Use the defaults for cellMin and cellMax to load the entire terrain.
        ///
        /// The previous terrain (if any) will be removed. However, if
        /// any cached textures or data can be retained, they will be.
        void Load(const TerrainConfig& cfg, UInt2 cellMin = UInt2(0,0), UInt2 cellMax = UInt2(~0u, ~0u), bool allowModification = false);
        void LoadMaterial(const TerrainMaterialConfig& matCfg);
        void LoadUberSurface(const ::Assets::ResChar uberSurfaceDir[]);

        /// <summary>Write all changes to disk</summary>
        /// Also unloads and reloads the terrain, in the process
        void FlushToDisk(ConsoleRig::IProgress* progress = nullptr);

        /// <summary>Unloads all terrain</summary>
        /// Removes all cells and removes the terrain from the world.
        /// Note that the rendering resources are not released immediately. They can be
        /// reused if another compatible terrain is loaded (using the Load method).
        /// To release all resources, destroy the TerrainManager object.
        void Reset();

        TerrainManager(std::shared_ptr<ITerrainFormat> ioFormat);
        ~TerrainManager();

        TerrainManager(const TerrainManager&) = delete;
        TerrainManager& operator=(const TerrainManager&) = delete;

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> _pimpl;
    };

    /// <summary>Gets the height of the terrain at a position, without using the GPU</summary>
    /// There are 2 forms of intersection testing supported by the system.
    /// TerrainManager::CalculateIntersections uses the GPU, and calculates an intersection against
    /// post-LOD geometry. It is intended for tools that want match mouse clicks against rendered
    /// geometry.
    /// In constrast, GetTerrainHeight does not use the GPU and only tests against the top LOD.
    /// This is used by simple physical simulations (such as sliding a character across the terrain
    /// surface).
    /// Note that this requires loading some terrain height data into main memory (whereas rendering 
    /// only requires height data in GPU memory). So, this can require reading height data from disk
    /// a second time.
    float GetTerrainHeight(
        ITerrainFormat& ioFormat, const TerrainConfig& cfg, 
        const TerrainCoordinateSystem& coords, Float2 queryPosition);

    /// <summary>Like GetTerrainHeight, but also returns the normal</summary>
    bool GetTerrainHeightAndNormal(
        float& height, Float3& normal,
        ITerrainFormat& ioFormat, const TerrainConfig& cfg, 
        const TerrainCoordinateSystem& coords, Float2 queryPosition);

    class TerrainCell;
    class TerrainCellTexture;
    class TerrainUberSurfaceGeneric;

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
            const char destinationFile[], TerrainUberSurfaceGeneric& surface, 
            UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements) const = 0;
        virtual ~ITerrainFormat();
    };
}
