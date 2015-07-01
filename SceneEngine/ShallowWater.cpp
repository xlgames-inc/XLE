// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShallowWater.h"
#include "Ocean.h"
#include "SimplePatchBox.h"
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "SurfaceHeightsProvider.h"

#include "../RenderCore/Techniques/Techniques.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Metal/Format.h"
#include "../RenderCore/Metal/State.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/DeviceContextImpl.h"
#include "../RenderCore/Metal/Shader.h"

#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"

#include "../Math/ProjectionMath.h"
#include "../Math/Transformations.h"
#include "../ConsoleRig/Console.h"
#include "../Utility/BitUtils.h"
#include "../Utility/StringFormat.h"

#include "../Core/WinAPI/IncludeWindows.h"

#pragma warning(disable:4505)       // warning C4505: 'SceneEngine::BuildSurfaceHeightsTexture' : unreferenced local function has been removed

namespace SceneEngine
{
    using namespace RenderCore;
    using namespace RenderCore::Metal;

///////////////////////////////////////////////////////////////////////////////////////////////////
    class ShallowWaterGrid
    {
    public:
        intrusive_ptr<ID3D::Resource>           _waterHeightsTextures[3];
        RenderCore::Metal::ShaderResourceView   _waterHeightsSRV[3];
        RenderCore::Metal::UnorderedAccessView  _waterHeightsUAV[3];

        static const unsigned VelTextures = 8;
        intrusive_ptr<ID3D::Resource>           _waterVelocitiesTexture[VelTextures];
        RenderCore::Metal::ShaderResourceView   _waterVelocitiesSRV[VelTextures];
        RenderCore::Metal::UnorderedAccessView  _waterVelocitiesUAV[VelTextures];

        intrusive_ptr<ID3D::Resource>           _slopesBuffer[2];
        RenderCore::Metal::UnorderedAccessView  _slopesBufferUAV[2];

        intrusive_ptr<ID3D::Resource>                       _normalsTexture;
        std::vector<RenderCore::Metal::UnorderedAccessView> _normalsTextureUAV;
        std::vector<RenderCore::Metal::ShaderResourceView>  _normalsSingleMipSRV;
        RenderCore::Metal::ShaderResourceView               _normalsTextureShaderResource;

        intrusive_ptr<ID3D::Resource>           _foamQuantity[2];
        RenderCore::Metal::UnorderedAccessView  _foamQuantityUAV[2];
        RenderCore::Metal::ShaderResourceView   _foamQuantitySRV[2];
        RenderCore::Metal::ShaderResourceView   _foamQuantitySRV2[2];

        unsigned    _rotatingBufferCount;

        ShallowWaterGrid();
        ShallowWaterGrid(unsigned width, unsigned height, unsigned maxSimulationGrids, bool pipeModel, bool calculateVelocities);
        ~ShallowWaterGrid();
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    ShallowWaterGrid::ShallowWaterGrid() {}
    ShallowWaterGrid::ShallowWaterGrid(unsigned width, unsigned height, unsigned maxSimulationGrids, bool pipeModel, bool calculateVelocities)
    {
        using namespace BufferUploads;
        auto& uploads = GetBufferUploads();

        auto tDesc = BufferUploads::TextureDesc::Plain2D(width, height, NativeFormat::R32_TYPELESS, 1, uint8(maxSimulationGrids));
        if (height<=1) {
            tDesc = BufferUploads::TextureDesc::Plain1D(width, NativeFormat::R32_TYPELESS, 1, uint8(maxSimulationGrids));
        }

        BufferDesc targetDesc;
        targetDesc._type = BufferDesc::Type::Texture;
        targetDesc._bindFlags = BindFlag::ShaderResource|BindFlag::UnorderedAccess;
        targetDesc._cpuAccess = 0;
        targetDesc._gpuAccess = GPUAccess::Read|GPUAccess::Write;
        targetDesc._allocationRules = 0;
        targetDesc._name[0] = '\0';
        targetDesc._textureDesc = tDesc;

        intrusive_ptr<ID3D::Resource> waterHeightsTextures[3];
        UnorderedAccessView waterHeightsUAV[3];
        ShaderResourceView waterHeightsSRV[3];

        intrusive_ptr<ID3D::Resource> waterVelocitiesTexture[VelTextures];
        UnorderedAccessView waterVelocitiesUAV[VelTextures];
        ShaderResourceView waterVelocitiesSRV[VelTextures];

        intrusive_ptr<ID3D::Resource> slopesBuffer[2];
        UnorderedAccessView slopesBufferUAV[2];

        unsigned heightsTextureCount = pipeModel?1:3;

        for (unsigned c=0; c<heightsTextureCount; ++c) {
            waterHeightsTextures[c] = uploads.Transaction_Immediate(targetDesc)->AdoptUnderlying();
            waterHeightsUAV[c] = UnorderedAccessView(waterHeightsTextures[c].get(), NativeFormat::R32_FLOAT, 0, false, true);
            waterHeightsSRV[c] = ShaderResourceView(waterHeightsTextures[c].get(), NativeFormat::R32_FLOAT, maxSimulationGrids);
        }

            //  The pipe model always needs velocities. Otherwise, we only calculate them
            //  when the "calculateVelocities" flag is set. These velocities represent
            //  the movement of water from place to place.

        if (pipeModel || calculateVelocities) {
            targetDesc._textureDesc._nativePixelFormat = NativeFormat::R32_TYPELESS;
            auto velBuffer = BufferUploads::CreateBasicPacket(sizeof(float)*width*height, nullptr, BufferUploads::TexturePitches(width*4, width*height*4));
            std::fill((float*)velBuffer->GetData(), (float*)PtrAdd(velBuffer->GetData(), width*height*sizeof(float)), 0.f);
            for (unsigned c=0; c<VelTextures; ++c) {
                waterVelocitiesTexture[c] = uploads.Transaction_Immediate(targetDesc, velBuffer.get())->AdoptUnderlying();
                waterVelocitiesUAV[c] = UnorderedAccessView(waterVelocitiesTexture[c].get(), NativeFormat::R32_FLOAT, 0, false, true);
                waterVelocitiesSRV[c] = ShaderResourceView(waterVelocitiesTexture[c].get(), NativeFormat::R32_FLOAT, maxSimulationGrids);
            }
        }

        if (!pipeModel && calculateVelocities) {
            targetDesc._textureDesc._nativePixelFormat = NativeFormat::R32_TYPELESS;
            for (unsigned c=0; c<2; ++c) {
                slopesBuffer[c] = uploads.Transaction_Immediate(targetDesc)->AdoptUnderlying();
                slopesBufferUAV[c] = UnorderedAccessView(slopesBuffer[c].get(), NativeFormat::R32_FLOAT, 0, false, true);
            }
        }

                ////
        const unsigned normalsMipCount = IntegerLog2(std::max(width, height));
        const auto typelessNormalFormat = NativeFormat::R8G8_TYPELESS;
        const auto uintNormalFormat = NativeFormat::R8G8_UINT;
        const auto unormNormalFormat = NativeFormat::R8G8_UNORM;
        auto normalsBufferUploadsDesc = BuildRenderTargetDesc(
            BindFlag::UnorderedAccess|BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain2D(width, height, typelessNormalFormat, uint8(normalsMipCount), uint8(maxSimulationGrids)),
            "ShallowNormals");
        auto normalsTexture = uploads.Transaction_Immediate(normalsBufferUploadsDesc, nullptr)->AdoptUnderlying();
        std::vector<UnorderedAccessView> normalsTextureUVA;
        std::vector<ShaderResourceView> normalsSingleMipSRV;
        normalsTextureUVA.reserve(normalsMipCount);
        normalsSingleMipSRV.reserve(normalsMipCount);
        for (unsigned c=0; c<normalsMipCount; ++c) {
            normalsTextureUVA.push_back(UnorderedAccessView(normalsTexture.get(), uintNormalFormat, c, false, true));
            normalsSingleMipSRV.push_back(ShaderResourceView(normalsTexture.get(), uintNormalFormat, MipSlice(c, 1)));
        }
        ShaderResourceView normalsTextureShaderResource(normalsTexture.get(), unormNormalFormat, MipSlice(0, normalsMipCount));

                ////
        auto foamTextureDesc = BuildRenderTargetDesc(
            BindFlag::UnorderedAccess|BindFlag::ShaderResource,
            BufferUploads::TextureDesc::Plain2D(width, height, NativeFormat::R8_TYPELESS, 1, uint8(maxSimulationGrids)),
            "ShallowFoam");
        auto foamQuantity0 = uploads.Transaction_Immediate(foamTextureDesc, nullptr)->AdoptUnderlying();
        auto foamQuantity1 = uploads.Transaction_Immediate(foamTextureDesc, nullptr)->AdoptUnderlying();
        UnorderedAccessView foamQuantityUVA0(foamQuantity0.get(), NativeFormat::R8_UINT);
        ShaderResourceView foamQuantitySRV0(foamQuantity0.get(), NativeFormat::R8_UNORM);
        ShaderResourceView foamQuantitySRV20(foamQuantity0.get(), NativeFormat::R8_UINT);
        UnorderedAccessView foamQuantityUVA1(foamQuantity1.get(), NativeFormat::R8_UINT);
        ShaderResourceView foamQuantitySRV1(foamQuantity1.get(), NativeFormat::R8_UNORM);
        ShaderResourceView foamQuantitySRV21(foamQuantity1.get(), NativeFormat::R8_UINT);
    
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

                ////
        _normalsTexture = std::move(normalsTexture);
        _normalsTextureUAV = std::move(normalsTextureUVA);
        _normalsSingleMipSRV = std::move(normalsSingleMipSRV);
        _normalsTextureShaderResource = std::move(normalsTextureShaderResource);

                ////
        _foamQuantity[0] = std::move(foamQuantity0);
        _foamQuantity[1] = std::move(foamQuantity1);
        _foamQuantityUAV[0] = std::move(foamQuantityUVA0);
        _foamQuantityUAV[1] = std::move(foamQuantityUVA1);
        _foamQuantitySRV[0] = std::move(foamQuantitySRV0);
        _foamQuantitySRV[1] = std::move(foamQuantitySRV1);
        _foamQuantitySRV2[0] = std::move(foamQuantitySRV20);
        _foamQuantitySRV2[1] = std::move(foamQuantitySRV21);
    }

