// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShallowSurface.h"
#include "ShallowWater.h"
#include "DeepOceanSim.h"
#include "RefractionsBuffer.h"
#include "SceneEngineUtils.h"
#include "../SceneEngine/SceneParser.h"
#include "../SceneEngine/Sky.h"
#include "../SceneEngine/LightDesc.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Techniques/TechniqueMaterial.h"
#include "../RenderCore/Techniques/CommonBindings.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/PredefinedCBLayout.h"
#include "../RenderCore/Techniques/ResourceBox.h"
#include "../ConsoleRig/Console.h"
#include "../Assets/Assets.h"
#include "../Math/Matrix.h"
#include "../Math/Transformations.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/MemoryUtils.h"
#include "../Utility/Meta/ClassAccessorsImpl.h"

#include "../RenderCore/DX11/Metal/DX11Utils.h"

#include "Ocean.h"  // for WaterNoiseTexture

namespace SceneEngine
{
    using namespace RenderCore;

///////////////////////////////////////////////////////////////////////////////////////////////////

    class RasterizationSurface
    {
    public:
        void Draw(Int2 a, Int2 b, Int2 c);
        bool Get(unsigned x, unsigned y) const 
        { 
            if (x>=_width || y >= _height) return false;
            return _mask[y*_width+x]; 
        }

        RasterizationSurface(unsigned width, unsigned height);
        ~RasterizationSurface();
    protected:
        std::unique_ptr<bool[]> _mask;
        unsigned _width, _height;

        void RasterizeLineBetweenEdges(	
            Int2 e00, Int2 e01, Int2 e10, Int2 e11);
    };

    void RasterizationSurface::Draw(Int2 a, Int2 b, Int2 c)
    {
            //	Sort min y to max y (minimal bubble sort)
	    if (a[1] > b[1]) {
		    std::swap(a, b);
	    }
	    if (b[1] > c[1]) {
		    std::swap(b, c);
		    if (a[1] > b[1]) {
			    std::swap(a, b);
		    }
	    }

        RasterizeLineBetweenEdges(a, b, a, c);
	    RasterizeLineBetweenEdges(b, c, a, c);
    }

    void RasterizationSurface::RasterizeLineBetweenEdges(
        Int2 e00, Int2 e01, Int2 e10, Int2 e11)
    {
	    auto y = e00[1];
	    for (;y<e01[1]; ++y) {
		    float e0a = (y - e00[1]) / float(e01[1] - e00[1]);
		    float e2a = (y - e10[1]) / float(e11[1] - e10[1]);

		    float spanx0 = LinearInterpolate(float(e00[0]), float(e01[0]), e0a);
		    float spanx1 = LinearInterpolate(float(e10[0]), float(e11[0]), e2a);
		    // float spanz0 = LinearInterpolate(float(e00[2]), float(e01[2]), e0a);
		    // float spanz1 = LinearInterpolate(float(e10[2]), float(e11[2]), e2a);

            if (spanx0 > spanx1)
                std::swap(spanx0, spanx1);
			Int2 start((int)XlFloor(spanx0), y);
			Int2 end((int)XlCeil(spanx1), y);

            assert(start[0] <= end[0] && start[0] >= 0 && end[0] < (int)_width);
            assert(y >= 0 && y < (int)_height);

            auto* write = &_mask[y*_width+start[0]];
            for (auto x=start[0]; x<=end[0]; ++x, ++write)
                *write = true;
	    }
    }

    RasterizationSurface::RasterizationSurface(unsigned width, unsigned height)
    {
        _mask = std::make_unique<bool[]>(width * height);
        XlSetMemory(_mask.get(), 0, width * height * sizeof(bool));
        _width = width;
        _height = height;
    }

    RasterizationSurface::~RasterizationSurface() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ShallowSurface::Pimpl
    {
    public:
        class SimGrid
        {
        public:
            Metal::IndexBuffer _ib;
            Float4x4 _gridToWorld;
            unsigned _indexCount;
            Int2 _gridCoord;
        };

        Metal::IndexBuffer _defaultIB;
        Metal::IndexBuffer _unsimDefaultIB;
        std::vector<SimGrid> _simGrids;
        std::vector<Int2> _validGridList;
        std::unique_ptr<ShallowWaterSim> _sim;
        Float2 _simulationMins;
        unsigned _bufferCounter;
        Config _cfg;
        LightingConfig _lightingCfg;
    };

