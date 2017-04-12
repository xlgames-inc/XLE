// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShallowWater.h"
#include "Ocean.h"
#include "DeepOceanSim.h"
#include "SimplePatchBox.h"
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "SurfaceHeightsProvider.h"

#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Format.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Shader.h"

#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"

#include "../Math/ProjectionMath.h"
#include "../Math/Transformations.h"
#include "../ConsoleRig/Console.h"
#include "../Utility/BitUtils.h"
#include "../Utility/StringFormat.h"

#pragma warning(disable:4505)       // warning C4505: 'SceneEngine::BuildSurfaceHeightsTexture' : unreferenced local function has been removed

namespace SceneEngine
{
    using namespace RenderCore;

    using SRV = RenderCore::Metal::ShaderResourceView;
    using UAV = RenderCore::Metal::UnorderedAccessView;
    using MetalContext = RenderCore::Metal::DeviceContext;
    using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;

///////////////////////////////////////////////////////////////////////////////////////////////////
    class ShallowWaterGrid
    {
    public:
        ResLocator          _waterHeightsTextures[3];
        SRV                 _waterHeightsSRV[3];
        UAV                 _waterHeightsUAV[3];

        static const unsigned VelTextures = 8;
        ResLocator          _waterVelocitiesTexture[VelTextures];
        SRV                 _waterVelocitiesSRV[VelTextures];
        UAV                 _waterVelocitiesUAV[VelTextures];

        ResLocator          _slopesBuffer[2];
        UAV                 _slopesBufferUAV[2];

        ResLocator          _normalsTexture;
        std::vector<UAV>    _normalsTextureUAV;
        std::vector<SRV>    _normalsSingleMipSRV;
        SRV                 _normalsTextureShaderResource;

        ResLocator          _foamQuantity[2];
        UAV                 _foamQuantityUAV[2];
        SRV                 _foamQuantitySRV[2];
        SRV                 _foamQuantitySRV2[2];

        unsigned            _rotatingBufferCount;
        bool                _pendingInitialClear;

        ShallowWaterGrid();
        ShallowWaterGrid(
            unsigned width, unsigned height, unsigned maxSimulationGrids, 
            bool pipeModel, bool calculateVelocities, bool calculateFoam);
        ~ShallowWaterGrid();
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    ShallowWaterGrid::ShallowWaterGrid() {}
    ShallowWaterGrid::ShallowWaterGrid(
        unsigned width, unsigned height, unsigned maxSimulationGrids, 
        bool pipeModel, bool calculateVelocities, bool calculateFoam)
    {
        using namespace RenderCore;
        auto& uploads = GetBufferUploads();

        auto tDesc = TextureDesc::Plain2D(width, height, Format::R32_TYPELESS, 1, uint16(maxSimulationGrids));
        if (height<=1) {
            tDesc = TextureDesc::Plain1D(width, Format::R32_TYPELESS, 1, uint16(maxSimulationGrids));
        }

        ResourceDesc targetDesc;
        targetDesc._type = ResourceDesc::Type::Texture;
        targetDesc._bindFlags = BindFlag::ShaderResource|BindFlag::UnorderedAccess;
        targetDesc._cpuAccess = 0;
        targetDesc._gpuAccess = GPUAccess::Read|GPUAccess::Write;
        targetDesc._allocationRules = 0;
        targetDesc._name[0] = '\0';
        targetDesc._textureDesc = tDesc;

        ResLocator waterHeightsTextures[3];
        UAV waterHeightsUAV[3];
        SRV waterHeightsSRV[3];

        ResLocator waterVelocitiesTexture[VelTextures];
        UAV waterVelocitiesUAV[VelTextures];
        SRV waterVelocitiesSRV[VelTextures];

        ResLocator slopesBuffer[2];
        UAV slopesBufferUAV[2];

        unsigned heightsTextureCount = pipeModel?1:3;

		auto windowForUAVs = TextureViewWindow(
			Format::R32_FLOAT, TextureDesc::Dimensionality::Undefined,
			TextureViewWindow::SubResourceRange{0,1},
			TextureViewWindow::All,
			TextureViewWindow::Flags::ForceArray);

        for (unsigned c=0; c<heightsTextureCount; ++c) {
            waterHeightsTextures[c] = uploads.Transaction_Immediate(targetDesc);
            waterHeightsUAV[c] = UAV(waterHeightsTextures[c]->ShareUnderlying(), {windowForUAVs});
            waterHeightsSRV[c] = SRV(waterHeightsTextures[c]->ShareUnderlying(), {Format::R32_FLOAT});
        }

            //  The pipe model always needs velocities. Otherwise, we only calculate them
            //  when the "calculateVelocities" flag is set. These velocities represent
            //  the movement of water from place to place.

        if (pipeModel || calculateVelocities) {
            targetDesc._textureDesc._format = Format::R32_TYPELESS;
            for (unsigned c=0; c<VelTextures; ++c) {
                waterVelocitiesTexture[c] = uploads.Transaction_Immediate(targetDesc);
                waterVelocitiesUAV[c] = UAV(waterVelocitiesTexture[c]->ShareUnderlying(), {windowForUAVs});
                waterVelocitiesSRV[c] = SRV(waterVelocitiesTexture[c]->ShareUnderlying(), {Format::R32_FLOAT});
            }
        }

        if (!pipeModel && calculateVelocities) {
            targetDesc._textureDesc._format = Format::R32_TYPELESS;
            for (unsigned c=0; c<2; ++c) {
                slopesBuffer[c] = uploads.Transaction_Immediate(targetDesc);
                slopesBufferUAV[c] = UAV(slopesBuffer[c]->ShareUnderlying(), {windowForUAVs});
            }
        }

                ////
        const unsigned normalsMipCount = IntegerLog2(std::max(width, height));
        const auto typelessNormalFormat = Format::R8G8_TYPELESS;
        const auto uintNormalFormat = Format::R8G8_UINT;
        const auto unormNormalFormat = Format::R8G8_UNORM;
        auto normalsBufferUploadsDesc = BuildRenderTargetDesc(
            BindFlag::UnorderedAccess|BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain2D(width, height, typelessNormalFormat, uint8(normalsMipCount), uint8(maxSimulationGrids)),
            "ShallowNormals");
        auto normalsTexture = uploads.Transaction_Immediate(normalsBufferUploadsDesc, nullptr);
        std::vector<UAV> normalsTextureUVA;
        std::vector<SRV> normalsSingleMipSRV;
        normalsTextureUVA.reserve(normalsMipCount);
        normalsSingleMipSRV.reserve(normalsMipCount);
        for (unsigned c=0; c<normalsMipCount; ++c) {
			auto window = TextureViewWindow(
				uintNormalFormat, TextureDesc::Dimensionality::Undefined,
				TextureViewWindow::SubResourceRange{c,1},
				TextureViewWindow::All,
				TextureViewWindow::Flags::ForceArray);
            normalsTextureUVA.push_back(UAV(normalsTexture->ShareUnderlying(), window));
            normalsSingleMipSRV.push_back(SRV(normalsTexture->ShareUnderlying(), window));
        }
        SRV normalsTextureShaderResource(normalsTexture->ShareUnderlying(), {unormNormalFormat});

                ////
        if (calculateFoam) {
            auto foamTextureDesc = BuildRenderTargetDesc(
                BindFlag::UnorderedAccess|BindFlag::ShaderResource,
                TextureDesc::Plain2D(width, height, Format::R8_TYPELESS, 1, uint8(maxSimulationGrids)),
                "ShallowFoam");
            for (unsigned c=0; c<2; ++c) {
                _foamQuantity[c] = uploads.Transaction_Immediate(foamTextureDesc, nullptr);
                _foamQuantityUAV[c] = UAV(_foamQuantity[c]->ShareUnderlying(), {Format::R8_UINT});
                _foamQuantitySRV[c] = SRV(_foamQuantity[c]->ShareUnderlying(), {Format::R8_UNORM});
                _foamQuantitySRV2[c] = SRV(_foamQuantity[c]->ShareUnderlying(), {Format::R8_UINT});
            }
        }
    
                ////
        for (unsigned c=0; c<3; ++c) {
            _waterHeightsTextures[c] = std::move(waterHeightsTextures[c]);
            _waterHeightsSRV[c] = std::move(waterHeightsSRV[c]);
            _waterHeightsUAV[c] = std::move(waterHeightsUAV[c]);
        }

        for (unsigned c=0; c<VelTextures; ++c) {
            _waterVelocitiesTexture[c] = std::move(waterVelocitiesTexture[c]);
            _waterVelocitiesUAV[c] = std::move(waterVelocitiesUAV[c]);
            _waterVelocitiesSRV[c] = std::move(waterVelocitiesSRV[c]);
        }

        for (unsigned c=0; c<2; ++c) {
            _slopesBuffer[c] = std::move(slopesBuffer[c]);
            _slopesBufferUAV[c] = std::move(slopesBufferUAV[c]);
        }

        _rotatingBufferCount = heightsTextureCount;
        _pendingInitialClear = true;

                ////
        _normalsTexture = std::move(normalsTexture);
        _normalsTextureUAV = std::move(normalsTextureUVA);
        _normalsSingleMipSRV = std::move(normalsSingleMipSRV);
        _normalsTextureShaderResource = std::move(normalsTextureShaderResource);
    }

