// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "..\RenderCore\Metal\Forward.h"
#include "..\RenderCore\Metal\ShaderResource.h"
#include "..\RenderCore\Metal\RenderTargetView.h"
#include "..\RenderCore\DX11\Metal\DX11.h"
#include "..\Utility\IntrusivePtr.h"
#include "..\Math\Vector.h"
#include <vector>

namespace SceneEngine
{

    class ShallowWaterGrid;
    class FFTBufferBox;
    class OceanSettings;
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

            Desc(unsigned gridDimension, unsigned maxSimulationGrid, bool usePipeModel, bool buildVelocities) 
                : _gridDimension(gridDimension), _maxSimulationGrid(maxSimulationGrid), _usePipeModel(usePipeModel), _buildVelocities(buildVelocities) {}
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
            const OceanSettings* _oceanSettings;
            float _gridPhysicalDimension;
            ISurfaceHeightsProvider* _surfaceHeightsProvider;
            SRV* _globalOceanWorkingHeights;
            BorderMode::Enum _borderMode;

            SimulationContext(
                MetalContext& metalContext,
                const OceanSettings& oceanSettings,
                float gridPhysicalDimension,
                ISurfaceHeightsProvider* surfaceHeightsProvider,
                SRV* globalOceanWorkingHeights,
                BorderMode::Enum borderMode);
        };

        void ExecuteSim(
            const SimulationContext& context, 
            LightingParserContext& parserContext, 
            unsigned bufferCounter);

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
            const OceanSettings& oceanSettings, float gridPhysicalDimension, Float2 offset,
            unsigned bufferCounter, BorderMode::Enum borderMode);

        void RenderVelocities(
            MetalContext& context, LightingParserContext& parserContext, 
            const OceanSettings& oceanSettings, float gridPhysicalDimension, Float2 offset,
            unsigned bufferCounter, BorderMode::Enum borderMode,
            bool showErosion);

        unsigned GetGridDimension() const { return _gridDimension; }
        bool IsActive() const { return _simulatingGridsCount != 0; }

        ShallowWaterSim(const Desc& desc);
        ~ShallowWaterSim();

        struct ActiveElement;

    protected:
        std::unique_ptr<ShallowWaterGrid>   _simulationGrid;

        intrusive_ptr<ID3D::Resource>   _lookupTable;
        SRV                             _lookupTableSRV;
        UAV                             _lookupTableUAV;
        
        std::vector<ActiveElement>  _activeSimulationElements;
        std::vector<unsigned>       _poolOfUnallocatedArrayIndices;
        unsigned                    _simulatingGridsCount;
        unsigned                    _gridDimension;
        bool                        _usePipeModel;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////
   
    ShallowWaterSim::SimulationContext::SimulationContext(
        MetalContext& metalContext,
        const OceanSettings& oceanSettings,
        float gridPhysicalDimension,
        ISurfaceHeightsProvider* surfaceHeightsProvider,
        SRV* globalOceanWorkingHeights,
        BorderMode::Enum borderMode)
    {
        _metalContext = &metalContext;
        _oceanSettings = &oceanSettings;
        _gridPhysicalDimension = gridPhysicalDimension;
        _globalOceanWorkingHeights = globalOceanWorkingHeights;
        _surfaceHeightsProvider = surfaceHeightsProvider;
        _borderMode = borderMode;
    }

}