    ShallowSurface::ShallowSurface(
        const Float2 triangleList[], size_t stride,
        size_t ptCount,
        const Config& settings,
        const LightingConfig& lightingSettings)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_cfg = settings;
        _pimpl->_lightingCfg = lightingSettings;
        _pimpl->_bufferCounter = 0;

        const auto maxSimulationGrids = settings._simGridCount;
        const bool usePipeModel = settings._usePipeModel;
        _pimpl->_sim = std::make_unique<ShallowWaterSim>(
            ShallowWaterSim::Desc(
                settings._simGridDims, maxSimulationGrids, usePipeModel, false, false, false));

            //
            //      Given the input points, we want to create a set of sim grids
            //      that roughly match that shape. The vertices of the sim grids
            //      will be driven by the shallow water simulation. So, in this case,
            //      we're going to keep a 1:1 ratio between vertex and simulated
            //      sample point.
            //
            //      We need to find the maximum extents of the input geometry.
            //      We will create a large array for those extentds, with cells
            //      defined by the simulation cell size.
            //      Then we will rasterize the input triangles into that. This will
            //      give us on/off information for each cell. We then split that
            //      area up into separate simgrids. And from each simgrid, we create
            //      a index buffer. This index buffer will define base water geometry
            //      in the resolution of the sim grid.
            //
            //      Note that because we're only using vertices that fall exactly on the
            //      simgrid, the final geometry will be slightly bigger than the input
            //      geometry, and will not have smooth edges on diagonals. If we needed
            //      to match the input geometry exactly, we will need to do proper clipping,
            //      and generate new vertex positions for the intersection of the input
            //      triangles with the simulation grid.
            //
            //      We're also going to assume that the input geometry is completely flat
            //      and lying on the XY plane. We could support sloped water (ie, a river
            //      on a slight decline) -- but the simulation would look wierd if the 
            //      surface wasn't reasonably flat)
            //

        Float2 mins(FLT_MAX, FLT_MAX);
        Float2 maxs(-FLT_MAX, -FLT_MAX);
        for (size_t c=0; c<ptCount; ++c) {
            auto pt = *PtrAdd(triangleList, stride*c);
            mins[0] = std::min(pt[0], mins[0]);
            mins[1] = std::min(pt[1], mins[1]);
            maxs[0] = std::max(pt[0], maxs[0]);
            maxs[1] = std::max(pt[1], maxs[1]);
        }   
        
            //   todo -- we have to align the sim grids to the terrain surface heights
            //      provider! We want to only require the terrain height values for
            //      a single node to simulate a given water cell.
            //
            //  if _gridPhysicalSize divides evenly into the terrain provider nodes,
            //  then we have to align the "mins" with the node edge (but max can be aligned
            //  to a terrain element)

        float cellPhySize = settings._gridPhysicalSize / float(settings._simGridDims);
        mins[0] = XlFloor(mins[0] / settings._gridPhysicalSize) * settings._gridPhysicalSize;
        mins[1] = XlFloor(mins[1] / settings._gridPhysicalSize) * settings._gridPhysicalSize;
        maxs[0] =  XlCeil(maxs[0] / cellPhySize) * cellPhySize;
        maxs[1] =  XlCeil(maxs[1] / cellPhySize) * cellPhySize;

        _pimpl->_simulationMins = mins;

        UInt2 surfaceSize;
        surfaceSize[0] = unsigned((maxs[0] - mins[0]) / cellPhySize)+1;
        surfaceSize[1] = unsigned((maxs[1] - mins[1]) / cellPhySize)+1;

        RasterizationSurface mask(surfaceSize[0], surfaceSize[1]);

            //      Rasterize each triangle into the surface, setting cells to
            //      active for any that we encounter. Note that it's ok if we
            //      get overlaps during rasterization for adjacent triangles!