    ShallowWaterGrid::~ShallowWaterGrid() {}
    
    ShallowWaterSim::ShallowWaterSim(const Desc& desc)
    {
        using namespace BufferUploads;
        auto& uploads = GetBufferUploads();

        auto simulationGrid = std::make_unique<ShallowWaterGrid>(desc._gridDimension, desc._gridDimension, desc._maxSimulationGrid, desc._usePipeModel, desc._buildVelocities);

            //
            //      Build a lookup table that will provide the indices into
            //      the array of simulation grids... More complex implementations
            //      could use a sparse tree for this -- so that an infinitely
            //      large world could be supported.
            //
        const unsigned lookupTableDimensions = 512;
        BufferDesc targetDesc;
        targetDesc._type = BufferDesc::Type::Texture;
        targetDesc._bindFlags = BindFlag::ShaderResource|BindFlag::UnorderedAccess;
        targetDesc._cpuAccess = 0;
        targetDesc._gpuAccess = GPUAccess::Read|GPUAccess::Write;
        targetDesc._allocationRules = 0;
        targetDesc._name[0] = '\0';
        targetDesc._textureDesc = 
            BufferUploads::TextureDesc::Plain2D(
                lookupTableDimensions, lookupTableDimensions, NativeFormat::R8_TYPELESS);

        auto initData = BufferUploads::CreateEmptyPacket(targetDesc);
        XlSetMemory(initData->GetData(), 0xff, initData->GetDataSize());
        auto lookupTable = uploads.Transaction_Immediate(targetDesc, initData.get())->AdoptUnderlying();
        UnorderedAccessView lookupTableUAV(lookupTable.get(), NativeFormat::R8_UINT);
        ShaderResourceView lookupTableSRV(lookupTable.get(), NativeFormat::R8_UINT);

        _poolOfUnallocatedArrayIndices.reserve(desc._maxSimulationGrid);
        for (unsigned c=0; c<desc._maxSimulationGrid; ++c) {
            _poolOfUnallocatedArrayIndices.push_back(c);
        }
    
        _lookupTable = std::move(lookupTable);
        _lookupTableSRV = std::move(lookupTableSRV);
        _lookupTableUAV = std::move(lookupTableUAV);
        _simulationGrid = std::move(simulationGrid);
        _simulatingGridsCount = desc._maxSimulationGrid;
        _gridDimension = desc._gridDimension;
        _usePipeModel = desc._usePipeModel;
    }

    ShallowWaterSim::~ShallowWaterSim() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename First, typename Second>
        static bool SortCells(const std::pair<First,Second>& lhs, const std::pair<First,Second>& rhs) 
            { return (lhs.first + lhs.second) < (rhs.first + rhs.second); }

    static bool SortOceanGridElement(   const ShallowWaterSim::ActiveElement& lhs, 
                                        const ShallowWaterSim::ActiveElement& rhs)
    {
        return (lhs._gridX + lhs._gridY) < (rhs._gridX + rhs._gridY);
    }

    static bool GridIsVisible(LightingParserContext& parserContext, int gridX, int gridY, float gridPhysicalDimension, float baseWaterHeight)
    {
        Float3 mins( gridX    * gridPhysicalDimension,  gridY    * gridPhysicalDimension, baseWaterHeight - 3.f);
        Float3 maxs((gridX+1) * gridPhysicalDimension, (gridY+1) * gridPhysicalDimension, baseWaterHeight + 3.f);
        return !CullAABB_Aligned(AsFloatArray(parserContext.GetProjectionDesc()._worldToProjection), mins, maxs);
    }

    struct PrioritisedActiveElement
    {
    public:
        ShallowWaterSim::ActiveElement _e;
        float   _priority;
        PrioritisedActiveElement(signed gridX, signed gridY, float priority) : _e(gridX, gridY), _priority(priority) {}
        PrioritisedActiveElement(const ShallowWaterSim::ActiveElement& e, float priority) : _e(e), _priority(priority) {}
        PrioritisedActiveElement() {}
    };

