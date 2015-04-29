// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/TerrainInternal.h"

namespace RenderCore { namespace Assets
{
    class TerrainCell : public SceneEngine::TerrainCell
    {
    public:
        TerrainCell(const char filename[]);
    };

    class TerrainCellTexture : public SceneEngine::TerrainCellTexture
    {
    public:
        TerrainCellTexture(const char filename[]);
    };

    /// <summary>Native XLE file format for terrain</summary>
    /// XLE allows for support for multiple formats for storing
    /// terrain data using the ITerrainFormat interface. This
    /// implementation is a native format for use with XLE centric
    /// applications.
    class TerrainFormat : public SceneEngine::ITerrainFormat
    {
    public:
        virtual const SceneEngine::TerrainCell& LoadHeights(const char filename[], bool skipDependsCheck) const;
        virtual const SceneEngine::TerrainCellTexture& LoadCoverage(const char filename[]) const;
        virtual void WriteCell( 
            const char destinationFile[], SceneEngine::TerrainUberSurface<float>& surface, 
            UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements) const;
        virtual void WriteCell(
            const char destinationFile[], SceneEngine::TerrainUberSurface<SceneEngine::ShadowSample>& surface, 
            UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements) const;
        virtual void WriteCell(
            const char destinationFile[], SceneEngine::TerrainUberSurface<uint8>& surface, 
            UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements) const;
    };
}}