        for (size_t c=0; (c+3)<=ptCount; c+=3) {
            auto p0 = *PtrAdd(triangleList, stride*(c+0));
            auto p1 = *PtrAdd(triangleList, stride*(c+1));
            auto p2 = *PtrAdd(triangleList, stride*(c+2));

            auto a0 = (p0 - mins) / cellPhySize;
            auto a1 = (p1 - mins) / cellPhySize;
            auto a2 = (p2 - mins) / cellPhySize;

            mask.Draw(
                Int2((int)a0[0], (int)a0[1]), 
                Int2((int)a1[0], (int)a1[1]), 
                Int2((int)a2[0], (int)a2[1]));
        }
            
            //      Our mask should have active/inactive cells. We should go through and 
            //      create simgrids as needed, and build the index buffer for each.

        BuildDefaultIB();

        unsigned simGridsX = (surfaceSize[0] + settings._simGridDims + 1) / settings._simGridDims;
        unsigned simGridsY = (surfaceSize[1] + settings._simGridDims + 1) / settings._simGridDims;
        for (unsigned y=0; y<simGridsY; ++y)
            for (unsigned x=0; x<simGridsX; ++x)
                MaybeCreateGrid(mask, Int2(x, y));
    }

    ShallowSurface::~ShallowSurface()
    {}

    void ShallowSurface::BuildDefaultIB()
    {
        const auto& settings = _pimpl->_cfg;
        Int2 mins(0, 0);
        Int2 maxs(settings._simGridDims, settings._simGridDims);

        std::vector<uint16> ibData;
        ibData.reserve((maxs[0] - mins[0]) * (maxs[1] - mins[1]) * 6);

        auto vbWidth = maxs[0] - mins[0] + 1;

        for (int y=mins[1]; y<maxs[1]; ++y) {
            for (int x=mins[0]; x<maxs[0]; ++x) {
                auto cx = x-mins[0];
                auto cy = y-mins[1];
                auto 
                    a = (cy*vbWidth)+cx,
                    b = (cy*vbWidth)+cx+1,
                    c = ((cy+1)*vbWidth)+cx,
                    d = ((cy+1)*vbWidth)+cx+1;

                ibData.push_back((uint16)a);
                ibData.push_back((uint16)b);
                ibData.push_back((uint16)c);
                ibData.push_back((uint16)c);
                ibData.push_back((uint16)b);
                ibData.push_back((uint16)d);
            }
        }

        _pimpl->_defaultIB = Metal::IndexBuffer(
            AsPointer(ibData.cbegin()), 
            ibData.size() * sizeof(unsigned short));

        {
            auto    a = 0u,
                    b = settings._simGridDims,
                    c = (settings._simGridDims*vbWidth),
                    d = (settings._simGridDims*vbWidth)+settings._simGridDims;
            unsigned short ibData[] = 
            {
                (uint16)a,
                (uint16)b,
                (uint16)c,
                (uint16)c,
                (uint16)b,
                (uint16)d
            };
            _pimpl->_unsimDefaultIB = Metal::IndexBuffer(ibData, sizeof(ibData));
        }
    }

    void ShallowSurface::MaybeCreateGrid(RasterizationSurface& mask, Int2 gridCoords)
    {
        const auto& settings = _pimpl->_cfg;
        auto mins = Int2(gridCoords[0]*settings._simGridDims, gridCoords[1]*settings._simGridDims);
        auto maxs = Int2((gridCoords[0]+1)*settings._simGridDims, (gridCoords[1]+1) * settings._simGridDims);
        Float2 physicalMins = mins * settings._gridPhysicalSize / float(settings._simGridDims) + _pimpl->_simulationMins;
        Float2 physicalMaxs = maxs * settings._gridPhysicalSize / float(settings._simGridDims) + _pimpl->_simulationMins;
        const float physicalHeight = 0.f;

        Pimpl::SimGrid simGrid;

            // when no cells are masked out, we can just use the default IB
        bool useDefaultIB = true;
        for (int y=mins[1]; y<maxs[1]; ++y)
            for (int x=mins[0]; x<maxs[0]; ++x)
                if (!mask.Get(x, y)) {
                    useDefaultIB = false;
                    break;
                }

        if (!useDefaultIB) {
            std::vector<unsigned short> ibData;
            ibData.reserve((maxs[0] - mins[0]) * (maxs[1] - mins[1]) * 6);
            auto vbWidth = maxs[0] - mins[0] + 1;

                // For each active cell, we create 2 triangles, and
                // add them to the index buffer
            for (int y=mins[1]; y<maxs[1]; ++y) {
                for (int x=mins[0]; x<maxs[0]; ++x) {
                    auto active = mask.Get(x, y);
                    if (active) {
                        auto cx = x-mins[0];
                        auto cy = y-mins[1];
                        auto 
                            a = (cy*vbWidth)+cx,
                            b = (cy*vbWidth)+cx+1,
                            c = ((cy+1)*vbWidth)+cx,
                            d = ((cy+1)*vbWidth)+cx+1;

                        ibData.push_back((unsigned short)a);
                        ibData.push_back((unsigned short)b);
                        ibData.push_back((unsigned short)c);
                        ibData.push_back((unsigned short)c);
                        ibData.push_back((unsigned short)b);
                        ibData.push_back((unsigned short)d);
                    }
                }
            }

            if (ibData.empty()) return; // no cells in this one

            simGrid._ib = Metal::IndexBuffer(
                AsPointer(ibData.cbegin()), 
                ibData.size() * sizeof(unsigned short));
            simGrid._indexCount = (unsigned)ibData.size();
        } else {
            simGrid._indexCount = settings._simGridDims * settings._simGridDims * 6;
        }
        
        simGrid._gridToWorld = 
            AsFloat4x4(
                ScaleTranslation(
                    Expand(Float2(physicalMaxs - physicalMins), 1.f),
                    Expand(physicalMins, physicalHeight)));
        simGrid._gridCoord = gridCoords;

        _pimpl->_simGrids.push_back(simGrid);
        _pimpl->_validGridList.push_back(gridCoords);
    }

    void ShallowSurface::UpdateSimulation(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        ISurfaceHeightsProvider* surfaceHeights)
    {
        _pimpl->_bufferCounter = (_pimpl->_bufferCounter+1)%3;

        DeepOceanSimSettings oceanSettings;
        oceanSettings._baseHeight = _pimpl->_cfg._baseHeight;

        ShallowWaterSim::SimulationContext simContext(
            metalContext, oceanSettings,
            _pimpl->_cfg._gridPhysicalSize,
            _pimpl->_simulationMins,
            surfaceHeights, 
            nullptr, ShallowWaterSim::BorderMode::BaseHeight);

        ShallowWaterSim::SimSettings settings;
        settings._rainQuantityPerFrame = _pimpl->_cfg._rainQuantity;
        settings._evaporationConstant = _pimpl->_cfg._evaporationConstant;
        settings._pressureConstant = _pimpl->_cfg._pressureConstant;
        settings._compressionConstants = 
            OceanHack_CompressionConstants(
                metalContext, parserContext, 
                _pimpl->_cfg._baseHeight, 0.2f, 6.f);
        settings._compressionConstants[0] -= _pimpl->_simulationMins[0];
        settings._compressionConstants[1] -= _pimpl->_simulationMins[1];

        _pimpl->_sim->ExecuteSim(
            simContext, parserContext, settings, _pimpl->_bufferCounter,
            AsPointer(_pimpl->_validGridList.cbegin()), AsPointer(_pimpl->_validGridList.cend()));
    }

    static SharedPkt MakeLightingConstants(const ShallowSurface::LightingConfig& cfg)
    {
        class LightingConstants
        {
        public:
            Float3 OpticalThickness;
            unsigned dummy0;
            Float3 FoamColor;
            float Specular;
            float Roughness;
            float RefractiveIndex;
            float UpwellingScale;
            float SkyReflectionScale;
        };
        Float3 temp = cfg._opticalThicknessReciprocalScalar * AsFloat3Color(cfg._opticalThicknessReciprocalColor);
        temp = Float3(1.0f / std::max(1e-5f, temp[0]), 1.0f / std::max(1e-5f, temp[1]), 1.0f / std::max(1e-5f, temp[2]));
        return MakeSharedPkt(
            LightingConstants
            {
                temp,
                0,
                AsFloat3Color(cfg._foamColor),
                cfg._specular, cfg._roughness,
                cfg._refractiveIndex, cfg._upwellingScale,
                cfg._skyReflectionScale
            });
    }

    void ShallowSurface::RenderDebugging(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        unsigned techniqueIndex,
        unsigned skyProjType, bool refractionsEnable)
    {
        using namespace Techniques;
        ParameterBox matParam;
        matParam.SetParameter(
            (const utf8*)"SHALLOW_WATER_TILE_DIMENSION", 
            _pimpl->_cfg._simGridDims);
        matParam.SetParameter((const utf8*)"MAT_DO_REFRACTION", int(refractionsEnable));
        matParam.SetParameter((const utf8*)"SKY_PROJECTION", skyProjType);
        TechniqueMaterial simMaterial(
            InputLayout(nullptr, 0),
            {   ObjectCB::LocalTransform,
                Hash64("ShallowWaterCellConstants"), Hash64("ShallowWaterLighting") },
            matParam);

        matParam.SetParameter((const utf8*)"SHALLOW_WATER_IS_SIMULATED", 0);
        TechniqueMaterial unsimMaterial(
            InputLayout(nullptr, 0),
            { ObjectCB::LocalTransform, Hash64("ShallowWaterLighting") },
            matParam);

            // set up basic render state
        metalContext.Bind(Metal::Topology::TriangleList);
        metalContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
        metalContext.Unbind<Metal::VertexBuffer>();
        metalContext.Unbind<Metal::BoundInputLayout>();
        _pimpl->_sim->BindForOceanRender(metalContext, _pimpl->_bufferCounter);

        auto lightingConstantsBuffer = MakeLightingConstants(_pimpl->_lightingCfg);

        std::vector<unsigned> unsimulated;  // todo -- use frame temporary heap
        unsimulated.reserve(_pimpl->_simGrids.size());

            // First, render the tiles that are currently being simulated
        auto shader = simMaterial.FindVariation(
            parserContext, techniqueIndex, "game/xleres/ocean/shallowsurface.tech");
        if (shader._shader._shaderProgram) {
            for (auto i=_pimpl->_simGrids.cbegin(); i!=_pimpl->_simGrids.cend(); ++i) {
                auto page = _pimpl->_sim->BuildCellConstants(i->_gridCoord);
                if (!page) {
                    unsimulated.push_back((unsigned)std::distance(_pimpl->_simGrids.cbegin(), i));
                    continue;
                }

                shader._shader.Apply(
                    metalContext, parserContext, 
                    {
                        MakeLocalTransformPacket(
                            i->_gridToWorld,
                            ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld)),
                        page, lightingConstantsBuffer
                    });

                if (i->_ib.GetUnderlying()) {
                    metalContext.Bind(i->_ib, Format::R16_UINT);
                } else {
                    metalContext.Bind(_pimpl->_defaultIB, Format::R16_UINT);
                }
                metalContext.DrawIndexed(i->_indexCount);
            }
        }

            //  We must also render distant tiles that don't have active simulation
        auto unsimShader = unsimMaterial.FindVariation(
            parserContext, techniqueIndex, "game/xleres/ocean/shallowsurface.tech");
        if (unsimShader._shader._shaderProgram) {
            for (auto i=unsimulated.cbegin(); i!=unsimulated.cend(); ++i) {
                auto& grid = _pimpl->_simGrids[*i];

                auto gridToWorld = grid._gridToWorld;
                gridToWorld(2, 3) = _pimpl->_cfg._baseHeight;   // apply base height to the grid-to-world transform

                unsimShader._shader.Apply(
                    metalContext, parserContext, 
                    {
                        MakeLocalTransformPacket(
                            gridToWorld,
                            ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld)),
                        lightingConstantsBuffer
                    });

                if (grid._ib.GetUnderlying()) {
                    metalContext.Bind(grid._ib, Format::R16_UINT);
                    metalContext.DrawIndexed(grid._indexCount);
                } else {
                    metalContext.Bind(_pimpl->_unsimDefaultIB, Format::R16_UINT);
                    metalContext.DrawIndexed(6);
                }
            }
        }

        _pimpl->_sim->UnbindForOceanRender(metalContext);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ShallowSurfaceManager::Pimpl
    {
    public:
        std::vector<std::shared_ptr<ShallowSurface>> _surfaces;
        std::unique_ptr<DeepOceanSim> _oceanSim;
    };

    void ShallowSurfaceManager::Add(std::shared_ptr<ShallowSurface> surface)
    {
        _pimpl->_surfaces.push_back(std::move(surface));
    }

    void ShallowSurfaceManager::Clear()
    {
        _pimpl->_surfaces.clear();
    }

    static bool BindRefractions(
        Metal::DeviceContext& metalContext, 
        LightingParserContext& parserContext,
        float refractionStdDev, bool doStepDown)
    {
        Metal::ViewportDesc mainViewportDesc(metalContext);
        float scale = doStepDown ? .5f : 1.f;
        auto& refractionBox = Techniques::FindCachedBox2<RefractionsBuffer>(
            unsigned(mainViewportDesc.Width*scale), 
            unsigned(mainViewportDesc.Height*scale));
        refractionBox.Build(metalContext, parserContext, refractionStdDev);

#if GFXAPI_ACTIVE == GFXAPI_DX11	// platformtemp
        SavedTargets targets(metalContext);
        if (!targets.GetDepthStencilView()) return false;

        auto duplicatedDepthBuffer = Metal::Duplicate(metalContext, Metal::ExtractResource(targets.GetDepthStencilView()));
        Metal::ShaderResourceView secondaryDepthBufferSRV(duplicatedDepthBuffer, 
            {{TextureViewWindow::Aspect::Depth}});

        metalContext.BindPS(MakeResourceList(9, refractionBox.GetSRV(), secondaryDepthBufferSRV));
#endif
		return true;
    }

    void ShallowSurfaceManager::RenderDebugging(
        Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        unsigned techniqueIndex,
        ISurfaceHeightsProvider* surfaceHeights)
    {
        if (_pimpl->_surfaces.empty()) return;
        if (!Tweakable("DoShallowSurface", true)) return;

        if (Tweakable("ShallowSurfaceWireframe", false))
            techniqueIndex = 8;

        CATCH_ASSETS_BEGIN
            for (auto i : _pimpl->_surfaces)
                i->UpdateSimulation(metalContext, parserContext, surfaceHeights);

            static bool doStepDown = true;
            static float refractionBlur = 1.3f;
            bool refractionsEnable = BindRefractions(metalContext, parserContext, refractionBlur, doStepDown);

            {
                static DeepOceanSimSettings deepOceanSettings;
                deepOceanSettings._gridDimensions = 128;
                deepOceanSettings._physicalDimensions = 64.f;
                deepOceanSettings._windVelocity[0] = 10.f;
                deepOceanSettings._scaleAgainstWind[0] = 1.f;
                deepOceanSettings._strengthConstantZ = 1.f;
                deepOceanSettings._gridShiftSpeed = 0.f;
                _pimpl->_oceanSim->Update(
                    &metalContext, parserContext,
                    deepOceanSettings, 0);
                metalContext.BindPS(MakeResourceList(1, _pimpl->_oceanSim->_normalsTextureSRV));
            }

            auto skyProjectionType = SkyTextureParts(parserContext.GetSceneParser()->GetGlobalLightingDesc()).BindPS(metalContext, 11);

            metalContext.BindPS(MakeResourceList(4,
                Techniques::FindCachedBox2<WaterNoiseTexture>()._srv));

            for (auto i : _pimpl->_surfaces)
                i->RenderDebugging(
                    metalContext, parserContext, techniqueIndex, 
                    skyProjectionType, refractionsEnable);

                // unbind refractions
            metalContext.UnbindPS<Metal::ShaderResourceView>(9, 2);
        CATCH_ASSETS_END(parserContext)
    }

    ShallowSurfaceManager::ShallowSurfaceManager() 
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_oceanSim = std::make_unique<DeepOceanSim>(
            DeepOceanSim::Desc(128, 128, true, false));
    }

    ShallowSurfaceManager::~ShallowSurfaceManager() {}

}