    static bool SortByPriority(const PrioritisedActiveElement& lhs, const PrioritisedActiveElement& rhs)        { return lhs._priority < rhs._priority; }
    static bool SortByGridIndex(const PrioritisedActiveElement& lhs, const PrioritisedActiveElement& rhs)       { return SortOceanGridElement(lhs._e, rhs._e); }

///////////////////////////////////////////////////////////////////////////////////////////////////

    static Int2 GetCursorPos()
    {
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        ScreenToClient((HWND)::GetActiveWindow(), &cursorPos);
        return Int2(cursorPos.x, cursorPos.y);
    }

    static std::pair<Float3, Float3> CalculateMouseOverRay(RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext)
    {
            // calculate a world space ray underneath the mouse cursor
        auto cursorPos = GetCursorPos();

        ViewportDesc viewport(*context);
        float A = Clamp(cursorPos[0] / viewport.Width, 0.f, 1.f);
        float B = Clamp(cursorPos[1] / viewport.Height, 0.f, 1.f);

        float weights[4] = {
            (1.0f - A) * (1.0f - B),
            (1.0f - A) * B,
            A * (1.0f - B),
            A * B,
        };
    
        Float3 absFrustumCorners[8];
        CalculateAbsFrustumCorners(absFrustumCorners, parserContext.GetProjectionDesc()._worldToProjection);

            // use the weights to find positions on the near and far plane...
        return std::make_pair(
            weights[0] * absFrustumCorners[0] + weights[1] * absFrustumCorners[1] + weights[2] * absFrustumCorners[2] + weights[3] * absFrustumCorners[3],
            weights[0] * absFrustumCorners[4] + weights[1] * absFrustumCorners[5] + weights[2] * absFrustumCorners[6] + weights[3] * absFrustumCorners[7]);
    }

        ////////////////////////////////

    static ShallowWaterSim::SurfaceHeightsAddressingConstants
        CalculateAddressing(
            const ShallowWaterSim::ActiveElement& e, 
            ISurfaceHeightsProvider* surfaceHeightsProvider, 
            float gridPhysicalDimension)
    {
            //  Even though we have a cached heights addressing, we should recalculate height addressing to 
            //  match new uploads, etc...
            //  Note that if the terrain is edited, it can change the height scale & offset
        Float2 gridMins(float(e._gridX) * gridPhysicalDimension, float(e._gridY) * gridPhysicalDimension);
        Float2 gridMaxs(float(e._gridX+1) * gridPhysicalDimension, float(e._gridY+1) * gridPhysicalDimension);
        auto surfaceAddressing = surfaceHeightsProvider->GetAddress(gridMins, gridMaxs);
        assert(surfaceAddressing._valid);
        auto newHeightsAddressing = e._heightsAddressing;
        newHeightsAddressing._baseCoord = surfaceAddressing._baseCoordinate;
        newHeightsAddressing._textureMin = surfaceAddressing._minCoordOffset;
        newHeightsAddressing._textureMax = surfaceAddressing._maxCoordOffset;
        newHeightsAddressing._scale = surfaceAddressing._heightScale;
        newHeightsAddressing._offset = surfaceAddressing._heightOffset;
        return newHeightsAddressing;
    }

    static void SetAddressingConstants(RenderCore::Metal::DeviceContext* context, ConstantBuffer& cb, 
        const ShallowWaterSim::ActiveElement& e, 
        ISurfaceHeightsProvider* surfaceHeightsProvider, 
        float gridPhysicalDimension)
    {
        auto newHeightsAddressing = CalculateAddressing(e, surfaceHeightsProvider, gridPhysicalDimension);
        cb.Update(*context, &newHeightsAddressing, sizeof(newHeightsAddressing));
    }

    struct SimulatingConstants
    {
        Int2	    _simulatingIndex; 
	    unsigned    _arrayIndex;
	    float       _rainQuantityPerFrame;
        Float2      _worldSpaceOffset;
        unsigned    _dummy[2];
    };

    static void SetSimulatingConstants(RenderCore::Metal::DeviceContext* context, ConstantBuffer& cb, const ShallowWaterSim::ActiveElement& ele, float rainQuantityPerFrame, Float2 offset = Float2(0,0))
    {
        SimulatingConstants constants = { 
            Int2(ele._gridX, ele._gridY), ele._arrayIndex, 
            rainQuantityPerFrame, 
            offset, {0,0} 
        };

        cb.Update(*context, &constants, sizeof(SimulatingConstants));
    }

    static void DispatchEachElement(
        RenderCore::Metal::DeviceContext* context,
        const std::vector<ShallowWaterSim::ActiveElement>& elements,
        ConstantBuffer& basicConstantsBuffer, ConstantBuffer& surfaceHeightsConstantsBuffer,
        ISurfaceHeightsProvider* surfaceHeightsProvider, 
        float gridPhysicalDimension, float rainQuantityPerFrame, 
        unsigned elementDimension)
    {
        for (auto i=elements.cbegin(); i!=elements.cend(); ++i) {
            if (i->_arrayIndex < 128) {
                SetSimulatingConstants(context, basicConstantsBuffer, *i, rainQuantityPerFrame);
                SetAddressingConstants(context, surfaceHeightsConstantsBuffer, *i, surfaceHeightsProvider, gridPhysicalDimension);
                context->Dispatch(1, elementDimension, 1);
            }
        }
    }

    template<int Count>
        static void BuildShaderDefines(char (&result)[Count], unsigned gridDimension, ISurfaceHeightsProvider* surfaceHeightsProvider = nullptr, ShallowBorderMode::Enum borderMode = ShallowBorderMode::BaseHeight)
    {
        _snprintf_s(result, Count, 
            "SHALLOW_WATER_TILE_DIMENSION=%i;SHALLOW_WATER_BOUNDARY=%i;SURFACE_HEIGHTS_FLOAT=%i", 
            gridDimension, unsigned(borderMode), surfaceHeightsProvider ? int(surfaceHeightsProvider->IsFloatFormat()) : 0);
    }