    ShallowWaterGrid::~ShallowWaterGrid() {}
    
    void CheckInitialClear(MetalContext& context, ShallowWaterGrid& grid)
    {
        // When we create the resources initially, we don't have a device context, so we can't do
        // a clear... We have to deferred until we have a device context
        if (grid._pendingInitialClear) {
            float clearValues[4] = {0,0,0,0};
            for (unsigned c=0; c<ShallowWaterGrid::VelTextures; ++c)
                if (grid._waterVelocitiesUAV[c].IsGood())
                    context.ClearFloat(grid._waterVelocitiesUAV[c], clearValues);
            grid._pendingInitialClear = false;
        }
    }

    ShallowWaterSim::ShallowWaterSim(const Desc& desc)
    {
        using namespace RenderCore;
        auto& uploads = GetBufferUploads();

        auto simulationGrid = std::make_unique<ShallowWaterGrid>(
            desc._gridDimension, desc._gridDimension, 
            desc._maxSimulationGrid, desc._usePipeModel, 
            desc._buildVelocities, desc._calculateFoam);

            //
            //      Build a lookup table that will provide the indices into
            //      the array of simulation grids... More complex implementations
            //      could use a sparse tree for this -- so that an infinitely
            //      large world could be supported.
            //
        if (desc._useLookupTable) {
            const unsigned lookupTableDimensions = 512;
            ResourceDesc targetDesc;
            targetDesc._type = ResourceDesc::Type::Texture;
            targetDesc._bindFlags = BindFlag::ShaderResource|BindFlag::UnorderedAccess;
            targetDesc._cpuAccess = 0;
            targetDesc._gpuAccess = GPUAccess::Read|GPUAccess::Write;
            targetDesc._allocationRules = 0;
            targetDesc._name[0] = '\0';
            targetDesc._textureDesc = 
                TextureDesc::Plain2D(
                    lookupTableDimensions, lookupTableDimensions, Format::R8_TYPELESS);

            auto initData = BufferUploads::CreateEmptyPacket(targetDesc);
            XlSetMemory(initData->GetData(), 0xff, initData->GetDataSize());
            _lookupTable = uploads.Transaction_Immediate(targetDesc, initData.get());
            _lookupTableUAV = UAV(_lookupTable->ShareUnderlying(), {Format::R8_UINT});
            _lookupTableSRV = SRV(_lookupTable->ShareUnderlying(), {Format::R8_UINT});
        }

        _poolOfUnallocatedArrayIndices.reserve(desc._maxSimulationGrid);
        for (unsigned c=0; c<desc._maxSimulationGrid; ++c)
            _poolOfUnallocatedArrayIndices.push_back(c);
        _simulationGrid = std::move(simulationGrid);
        _simulatingGridsCount = desc._maxSimulationGrid;
        _gridDimension = desc._gridDimension;
        _usePipeModel = desc._usePipeModel;
    }

    ShallowWaterSim::~ShallowWaterSim() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    struct ShallowWaterSim::ActiveElement
    {
        Int2        _gridCoords;
        unsigned    _arrayIndex;

        ActiveElement() {}
        ActiveElement(Int2 gridCoords, unsigned arrayIndex = ~unsigned(0x0))
        :   _gridCoords(gridCoords), _arrayIndex(arrayIndex) {}
    };

    template<typename First, typename Second>
        static bool SortCells(const std::pair<First,Second>& lhs, const std::pair<First,Second>& rhs) 
            { return (lhs.first + lhs.second) < (rhs.first + rhs.second); }

    static bool SortOceanGridElement(   const ShallowWaterSim::ActiveElement& lhs, 
                                        const ShallowWaterSim::ActiveElement& rhs)
    {
        return (lhs._gridCoords[0] + lhs._gridCoords[1]) < (rhs._gridCoords[0] + rhs._gridCoords[1]);
    }

    static bool GridIsVisible(
        const ShallowWaterSim::SimulationContext& context,
        LightingParserContext& parserContext, Int2 grid, 
        float baseWaterHeight)
    {
        Float3 mins( grid[0]    * context._gridPhysicalDimension + context._physicalMins[0],  grid[1]    * context._gridPhysicalDimension + context._physicalMins[1], baseWaterHeight - 3.f);
        Float3 maxs((grid[0]+1) * context._gridPhysicalDimension + context._physicalMins[0], (grid[1]+1) * context._gridPhysicalDimension + context._physicalMins[1], baseWaterHeight + 3.f);
        return !CullAABB_Aligned(parserContext.GetProjectionDesc()._worldToProjection, mins, maxs, RenderCore::Techniques::GetDefaultClipSpaceType());
    }

    struct PrioritisedActiveElement
    {
    public:
        ShallowWaterSim::ActiveElement _e;
        float   _priority;
        PrioritisedActiveElement(Int2 gridCoords, float priority) : _e(gridCoords), _priority(priority) {}
        PrioritisedActiveElement(const ShallowWaterSim::ActiveElement& e, float priority) : _e(e), _priority(priority) {}
        PrioritisedActiveElement() {}
    };

    static bool SortByPriority(const PrioritisedActiveElement& lhs, const PrioritisedActiveElement& rhs)        { return lhs._priority < rhs._priority; }
    static bool SortByGridIndex(const PrioritisedActiveElement& lhs, const PrioritisedActiveElement& rhs)       { return SortOceanGridElement(lhs._e, rhs._e); }

///////////////////////////////////////////////////////////////////////////////////////////////////

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

