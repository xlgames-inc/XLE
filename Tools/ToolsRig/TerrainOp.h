// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Assets/AssetsCore.h"
#include "../../Math/Vector.h"

namespace SceneEngine 
{ 
    template <typename Type> class TerrainUberSurface;
    typedef TerrainUberSurface<float> TerrainUberHeightsSurface;
}
namespace ConsoleRig { class IProgress; }
namespace Utility { namespace ImpliedTyping { class TypeDesc; }}

namespace ToolsRig
{
    /// <summary>Terrain operation that executes on heights</summary>
    /// This is an interface class for simple terrain operations
    /// (like shadows and ambient occlusion).
    ///
    /// These operations happen on the CPU, and use the heightmap as
    /// input.
    class ITerrainOp
    {
    public:
        virtual void Calculate(
            void* dst, Float2 coord, 
            SceneEngine::TerrainUberHeightsSurface& heightsSurface, float xyScale) const = 0;
        virtual ImpliedTyping::TypeDesc GetOutputFormat() const = 0;
        virtual void FillDefault(void* dst, unsigned count) const = 0;
        virtual const char* GetName() const = 0;
    };

    class TerrainOpConfig;

    void BuildUberSurface(
        const ::Assets::ResChar destinationFile[],
        ITerrainOp& operation,
        SceneEngine::TerrainUberHeightsSurface& heightsSurface,
        Int2 interestingMins, Int2 interestingMaxs,
        float xyScale, float relativeResolution,
        const TerrainOpConfig& cfg,
        ConsoleRig::IProgress* progress = nullptr);

    class TerrainOpConfig
    {
    public:
        unsigned _maxThreadCount;
        explicit TerrainOpConfig(unsigned maxThreadCount) : _maxThreadCount(maxThreadCount) {}
        TerrainOpConfig();
    };

}