template<> const ClassAccessors& GetAccessors<SceneEngine::ShallowSurface::Config>()
{
    using Obj = SceneEngine::ShallowSurface::Config;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add(u("GridPhysicalSize"),    DefaultGet(Obj, _gridPhysicalSize), DefaultSet(Obj, _gridPhysicalSize));
        props.Add(u("GridDims"),            DefaultGet(Obj, _simGridDims),      DefaultSet(Obj, _simGridDims));
        props.Add(u("SimGridCount"),        DefaultGet(Obj, _simGridCount),     DefaultSet(Obj, _simGridCount));
        props.Add(u("BaseHeight"),          DefaultGet(Obj, _baseHeight),       DefaultSet(Obj, _baseHeight));

        props.Add(u("SimMethod"),
            [](const Obj& obj) { return obj._usePipeModel ? 0 : 1; },
            [](Obj& obj, unsigned value) { obj._usePipeModel = (value==0); });

        props.Add(u("RainQuantity"),            DefaultGet(Obj, _rainQuantity),         DefaultSet(Obj, _rainQuantity));
        props.Add(u("EvaporationConstant"),     DefaultGet(Obj, _evaporationConstant),  DefaultSet(Obj, _evaporationConstant));
        props.Add(u("PressureConstant"),        DefaultGet(Obj, _pressureConstant),     DefaultSet(Obj, _pressureConstant));

        init = true;
    }
    return props;
}