    void ShallowWater_ExecuteInternalSimulation(
        RenderCore::Metal::DeviceContext* context,
        const OceanSettings& oceanSettings, float gridPhysicalDimension,
        RenderCore::Metal::ShaderResourceView* globalOceanWorkingHeights,
        ISurfaceHeightsProvider* surfaceHeightsProvider, ShallowWaterSim& shallowBox, 
        unsigned bufferCounter, const float compressionConstants[4], 
        float rainQuantityPerFrame, ShallowBorderMode::Enum borderMode)
    {
        unsigned thisFrameBuffer     = (bufferCounter+0) % shallowBox._simulationGrid->_rotatingBufferCount;
        unsigned prevFrameBuffer     = (bufferCounter+2) % shallowBox._simulationGrid->_rotatingBufferCount;     // (ie, -1 then +3)
        unsigned prevPrevFrameBuffer = (bufferCounter+1) % shallowBox._simulationGrid->_rotatingBufferCount;

        auto materialConstants = Internal::BuildOceanMaterialConstants(oceanSettings, gridPhysicalDimension);
        ConstantBuffer globalOceanMaterialConstantBuffer(&materialConstants, sizeof(materialConstants));

        char shaderDefines[256]; 
        BuildShaderDefines(shaderDefines, shallowBox._gridDimension, surfaceHeightsProvider, borderMode);

        if (prevFrameBuffer!=thisFrameBuffer) {
            context->BindCS(MakeResourceList(
                surfaceHeightsProvider->GetSRV(),
                shallowBox._simulationGrid->_waterHeightsSRV[prevFrameBuffer], 
                shallowBox._simulationGrid->_waterHeightsSRV[prevPrevFrameBuffer],
                shallowBox._lookupTableSRV));
        } else {
            context->BindCS(MakeResourceList(
                surfaceHeightsProvider->GetSRV(),
                RenderCore::Metal::ShaderResourceView(), RenderCore::Metal::ShaderResourceView(),
                shallowBox._lookupTableSRV));
        }

        if (globalOceanWorkingHeights) {
            context->BindCS(MakeResourceList(4, *globalOceanWorkingHeights));
        }

        context->BindCS(MakeResourceList(0, globalOceanMaterialConstantBuffer));

        ConstantBuffer surfaceHeightsConstantsBuffer(nullptr, sizeof(ShallowWaterSim::SurfaceHeightsAddressingConstants));
        ConstantBuffer basicConstantsBuffer(nullptr, sizeof(SimulatingConstants));
        context->BindCS(MakeResourceList(1, surfaceHeightsConstantsBuffer, basicConstantsBuffer, ConstantBuffer(compressionConstants, 4*sizeof(float))));

        if (!shallowBox._usePipeModel) {

            context->BindCS(MakeResourceList(
                shallowBox._simulationGrid->_waterHeightsUAV[thisFrameBuffer],
                shallowBox._simulationGrid->_waterVelocitiesUAV[0],
                shallowBox._simulationGrid->_waterVelocitiesUAV[1],
                shallowBox._simulationGrid->_waterVelocitiesUAV[2],
                shallowBox._simulationGrid->_waterVelocitiesUAV[3]));

            auto& cshaderH = Assets::GetAssetDep<ComputeShader>("game/xleres/Ocean/ShallowWaterSim.csh:RunSimulationH:cs_*", shaderDefines);
            auto& cshaderV = Assets::GetAssetDep<ComputeShader>("game/xleres/Ocean/ShallowWaterSim.csh:RunSimulationV:cs_*", shaderDefines);

            for (unsigned p=0; p<2; ++p) {
                    // flip forward and reverse iteration through "box._activeSimulationElements" every frame
                    //  (and every pass)
                for (auto i = shallowBox._activeSimulationElements.cbegin(); i!=shallowBox._activeSimulationElements.cend(); ++i) {
                    if (i->_arrayIndex < 128) {
                        SetSimulatingConstants(context, basicConstantsBuffer, *i, rainQuantityPerFrame);
                        SetAddressingConstants(context, surfaceHeightsConstantsBuffer, *i, surfaceHeightsProvider, gridPhysicalDimension);

                            // checkerboard pattern flip horizontal/vertical
                        int flip = (i->_gridX + i->_gridY + bufferCounter + p)&1;
                        context->Bind((flip)?cshaderH:cshaderV); context->Dispatch(1, shallowBox._gridDimension, 1);
                    }
                }
            }

                //  if we've requested normals, we should calculate them now. We can use the same method as
                //  the pipe model to calculate a rough approximation of the movement of water.
            if (Tweakable("OceanVelMethod", 1)==0) {
                auto& cshaderVel = Assets::GetAssetDep<ComputeShader>(
                    "game/xleres/Ocean/PipeModelShallowWaterSim.csh:UpdateVelocities:cs_*", shaderDefines);
                context->Bind(cshaderVel);
                DispatchEachElement(
                    context, shallowBox._activeSimulationElements, basicConstantsBuffer, surfaceHeightsConstantsBuffer, 
                    surfaceHeightsProvider, gridPhysicalDimension, rainQuantityPerFrame, shallowBox._gridDimension);
            } else {

                    //      Second method for calculating velocity
                    //      This method uses the slopes of the change in height to attempt to estimate water flow
                    
                context->BindCS(MakeResourceList(
                    5, shallowBox._simulationGrid->_slopesBufferUAV[0],
                    shallowBox._simulationGrid->_slopesBufferUAV[1]));

                context->Bind(Assets::GetAssetDep<ComputeShader>("game/xleres/Ocean/ShallowWaterSim.csh:UpdateVelocities0:cs_*", shaderDefines));
                DispatchEachElement(
                    context, shallowBox._activeSimulationElements, basicConstantsBuffer, surfaceHeightsConstantsBuffer, 
                    surfaceHeightsProvider, gridPhysicalDimension, rainQuantityPerFrame, shallowBox._gridDimension);


                context->Bind(Assets::GetAssetDep<ComputeShader>("game/xleres/Ocean/ShallowWaterSim.csh:UpdateVelocities1:cs_*", shaderDefines));
                DispatchEachElement(
                    context, shallowBox._activeSimulationElements, basicConstantsBuffer, surfaceHeightsConstantsBuffer, 
                    surfaceHeightsProvider, gridPhysicalDimension, rainQuantityPerFrame, shallowBox._gridDimension);
            }

        } else {

                // have to run all of the "update velocities" first and then update heights
            auto& cshader0 = Assets::GetAssetDep<ComputeShader>(
                "game/xleres/Ocean/PipeModelShallowWaterSim.csh:UpdateVelocities:cs_*", 
                (StringMeld<256>() << shaderDefines << ";WRITING_VELOCITIES=1").get());
            auto& cshader1 = Assets::GetAssetDep<ComputeShader>(
                "game/xleres/Ocean/PipeModelShallowWaterSim.csh:UpdateHeights:cs_*", shaderDefines);

                // order is important... We must start in the bottom right corner and work to the top left
            auto sortedElements = shallowBox._activeSimulationElements;
            std::sort(sortedElements.begin(), sortedElements.end(),
                [](const ShallowWaterSim::ActiveElement& lhs, const ShallowWaterSim::ActiveElement& rhs) -> bool
                {
                    if (lhs._gridY == rhs._gridY) return lhs._gridX > rhs._gridX;
                    return lhs._gridY > rhs._gridY;
                });

            for (unsigned pass=0; pass<2; ++pass) {
                context->Bind((pass==0)?cshader0:cshader1);

                    // limit of 8 UAV slots means that we can't bind 8 velocity UAVs 
                    // and a heights UAV at the same time
                if (pass == 0) {
                    context->BindCS(MakeResourceList(
                        shallowBox._simulationGrid->_waterVelocitiesUAV[0],
                        shallowBox._simulationGrid->_waterVelocitiesUAV[1],
                        shallowBox._simulationGrid->_waterVelocitiesUAV[2],
                        shallowBox._simulationGrid->_waterVelocitiesUAV[3],
                        shallowBox._simulationGrid->_waterVelocitiesUAV[4],
                        shallowBox._simulationGrid->_waterVelocitiesUAV[5],
                        shallowBox._simulationGrid->_waterVelocitiesUAV[6],
                        shallowBox._simulationGrid->_waterVelocitiesUAV[7]));
                    context->BindCS(MakeResourceList(
                        5, shallowBox._simulationGrid->_waterHeightsSRV[thisFrameBuffer]));
                } else {
                    context->BindCS(MakeResourceList(
                        5,
                        shallowBox._simulationGrid->_waterVelocitiesSRV[0],
                        shallowBox._simulationGrid->_waterVelocitiesSRV[1],
                        shallowBox._simulationGrid->_waterVelocitiesSRV[2],
                        shallowBox._simulationGrid->_waterVelocitiesSRV[3],
                        shallowBox._simulationGrid->_waterVelocitiesSRV[4],
                        shallowBox._simulationGrid->_waterVelocitiesSRV[5],
                        shallowBox._simulationGrid->_waterVelocitiesSRV[6],
                        shallowBox._simulationGrid->_waterVelocitiesSRV[7]));
                    context->BindCS(MakeResourceList(
                        shallowBox._simulationGrid->_waterHeightsUAV[thisFrameBuffer]));
                }

                DispatchEachElement(
                    context, sortedElements, basicConstantsBuffer, surfaceHeightsConstantsBuffer, 
                    surfaceHeightsProvider, gridPhysicalDimension, rainQuantityPerFrame, shallowBox._gridDimension);

                context->UnbindCS<RenderCore::Metal::UnorderedAccessView>(0, 8);
            }

        }

        context->UnbindCS<UnorderedAccessView>(0, 5);
        context->UnbindVS<ShaderResourceView>(0, 5);
    }

