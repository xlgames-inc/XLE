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

        ////////////////////////////////////////////////////////////////////////////////////////
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

        ShallowWaterSim(const Desc& desc);
        ~ShallowWaterSim();

        std::unique_ptr<ShallowWaterGrid>       _simulationGrid;

        intrusive_ptr<ID3D::Resource>              _lookupTable;
        RenderCore::Metal::ShaderResourceView   _lookupTableSRV;
        RenderCore::Metal::UnorderedAccessView  _lookupTableUAV;

        class SurfaceHeightsAddressingConstants
        {
        public:
            Int3 _baseCoord;
            unsigned _dummy0;
            Int2 _textureMin, _textureMax;
            float _scale;
            float _offset;
            unsigned _dummy1[2];

            SurfaceHeightsAddressingConstants() 
                : _baseCoord(Int3(0,0,0))
                , _dummy0(0), _textureMin(0,0), _textureMax(0,0)
                , _scale(0.f), _offset(0.f) 
            {
                _dummy1[0] = _dummy1[1] = 0;
            }
        };

        struct ActiveElement
        {
            signed      _gridX, _gridY;
            unsigned    _arrayIndex;
            SurfaceHeightsAddressingConstants _heightsAddressing;

            ActiveElement() {}
            ActiveElement(signed gridX, signed gridY, unsigned arrayIndex = ~unsigned(0x0))
                :   _gridX(gridX), _gridY(gridY), _arrayIndex(arrayIndex) {}
        };

        std::vector<ActiveElement>  _activeSimulationElements;
        std::vector<unsigned>       _poolOfUnallocatedArrayIndices;
        unsigned                    _simulatingGridsCount;
        unsigned                    _gridDimension;
        bool                        _usePipeModel;
    };

        ////////////////////////////////////////////////////////////////////////////////////////

    class FFTBufferBox;
    class OceanSettings;
    class LightingParserContext;
    class ISurfaceHeightsProvider;

    namespace ShallowBorderMode
    {
        enum Enum { GlobalWaves = 1, Surface = 2, BaseHeight = 3 };
    }


    void ShallowWater_DoSim(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, 
        const OceanSettings& oceanSettings, float gridPhysicalDimension,
        RenderCore::Metal::ShaderResourceView* globalOceanWorkingHeights,
        ISurfaceHeightsProvider* surfaceHeightsProvider, ShallowWaterSim& shallowBox, unsigned bufferCounter);
    void ShallowWater_BindForOceanRender(
        RenderCore::Metal::DeviceContext* context, ShallowWaterSim& shallowBox, unsigned bufferCounter);
    void ShallowWater_BindForErosionSimulation(
        RenderCore::Metal::DeviceContext* context, ShallowWaterSim& shallowBox, unsigned bufferCounter);
    void ShallowWater_ExecuteInternalSimulation(
        RenderCore::Metal::DeviceContext* context,
        const OceanSettings& oceanSettings, float gridPhysicalDimension,
        RenderCore::Metal::ShaderResourceView* globalOceanWorkingHeights,
        ISurfaceHeightsProvider* surfaceHeightsProvider, ShallowWaterSim& shallowBox, 
        unsigned bufferCounter, const float compressionConstants[4],
        float rainQuantityPerFrame = 0.f, ShallowBorderMode::Enum borderMode = ShallowBorderMode::GlobalWaves);
    void ShallowWater_NewElements(
        RenderCore::Metal::DeviceContext* context, 
        ShallowWaterSim& shallowBox, ISurfaceHeightsProvider& surfaceHeightsProvider,
        const OceanSettings& oceanSettings, const float gridPhysicalDimension,
        RenderCore::Metal::ShaderResourceView* globalOceanWorkingHeights,
        ShallowBorderMode::Enum borderMode,
        const ShallowWaterSim::ActiveElement* newElementsBegin, const ShallowWaterSim::ActiveElement* newElementsEnd,
        size_t stride = sizeof(ShallowWaterSim::ActiveElement));
    void ShallowWater_RenderWireframe(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, 
        const OceanSettings& oceanSettings, float gridPhysicalDimension, Float2 offset,
        ShallowWaterSim& shallowBox, unsigned bufferCounter, ShallowBorderMode::Enum borderMode);
    void ShallowWater_RenderVelocities(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, 
        const OceanSettings& oceanSettings, float gridPhysicalDimension, Float2 offset,
        ShallowWaterSim& shallowBox, unsigned bufferCounter, ShallowBorderMode::Enum borderMode);
}