    static bool CalculateAddressing(
        const ShallowWaterSim::SimulationContext& context,
        SurfaceHeightsAddressingConstants& dest,
        const Int2& gridCoords)
    {
        if (!context._surfaceHeightsProvider) return false;

            //  Even though we have a cached heights addressing, we should recalculate height addressing to 
            //  match new uploads, etc...
            //  Note that if the terrain is edited, it can change the height scale & offset
            //
            // Note that there might be a problem with the terrain offset here -- it can throw
            // the terrain out of alignment with how we divide up our terrain.
        Float2 gridMins = context._physicalMins + Float2(float(gridCoords[0]) * context._gridPhysicalDimension, float(gridCoords[1]) * context._gridPhysicalDimension);
        Float2 gridMaxs = context._physicalMins + Float2(float(gridCoords[0]+1) * context._gridPhysicalDimension, float(gridCoords[1]+1) * context._gridPhysicalDimension);
        auto surfaceAddressing = context._surfaceHeightsProvider->GetAddress(gridMins, gridMaxs);
        if (!surfaceAddressing._valid) return false;

        dest._baseCoord = surfaceAddressing._baseCoordinate;
        dest._textureMin = surfaceAddressing._minCoordOffset;
        dest._textureMax = surfaceAddressing._maxCoordOffset;
        dest._scale = surfaceAddressing._heightScale;
        dest._offset = surfaceAddressing._heightOffset;
        return true;
    }

    static bool SetAddressingConstants(
        const ShallowWaterSim::SimulationContext& context,
        Metal::ConstantBuffer& cb, 
        const Int2& gridCoords)
    {
        SurfaceHeightsAddressingConstants newHeightsAddressing;
        if (!CalculateAddressing(context, newHeightsAddressing, gridCoords))
            return false;
        cb.Update(*context._metalContext, &newHeightsAddressing, sizeof(newHeightsAddressing));
        return true;
    }

    ShallowWaterSim::SimSettings::SimSettings()
    {
        _rainQuantityPerFrame = Tweakable("OceanRainQuantity", 0.f);
        _evaporationConstant = Tweakable("OceanEvaporation", 0.985f);
        _pressureConstant = Tweakable("OceanPressureConstant", 150.f);
        _compressionConstants = Float4(0.f, 0.f, 1000.f, 1.f);
    }

    struct CellConstants
    {
        Int2	    _simulatingIndex; 
	    unsigned    _arrayIndex;
        unsigned    _dummy0;
        Float2      _worldSpaceOffset;
        int         _adjacentGrids[8];
        unsigned    _dummy1[2];
    };

    struct SimulatingConstants
    {
        float       _rainQuantityPerFrame;
        float       _evaporationConstant;
        float       _pressureConstant;
        unsigned    _dummy;
        Float4      _compressionConstants;
    };

    static const Int2 AdjOffset[8] = 
    {
        Int2(-1, -1), Int2(0, -1), Int2(1, -1),
        Int2(-1,  0),              Int2(1,  0),
        Int2(-1,  1), Int2(0,  1), Int2(1,  1)
    };

    static void FindAdjacentGrids(int result[8], Int2 baseCoord, const std::vector<ShallowWaterSim::ActiveElement>& elements)
    {
            //  Find the adjacent grids and set the grid indices 
            //  as appropriate. Just doing a brute-force search!
        for (unsigned c=0; c<dimof(AdjOffset); ++c) {
            for (auto i=elements.cbegin(); i!=elements.cend(); ++i) {
                if (i->_gridCoords == (baseCoord + AdjOffset[c])) {
                    result[c] = i->_arrayIndex;
                    break;
                }
            }
        }
    }

    static CellConstants MakeCellConstants(
        const ShallowWaterSim::ActiveElement& ele,
        const std::vector<ShallowWaterSim::ActiveElement>& elements,
        Float2 offset = Float2(0,0))
    {
        CellConstants constants = { 
            Int2(ele._gridCoords[0], ele._gridCoords[1]), ele._arrayIndex, 0,
            offset, 
            {-1, -1, -1, -1, -1, -1, -1, -1},
            {0,0}
        };
        FindAdjacentGrids(constants._adjacentGrids, ele._gridCoords, elements);
        return constants;
    }

    static void SetCellConstants(
        RenderCore::Metal::DeviceContext& context, 
        Metal::ConstantBuffer& cb, 
        const ShallowWaterSim::ActiveElement& ele,
        const std::vector<ShallowWaterSim::ActiveElement>& elements,
        Float2 offset = Float2(0,0))
    {
        auto constants = MakeCellConstants(ele, elements, offset);
        cb.Update(context, &constants, sizeof(CellConstants));
    }