template<> const ClassAccessors& GetAccessors<SceneEngine::ShallowSurface::LightingConfig>()
{
    using Obj = SceneEngine::ShallowSurface::LightingConfig;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add(u("OpticalThicknessReciprocalColor"), DefaultGet(Obj, _opticalThicknessReciprocalColor), DefaultSet(Obj, _opticalThicknessReciprocalColor));
        props.Add(u("OpticalThicknessReciprocalScalar"),  DefaultGet(Obj, _opticalThicknessReciprocalScalar),   DefaultSet(Obj, _opticalThicknessReciprocalScalar));
        props.Add(u("FoamColor"),               DefaultGet(Obj, _foamColor),                DefaultSet(Obj, _foamColor));
        props.Add(u("Specular"),                DefaultGet(Obj, _specular),                 DefaultSet(Obj, _specular));
        props.Add(u("Roughness"),               DefaultGet(Obj, _roughness),                DefaultSet(Obj, _roughness));
        props.Add(u("RefractiveIndex"),         DefaultGet(Obj, _refractiveIndex),          DefaultSet(Obj, _refractiveIndex));
        props.Add(u("UpwellingScale"),          DefaultGet(Obj, _upwellingScale),           DefaultSet(Obj, _upwellingScale));
        props.Add(u("SkyReflectionScale"),      DefaultGet(Obj, _skyReflectionScale),       DefaultSet(Obj, _skyReflectionScale));
        init = true;
    }
    return props;
}

namespace SceneEngine 
{
    ShallowSurface::Config::Config()
    {
        _gridPhysicalSize = 64.f;
        _simGridDims = 128;
        _simGridCount = 12;
        _baseHeight = 0.f;
        _usePipeModel = true;
        _rainQuantity = 0.f;
        _evaporationConstant = 1.f;
        _pressureConstant = 150.f;
    }

    ShallowSurface::LightingConfig::LightingConfig()
    {
        _opticalThicknessReciprocalColor = ~0u;
        _opticalThicknessReciprocalScalar = 1.f;
        _foamColor = ~0u;
        _specular = .22f;
        _roughness = .06f;
        _refractiveIndex = 1.333f;
        _upwellingScale = 0.33f;
        _skyReflectionScale = 0.75f;
    }
}