        ////////////////////////////////

    void ShallowWater_NewElements(
        RenderCore::Metal::DeviceContext* context, 
        ShallowWaterSim& shallowBox, ISurfaceHeightsProvider& surfaceHeightsProvider,
        const OceanSettings& oceanSettings, const float gridPhysicalDimension,
        RenderCore::Metal::ShaderResourceView* globalOceanWorkingHeights,
        ShallowBorderMode::Enum borderMode,
        const ShallowWaterSim::ActiveElement* newElementsBegin, const ShallowWaterSim::ActiveElement* newElementsEnd,
        size_t stride)
    {
        const bool usePipeModel = shallowBox._usePipeModel;
        std::vector<ShallowWaterSim::ActiveElement> newElements;

        if (!shallowBox._usePipeModel) {
            context->BindCS(MakeResourceList(
                shallowBox._simulationGrid->_waterHeightsUAV[0],
                shallowBox._simulationGrid->_waterHeightsUAV[1], 
                shallowBox._simulationGrid->_waterHeightsUAV[2],
                shallowBox._lookupTableUAV));
        } else {
            context->BindCS(MakeResourceList(shallowBox._simulationGrid->_waterHeightsUAV[0]));
            context->BindCS(MakeResourceList(3,
                shallowBox._lookupTableUAV,
                shallowBox._simulationGrid->_waterVelocitiesUAV[0],
                shallowBox._simulationGrid->_waterVelocitiesUAV[1],
                shallowBox._simulationGrid->_waterVelocitiesUAV[2],
                shallowBox._simulationGrid->_waterVelocitiesUAV[3]));
        }

        if (globalOceanWorkingHeights) {
            context->BindCS(MakeResourceList(4, *globalOceanWorkingHeights));
        }

        char shaderDefines[256]; 
        BuildShaderDefines(shaderDefines, shallowBox._gridDimension, &surfaceHeightsProvider, borderMode);

        auto& cshader = Assets::GetAssetDep<ComputeShader>(
            usePipeModel?"game/xleres/Ocean/InitSimGrid.csh:InitPipeModel:cs_*":"game/xleres/Ocean/InitSimGrid.csh:main:cs_*", shaderDefines);

        auto materialConstants = Internal::BuildOceanMaterialConstants(oceanSettings, gridPhysicalDimension);
        ConstantBuffer globalOceanMaterialConstantBuffer(&materialConstants, sizeof(materialConstants));
        context->BindCS(MakeResourceList(globalOceanMaterialConstantBuffer));
        context->Bind(cshader);

        context->BindCS(MakeResourceList(surfaceHeightsProvider.GetSRV()));
            
        for (auto i = newElementsBegin; i!=newElementsEnd; i=PtrAdd(i, stride)) {
            if (i->_arrayIndex == ~unsigned(0x0)) {

                assert(!shallowBox._poolOfUnallocatedArrayIndices.empty());        // there should always been at least one unallocated array index

                    //  Check the surface heights provider, to get the surface heights
                    //  if this fails, we can't render this grid element
                    //      todo -- we need to not only "get", but "lock" this data, so it's not swapped out
                Float2 gridMins(float(i->_gridX) * gridPhysicalDimension, float(i->_gridY) * gridPhysicalDimension);
                Float2 gridMaxs(float(i->_gridX+1) * gridPhysicalDimension, float(i->_gridY+1) * gridPhysicalDimension);
                auto surfaceAddressing = surfaceHeightsProvider.GetAddress(gridMins, gridMaxs);
                if (!surfaceAddressing._valid)
                    continue;       // we can't render it just now ... (maybe after the surface heights load in)

                    //  assign one of the free grids (or destroy the least recently used one)
                    //  call a compute shader to fill out the simulation grids with the new values
                    //  (this will also set the simulation grid value into the lookup table)
                unsigned assignmentIndex = *(shallowBox._poolOfUnallocatedArrayIndices.cend()-1);
                shallowBox._poolOfUnallocatedArrayIndices.erase(shallowBox._poolOfUnallocatedArrayIndices.cend()-1);

                ShallowWaterSim::ActiveElement newElement(i->_gridX, i->_gridY, assignmentIndex);

                struct InitCellConstants
                {
                    Int2 _lookableTableCoords;
                    unsigned _simulatingGridIndex;
                    unsigned _dummy;
                } initCellConstants = { Int2(i->_gridX, i->_gridY), assignmentIndex, 0 };

                newElement._heightsAddressing._baseCoord = surfaceAddressing._baseCoordinate;
                newElement._heightsAddressing._textureMin = surfaceAddressing._minCoordOffset;
                newElement._heightsAddressing._textureMax = surfaceAddressing._maxCoordOffset;
                newElement._heightsAddressing._scale = surfaceAddressing._heightScale;
                newElement._heightsAddressing._offset = surfaceAddressing._heightOffset;
                ConstantBuffer initCellConstantsBuffer(&initCellConstants, sizeof(initCellConstants));
                ConstantBuffer surfaceHeightsAddressingBuffer(&newElement._heightsAddressing, sizeof(newElement._heightsAddressing));
                context->BindCS(MakeResourceList(1, surfaceHeightsAddressingBuffer, initCellConstantsBuffer));
                context->Dispatch(1, shallowBox._gridDimension, 1);
                    
                auto insertPoint = std::lower_bound(newElements.begin(), newElements.end(), newElement, SortOceanGridElement);
                newElements.insert(insertPoint, newElement);

            } else {

                auto insertPoint = std::lower_bound(newElements.begin(), newElements.end(), *i, SortOceanGridElement);
                newElements.insert(insertPoint, *i);

            }
        }

        shallowBox._activeSimulationElements = std::move(newElements);
    }

