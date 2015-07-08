// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Terrain.h"

namespace SceneEngine
{
    class GradientFlagsSettings
    {
    public:
        bool    _enable;
        float   _elementSpacing;
        float   _slopeThreshold;
        float   _transThreshold;

        explicit GradientFlagsSettings(
            bool enable = false,
            float elementSpacing = 2.f,
            float slopeThreshold = 1.5f,
            float transThreshold = .4f);
    };

    /// <summary>Native XLE file format for terrain</summary>
    /// XLE allows for support for multiple formats for storing
    /// terrain data using the ITerrainFormat interface. This
    /// implementation is a native format for use with XLE centric
    /// applications.
    class TerrainFormat : public ITerrainFormat
    {
    public:
        virtual const TerrainCell& LoadHeights(const char filename[], bool skipDependsCheck) const;
        virtual const TerrainCellTexture& LoadCoverage(const char filename[]) const;
        virtual void WriteCell( 
            const char destinationFile[], TerrainUberSurface<float>& surface, 
            UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements) const;
        virtual void WriteCell(
            const char destinationFile[], TerrainUberSurface<ShadowSample>& surface, 
            UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements) const;
        virtual void WriteCell(
            const char destinationFile[], TerrainUberSurface<uint8>& surface, 
            UInt2 cellMins, UInt2 cellMaxs, unsigned treeDepth, unsigned overlapElements) const;

        TerrainFormat(const GradientFlagsSettings& gradFlagsSettings = GradientFlagsSettings());
        ~TerrainFormat();

    protected:
        GradientFlagsSettings _gradFlagsSettings;
    };
}