    static void DispatchEachElement(
        const ShallowWaterSim::SimulationContext& context,
        const std::vector<ShallowWaterSim::ActiveElement>& elements,
        Metal::ConstantBuffer& basicConstantsBuffer, Metal::ConstantBuffer& surfaceHeightsConstantsBuffer,
        const ShallowWaterSim::SimSettings& settings, 
        unsigned elementDimension)
    {
        for (auto i=elements.cbegin(); i!=elements.cend(); ++i) {
            if (i->_arrayIndex < 128) {
                if (!SetAddressingConstants(context, surfaceHeightsConstantsBuffer, i->_gridCoords))
                    continue;
                SetCellConstants(*context._metalContext, basicConstantsBuffer, *i, elements);
                context._metalContext->Dispatch(1, elementDimension, 1);
            }
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<int Count>
        static void BuildShaderDefines(
            char (&result)[Count], unsigned gridDimension, 
            ISurfaceHeightsProvider* surfaceHeightsProvider = nullptr, 
            ShallowWaterSim::BorderMode::Enum borderMode = ShallowWaterSim::BorderMode::BaseHeight,
            bool useLookupTable = false)
    {
        _snprintf_s(result, _TRUNCATE,
            "SHALLOW_WATER_TILE_DIMENSION=%i;SHALLOW_WATER_BOUNDARY=%i;SURFACE_HEIGHTS_FLOAT=%i;USE_LOOKUP_TABLE=%i", 
            gridDimension, unsigned(borderMode), 
            surfaceHeightsProvider ? int(surfaceHeightsProvider->IsFloatFormat()) : 0,
            int(useLookupTable));
    }

    void ShallowWaterSim::ExecuteInternalSimulation(
        const SimulationContext& context,
        const SimSettings& settings,
        unsigned bufferCounter)
    {
        unsigned thisFrameBuffer     = (bufferCounter+0) % _simulationGrid->_rotatingBufferCount;
        unsigned prevFrameBuffer     = (bufferCounter+2) % _simulationGrid->_rotatingBufferCount;     // (ie, -1 then +3)
        unsigned prevPrevFrameBuffer = (bufferCounter+1) % _simulationGrid->_rotatingBufferCount;

        auto& metalContext = *context._metalContext;
        CheckInitialClear(metalContext, *_simulationGrid);

        auto materialConstants = Internal::BuildOceanMaterialConstants(
            *context._oceanSettings, context._gridPhysicalDimension);
        Metal::ConstantBuffer globalOceanMaterialConstantBuffer(&materialConstants, sizeof(materialConstants));

        char shaderDefines[256]; 
        BuildShaderDefines(shaderDefines, _gridDimension, context._surfaceHeightsProvider, context._borderMode, _lookupTableSRV.IsGood());

        if (prevFrameBuffer!=thisFrameBuffer) {
            metalContext.BindCS(MakeResourceList(
                context._surfaceHeightsProvider->GetSRV(),
                _simulationGrid->_waterHeightsSRV[prevFrameBuffer], 
                _simulationGrid->_waterHeightsSRV[prevPrevFrameBuffer],
                _lookupTableSRV));
        } else {
            metalContext.BindCS(MakeResourceList(
                context._surfaceHeightsProvider->GetSRV(), SRV(), SRV(), _lookupTableSRV));
        }

        if (context._globalOceanWorkingHeights) {
            metalContext.BindCS(MakeResourceList(4, *context._globalOceanWorkingHeights));
        }

        metalContext.BindCS(MakeResourceList(0, globalOceanMaterialConstantBuffer));

        SimulatingConstants simConstants = {
            settings._rainQuantityPerFrame, 
            settings._evaporationConstant,
            settings._pressureConstant,
            0,
            settings._compressionConstants
        };

        Metal::ConstantBuffer surfaceHeightsConstantsBuffer(nullptr, sizeof(SurfaceHeightsAddressingConstants));
        Metal::ConstantBuffer basicConstantsBuffer(nullptr, sizeof(CellConstants));
        metalContext.BindCS(
            MakeResourceList(
                1, surfaceHeightsConstantsBuffer, basicConstantsBuffer, 
                Metal::ConstantBuffer(&simConstants, sizeof(simConstants))));

        if (!_usePipeModel) {

            metalContext.BindCS(MakeResourceList(
                _simulationGrid->_waterHeightsUAV[thisFrameBuffer],
                _simulationGrid->_waterVelocitiesUAV[0],
                _simulationGrid->_waterVelocitiesUAV[1],
                _simulationGrid->_waterVelocitiesUAV[2],
                _simulationGrid->_waterVelocitiesUAV[3]));

            auto& cshaderH = ::Assets::GetAssetDep<Metal::ComputeShader>("xleres/Ocean/ShallowWaterSim.csh:RunSimulationH:cs_*", shaderDefines);
            auto& cshaderV = ::Assets::GetAssetDep<Metal::ComputeShader>("xleres/Ocean/ShallowWaterSim.csh:RunSimulationV:cs_*", shaderDefines);

            for (unsigned p=0; p<2; ++p) {
                    // flip forward and reverse iteration through "box._activeSimulationElements" every frame
                    //  (and every pass)
                for (auto i = _activeSimulationElements.cbegin(); i!=_activeSimulationElements.cend(); ++i) {
                    if (i->_arrayIndex < 128) {
                        SetCellConstants(metalContext, basicConstantsBuffer, *i, _activeSimulationElements);
                        if (!SetAddressingConstants(context, surfaceHeightsConstantsBuffer, i->_gridCoords))
                            continue;

                            // checkerboard pattern flip horizontal/vertical
                        int flip = (i->_gridCoords[0] + i->_gridCoords[1] + bufferCounter + p)&1;
                        metalContext.Bind((flip)?cshaderH:cshaderV); metalContext.Dispatch(1, _gridDimension, 1);
                    }
                }
            }

                //  if we've requested normals, we should calculate them now. We can use the same method as
                //  the pipe model to calculate a rough approximation of the movement of water.
            if (Tweakable("OceanVelMethod", 1)==0) {
                auto& cshaderVel = ::Assets::GetAssetDep<Metal::ComputeShader>(
                    "xleres/Ocean/PipeModelShallowWaterSim.csh:UpdateVelocities:cs_*", shaderDefines);
                metalContext.Bind(cshaderVel);
                DispatchEachElement(
                    context, _activeSimulationElements, basicConstantsBuffer, surfaceHeightsConstantsBuffer, 
                    settings, _gridDimension);
            } else {

                    //      Second method for calculating velocity
                    //      This method uses the slopes of the change in height to attempt to estimate water flow
                    
                metalContext.BindCS(MakeResourceList(
                    5, _simulationGrid->_slopesBufferUAV[0],
                    _simulationGrid->_slopesBufferUAV[1]));

                metalContext.Bind(::Assets::GetAssetDep<Metal::ComputeShader>("xleres/Ocean/ShallowWaterSim.csh:UpdateVelocities0:cs_*", shaderDefines));
                DispatchEachElement(
                    context, _activeSimulationElements, basicConstantsBuffer, surfaceHeightsConstantsBuffer, 
                    settings, _gridDimension);


                metalContext.Bind(::Assets::GetAssetDep<Metal::ComputeShader>("xleres/Ocean/ShallowWaterSim.csh:UpdateVelocities1:cs_*", shaderDefines));
                DispatchEachElement(
                    context, _activeSimulationElements, basicConstantsBuffer, surfaceHeightsConstantsBuffer, 
                    settings, _gridDimension);
            }

        } else {

                // have to run all of the "update velocities" first and then update heights
            auto& cshader0 = ::Assets::GetAssetDep<Metal::ComputeShader>(
                "xleres/Ocean/PipeModelShallowWaterSim.csh:UpdateVelocities:cs_*", 
                (StringMeld<256>() << shaderDefines << ";WRITING_VELOCITIES=1").get());
            auto& cshader1 = ::Assets::GetAssetDep<Metal::ComputeShader>(
                "xleres/Ocean/PipeModelShallowWaterSim.csh:UpdateHeights:cs_*", shaderDefines);

                // order is important... We must start in the bottom right corner and work to the top left
            auto sortedElements = _activeSimulationElements;
            std::sort(sortedElements.begin(), sortedElements.end(),
                [](const ShallowWaterSim::ActiveElement& lhs, const ShallowWaterSim::ActiveElement& rhs) -> bool
                {
                    if (lhs._gridCoords[1] == rhs._gridCoords[1]) return lhs._gridCoords[0] > rhs._gridCoords[0];
                    return lhs._gridCoords[1] > rhs._gridCoords[1];
                });

            for (unsigned pass=0; pass<2; ++pass) {
                metalContext.Bind((pass==0)?cshader0:cshader1);

                    // limit of 8 UAV slots means that we can't bind 8 velocity UAVs 
                    // and a heights UAV at the same time
                if (pass == 0) {
                    metalContext.BindCS(MakeResourceList(
                        _simulationGrid->_waterVelocitiesUAV[0],
                        _simulationGrid->_waterVelocitiesUAV[1],
                        _simulationGrid->_waterVelocitiesUAV[2],
                        _simulationGrid->_waterVelocitiesUAV[3],
                        _simulationGrid->_waterVelocitiesUAV[4],
                        _simulationGrid->_waterVelocitiesUAV[5],
                        _simulationGrid->_waterVelocitiesUAV[6],
                        _simulationGrid->_waterVelocitiesUAV[7]));
                    metalContext.BindCS(MakeResourceList(
                        5, _simulationGrid->_waterHeightsSRV[thisFrameBuffer]));
                } else {
                    metalContext.BindCS(MakeResourceList(
                        5,
                        _simulationGrid->_waterVelocitiesSRV[0],
                        _simulationGrid->_waterVelocitiesSRV[1],
                        _simulationGrid->_waterVelocitiesSRV[2],
                        _simulationGrid->_waterVelocitiesSRV[3],
                        _simulationGrid->_waterVelocitiesSRV[4],
                        _simulationGrid->_waterVelocitiesSRV[5],
                        _simulationGrid->_waterVelocitiesSRV[6],
                        _simulationGrid->_waterVelocitiesSRV[7]));
                    metalContext.BindCS(MakeResourceList(
                        _simulationGrid->_waterHeightsUAV[thisFrameBuffer]));
                }

                DispatchEachElement(
                    context, sortedElements, basicConstantsBuffer, surfaceHeightsConstantsBuffer, 
                    settings, _gridDimension);

                if (pass == 0) {
                    metalContext.UnbindCS<UAV>(0, 8);
                    metalContext.UnbindCS<SRV>(5, 1);
                } else {
                    metalContext.UnbindCS<SRV>(5, 8);
                    metalContext.UnbindCS<UAV>(0, 1);
                }
            }

        }

        metalContext.UnbindCS<UAV>(0, 5);
        metalContext.UnbindVS<SRV>(0, 5);
    }

        ////////////////////////////////

    void ShallowWaterSim::BeginElements(
        const SimulationContext& context,
        const Int2* newElementsBegin, const Int2* newElementsEnd)
    {
        const bool usePipeModel = _usePipeModel;

        auto& metalContext = *context._metalContext;
        CheckInitialClear(metalContext, *_simulationGrid);

///////////////////////////////////////////////////////////////////////////////////////////////////

        if (!usePipeModel) {
            metalContext.BindCS(MakeResourceList(
                _simulationGrid->_waterHeightsUAV[0],
                _simulationGrid->_waterHeightsUAV[1], 
                _simulationGrid->_waterHeightsUAV[2],
                _lookupTableUAV));
        } else {
            metalContext.BindCS(MakeResourceList(
                _simulationGrid->_waterHeightsUAV[0],
                UAV(), UAV(), _lookupTableUAV));
        }

        if (context._globalOceanWorkingHeights)
            metalContext.BindCS(MakeResourceList(4, *context._globalOceanWorkingHeights));
        metalContext.BindCS(MakeResourceList(1, Techniques::CommonResources()._linearClampSampler));

///////////////////////////////////////////////////////////////////////////////////////////////////

        char shaderDefines[256]; 
        BuildShaderDefines(shaderDefines, _gridDimension, context._surfaceHeightsProvider, context._borderMode, _lookupTableSRV.IsGood());

        auto& cshader = ::Assets::GetAssetDep<Metal::ComputeShader>(
            usePipeModel?"xleres/Ocean/InitSimGrid.csh:InitPipeModel:cs_*":"xleres/Ocean/InitSimGrid.csh:main:cs_*", shaderDefines);

        auto materialConstants = Internal::BuildOceanMaterialConstants(*context._oceanSettings, context._gridPhysicalDimension);
        Metal::ConstantBuffer globalOceanMaterialConstantBuffer(&materialConstants, sizeof(materialConstants));
        metalContext.BindCS(MakeResourceList(globalOceanMaterialConstantBuffer));
        metalContext.Bind(cshader);
        if (context._surfaceHeightsProvider)
            metalContext.BindCS(MakeResourceList(context._surfaceHeightsProvider->GetSRV()));

///////////////////////////////////////////////////////////////////////////////////////////////////

        Metal::ConstantBuffer initCellConstantsBuffer(nullptr, sizeof(CellConstants));
        Metal::ConstantBuffer surfaceHeightsAddressingBuffer(nullptr, sizeof(SurfaceHeightsAddressingConstants));
        metalContext.BindCS(MakeResourceList(1, surfaceHeightsAddressingBuffer, initCellConstantsBuffer));

        std::vector<ShallowWaterSim::ActiveElement> gridsForSecondInitPhase;
        gridsForSecondInitPhase.reserve(newElementsEnd - newElementsBegin);
        auto poolOfUnallocatedArrayIndices = _poolOfUnallocatedArrayIndices;
        auto newElements = _activeSimulationElements;
            
        for (auto i=newElementsBegin; i!=newElementsEnd; ++i) {
            assert(!poolOfUnallocatedArrayIndices.empty());        // there should always been at least one unallocated array index

                //  Check the surface heights provider, to get the surface heights
                //  if this fails, we can't render this grid element
                //      todo -- we need to not only "get", but "lock" this data, so it's not swapped out
            if (!SetAddressingConstants(context, surfaceHeightsAddressingBuffer, *i))
                continue;

                //  Assign one of the free grids (or destroy the least recently used one)
                //  call a compute shader to fill out the simulation grids with the new values
                //  (this will also set the simulation grid value into the lookup table)
            unsigned assignmentIndex = *(poolOfUnallocatedArrayIndices.cend()-1);
            poolOfUnallocatedArrayIndices.erase(poolOfUnallocatedArrayIndices.cend()-1);

            ShallowWaterSim::ActiveElement newElement(*i, assignmentIndex);
            auto insertPoint = std::lower_bound(newElements.begin(), newElements.end(), newElement, SortOceanGridElement);
            newElements.insert(insertPoint, newElement);

            gridsForSecondInitPhase.push_back(newElement);

            SetCellConstants(metalContext, initCellConstantsBuffer, newElement, std::vector<ActiveElement>());
            metalContext.Dispatch(1, _gridDimension, 1);
        }

        if (usePipeModel) {
            metalContext.BindCS(MakeResourceList(
                _simulationGrid->_waterVelocitiesUAV[0],
                _simulationGrid->_waterVelocitiesUAV[1],
                _simulationGrid->_waterVelocitiesUAV[2],
                _simulationGrid->_waterVelocitiesUAV[3],
                _simulationGrid->_waterVelocitiesUAV[4],
                _simulationGrid->_waterVelocitiesUAV[5],
                _simulationGrid->_waterVelocitiesUAV[6],
                _simulationGrid->_waterVelocitiesUAV[7]));

            auto& cshader = ::Assets::GetAssetDep<Metal::ComputeShader>(
                "xleres/Ocean/InitSimGrid2.csh:InitPipeModel2:cs_*", shaderDefines);
            metalContext.Bind(cshader);
            for (auto i = gridsForSecondInitPhase.cbegin(); i!=gridsForSecondInitPhase.cend(); ++i) {
                SetCellConstants(*context._metalContext, initCellConstantsBuffer, *i, std::vector<ActiveElement>());
                metalContext.Dispatch(1, _gridDimension, 1);
            }
        }

        metalContext.UnbindCS<UAV>(0,8);

        _poolOfUnallocatedArrayIndices = std::move(poolOfUnallocatedArrayIndices);
        _activeSimulationElements = std::move(newElements);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void ShallowWaterSim::ExecuteSim(
        const SimulationContext& context, 
        LightingParserContext& parserContext, 
        const SimSettings& settings,
        unsigned bufferCounter,
        const Int2* validGridBegin, const Int2* validGridEnd)
    {
            // run a simulation of shallow water (for some interesting wave dynamics near the shore...)
        auto& oceanReset = Tweakable("OceanReset", false);
        const auto shallowWaterBorderMode = (BorderMode::Enum)Tweakable("OceanShallowBorder", 1);
        const float baseHeight = context._oceanSettings->_baseHeight;

        auto materialConstants = Internal::BuildOceanMaterialConstants(*context._oceanSettings, context._gridPhysicalDimension);
        Metal::ConstantBuffer globalOceanMaterialConstantBuffer(&materialConstants, sizeof(materialConstants));
        auto& metalContext = *context._metalContext;

            // unbind resources that were bound in ShallowWater_BindForOceanRender
        metalContext.UnbindVS<SRV>( 3, 2);
        metalContext.UnbindPS<SRV>( 5, 1);
        metalContext.UnbindPS<SRV>(11, 1);
        metalContext.UnbindPS<SRV>(15, 1);
        metalContext.BindCS(MakeResourceList(1, Techniques::CommonResources()._linearClampSampler));

        if (oceanReset) {
            _activeSimulationElements.clear();
            _poolOfUnallocatedArrayIndices.clear();
            _poolOfUnallocatedArrayIndices.reserve(_simulatingGridsCount);
            for (unsigned c=0; c<_simulatingGridsCount; ++c) {
                _poolOfUnallocatedArrayIndices.push_back(c);
            }

            if (_lookupTableUAV.IsGood()) {
                unsigned clearInts[] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
                metalContext.ClearUInt(_lookupTableUAV, clearInts);
            }
        }

        unsigned thisFrameBuffer = (bufferCounter+0)%_simulationGrid->_rotatingBufferCount;

            //  Calculate what grids we need for the current frame. If those grids aren't currently
            //  simulated, we need to set up the simulation (calculate depths and initialise the 
            //  water height
        std::vector<PrioritisedActiveElement> scheduledGrids;
        std::vector<PrioritisedActiveElement> gridsToPrioritise;
    
            //  Either we assume that all possible grids are valid (ie, when simulating a large open
            //  ocean). Or we pick from a limited number of available grids (ie, when simulating a
            //  restricted body of water)
        auto cameraPosition = ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld);
        if (validGridBegin && validGridEnd) {
            for (auto i = validGridBegin; i != validGridEnd; ++i) {
                auto visibility = GridIsVisible(context, parserContext, *i, baseHeight);
                    if (visibility) {
                            // calculate priority
                        Float2 gridCentrePosition = 
                            Float2(float((*i)[0]) + 0.5f, float((*i)[1]) + 0.5f) * context._gridPhysicalDimension
                            + context._physicalMins;

                        float gridDistance = Magnitude(gridCentrePosition - Float2(cameraPosition[0], cameraPosition[1]));
                        scheduledGrids.push_back(PrioritisedActiveElement(*i, gridDistance));
                    }
            }
        } else {
            signed baseGridX = signed((cameraPosition[0] - context._physicalMins[0]) / context._gridPhysicalDimension);
            signed baseGridY = signed((cameraPosition[1] - context._physicalMins[1]) / context._gridPhysicalDimension);
            for (signed y=0; y<5; ++y) {
                for (signed x=0; x<5; ++x) {
                    Int2 testGrid(baseGridX + x - 2, baseGridY + y - 2);
                    auto visibility = GridIsVisible(context, parserContext, testGrid, baseHeight);
                    if (visibility) {
                            // calculate priority
                        Float2 gridCentrePosition = 
                            Float2(float(testGrid[0]) + 0.5f, float(testGrid[1]) + 0.5f) * context._gridPhysicalDimension
                            + context._physicalMins;

                        float gridDistance = Magnitude(gridCentrePosition - Float2(cameraPosition[0], cameraPosition[1]));
                        float priority = gridDistance;
                        // if (visibility != CULL_INCLUSION) {
                        //     priority += 512.f;   // give priority penalty for grids on the edge of the screen (but maybe exclude the grid the camera is immediately over?)
                        // }

                        scheduledGrids.push_back(PrioritisedActiveElement(testGrid, priority));
                    }
                }
            }
        }

        std::sort(scheduledGrids.begin(), scheduledGrids.end(), SortByGridIndex);
        auto i2 = _activeSimulationElements.begin();
        for (auto i = scheduledGrids.cbegin(); i != scheduledGrids.cend(); ++i) {

                //  since "scheduledGrids" is sorted, we can truncate the search through
                //  box._activeSimulationElements every time
            auto t = std::equal_range(i2, _activeSimulationElements.end(), i->_e, SortOceanGridElement);
            bool foundEqual = false;
            for (auto t2 = t.first; t2 != t.second; ++t2) {
                if (t2->_gridCoords[0] == i->_e._gridCoords[0] && t2->_gridCoords[0] == i->_e._gridCoords[0]) {
                    foundEqual = true;
                    break;
                }
            }

            if (!foundEqual) {
                gridsToPrioritise.push_back(*i);
            }
            i2 = t.first;
        }

            // add the old grids into the list of grids to prioritize
        for (auto i=_activeSimulationElements.cbegin(); i!=_activeSimulationElements.cend(); ++i) {
            Float2 gridCentrePosition = Float2(
                (float(i->_gridCoords[0]) + 0.5f) * context._gridPhysicalDimension, 
                (float(i->_gridCoords[1]) + 0.5f) * context._gridPhysicalDimension)
                + context._physicalMins;
            float gridDistance = Magnitude(gridCentrePosition - Float2(cameraPosition[0], cameraPosition[1]));
                //  Prioritize existing grids while ignoring camera facing. The way, the simulation won't
                //  be stopped as soon as it goes off screen... Perhaps we can stop if the grid stays off the
                //  screen for a number of frames.
            float priority = gridDistance;  
            gridsToPrioritise.push_back(PrioritisedActiveElement(*i, priority));
        }

        std::vector<Int4> gridsDestroyedThisFrame;
        bool activeGridsChanged = false;
        std::sort(gridsToPrioritise.begin(), gridsToPrioritise.end(), SortByPriority);
        if (gridsToPrioritise.size() > _simulatingGridsCount) {
                // cancel some grids, and return their ids to the pool
            for (auto i=gridsToPrioritise.begin() + _simulatingGridsCount; i!=gridsToPrioritise.end(); ++i) {
                if (i->_e._arrayIndex!=~unsigned(0x0)) {
                    _poolOfUnallocatedArrayIndices.push_back(i->_e._arrayIndex);
                    gridsDestroyedThisFrame.push_back(Int4(i->_e._gridCoords[0], i->_e._gridCoords[1], 0, 0));
                    activeGridsChanged = true;
                }
            }
            gridsToPrioritise.erase(gridsToPrioritise.begin() + _simulatingGridsCount, gridsToPrioritise.end());
        }
        activeGridsChanged |= gridsToPrioritise.size() > _activeSimulationElements.size();

            // Setup any new grids that have been priortised into the list...

            // todo --  should we have a tree of these simulation grids... Some distance grids could be
            //          at lower resolution. Only closer grids would be at maximum resolution...?

        if (activeGridsChanged) {
            _activeSimulationElements.clear();
            std::vector<Int2> gridsToBegin;
            for (const auto& i : gridsToPrioritise) {
                if (i._e._arrayIndex != ~unsigned(0x0)) {
                    auto insertPoint = std::lower_bound(_activeSimulationElements.begin(), _activeSimulationElements.end(), i._e, SortOceanGridElement);
                    _activeSimulationElements.insert(insertPoint, i._e);
                } else {
                    gridsToBegin.push_back(i._e._gridCoords);
                }
            }

            if (!gridsToBegin.empty())
                BeginElements(context, AsPointer(gridsToBegin.cbegin()), AsPointer(gridsToBegin.cend()));
        }

        if (!gridsDestroyedThisFrame.empty()) {

                //  if there are any grids that were removed this frame, we have to clear their
                //  entry in "shallowBox._lookupTableUAV" (otherwise they cause lots of trouble)
                //  There isn't a really convenient way to do this with a compute shader. But
                //  we can execute a simple shader with a (1,1,1) thread count...

            struct ClearGridsConstants
            {
                unsigned _indexCount;
                unsigned _dummy[3];
                Int4 _indices[128];
            } clearGridsConstants;
            std::fill(clearGridsConstants._indices, &clearGridsConstants._indices[dimof(clearGridsConstants._indices)], Int4(INT_MAX, INT_MAX, INT_MAX, INT_MAX));
            std::copy(gridsDestroyedThisFrame.begin(), gridsDestroyedThisFrame.end(), clearGridsConstants._indices);
            clearGridsConstants._indexCount = unsigned(gridsDestroyedThisFrame.size());

            metalContext.Bind(::Assets::GetAssetDep<Metal::ComputeShader>("xleres/Ocean/InitSimGrid.csh:ClearGrids:cs_*"));
            metalContext.BindCS(MakeResourceList(0, Metal::ConstantBuffer(&clearGridsConstants, sizeof(clearGridsConstants))));
            metalContext.Dispatch(unsigned(gridsDestroyedThisFrame.size()));

        }

        metalContext.UnbindCS<UAV>(0, 8);

            //  For each actively simulated grid, run the compute shader to calculate the heights 
            //  for the new frame. We simulate horizontally and vertically separate. Between frames we
            //  alternate the order.

        if (!_activeSimulationElements.empty())
            ExecuteInternalSimulation(context, settings, bufferCounter);

        char shaderDefines[256];
        BuildShaderDefines(shaderDefines, _gridDimension, nullptr, ShallowWaterSim::BorderMode::BaseHeight, _lookupTableSRV.IsGood());

                //  Generate normals using the displacement textures
                //  Note, this will generate the normals for every array slice, even
                //  for slices that aren't actually used.
        if (!_simulationGrid->_normalsTextureUAV.empty()) {

            auto& buildNormals = ::Assets::GetAssetDep<Metal::ComputeShader>("xleres/Ocean/OceanNormalsShallow.csh:BuildDerivatives:cs_*", shaderDefines);
            auto& buildNormalsMipmaps = ::Assets::GetAssetDep<Metal::ComputeShader>("xleres/Ocean/OceanNormalsShallow.csh:BuildDerivativesMipmap:cs_*", shaderDefines);

                // build devs shader needs to know adjacent cells on right, bottom and bottom-right edges
            int buildDevsConstants[4*128];
            XlSetMemory(buildDevsConstants, 0xff, sizeof(buildDevsConstants));
            for (auto i = _activeSimulationElements.cbegin(); i!=_activeSimulationElements.cend(); ++i) {
                if (i->_arrayIndex < dimof(buildDevsConstants)/4) {
                    int adj[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
                    FindAdjacentGrids(adj, i->_gridCoords, _activeSimulationElements);
                    buildDevsConstants[i->_arrayIndex*4+0] = adj[4];
                    buildDevsConstants[i->_arrayIndex*4+1] = adj[6];
                    buildDevsConstants[i->_arrayIndex*4+2] = adj[7];
                }
            }

            metalContext.BindCS(MakeResourceList(globalOceanMaterialConstantBuffer));
            metalContext.BindCS(MakeResourceList(2, Metal::ConstantBuffer(buildDevsConstants, sizeof(buildDevsConstants))));

            metalContext.BindCS(MakeResourceList(   
                _simulationGrid->_waterHeightsSRV[thisFrameBuffer],
                _lookupTableSRV,
                _simulationGrid->_foamQuantitySRV2[(bufferCounter+1)&1]));
            metalContext.BindCS(MakeResourceList(   
                _simulationGrid->_normalsTextureUAV[0], 
                _simulationGrid->_foamQuantityUAV[bufferCounter&1]));

            metalContext.Bind(buildNormals); 
            metalContext.Dispatch(1, 1, _simulatingGridsCount);
            metalContext.UnbindCS<UAV>(0, 2);

                // do we really need mipmaps for shallow water grids?
            metalContext.Bind(buildNormalsMipmaps);
            for (unsigned step = 0; step<_simulationGrid->_normalsTextureUAV.size()-1; ++step) {
                unsigned mipDims = _gridDimension >> (step+1);
                unsigned constants[4] = { mipDims, mipDims, 0, 0 };
                metalContext.BindCS(MakeResourceList(Metal::ConstantBuffer(constants, sizeof(constants))));

                metalContext.BindCS(MakeResourceList(4, _simulationGrid->_normalsSingleMipSRV[step]));
                metalContext.BindCS(MakeResourceList(_simulationGrid->_normalsTextureUAV[step+1]));
            
                metalContext.Dispatch((mipDims + (8-1))/8, (mipDims + (8-1))/8, _simulatingGridsCount);
                metalContext.UnbindCS<UAV>(0, 1);
            }
        }

            // Draw some debugging information displaying the current heights of the liquid
            // {
            //  SetupVertexGeneratorShader(context);
            //  context->Bind(Assets::GetAssetDep<Metal::ShaderProgram>(
            //      "xleres/basic2D.vsh:fullscreen:vs_*", 
            //      "xleres/Ocean/FFTDebugging.psh:ShallowWaterDebugging:ps_*"));
            //  context->BindPS(MakeResourceList(   4, box._simulationGrid->_waterHeightsSRV[thisFrameBuffer],
            //                                      box._simulationGrid->_surfaceHeightsSRV));
            //  context->Draw(4);
            // }

        if (Tweakable("OceanShallowDrawWireframe", false)) {
            RenderWireframe(
                metalContext, parserContext, 
                *context._oceanSettings, 
                context._gridPhysicalDimension, Float2(0.f, 0.f), bufferCounter, shallowWaterBorderMode);
        }
    }

    void ShallowWaterSim::RenderWireframe(
        MetalContext& metalContext, LightingParserContext& parserContext, 
        const DeepOceanSimSettings& oceanSettings, float gridPhysicalDimension, Float2 offset,
        unsigned bufferCounter, BorderMode::Enum borderMode)
    {
        auto materialConstants = Internal::BuildOceanMaterialConstants(oceanSettings, gridPhysicalDimension);
        Metal::ConstantBuffer globalOceanMaterialConstantBuffer(&materialConstants, sizeof(materialConstants));

        char shaderDefines[256]; 
        BuildShaderDefines(shaderDefines, _gridDimension, nullptr, borderMode, _lookupTableSRV.IsGood());

        unsigned thisFrameBuffer = (bufferCounter+0) % _simulationGrid->_rotatingBufferCount;
        auto& patchRender = ::Assets::GetAssetDep<Metal::ShaderProgram>(
            "xleres/Ocean/OceanPatch.vsh:ShallowWater:vs_*",
            "xleres/solidwireframe.gsh:main:gs_*",
            "xleres/solidwireframe.psh:main:ps_*",
            shaderDefines);
        Metal::BoundUniforms boundUniforms(patchRender);
        Techniques::TechniqueContext::BindGlobalUniforms(boundUniforms);
        boundUniforms.BindConstantBuffers(1, {"OceanMaterialSettings", "ShallowWaterCellConstants"});

        metalContext.Bind(patchRender);

        SetupVertexGeneratorShader(metalContext);
        metalContext.BindVS(MakeResourceList(3, _simulationGrid->_waterHeightsSRV[thisFrameBuffer]));
        metalContext.Bind(Topology::TriangleList);
        metalContext.Bind(Techniques::CommonResources()._dssReadWrite);

        auto& simplePatchBox = Techniques::FindCachedBox<SimplePatchBox>(SimplePatchBox::Desc(_gridDimension, _gridDimension, true));
        metalContext.Bind(simplePatchBox._simplePatchIndexBuffer, Format::R32_UINT);

        Metal::ConstantBuffer simulatingCB(nullptr, sizeof(CellConstants));

        const Metal::ConstantBuffer* prebuiltBuffers[] = { &globalOceanMaterialConstantBuffer, &simulatingCB };
        boundUniforms.Apply(
            metalContext, 
            parserContext.GetGlobalUniformsStream(),
            Metal::UniformsStream(nullptr, prebuiltBuffers, dimof(prebuiltBuffers)));

        for (auto i=_activeSimulationElements.cbegin(); i!=_activeSimulationElements.cend(); ++i) {
            if (i->_arrayIndex < 128) {
                SetCellConstants(metalContext, simulatingCB, *i, _activeSimulationElements, offset);
                metalContext.DrawIndexed(simplePatchBox._simplePatchIndexCount);
            }
        }

        metalContext.UnbindVS<SRV>(3, 1);
    }

    void ShallowWaterSim::RenderVelocities(
        MetalContext& metalContext, LightingParserContext& parserContext, 
        const DeepOceanSimSettings& oceanSettings, float gridPhysicalDimension, Float2 offset,
        unsigned bufferCounter, BorderMode::Enum borderMode,
        bool showErosion)
    {
        auto materialConstants = Internal::BuildOceanMaterialConstants(oceanSettings, gridPhysicalDimension);
        Metal::ConstantBuffer globalOceanMaterialConstantBuffer(&materialConstants, sizeof(materialConstants));

        char shaderDefines[256]; 
        BuildShaderDefines(shaderDefines, _gridDimension, nullptr, borderMode, _lookupTableSRV.IsGood());
        if (showErosion)
            XlCatString(shaderDefines, dimof(shaderDefines), ";SHOW_EROSION=1");

        unsigned thisFrameBuffer = (bufferCounter+0) % _simulationGrid->_rotatingBufferCount;
        auto& patchRender = ::Assets::GetAssetDep<Metal::ShaderProgram>(
            "xleres/Ocean/OceanVelocitiesDebugging.sh:vs_main:vs_*",
            "xleres/Ocean/OceanVelocitiesDebugging.sh:ps_main:ps_*",
            shaderDefines);

        Metal::BoundUniforms boundUniforms(patchRender);
        Techniques::TechniqueContext::BindGlobalUniforms(boundUniforms);
        boundUniforms.BindConstantBuffers(1, { "OceanMaterialSettings", "ShallowWaterCellConstants" });

        metalContext.Bind(patchRender);

        SetupVertexGeneratorShader(metalContext);
        metalContext.BindVS(MakeResourceList(1, 
            _simulationGrid->_waterHeightsSRV[thisFrameBuffer]));

        metalContext.BindPS(MakeResourceList(4, 
            _lookupTableSRV,
            _simulationGrid->_waterVelocitiesSRV[0],
            _simulationGrid->_waterVelocitiesSRV[1],
            _simulationGrid->_waterVelocitiesSRV[2],
            _simulationGrid->_waterVelocitiesSRV[3],
            _simulationGrid->_waterVelocitiesSRV[4],
            _simulationGrid->_waterVelocitiesSRV[5],
            _simulationGrid->_waterVelocitiesSRV[6],
            _simulationGrid->_waterVelocitiesSRV[7]));
        metalContext.Bind(Topology::TriangleList);
        metalContext.Bind(Techniques::CommonResources()._dssReadWrite);

        auto& simplePatchBox = Techniques::FindCachedBox<SimplePatchBox>(
            SimplePatchBox::Desc(_gridDimension, _gridDimension, true));
        metalContext.Bind(simplePatchBox._simplePatchIndexBuffer, Format::R32_UINT);

        Metal::ConstantBuffer simulatingCB(nullptr, sizeof(CellConstants));
        const Metal::ConstantBuffer* prebuiltBuffers[] = { &globalOceanMaterialConstantBuffer, &simulatingCB };
        boundUniforms.Apply(
            metalContext, 
            parserContext.GetGlobalUniformsStream(),
            Metal::UniformsStream(nullptr, prebuiltBuffers, dimof(prebuiltBuffers)));

        for (auto i=_activeSimulationElements.cbegin(); i!=_activeSimulationElements.cend(); ++i) {
            if (i->_arrayIndex < 128) {
                SetCellConstants(metalContext, simulatingCB, *i, _activeSimulationElements, offset);
                metalContext.DrawIndexed(simplePatchBox._simplePatchIndexCount);
            }
        }

        metalContext.UnbindVS<SRV>(3, 1);
    }

    void ShallowWaterSim::BindForOceanRender(MetalContext& metalContext, unsigned bufferCounter)
    {
        unsigned thisFrameBuffer = (bufferCounter+0)%_simulationGrid->_rotatingBufferCount;
        metalContext.BindVS(MakeResourceList(3, 
            _simulationGrid->_waterHeightsSRV[thisFrameBuffer],
            _lookupTableSRV));
        metalContext.BindPS(MakeResourceList(5, _simulationGrid->_normalsTextureShaderResource));
        if (_simulationGrid->_foamQuantitySRV[bufferCounter&1].IsGood())
            metalContext.BindPS(MakeResourceList(14, _simulationGrid->_foamQuantitySRV[bufferCounter&1]));
        metalContext.BindPS(MakeResourceList(15, _lookupTableSRV));
    }

    void ShallowWaterSim::UnbindForOceanRender(MetalContext& metalContext)
    {
        metalContext.UnbindVS<Metal::ShaderResourceView>(3, 2);
        metalContext.UnbindPS<Metal::ShaderResourceView>(5, 1);
        metalContext.UnbindPS<Metal::ShaderResourceView>(14, 2);
    }

    void ShallowWaterSim::BindForErosionSimulation(MetalContext& metalContext, unsigned bufferCounter)
    {
        unsigned thisFrameBuffer = (bufferCounter+0)%_simulationGrid->_rotatingBufferCount;
        metalContext.BindCS(MakeResourceList(3, _simulationGrid->_waterHeightsSRV[thisFrameBuffer], _lookupTableSRV));
        metalContext.BindCS(MakeResourceList(5, _simulationGrid->_waterVelocitiesSRV[0], _simulationGrid->_waterVelocitiesSRV[1], _simulationGrid->_waterVelocitiesSRV[2], _simulationGrid->_waterVelocitiesSRV[3]));
    }

    unsigned ShallowWaterSim::FindActiveGrid(Int2 gridCoords)
    {
        for (const auto& i : _activeSimulationElements)
            if (i._gridCoords == gridCoords)
                return i._arrayIndex;
        return ~unsigned(0x0);
    }

    RenderCore::SharedPkt ShallowWaterSim::BuildCellConstants(Int2 gridCoords)
    {
        for (const auto& i : _activeSimulationElements)
            if (i._gridCoords == gridCoords) {
                auto constants = MakeCellConstants(i, _activeSimulationElements);
                return RenderCore::MakeSharedPkt(constants);
            }
        return RenderCore::SharedPkt();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static std::pair<Float3, Float3> CalculateMouseOverRay(MetalContext& context, LightingParserContext& parserContext)
    {
            // calculate a world space ray underneath the mouse cursor
        auto cursorPos = GetCursorPos();

        Metal::ViewportDesc viewport(context);
        float A = Clamp(cursorPos[0] / viewport.Width, 0.f, 1.f);
        float B = Clamp(cursorPos[1] / viewport.Height, 0.f, 1.f);

        float weights[4] = {
            (1.0f - A) * (1.0f - B),
            (1.0f - A) * B,
            A * (1.0f - B),
            A * B,
        };
    
        Float3 absFrustumCorners[8];
        CalculateAbsFrustumCorners(
            absFrustumCorners, parserContext.GetProjectionDesc()._worldToProjection,
            RenderCore::Techniques::GetDefaultClipSpaceType());

            // use the weights to find positions on the near and far plane...
        return std::make_pair(
            weights[0] * absFrustumCorners[0] + weights[1] * absFrustumCorners[1] + weights[2] * absFrustumCorners[2] + weights[3] * absFrustumCorners[3],
            weights[0] * absFrustumCorners[4] + weights[1] * absFrustumCorners[5] + weights[2] * absFrustumCorners[6] + weights[3] * absFrustumCorners[7]);
    }

    Float4 OceanHack_CompressionConstants(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext, 
        float baseHeight, float compressionAmount, float compressionRadius)
    {
        static unsigned framesMouseDown = 0;
        if (IsLButtonDown() && IsShiftDown()) {
                //  Find the mouse over ray, and find the intersection
                //  point with the ocean water plane
            auto mouseOverRay = CalculateMouseOverRay(metalContext, parserContext);
            float a = mouseOverRay.first[2] - baseHeight;
            float b = mouseOverRay.second[2] - mouseOverRay.first[2];
            float alpha = -a / b;
            Float3 intersectionPoint = LinearInterpolate(mouseOverRay.first, mouseOverRay.second, alpha);
            
            Float4 result;
            result[0] = intersectionPoint[0];
            result[1] = intersectionPoint[1];
            result[2] = baseHeight-compressionAmount;
            result[3] = compressionRadius;

            ++framesMouseDown;
            return result;
        }

        framesMouseDown = 0;
        return Float4(0.f, 0.f, 1000.f, 1.f);
    }
}