        ////////////////////////////////

    void ShallowWater_DoSim(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, 
        const OceanSettings& oceanSettings, float gridPhysicalDimension,
        RenderCore::Metal::ShaderResourceView* globalOceanWorkingHeights,
        ISurfaceHeightsProvider* surfaceHeightsProvider, ShallowWaterSim& shallowBox, unsigned bufferCounter)
    {
            // run a simulation of shallow water (for some interesting wave dynamics near the shore...)
        auto& oceanReset = Tweakable("OceanReset", false);
        const float rainQuantity = Tweakable("OceanRainQuantity", 0.f);
        const auto shallowWaterBorderMode = (ShallowBorderMode::Enum)Tweakable("OceanShallowBorder", 1);
        const float baseHeight = oceanSettings._baseHeight;

        auto materialConstants = Internal::BuildOceanMaterialConstants(oceanSettings, gridPhysicalDimension);
        ConstantBuffer globalOceanMaterialConstantBuffer(&materialConstants, sizeof(materialConstants));

            // unbind resources that were bound in ShallowWater_BindForOceanRender
        context->UnbindVS<ShaderResourceView>( 3, 2);
        context->UnbindPS<ShaderResourceView>( 5, 1);
        context->UnbindPS<ShaderResourceView>(11, 1);
        context->UnbindPS<ShaderResourceView>(15, 1);

        unsigned thisFrameBuffer = (bufferCounter+0)%shallowBox._simulationGrid->_rotatingBufferCount;

            //  Calculate what grids we need for the current frame. If those grids aren't currently
            //  simulated, we need to set up the simulation (calculate depths and initialise the 
            //  water height
        std::vector<PrioritisedActiveElement> scheduledGrids;
        std::vector<PrioritisedActiveElement> gridsToPrioritise;
    
        auto cameraPosition = ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld);
        signed baseGridX = signed(cameraPosition[0] / gridPhysicalDimension);
        signed baseGridY = signed(cameraPosition[1] / gridPhysicalDimension);
        for (signed y=0; y<5; ++y) {
            for (signed x=0; x<5; ++x) {
                signed testGridX = baseGridX + x - 2;
                signed testGridY = baseGridY + y - 2;
                auto visibility = GridIsVisible(parserContext, testGridX, testGridY, gridPhysicalDimension, baseHeight);
                if (visibility) {
                        // calculate priority
                    Float2 gridCentrePosition = Float2(float(testGridX) + 0.5f, float(testGridY) + 0.5f) * gridPhysicalDimension;
                    float gridDistance = Magnitude(gridCentrePosition - Float2(cameraPosition[0], cameraPosition[1]));
                    float priority = gridDistance;
                    // if (visibility != CULL_INCLUSION) {
                    //     priority += 512.f;   // give priority penalty for grids on the edge of the screen (but maybe exclude the grid the camera is immediately over?)
                    // }

                    scheduledGrids.push_back(
                        PrioritisedActiveElement(testGridX, testGridY, priority));
                }
            }
        }

