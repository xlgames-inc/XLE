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
#include "../Assets/Assets.h"
#include "../Math/Matrix.h"
#include "../Math/Transformations.h"
#include "../Utility/PtrUtils.h"
#include "../Utility/MemoryUtils.h"

#include "../RenderCore/DX11/Metal/DX11Utils.h"

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

        std::vector<SimGrid> _simGrids;
        std::vector<Int2> _validGridList;
        std::unique_ptr<ShallowWaterSim> _sim;
        Float2 _simulationMins;
        unsigned _bufferCounter;
        Config _cfg;
    };

    ShallowSurface::ShallowSurface(
        const Float2 triangleList[], size_t stride,
        size_t ptCount,
        const Config& settings)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_cfg = settings;
        _pimpl->_bufferCounter = 0;

        const auto maxSimulationGrids = settings._simGridCount;
        const bool usePipeModel = settings._usePipeModel;
        _pimpl->_sim = std::make_unique<ShallowWaterSim>(
            ShallowWaterSim::Desc(
                settings._simGridDims, maxSimulationGrids, usePipeModel, false, false));

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

        float cellPhySize = settings._gridPhysicalSize / float(settings._simGridDims);
        mins[0] = XlFloor(mins[0] / cellPhySize) * cellPhySize;
        mins[1] = XlFloor(mins[1] / cellPhySize) * cellPhySize;
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

        unsigned simGridsX = (surfaceSize[0] + settings._simGridDims + 1) / settings._simGridDims;
        unsigned simGridsY = (surfaceSize[1] + settings._simGridDims + 1) / settings._simGridDims;
        for (unsigned y=0; y<simGridsY; ++y)
            for (unsigned x=0; x<simGridsX; ++x)
                MaybeCreateGrid(mask, Int2(x, y));
    }

    ShallowSurface::~ShallowSurface()
    {}

    void ShallowSurface::MaybeCreateGrid(RasterizationSurface& mask, Int2 gridCoords)
    {
        const auto& settings = _pimpl->_cfg;
        auto mins = Int2(gridCoords[0]*settings._simGridDims, gridCoords[1]*settings._simGridDims);
        auto maxs = Int2((gridCoords[0]+1)*settings._simGridDims, (gridCoords[1]+1) * settings._simGridDims);
        Float2 physicalMins = mins * settings._gridPhysicalSize / float(settings._simGridDims) + _pimpl->_simulationMins;
        Float2 physicalMaxs = maxs * settings._gridPhysicalSize / float(settings._simGridDims) + _pimpl->_simulationMins;
        const float physicalHeight = 0.f;

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

        Pimpl::SimGrid simGrid;
        simGrid._ib = Metal::IndexBuffer(
            AsPointer(ibData.cbegin()), 
            ibData.size() * sizeof(unsigned short));
        simGrid._gridToWorld = 
            AsFloat4x4(
                ScaleTranslation(
                    Expand(Float2(physicalMaxs - physicalMins), 1.f),
                    Expand(physicalMins, physicalHeight)));
        simGrid._indexCount = (unsigned)ibData.size();
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

        _pimpl->_sim->ExecuteSim(
            simContext, parserContext, settings, _pimpl->_bufferCounter,
            AsPointer(_pimpl->_validGridList.cbegin()), AsPointer(_pimpl->_validGridList.cend()));
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
        TechniqueMaterial material(
            Metal::InputLayout(nullptr, 0),
            {   ObjectCBs::LocalTransform, ObjectCBs::BasicMaterialConstants, 
                Hash64("ShallowWaterCellConstants") },
            matParam);

        auto shader = material.FindVariation(
            parserContext, techniqueIndex, "game/xleres/ocean/shallowsurface.txt");
        if (shader._shaderProgram) {
            metalContext.Bind(Metal::Topology::TriangleList);
            metalContext.Bind(Techniques::CommonResources()._blendAlphaPremultiplied);
            _pimpl->_sim->BindForOceanRender(metalContext, _pimpl->_bufferCounter);

            const auto& cbLayout = ::Assets::GetAssetDep<Techniques::PredefinedCBLayout>(
                "game/xleres/BasicMaterialConstants.txt");
            auto matParam = cbLayout.BuildCBDataAsPkt(ParameterBox());
            
            for (auto i=_pimpl->_simGrids.cbegin(); i!=_pimpl->_simGrids.cend(); ++i) {
                auto page = _pimpl->_sim->BuildCellConstants(i->_gridCoord);
                if (!page) continue;

                shader.Apply(
                    metalContext, parserContext, 
                    {
                        MakeLocalTransformPacket(
                            i->_gridToWorld,
                            ExtractTranslation(parserContext.GetProjectionDesc()._cameraToWorld)),
                        matParam,
                        page
                    });
                metalContext.Unbind<Metal::VertexBuffer>();
                metalContext.Unbind<Metal::BoundInputLayout>();

                metalContext.Bind(i->_ib, Metal::NativeFormat::R16_UINT);
                metalContext.DrawIndexed(i->_indexCount);
            }
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    void ShallowSurfaceManager::Add(std::shared_ptr<ShallowSurface> surface)
    {
        _surfaces.push_back(std::move(surface));
    }

    void ShallowSurfaceManager::Clear()
    {
        _surfaces.clear();
    }

    static bool BindRefractions(
        Metal::DeviceContext& metalContext, 
        LightingParserContext& parserContext,
        float refractionStdDev)
    {
        Metal::ViewportDesc mainViewportDesc(metalContext);
        auto& refractionBox = Techniques::FindCachedBox2<RefractionsBuffer>(
            unsigned(mainViewportDesc.Width/2.f), unsigned(mainViewportDesc.Height/2.f));
        refractionBox.Build(metalContext, parserContext, refractionStdDev);

        SavedTargets targets(&metalContext);
        auto duplicatedDepthBuffer = Metal::DuplicateResource(
            metalContext.GetUnderlying(), 
            Metal::ExtractResource<ID3D::Resource>(targets.GetDepthStencilView()).get());
        Metal::ShaderResourceView secondaryDepthBufferSRV(
            duplicatedDepthBuffer.get(), (Metal::NativeFormat::Enum)DXGI_FORMAT_R24_UNORM_X8_TYPELESS);

        metalContext.BindPS(MakeResourceList(9, refractionBox.GetSRV(), secondaryDepthBufferSRV));
        return true;
    }

    void ShallowSurfaceManager::RenderDebugging(
        Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        unsigned techniqueIndex,
        ISurfaceHeightsProvider* surfaceHeights)
    {
        if (_surfaces.empty()) return;

        TRY 
        {
            for (auto i : _surfaces)
                i->UpdateSimulation(metalContext, parserContext, surfaceHeights);

            bool refractionsEnable = BindRefractions(metalContext, parserContext, 1.6f);

            unsigned skyProjectionType = 0;
            auto skyTexture = parserContext.GetSceneParser()->GetGlobalLightingDesc()._skyTexture;
            if (skyTexture[0]) {
                skyProjectionType = SkyTexture_BindPS(&metalContext, parserContext, skyTexture, 11);
            }

            for (auto i : _surfaces)
                i->RenderDebugging(
                    metalContext, parserContext, techniqueIndex, 
                    skyProjectionType, refractionsEnable);
        }
        CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
        CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
        CATCH_END
    }

    ShallowSurfaceManager::ShallowSurfaceManager() {}
    ShallowSurfaceManager::~ShallowSurfaceManager() {}
}

