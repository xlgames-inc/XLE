// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <memory>
#define ENABLE_TERRAIN

namespace SceneEngine
{
    class ITerrainFormat;
    class TerrainCoordinateSystem;
    class TerrainConfig;
}

namespace Utility { class HierarchicalCPUProfiler; }

namespace Sample
{
        // Source character files are original built 100 times larger than other assets (for artists' convenience)
    static const float CharactersScale = 100.f;

    extern Utility::HierarchicalCPUProfiler g_cpuProfiler;

    #if defined(ENABLE_TERRAIN)
        extern std::shared_ptr<SceneEngine::ITerrainFormat>     MainTerrainFormat;
        extern SceneEngine::TerrainCoordinateSystem             MainTerrainCoords;
        extern SceneEngine::TerrainConfig                       MainTerrainConfig;
    #endif
}

