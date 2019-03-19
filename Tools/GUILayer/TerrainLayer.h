// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"
#include "MathLayer.h"
#include "../../SceneEngine/TerrainConfig.h"

namespace GUILayer
{
    using NativeConfig = SceneEngine::TerrainConfig;

    /// <summary>Layer over the native terrain config</summary>
    /// To make it easier for C# code to interact with the terrain configuration settings,
    /// this just provides a layer over the underlying native settings.
    public ref class TerrainConfig
    {
    public:
        ref class CoverageLayerDesc
        {
        public:
            property System::String^ Name { System::String^ get(); }
            property SceneEngine::TerrainCoverageId Id { SceneEngine::TerrainCoverageId get(); }
            property VectorUInt2 NodeDims { VectorUInt2 get(); void set(VectorUInt2); }
            property unsigned Overlap { unsigned get(); void set(unsigned); }
            property unsigned FormatCat { unsigned get(); void set(unsigned); }
            property unsigned FormatArrayCount { unsigned get(); void set(unsigned); }
            property unsigned ShaderNormalizationMode { unsigned get(); void set(unsigned); }

            const NativeConfig::CoverageLayer& GetNative() { return *_native; }

            CoverageLayerDesc(
                System::String^ uberSurfaceDirectory, SceneEngine::TerrainCoverageId id);
            CoverageLayerDesc(const NativeConfig::CoverageLayer& native);
            ~CoverageLayerDesc();
        protected:
            clix::auto_ptr<NativeConfig::CoverageLayer> _native;
        };

        property VectorUInt2 CellCount { VectorUInt2 get(); void set(VectorUInt2); }
        property VectorUInt2 CellDimsInNodes { VectorUInt2 get(); }
        property VectorUInt2 NodeDims { VectorUInt2 get(); }
        property unsigned CellTreeDepth { unsigned get(); }
        property unsigned NodeOverlap { unsigned get(); }
        property float ElementSpacing { float get(); }
        property float SunPathAngle { float get(); }
        property bool EncodedGradientFlags { bool get(); }

        property unsigned CoverageLayerCount { unsigned get(); }
        CoverageLayerDesc^ GetCoverageLayer(unsigned index);
        void Add(CoverageLayerDesc^ layer);

        void InitCellCountFromUberSurface(System::String^ uberSurfaceDir);

        const NativeConfig& GetNative() { return *_native; }
        TerrainConfig(
            System::String^ cellsDirectory,
            unsigned nodeDimsInElements, unsigned cellTreeDepth, unsigned nodeOverlap,
            float elementSpacing, float sunPathAngle, bool encodedGradientFlags);
        TerrainConfig(const NativeConfig& native);
        ~TerrainConfig();
    protected:
        clix::auto_ptr<NativeConfig> _native;
    };

}


