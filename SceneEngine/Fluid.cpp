// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS
#define _SILENCE_CXX17_NEGATORS_DEPRECATION_WARNING

#include "Fluid.h"
#include "FluidAdvection.h"
#include "FluidHelper.h"
#include "../Math/RegularNumberField.h"
#include "../Math/PoissonSolver.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/Meta/ClassAccessorsImpl.h"

extern "C" void dens_step ( int N, float * x, float * x0, float * u, float * v, float diff, float dt );
extern "C" void vel_step ( int N, float * u, float * v, float * u0, float * v0, float visc, float dt );

#pragma warning(disable:4505) // warning C4505: 'SceneEngine::EnforceIncompressibility' : unreferenced local function has been removed

namespace SceneEngine
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    class ReferenceFluidSolver2D::Pimpl
    {
    public:
        std::unique_ptr<float[]> _velU;
        std::unique_ptr<float[]> _velV;
        std::unique_ptr<float[]> _density;
        std::unique_ptr<float[]> _prevVelU;
        std::unique_ptr<float[]> _prevVelV;
        std::unique_ptr<float[]> _prevDensity;
        UInt2 _dimensions;
    };

    void ReferenceFluidSolver2D::Tick(const Settings& settings)
    {
        auto N = _pimpl->_dimensions[0];
        assert(_pimpl->_dimensions[1] == _pimpl->_dimensions[0]);

        vel_step( 
            N, 
            _pimpl->_velU.get(), _pimpl->_velV.get(), 
            _pimpl->_prevVelU.get(), _pimpl->_prevVelV.get(), 
            settings._viscosity, settings._deltaTime );
	    dens_step( 
            N, _pimpl->_density.get(), _pimpl->_prevDensity.get(), 
            _pimpl->_velU.get(), _pimpl->_velV.get(), 
            settings._diffusionRate, settings._deltaTime );

        auto eleCount = (_pimpl->_dimensions[0]+2) * (_pimpl->_dimensions[1]+2);
        for (unsigned c=0; c<eleCount; ++c) {
            _pimpl->_prevVelU[c] = 0.f;
            _pimpl->_prevVelV[c] = 0.f;
            _pimpl->_prevDensity[c] = 0.f;
        }
    }

    void ReferenceFluidSolver2D::AddDensity(UInt2 coords, float amount)
    {
        if (coords[0] < _pimpl->_dimensions[0] && coords[1] < _pimpl->_dimensions[1]) {
            unsigned i = (coords[0]+1) + (coords[1]+1) * (_pimpl->_dimensions[0] + 2);
            _pimpl->_prevDensity[i] += amount;
        }
    }

    void ReferenceFluidSolver2D::AddVelocity(UInt2 coords, Float2 vel)
    {
        if (coords[0] < _pimpl->_dimensions[0] && coords[1] < _pimpl->_dimensions[1]) {
            unsigned i = (coords[0]+1) + (coords[1]+1) * (_pimpl->_dimensions[0] + 2);
            _pimpl->_prevVelU[i] += vel[0];
            _pimpl->_prevVelV[i] += vel[1];
        }
    }

    void ReferenceFluidSolver2D::RenderDebugging(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode)
    {
        switch (debuggingMode) {
        case FluidDebuggingMode::Density:
            RenderFluidDebugging2D(
                metalContext, parserContext, RenderFluidMode::Scalar,
                _pimpl->_dimensions + UInt2(2,2), 0.f, 1.f,
                { _pimpl->_density.get() });
            break;

        case FluidDebuggingMode::Velocity:
            RenderFluidDebugging2D(
                metalContext, parserContext, RenderFluidMode::Vector,
                _pimpl->_dimensions + UInt2(2,2), 0.f, 1.f,
                { _pimpl->_velU.get(), _pimpl->_velV.get() });
            break;
        }
    }

    UInt2 ReferenceFluidSolver2D::GetDimensions() const { return _pimpl->_dimensions; }

    ReferenceFluidSolver2D::ReferenceFluidSolver2D(UInt2 dimensions)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_dimensions = dimensions;
        auto eleCount = (dimensions[0]+2) * (dimensions[1]+2);
        _pimpl->_velU = std::make_unique<float[]>(eleCount);
        _pimpl->_velV = std::make_unique<float[]>(eleCount);
        _pimpl->_density = std::make_unique<float[]>(eleCount);
        _pimpl->_prevVelU = std::make_unique<float[]>(eleCount);
        _pimpl->_prevVelV = std::make_unique<float[]>(eleCount);
        _pimpl->_prevDensity = std::make_unique<float[]>(eleCount);

        for (unsigned c=0; c<eleCount; ++c) {
            _pimpl->_velU[c] = 0.f;
            _pimpl->_velV[c] = 0.f;
            _pimpl->_density[c] = 0.f;
            _pimpl->_prevVelU[c] = 0.f;
            _pimpl->_prevVelV[c] = 0.f;
            _pimpl->_prevDensity[c] = 0.f;
        }
    }

    ReferenceFluidSolver2D::~ReferenceFluidSolver2D() {}

    ReferenceFluidSolver2D::Settings::Settings()
    {
        _deltaTime = 1.0f/60.f;
        _viscosity = 0.f;
        _diffusionRate = 0.f;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class FluidSolver2D::Pimpl
    {
    public:
        VectorX _velU[3];
        VectorX _velV[3];

        VectorX _density[2];
        VectorX _temperature[2];

        UInt2 _dimsWithoutBorder;
        UInt2 _dimsWithBorder;
        unsigned _N;

        PoissonSolver _poissonSolver;
        DiffusionHelper _densityDiffusion;
        DiffusionHelper _velocityDiffusion;
        DiffusionHelper _temperatureDiffusion;
        EnforceIncompressibilityHelper _incompOp;
    };

    void FluidSolver2D::Tick(float deltaTime, const Settings& settings)
    {
        float dt = deltaTime;
        const auto N = _pimpl->_N;

        auto& velUT0 = _pimpl->_velU[0];
        auto& velUT1 = _pimpl->_velU[1];
        auto& velUSrc = _pimpl->_velU[2];
        auto& velUWorking = _pimpl->_velU[2];

        auto& velVT0 = _pimpl->_velV[0];
        auto& velVT1 = _pimpl->_velV[1];
        auto& velVSrc = _pimpl->_velV[2];
        auto& velVWorking = _pimpl->_velV[2];

        auto& densitySrc = _pimpl->_density[0];
        auto& densityWorking = _pimpl->_density[0];
        auto& densityT1 = _pimpl->_density[1];

        auto& temperatureSrc = _pimpl->_temperature[0];
        auto& temperatureWorking = _pimpl->_temperature[0];
        auto& temperatureT1 = _pimpl->_temperature[1];

        VorticityConfinement(
            VectorField2D(&velUSrc, &velVSrc, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),           // last frame results
            settings._vorticityConfinement, deltaTime);

                    // buoyancy force
        const float buoyancyAlpha = settings._buoyancyAlpha;
        const float buoyancyBeta = settings._buoyancyBeta;
        const UInt2 border(1,1);
        for (unsigned y=border[1]; y<_pimpl->_dimsWithBorder[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<_pimpl->_dimsWithBorder[0]-border[0]; ++x) {
                unsigned i=y*_pimpl->_dimsWithBorder[0]+x;
                velVSrc[i] +=     // (upwards is +1 in V)
                     -buoyancyAlpha * densityT1[i]
                    + buoyancyBeta  * temperatureT1[i];       // temperature field is just the difference from ambient
            }

        for (unsigned c=0; c<N; ++c) {
            velUT0[c] = velUT1[c];
            velVT0[c] = velVT1[c];
            velUWorking[c] = velUT1[c] + dt * velUSrc[c];
            velVWorking[c] = velVT1[c] + dt * velVSrc[c];

            densityWorking[c] = densityT1[c] + dt * densitySrc[c];
            temperatureWorking[c] = temperatureT1[c] + dt * temperatureSrc[c];
        }

        auto marginFlags = 0u;
        marginFlags |= (1<<0) * (settings._borderX == (int)AdvectionBorder::Margin);
        marginFlags |= (1<<1) * (settings._borderY == (int)AdvectionBorder::Margin);
        auto wrapEdges = 0u;
        wrapEdges |= (1<<0) * (settings._borderX == (int)AdvectionBorder::Wrap);
        wrapEdges |= (1<<1) * (settings._borderY == (int)AdvectionBorder::Wrap);

            //
            // Diffuse velocity! This is similar to other diffusion operations. 
            // In effect, the values in each cell should slowly "seep" into 
            // neighbour cells -- over time spreading out over the whole grid. 
            // This is diffusion.
            //
            // Mathematically, this operation is called the "heat equation."
            // It is the same equation that is used to model how heat spreads
            // through a room from some source. Actually, it also applies to
            // any radiation (including light).
            //
            // See reference here: https://en.wikipedia.org/wiki/Heat_equation
            // The equation can be written using the laplacian operation. So this
            // is a partial differential equation. We must solve it using an
            // estimate.
            //
        _pimpl->_velocityDiffusion.Execute(
            _pimpl->_poissonSolver, 
            VectorField2D(&velUWorking, &velVWorking, _pimpl->_dimsWithBorder),
            settings._viscosity, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, 
            wrapEdges, "Velocity");

        AdvectionSettings advSettings {
            (AdvectionMethod)settings._advectionMethod, (AdvectionInterp)settings._interpolationMethod, settings._advectionSteps,
            (AdvectionBorder)settings._borderX, (AdvectionBorder)settings._borderY, AdvectionBorder::None
        };
        PerformAdvection(
            VectorField2D(&velUT1,      &velVT1,        _pimpl->_dimsWithBorder),
            VectorField2D(&velUWorking, &velVWorking,   _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0,      &velVT0,        _pimpl->_dimsWithBorder),
            VectorField2D(&velUWorking, &velVWorking,   _pimpl->_dimsWithBorder),
            deltaTime, advSettings);

        ReflectUBorder2D(velUT1, _pimpl->_dimsWithBorder, marginFlags);
        ReflectVBorder2D(velVT1, _pimpl->_dimsWithBorder, marginFlags);
        _pimpl->_incompOp.Execute(
            _pimpl->_poissonSolver, 
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            (PoissonSolver::Method)settings._enforceIncompressibilityMethod, wrapEdges);

        SmearBorder2D(densityWorking, _pimpl->_dimsWithBorder, marginFlags);
        _pimpl->_densityDiffusion.Execute(
            _pimpl->_poissonSolver, 
            ScalarField2D(&densityWorking, _pimpl->_dimsWithBorder),
            settings._diffusionRate, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, 
            wrapEdges, "Density");
        PerformAdvection(
            ScalarField2D(&densityT1, _pimpl->_dimsWithBorder),
            ScalarField2D(&densityWorking, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0, &velVT0, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            deltaTime, advSettings);

        SmearBorder2D(temperatureWorking, _pimpl->_dimsWithBorder, marginFlags);
        _pimpl->_temperatureDiffusion.Execute(
            _pimpl->_poissonSolver, 
            ScalarField2D(&temperatureWorking, _pimpl->_dimsWithBorder),
            settings._tempDiffusion, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, 
            wrapEdges, "Temperature");
        PerformAdvection(
            ScalarField2D(&temperatureT1, _pimpl->_dimsWithBorder),
            ScalarField2D(&temperatureWorking, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0, &velVT0, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            deltaTime, advSettings);

        for (unsigned c=0; c<N; ++c) {
            velUSrc[c] = 0.f;
            velVSrc[c] = 0.f;
            densitySrc[c] = 0.f;
            temperatureSrc[c] = 0.f;
        }
    }

    void FluidSolver2D::AddDensity(UInt2 coords, float amount)
    {
        if (coords[0] < _pimpl->_dimsWithoutBorder[0] && coords[1] < _pimpl->_dimsWithoutBorder[1]) {
            unsigned i = (coords[0]+1) + (coords[1]+1) * _pimpl->_dimsWithBorder[0];
            _pimpl->_density[0][i] += amount;
        }
    }

    void FluidSolver2D::AddTemperature(UInt2 coords, float amount)
    {
        if (coords[0] < _pimpl->_dimsWithoutBorder[0] && coords[1] < _pimpl->_dimsWithoutBorder[1]) {
            unsigned i = (coords[0]+1) + (coords[1]+1) * _pimpl->_dimsWithBorder[0];
            // _pimpl->_prevTemperature[i] += amount;

                // heat up to approach this temperature
            auto oldTemp = _pimpl->_temperature[1][i];
            _pimpl->_temperature[1][i] = std::max(oldTemp, LinearInterpolate(oldTemp, amount, 0.5f));
        }
    }

    void FluidSolver2D::AddVelocity(UInt2 coords, Float2 vel)
    {
        if (coords[0] < _pimpl->_dimsWithoutBorder[0] && coords[1] < _pimpl->_dimsWithoutBorder[1]) {
            unsigned i = (coords[0]+1) + (coords[1]+1) * _pimpl->_dimsWithBorder[0];
            _pimpl->_velU[2][i] += vel[0];
            _pimpl->_velV[2][i] += vel[1];
        }
    }

    void FluidSolver2D::RenderDebugging(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode)
    {
        switch (debuggingMode) {
        case FluidDebuggingMode::Density:
            RenderFluidDebugging2D(
                metalContext, parserContext, RenderFluidMode::Scalar,
                _pimpl->_dimsWithBorder, 0.f, 1.f,
                { _pimpl->_density[1].data() });
            break;

        case FluidDebuggingMode::Velocity:
            RenderFluidDebugging2D(
                metalContext, parserContext, RenderFluidMode::Vector,
                _pimpl->_dimsWithBorder, 0.f, 1.f,
                { _pimpl->_velU[1].data(), _pimpl->_velV[1].data() });
            break;

        case FluidDebuggingMode::Temperature:
            RenderFluidDebugging2D(
                metalContext, parserContext, RenderFluidMode::Scalar,
                _pimpl->_dimsWithBorder, 0.f, 1.f,
                { _pimpl->_temperature[1].data() });
            break;
        }
    }

    UInt2 FluidSolver2D::GetDimensions() const { return _pimpl->_dimsWithBorder; }

    FluidSolver2D::FluidSolver2D(UInt2 dimensions)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_dimsWithoutBorder = dimensions;
        _pimpl->_dimsWithBorder = dimensions + UInt2(2, 2);
        auto N = _pimpl->_dimsWithBorder[0] * _pimpl->_dimsWithBorder[1];
        _pimpl->_N = N;

        for (unsigned c=0; c<dimof(_pimpl->_velU); ++c) {
            _pimpl->_velU[c] = VectorX(N);
            _pimpl->_velV[c] = VectorX(N);
            _pimpl->_velU[c].fill(0.f);
            _pimpl->_velV[c].fill(0.f);
        }

        for (unsigned c=0; c<dimof(_pimpl->_density); ++c) {
            _pimpl->_density[c] = VectorX(N);
            _pimpl->_temperature[c] = VectorX(N);
            _pimpl->_density[c].fill(0.f);
            _pimpl->_temperature[c].fill(0.f);
        }

        // const float dt = 1.0f / 60.f;
        // float a = 5.f * dt;

        // auto wh = _pimpl->_dimensions[0]+2;
        // auto AMat = [wh, a](unsigned i, unsigned j)
        //     {
        //         if (i == j) return 1.f + 4.f*a;
        //         // auto x0 = (i%wh), y0 = i/wh;
        //         // auto x1 = (j%wh), y1 = j/wh;
        //         // if (    (std::abs(int(x0)-int(x1)) == 1 && y0 == y1)
        //         //     ||  (std::abs(int(y0)-int(y1)) == 1 && x0 == x1)) {
        //         if (j==(i+1) || j==(i-1) || j==(i+wh) || j == (i-wh))
        //             return -a;   
        //         return 0.f;
        //     };
        // 
        // _pimpl->AMat = std::function<float(unsigned, unsigned)>(AMat);
        
        #if defined(_DEBUG)
            // {
            //     auto comparePrecon = CalculateIncompleteCholesky(AMat2D { wh, 1.f + 4.f * a, -a }, N, 0);
            //     float maxDiff = 0.f;
            //     for (unsigned y=0; y<5; ++y)
            //         for (unsigned x=0; x<wh; ++x) {
            //             auto diff = XlAbs(comparePrecon(x, y) - bandedPrecon(x, y));
            //             diff = std::max(diff, maxDiff);
            //         }
            //     LogInfo << "Preconditioner matrix error: " << maxDiff;
            // }

            // for (unsigned i=0; i<N; ++i)
            //     LogInfo << bandedPrecon(i, 0) << ", " << bandedPrecon(i, 1) << ", " << bandedPrecon(i, 2) << ", " << bandedPrecon(i, 3) << ", " << bandedPrecon(i, 4);
        
            // {
            //     auto fullPrecon = CalculateIncompleteCholesky(_pimpl->AMat, N);
            //     for (unsigned i=0; i<N; ++i) {
            //         int j2[] = { 
            //             int(i) + _pimpl->_bands[0], 
            //             int(i) + _pimpl->_bands[1],
            //             int(i) + _pimpl->_bands[2], 
            //             int(i) + _pimpl->_bands[3], 
            //             int(i) + _pimpl->_bands[4]
            //         };
            //         for (unsigned j=0; j<dimof(j2); ++j) {
            //             if (j2[j] >= 0 && j2[j] < int(N)) {
            //                 float a = bandedPrecon(i, j);
            //                 float b = fullPrecon(i, j2[j]);
            //                 assert(Equivalent(a, b, 1.e-3f));
            //             }
            //         }
            //     }
            // }
        #endif

        // _pimpl->_bandedPrecon = SparseBandedMatrix(std::move(bandedPrecon), _pimpl->_bands, dimof(_pimpl->_bands));

        UInt2 fullDims(dimensions[0]+2, dimensions[1]+2);
        _pimpl->_poissonSolver = PoissonSolver(2, &fullDims[0]);
    }

    FluidSolver2D::~FluidSolver2D(){}

    FluidSolver2D::Settings::Settings()
    {
        _viscosity = 0.05f;
        _diffusionRate = 0.05f;
        _tempDiffusion = 2.f;
        _diffusionMethod = 1;
        _advectionMethod = 3;
        _advectionSteps = 4;
        _enforceIncompressibilityMethod = 1;
        _buoyancyAlpha = 2.f;
        _buoyancyBeta = 2.2f;
        _vorticityConfinement = 0.75f;
        _interpolationMethod = 0;
        _borderX = _borderY = 1;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class FluidSolver3D::Pimpl
    {
    public:
        VectorX _velU[3];
        VectorX _velV[3];
        VectorX _velW[3];
        VectorX _density[2];

        UInt3 _dimsWithoutBorder;
        UInt3 _dimsWithBorder;
        unsigned _N;

        PoissonSolver _poissonSolver;
        std::shared_ptr<PoissonSolver::PreparedMatrix> _densityDiffusion;
        std::shared_ptr<PoissonSolver::PreparedMatrix> _velocityDiffusion;
        std::shared_ptr<PoissonSolver::PreparedMatrix> _incompressibility;

        float _preparedDensityDiffusion, _preparedVelocityDiffusion;

        void DensityDiffusion(float deltaTime, const Settings& settings);
        void VelocityDiffusion(float deltaTime, const Settings& settings);
        std::shared_ptr<PoissonSolver::PreparedMatrix> BuildDiffusionMethod(float diffusion);
    };

    void FluidSolver3D::Tick(float deltaTime, const Settings& settings)
    {
        float dt = deltaTime;
        const auto N = _pimpl->_N;
        auto& velUT0 = _pimpl->_velU[0];
        auto& velUT1 = _pimpl->_velU[1];
        auto& velUSrc = _pimpl->_velU[2];
        auto& velUWorking = _pimpl->_velU[2];

        auto& velVT0 = _pimpl->_velV[0];
        auto& velVT1 = _pimpl->_velV[1];
        auto& velVSrc = _pimpl->_velV[2];
        auto& velVWorking = _pimpl->_velV[2];

        auto& velWT0 = _pimpl->_velW[0];
        auto& velWT1 = _pimpl->_velW[1];
        auto& velWSrc = _pimpl->_velW[2];
        auto& velWWorking = _pimpl->_velW[2];

        auto& densitySrc = _pimpl->_density[0];
        auto& densityWorking = _pimpl->_density[0];
        auto& densityT1 = _pimpl->_density[1];

            // simple buoyancy... just add upwards force where there is density
        static float buoyancyScale = 25.f;
        const UInt3 border(1u,1u,1u);
        for (unsigned z=border[1]; z<_pimpl->_dimsWithBorder[2]-border[2]; ++z)
            for (unsigned y=border[1]; y<_pimpl->_dimsWithBorder[1]-border[1]; ++y)
                for (unsigned x=border[0]; x<_pimpl->_dimsWithBorder[0]-border[0]; ++x) {
                    unsigned i = (z*_pimpl->_dimsWithBorder[1]+y)*_pimpl->_dimsWithBorder[0]+x;
                    velWSrc[i] += buoyancyScale * densityT1[i];
                }

        for (unsigned c=0; c<N; ++c) {
            velUT0[c] = velUT1[c];
            velVT0[c] = velVT1[c];
            velWT0[c] = velWT1[c];
            velUWorking[c] = velUT1[c] + dt * velUSrc[c];
            velVWorking[c] = velVT1[c] + dt * velVSrc[c];
            velWWorking[c] = velWT1[c] + dt * velWSrc[c];
            densityWorking[c] = densityT1[c] + dt * densitySrc[c];
        }

        _pimpl->VelocityDiffusion(deltaTime, settings);

        AdvectionSettings advSettings { 
            (AdvectionMethod)settings._advectionMethod, (AdvectionInterp)settings._interpolationMethod, settings._advectionSteps,
            AdvectionBorder::Margin, AdvectionBorder::Margin, AdvectionBorder::Margin
        };
        PerformAdvection(
            VectorField3D(&velUT1,      &velVT1,        &velWT1,        _pimpl->_dimsWithBorder),
            VectorField3D(&velUWorking, &velVWorking,   &velWWorking,   _pimpl->_dimsWithBorder),
            VectorField3D(&velUT0,      &velVT0,        &velWT0,        _pimpl->_dimsWithBorder),
            VectorField3D(&velUWorking, &velVWorking,   &velWWorking,   _pimpl->_dimsWithBorder),
            deltaTime, advSettings);
        
        ReflectBorder3D(velUT1, _pimpl->_dimsWithBorder, 0);
        ReflectBorder3D(velVT1, _pimpl->_dimsWithBorder, 1);
        ReflectBorder3D(velWT1, _pimpl->_dimsWithBorder, 2);
        EnforceIncompressibility(
            VectorField3D(&velUT1, &velVT1, &velWT1, _pimpl->_dimsWithBorder),
            _pimpl->_poissonSolver, *_pimpl->_incompressibility,
            (PoissonSolver::Method)settings._enforceIncompressibilityMethod);

        _pimpl->DensityDiffusion(deltaTime, settings);
        PerformAdvection(
            ScalarField3D(&densityT1, _pimpl->_dimsWithBorder),
            ScalarField3D(&densityWorking, _pimpl->_dimsWithBorder),
            VectorField3D(&velUT0, &velVT0, &velWT0, _pimpl->_dimsWithBorder),
            VectorField3D(&velUT1, &velVT1, &velWT1, _pimpl->_dimsWithBorder),
            deltaTime, advSettings);

        for (unsigned c=0; c<N; ++c) {
            velUSrc[c] = 0.f;
            velVSrc[c] = 0.f;
            velWSrc[c] = 0.f;
            densitySrc[c] = 0.f;
        }
    }

    void FluidSolver3D::AddDensity(UInt3 coords, float amount)
    {
        if (    coords[0] < _pimpl->_dimsWithoutBorder[0] 
            &&  coords[1] < _pimpl->_dimsWithoutBorder[1]
            &&  coords[2] < _pimpl->_dimsWithoutBorder[2]) {

            unsigned i = (coords[0]+1) + _pimpl->_dimsWithBorder[0] * ((coords[1]+1) + (coords[2]+1) * _pimpl->_dimsWithBorder[1]);
            _pimpl->_density[0][i] += amount;
        }
    }

    void FluidSolver3D::RenderDebugging(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode)
    {
        switch (debuggingMode) {
        case FluidDebuggingMode::Density:
            RenderFluidDebugging3D(
                metalContext, parserContext, RenderFluidMode::Scalar,
                _pimpl->_dimsWithBorder, 0.f, 1.f,
                { _pimpl->_density[1].data() });
            break;

        case FluidDebuggingMode::Velocity:
            RenderFluidDebugging3D(
                metalContext, parserContext, RenderFluidMode::Vector,
                _pimpl->_dimsWithBorder, 0.f, 1.f,
                { _pimpl->_velU[1].data(), _pimpl->_velV[1].data(), _pimpl->_velW[1].data() });
            break;
        }
    }

    std::shared_ptr<PoissonSolver::PreparedMatrix> FluidSolver3D::Pimpl::BuildDiffusionMethod(float diffusion)
    {
        return _poissonSolver.PrepareDiffusionMatrix(diffusion, PoissonSolver::Method::PreconCG, 0u);
    }

    void FluidSolver3D::Pimpl::DensityDiffusion(float deltaTime, const Settings& settings)
    {
        if (!_densityDiffusion || _preparedDensityDiffusion != deltaTime * settings._diffusionRate) {
            _preparedDensityDiffusion = deltaTime * settings._diffusionRate;
            _densityDiffusion = BuildDiffusionMethod(_preparedDensityDiffusion);
        }

        auto iterations = _poissonSolver.Solve(
            AsScalarField1D(_density[0]), 
            *_densityDiffusion,
            AsScalarField1D(_density[0]), 
            (PoissonSolver::Method)settings._diffusionMethod);
        Log(Verbose) << "Density diffusion took: (" << iterations << ") iterations." << std::endl;
    }

    void FluidSolver3D::Pimpl::VelocityDiffusion(float deltaTime, const Settings& settings)
    {
        if (!_velocityDiffusion || _preparedVelocityDiffusion != deltaTime * settings._viscosity) {
            _preparedVelocityDiffusion = deltaTime * settings._viscosity;
            _velocityDiffusion = BuildDiffusionMethod(_preparedVelocityDiffusion);
        }

        auto iterationsu = _poissonSolver.Solve(
            AsScalarField1D(_velU[2]), *_velocityDiffusion, AsScalarField1D(_velU[2]), 
            (PoissonSolver::Method)settings._diffusionMethod);
        auto iterationsv = _poissonSolver.Solve(
            AsScalarField1D(_velV[2]), *_velocityDiffusion, AsScalarField1D(_velV[2]), 
            (PoissonSolver::Method)settings._diffusionMethod);
        Log(Verbose) << "Velocity diffusion took: (" << iterationsu << ", " << iterationsv << ") iterations." << std::endl;
    }

    UInt3 FluidSolver3D::GetDimensions() const { return _pimpl->_dimsWithoutBorder; }

    FluidSolver3D::FluidSolver3D(UInt3 dimensions)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_dimsWithoutBorder = dimensions;
        _pimpl->_dimsWithBorder = dimensions + UInt3(2, 2, 2);
        auto N = _pimpl->_dimsWithBorder[0] * _pimpl->_dimsWithBorder[1] * _pimpl->_dimsWithBorder[2];
        _pimpl->_N = N;

        for (unsigned c=0; c<dimof(_pimpl->_velU); ++c) {
            _pimpl->_velU[c] = VectorX(N);
            _pimpl->_velV[c] = VectorX(N);
            _pimpl->_velW[c] = VectorX(N);
            _pimpl->_velU[c].fill(0.f);
            _pimpl->_velV[c].fill(0.f);
            _pimpl->_velW[c].fill(0.f);
        }

        for (unsigned c=0; c<dimof(_pimpl->_density); ++c) {
            _pimpl->_density[c] = VectorX(N);
            _pimpl->_density[c].fill(0.f);
        }

        UInt3 fullDims(dimensions[0]+2, dimensions[1]+2, dimensions[2]+2);
        _pimpl->_poissonSolver = PoissonSolver(3, &fullDims[0]);
        _pimpl->_incompressibility = _pimpl->_poissonSolver.PrepareDivergenceMatrix(
            PoissonSolver::Method::PreconCG, 0u);

        _pimpl->_preparedDensityDiffusion = 0.f;
        _pimpl->_preparedVelocityDiffusion = 0.f;
    }

    FluidSolver3D::~FluidSolver3D() {}

    FluidSolver3D::Settings::Settings()
    {
        _viscosity = 0.05f;
        _diffusionRate = 0.05f;
        _diffusionMethod = 0;
        _advectionMethod = 3;
        _advectionSteps = 4;
        _enforceIncompressibilityMethod = 3;
        _vorticityConfinement = 0.75f;
        _interpolationMethod = 0;
    }


///////////////////////////////////////////////////////////////////////////////////////////////////


}



template<> const ClassAccessors& GetAccessors<SceneEngine::ReferenceFluidSolver2D::Settings>()
{
    using Obj = SceneEngine::ReferenceFluidSolver2D::Settings;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add("DeltaTime", DefaultGet(Obj, _deltaTime),  DefaultSet(Obj, _deltaTime));
        props.Add("Viscosity", DefaultGet(Obj, _viscosity),  DefaultSet(Obj, _viscosity));
        props.Add("DiffusionRate", DefaultGet(Obj, _diffusionRate),  DefaultSet(Obj, _diffusionRate));
        init = true;
    }
    return props;
}

template<> const ClassAccessors& GetAccessors<SceneEngine::FluidSolver2D::Settings>()
{
    using Obj = SceneEngine::FluidSolver2D::Settings;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add("Viscosity", DefaultGet(Obj, _viscosity),  DefaultSet(Obj, _viscosity));
        props.Add("DiffusionRate", DefaultGet(Obj, _diffusionRate),  DefaultSet(Obj, _diffusionRate));
        props.Add("TempDiffusionRate", DefaultGet(Obj, _tempDiffusion),  DefaultSet(Obj, _tempDiffusion));
        props.Add("DiffusionMethod", DefaultGet(Obj, _diffusionMethod),  DefaultSet(Obj, _diffusionMethod));
        props.Add("AdvectionMethod", DefaultGet(Obj, _advectionMethod),  DefaultSet(Obj, _advectionMethod));
        props.Add("AdvectionSteps", DefaultGet(Obj, _advectionSteps),  DefaultSet(Obj, _advectionSteps));
        props.Add("EnforceIncompressibility", DefaultGet(Obj, _enforceIncompressibilityMethod),  DefaultSet(Obj, _enforceIncompressibilityMethod));
        props.Add("BouyancyAlpha", DefaultGet(Obj, _buoyancyAlpha),  DefaultSet(Obj, _buoyancyAlpha));
        props.Add("BouyancyBeta", DefaultGet(Obj, _buoyancyBeta),  DefaultSet(Obj, _buoyancyBeta));
        props.Add("VorticityConfinement", DefaultGet(Obj, _vorticityConfinement),  DefaultSet(Obj, _vorticityConfinement));
        props.Add("InterpolationMethod", DefaultGet(Obj, _interpolationMethod),  DefaultSet(Obj, _interpolationMethod));
        props.Add("BorderX", DefaultGet(Obj, _borderX),  DefaultSet(Obj, _borderX));
        props.Add("BorderY", DefaultGet(Obj, _borderY),  DefaultSet(Obj, _borderY));
        init = true;
    }
    return props;
}

