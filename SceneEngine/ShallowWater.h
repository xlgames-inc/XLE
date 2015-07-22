// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderCore/Metal/Forward.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Metal/RenderTargetView.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../Utility/IntrusivePtr.h"
#include "../Math/Vector.h"
#include <vector>

namespace SceneEngine
{

    class ShallowWaterGrid;
    class DeepOceanSim;
    class DeepOceanSimSettings;
    class LightingParserContext;
    class ISurfaceHeightsProvider;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ShallowWaterSim
    {
    public:
        class Desc
        {
        public:
            unsigned _gridDimension;
            unsigned _maxSimulationGrid;
            bool _usePipeModel;
            bool _buildVelocities;
            bool _useLookupTable;

            Desc(unsigned gridDimension, unsigned maxSimulationGrid, bool usePipeModel, bool buildVelocities, bool useLookupTable) 
                : _gridDimension(gridDimension), _maxSimulationGrid(maxSimulationGrid), _usePipeModel(usePipeModel), _buildVelocities(buildVelocities) 
                , _useLookupTable(useLookupTable) {}
        };

        struct BorderMode
        {
            enum Enum { GlobalWaves = 1, Surface = 2, BaseHeight = 3 };
        };

        class SimSettings
        {
        public:
            float _rainQuantityPerFrame;
            float _evaporationConstant;
            float _pressureConstant;
            Float4 _compressionConstants;

            SimSettings();
        };

        using SRV = RenderCore::Metal::ShaderResourceView;
        using UAV = RenderCore::Metal::UnorderedAccessView;
        using MetalContext = RenderCore::Metal::DeviceContext;

        class SimulationContext
        {
        public:
            MetalContext* _metalContext;
            const DeepOceanSimSettings* _oceanSettings;
            float _gridPhysicalDimension;
            Float2 _physicalMins;
            ISurfaceHeightsProvider* _surfaceHeightsProvider;
            SRV* _globalOceanWorkingHeights;
            BorderMode::Enum _borderMode;

            SimulationContext(
                MetalContext& metalContext,
                const DeepOceanSimSettings& oceanSettings,
                float gridPhysicalDimension,
                Float2 physicalMins,
                ISurfaceHeightsProvider* surfaceHeightsProvider,
                SRV* globalOceanWorkingHeights,
                BorderMode::Enum borderMode);
        };

        void ExecuteSim(
            const SimulationContext& context, 
            LightingParserContext& parserContext, 
            const SimSettings& settings,
            unsigned bufferCounter,
            const Int2* validGridBegin = nullptr, const Int2* validGridEnd = nullptr);

        void ExecuteInternalSimulation(
            const SimulationContext& context,
            const SimSettings& settings,
            unsigned bufferCounter);

        void BeginElements(
            const SimulationContext& context,
            const Int2* newElementsBegin, const Int2* newElementsEnd);

        void BindForOceanRender(MetalContext& context, unsigned bufferCounter);
        void BindForErosionSimulation(MetalContext& context, unsigned bufferCounter);

        void RenderWireframe(
            MetalContext& context, LightingParserContext& parserContext, 
            const DeepOceanSimSettings& oceanSettings, float gridPhysicalDimension, Float2 offset,
            unsigned bufferCounter, BorderMode::Enum borderMode);

        void RenderVelocities(
            MetalContext& context, LightingParserContext& parserContext, 
            const DeepOceanSimSettings& oceanSettings, float gridPhysicalDimension, Float2 offset,
            unsigned bufferCounter, BorderMode::Enum borderMode,
            bool showErosion);

        unsigned    GetGridDimension() const    { return _gridDimension; }
        bool        IsActive() const            { return _simulatingGridsCount != 0; }
        unsigned    FindActiveGrid(Int2 gridCoords);
        RenderCore::SharedPkt BuildCellConstants(Int2 gridCoords);

        ShallowWaterSim(const Desc& desc);
        ~ShallowWaterSim();

        struct ActiveElement;

    protected:
        std::unique_ptr<ShallowWaterGrid>   _simulationGrid;

        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;
        ResLocator  _lookupTable;
        SRV         _lookupTableSRV;
        UAV         _lookupTableUAV;
        
        std::vector<ActiveElement>  _activeSimulationElements;
        std::vector<unsigned>       _poolOfUnallocatedArrayIndices;
        unsigned                    _simulatingGridsCount;
        unsigned                    _gridDimension;
        bool                        _usePipeModel;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////
   
    inline ShallowWaterSim::SimulationContext::SimulationContext(
        MetalContext& metalContext,
        const DeepOceanSimSettings& oceanSettings,
        float gridPhysicalDimension,
        Float2 physicalMins,
        ISurfaceHeightsProvider* surfaceHeightsProvider,
        SRV* globalOceanWorkingHeights,
        BorderMode::Enum borderMode)
    {
        _metalContext = &metalContext;
        _oceanSettings = &oceanSettings;
        _gridPhysicalDimension = gridPhysicalDimension;
        _physicalMins = physicalMins;
        _globalOceanWorkingHeights = globalOceanWorkingHeights;
        _surfaceHeightsProvider = surfaceHeightsProvider;
        _borderMode = borderMode;
    }

    Float4 OceanHack_CompressionConstants(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext, 
        float baseHeight, float compressionAmount, float compressionRadius);

}