        std::sort(scheduledGrids.begin(), scheduledGrids.end(), SortByGridIndex);
        auto i2 = shallowBox._activeSimulationElements.begin();
        for (auto i = scheduledGrids.cbegin(); i != scheduledGrids.cend(); ++i) {

                //  since "scheduledGrids" is sorted, we can truncate the search through
                //  box._activeSimulationElements every time
            auto t = std::equal_range(i2, shallowBox._activeSimulationElements.end(), i->_e, SortOceanGridElement);
            bool foundEqual = false;
            for (auto t2 = t.first; t2 != t.second; ++t2) {
                if (t2->_gridX == i->_e._gridX && t2->_gridY == i->_e._gridY) {
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
        for (auto i=shallowBox._activeSimulationElements.cbegin(); i!=shallowBox._activeSimulationElements.cend(); ++i) {
            Float2 gridCentrePosition(
                (float(i->_gridX) + 0.5f) * gridPhysicalDimension, 
                (float(i->_gridY) + 0.5f) * gridPhysicalDimension);
            float gridDistance = Magnitude(gridCentrePosition - Float2(cameraPosition[0], cameraPosition[1]));
                //  Prioritize existing grids while ignoring camera facing. The way, the simulation won't
                //  be stopped as soon as it goes off screen... Perhaps we can stop if the grid stays off the
                //  screen for a number of frames.
            float priority = gridDistance;  
            gridsToPrioritise.push_back(PrioritisedActiveElement(*i, priority));
        }

        std::vector<Int4> gridsDestroyedThisFrame;
        bool hasNewGrids = false;
        std::sort(gridsToPrioritise.begin(), gridsToPrioritise.end(), SortByPriority);
        if (gridsToPrioritise.size() > shallowBox._simulatingGridsCount) {
                // cancel some grids, and return their ids to the pool
            for (auto i=gridsToPrioritise.begin() + shallowBox._simulatingGridsCount; i!=gridsToPrioritise.end(); ++i) {
                if (i->_e._arrayIndex!=~unsigned(0x0)) {
                    shallowBox._poolOfUnallocatedArrayIndices.push_back(i->_e._arrayIndex);
                    gridsDestroyedThisFrame.push_back(Int4(i->_e._gridX, i->_e._gridY, 0, 0));
                    hasNewGrids = true;
                }
            }
            gridsToPrioritise.erase(gridsToPrioritise.begin() + shallowBox._simulatingGridsCount, gridsToPrioritise.end());
        }
        hasNewGrids |= gridsToPrioritise.size() > shallowBox._activeSimulationElements.size();

            // Setup any new grids that have been priortised into the list...

            // todo --  should we have a tree of these simulation grids... Some distance grids could be
            //          at lower resolution. Only closer grids would be at maximum resolution...?

        if (hasNewGrids) {
            ShallowWater_NewElements(
                context, shallowBox, *surfaceHeightsProvider, 
                oceanSettings, gridPhysicalDimension, globalOceanWorkingHeights,
                shallowWaterBorderMode,
                &gridsToPrioritise.cbegin()->_e, &AsPointer(gridsToPrioritise.cend())->_e,
                sizeof(PrioritisedActiveElement));
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

            context->Bind(Assets::GetAssetDep<ComputeShader>("game/xleres/Ocean/InitSimGrid.csh:ClearGrids:cs_*"));
            context->BindCS(MakeResourceList(0, ConstantBuffer(&clearGridsConstants, sizeof(clearGridsConstants))));
            context->Dispatch(unsigned(gridsDestroyedThisFrame.size()));

        }

        context->UnbindCS<UnorderedAccessView>(1, 8);

            //  For each actively simulated grid, run the compute shader to calculate the heights 
            //  for the new frame. We simulate horizontally and vertically separate. Between frames we
            //  alternate the order.

        if (!shallowBox._activeSimulationElements.empty()) {

            auto cursorPos = GetCursorPos();
            ViewportDesc vpd(*context);
            float compressionConstants[4] = { 0.f, 0.f, 1000.f, 1.f };

            static unsigned framesMouseDown = 0;
            if (GetKeyState(VK_MBUTTON)<0) {
                    //  Find the mouse over ray, and find the intersection
                    //  point with the ocean water plane
                auto mouseOverRay = CalculateMouseOverRay(context, parserContext);
                float a = mouseOverRay.first[2] - baseHeight;
                float b = mouseOverRay.second[2] - mouseOverRay.first[2];
                float alpha = -a / b;
                Float3 intersectionPoint = LinearInterpolate(mouseOverRay.first, mouseOverRay.second, alpha);
            
                compressionConstants[0] = intersectionPoint[0];
                compressionConstants[1] = intersectionPoint[1];
                compressionConstants[2] = baseHeight-.2f;
                compressionConstants[3] = 6.f;

                ++framesMouseDown;
            } else {
                framesMouseDown = 0;
            }

            ShallowWater_ExecuteInternalSimulation(
                context, oceanSettings, gridPhysicalDimension,
                globalOceanWorkingHeights, surfaceHeightsProvider, 
                shallowBox, bufferCounter, compressionConstants,
                rainQuantity, shallowWaterBorderMode);

        }

        char shaderDefines[256];
        BuildShaderDefines(shaderDefines, shallowBox._gridDimension);

                //  Generate normals using the displacement textures
                //  Note, this will generate the normals for every array slice, even
                //  for slices that aren't actually used.
        if (!shallowBox._simulationGrid->_normalsTextureUAV.empty()) {

            auto& buildNormals = Assets::GetAssetDep<ComputeShader>("game/xleres/Ocean/OceanNormalsShallow.csh:BuildDerivatives:cs_*", shaderDefines);
            auto& buildNormalsMipmaps = Assets::GetAssetDep<ComputeShader>("game/xleres/Ocean/OceanNormalsShallow.csh:BuildDerivativesMipmap:cs_*", shaderDefines);

                // build devs shader needs to know the gridXY for all simulated elements (so it can find the adjacent grids)
            signed buildDevsConstants[4*128];
            XlSetMemory(buildDevsConstants, 0, sizeof(buildDevsConstants));
            for (auto i = shallowBox._activeSimulationElements.cbegin(); i!=shallowBox._activeSimulationElements.cend(); ++i) {
                if (i->_arrayIndex < dimof(buildDevsConstants)/4) {
                    buildDevsConstants[i->_arrayIndex*4+0] = i->_gridX;
                    buildDevsConstants[i->_arrayIndex*4+1] = i->_gridY;
                }
            }

            context->BindCS(MakeResourceList(globalOceanMaterialConstantBuffer));
            context->BindCS(MakeResourceList(2, ConstantBuffer(buildDevsConstants, sizeof(buildDevsConstants))));

            context->BindCS(MakeResourceList(   shallowBox._simulationGrid->_waterHeightsSRV[thisFrameBuffer],
                                                shallowBox._lookupTableSRV,
                                                shallowBox._simulationGrid->_foamQuantitySRV2[(bufferCounter+1)&1]));
            context->BindCS(MakeResourceList(   shallowBox._simulationGrid->_normalsTextureUAV[0], 
                                                shallowBox._simulationGrid->_foamQuantityUAV[bufferCounter&1]));
            context->Bind(buildNormals); 
            context->Dispatch(1, 1, shallowBox._simulatingGridsCount);
            context->UnbindCS<UnorderedAccessView>(0, 2);

                // do we really need mipmaps for shallow water grids?
            context->Bind(buildNormalsMipmaps);
            for (unsigned step = 0; step<shallowBox._simulationGrid->_normalsTextureUAV.size()-1; ++step) {
                unsigned mipDims = shallowBox._gridDimension >> (step+1);
                unsigned constants[4] = { mipDims, mipDims, 0, 0 };
                context->BindCS(MakeResourceList(ConstantBuffer(constants, sizeof(constants))));

                context->BindCS(MakeResourceList(4, shallowBox._simulationGrid->_normalsSingleMipSRV[step]));
                context->BindCS(MakeResourceList(shallowBox._simulationGrid->_normalsTextureUAV[step+1]));
            
                context->Dispatch((mipDims + (8-1))/8, (mipDims + (8-1))/8, shallowBox._simulatingGridsCount);
                context->UnbindCS<UnorderedAccessView>(0, 1);
            }
        }

            // Draw some debugging information displaying the current heights of the liquid
    //    {
    //        SetupVertexGeneratorShader(context);
    //        context->Bind(Assets::GetAssetDep<Metal::ShaderProgram>(
    //            "game/xleres/basic2D.vsh:fullscreen:vs_*", 
    //            "game/xleres/Ocean/FFTDebugging.psh:ShallowWaterDebugging:ps_*"));
    //        context->BindPS(MakeResourceList(   4, box._simulationGrid->_waterHeightsSRV[thisFrameBuffer],
    //                                            box._simulationGrid->_surfaceHeightsSRV));
    //        context->Draw(4);
    //    }

        if (Tweakable("OceanShallowDrawWireframe", false)) {
            ShallowWater_RenderWireframe(context, parserContext, oceanSettings, gridPhysicalDimension, Float2(0.f, 0.f), shallowBox, bufferCounter, shallowWaterBorderMode);
        }

        if (oceanReset) {
            shallowBox._activeSimulationElements.clear();
            shallowBox._poolOfUnallocatedArrayIndices.clear();
            shallowBox._poolOfUnallocatedArrayIndices.reserve(shallowBox._simulatingGridsCount);
            for (unsigned c=0; c<shallowBox._simulatingGridsCount; ++c) {
                shallowBox._poolOfUnallocatedArrayIndices.push_back(c);
            }

            unsigned clearInts[] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
            context->Clear(shallowBox._lookupTableUAV, clearInts);
        }
    }

    void ShallowWater_RenderWireframe(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, 
        const OceanSettings& oceanSettings, float gridPhysicalDimension, Float2 offset,
        ShallowWaterSim& shallowBox, unsigned bufferCounter, ShallowBorderMode::Enum borderMode)
    {
        auto materialConstants = Internal::BuildOceanMaterialConstants(oceanSettings, gridPhysicalDimension);
        ConstantBuffer globalOceanMaterialConstantBuffer(&materialConstants, sizeof(materialConstants));

        char shaderDefines[256]; 
        BuildShaderDefines(shaderDefines, shallowBox._gridDimension, nullptr, borderMode);

        unsigned thisFrameBuffer = (bufferCounter+0) % shallowBox._simulationGrid->_rotatingBufferCount;
        auto& patchRender = Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/Ocean/OceanPatch.vsh:ShallowWater:vs_*",
            "game/xleres/solidwireframe.gsh:main:gs_*",
            "game/xleres/solidwireframe.psh:main:ps_*",
            shaderDefines);
        BoundUniforms boundUniforms(patchRender);
        Techniques::TechniqueContext::BindGlobalUniforms(boundUniforms);
        boundUniforms.BindConstantBuffer(Hash64("OceanMaterialSettings"), 0, 1);
        boundUniforms.BindConstantBuffer(Hash64("ShallowWaterUpdateConstants"), 1, 1);

        context->Bind(patchRender);

        SetupVertexGeneratorShader(context);
        context->BindVS(MakeResourceList(3, shallowBox._simulationGrid->_waterHeightsSRV[thisFrameBuffer]));
        context->Bind(Topology::TriangleList);
        context->Bind(Techniques::CommonResources()._dssReadWrite);

        auto& simplePatchBox = Techniques::FindCachedBox<SimplePatchBox>(SimplePatchBox::Desc(shallowBox._gridDimension, shallowBox._gridDimension, true));
        context->Bind(simplePatchBox._simplePatchIndexBuffer, NativeFormat::R32_UINT);

        ConstantBuffer simulatingCB(nullptr, sizeof(SimulatingConstants));

        const ConstantBuffer* prebuiltBuffers[] = { &globalOceanMaterialConstantBuffer, &simulatingCB };
        boundUniforms.Apply(
            *context, 
            parserContext.GetGlobalUniformsStream(),
            UniformsStream(nullptr, prebuiltBuffers, dimof(prebuiltBuffers)));

        for (auto i=shallowBox._activeSimulationElements.cbegin(); i!=shallowBox._activeSimulationElements.cend(); ++i) {
            if (i->_arrayIndex < 128) {
                SetSimulatingConstants(context, simulatingCB, *i, 0.f, offset);
                context->DrawIndexed(simplePatchBox._simplePatchIndexCount);
            }
        }

        context->UnbindVS<ShaderResourceView>(3, 1);
    }

    void ShallowWater_RenderVelocities(
        RenderCore::Metal::DeviceContext* context, LightingParserContext& parserContext, 
        const OceanSettings& oceanSettings, float gridPhysicalDimension, Float2 offset,
        ShallowWaterSim& shallowBox, unsigned bufferCounter, ShallowBorderMode::Enum borderMode)
    {
        auto materialConstants = Internal::BuildOceanMaterialConstants(oceanSettings, gridPhysicalDimension);
        ConstantBuffer globalOceanMaterialConstantBuffer(&materialConstants, sizeof(materialConstants));

        char shaderDefines[256]; 
        BuildShaderDefines(shaderDefines, shallowBox._gridDimension, nullptr, borderMode);

        unsigned thisFrameBuffer = (bufferCounter+0) % shallowBox._simulationGrid->_rotatingBufferCount;
        auto& patchRender = Assets::GetAssetDep<Metal::ShaderProgram>(
            "game/xleres/Ocean/OceanVelocitiesDebugging.sh:vs_main:vs_*",
            "game/xleres/Ocean/OceanVelocitiesDebugging.sh:ps_main:ps_*",
            shaderDefines);

        BoundUniforms boundUniforms(patchRender);
        Techniques::TechniqueContext::BindGlobalUniforms(boundUniforms);
        boundUniforms.BindConstantBuffer(Hash64("OceanMaterialSettings"), 0, 1);
        boundUniforms.BindConstantBuffer(Hash64("ShallowWaterUpdateConstants"), 1, 1);

        context->Bind(patchRender);

        SetupVertexGeneratorShader(context);
        context->BindVS(MakeResourceList(3, 
            shallowBox._simulationGrid->_waterHeightsSRV[thisFrameBuffer]));

        context->BindPS(MakeResourceList(4, 
            shallowBox._lookupTableSRV,
            shallowBox._simulationGrid->_waterVelocitiesSRV[0],
            shallowBox._simulationGrid->_waterVelocitiesSRV[1],
            shallowBox._simulationGrid->_waterVelocitiesSRV[2],
            shallowBox._simulationGrid->_waterVelocitiesSRV[3],
            shallowBox._simulationGrid->_waterVelocitiesSRV[4],
            shallowBox._simulationGrid->_waterVelocitiesSRV[5],
            shallowBox._simulationGrid->_waterVelocitiesSRV[6],
            shallowBox._simulationGrid->_waterVelocitiesSRV[7]));
        context->Bind(Topology::TriangleList);
        context->Bind(Techniques::CommonResources()._dssReadWrite);

        auto& simplePatchBox = Techniques::FindCachedBox<SimplePatchBox>(SimplePatchBox::Desc(shallowBox._gridDimension, shallowBox._gridDimension, true));
        context->Bind(simplePatchBox._simplePatchIndexBuffer, NativeFormat::R32_UINT);

        ConstantBuffer simulatingCB(nullptr, sizeof(SimulatingConstants));
        const ConstantBuffer* prebuiltBuffers[] = { &globalOceanMaterialConstantBuffer, &simulatingCB };
        boundUniforms.Apply(
            *context, 
            parserContext.GetGlobalUniformsStream(),
            UniformsStream(nullptr, prebuiltBuffers, dimof(prebuiltBuffers)));

        for (auto i=shallowBox._activeSimulationElements.cbegin(); i!=shallowBox._activeSimulationElements.cend(); ++i) {
            if (i->_arrayIndex < 128) {
                SetSimulatingConstants(context, simulatingCB, *i, 0.f, offset);
                context->DrawIndexed(simplePatchBox._simplePatchIndexCount);
            }
        }

        context->UnbindVS<ShaderResourceView>(3, 1);
    }

    void ShallowWater_BindForOceanRender(RenderCore::Metal::DeviceContext* context, ShallowWaterSim& shallowWater, unsigned bufferCounter)
    {
        unsigned thisFrameBuffer = (bufferCounter+0)%shallowWater._simulationGrid->_rotatingBufferCount;

        context->BindVS(MakeResourceList(3, 
            shallowWater._simulationGrid->_waterHeightsSRV[thisFrameBuffer],
            shallowWater._lookupTableSRV));
        context->BindPS(MakeResourceList(5, shallowWater._simulationGrid->_normalsTextureShaderResource));
        context->BindPS(MakeResourceList(11, shallowWater._simulationGrid->_foamQuantitySRV[bufferCounter&1]));
        context->BindPS(MakeResourceList(15, shallowWater._lookupTableSRV));
    }

    void ShallowWater_BindForErosionSimulation(RenderCore::Metal::DeviceContext* context, ShallowWaterSim& shallowWater, unsigned bufferCounter)
    {
        unsigned thisFrameBuffer = (bufferCounter+0)%shallowWater._simulationGrid->_rotatingBufferCount;
        context->BindCS(MakeResourceList(3, shallowWater._simulationGrid->_waterHeightsSRV[thisFrameBuffer], shallowWater._lookupTableSRV));
        context->BindCS(MakeResourceList(5, shallowWater._simulationGrid->_waterVelocitiesSRV[0], shallowWater._simulationGrid->_waterVelocitiesSRV[1], shallowWater._simulationGrid->_waterVelocitiesSRV[2], shallowWater._simulationGrid->_waterVelocitiesSRV[3]));
    }
}