template<> const ClassAccessors& GetAccessors<SceneEngine::FluidSolver3D::Settings>()
{
    using Obj = SceneEngine::FluidSolver3D::Settings;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add("Viscosity", DefaultGet(Obj, _viscosity),  DefaultSet(Obj, _viscosity));
        props.Add("DiffusionRate", DefaultGet(Obj, _diffusionRate),  DefaultSet(Obj, _diffusionRate));
        props.Add("DiffusionMethod", DefaultGet(Obj, _diffusionMethod),  DefaultSet(Obj, _diffusionMethod));

        props.Add("AdvectionMethod", DefaultGet(Obj, _advectionMethod),  DefaultSet(Obj, _advectionMethod));
        props.Add("AdvectionSteps", DefaultGet(Obj, _advectionSteps),  DefaultSet(Obj, _advectionSteps));
        props.Add("InterpolationMethod", DefaultGet(Obj, _interpolationMethod),  DefaultSet(Obj, _interpolationMethod));

        props.Add("EnforceIncompressibility", DefaultGet(Obj, _enforceIncompressibilityMethod),  DefaultSet(Obj, _enforceIncompressibilityMethod));
        props.Add("VorticityConfinement", DefaultGet(Obj, _vorticityConfinement),  DefaultSet(Obj, _vorticityConfinement));
        
        init = true;
    }
    return props;
}




