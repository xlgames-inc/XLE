// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "Fluid.h"
#include "FluidAdvection.h"
#include "../Math/RegularNumberField.h"
#include "../Math/PoissonSolver.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/Meta/ClassAccessorsImpl.h"

extern "C" void dens_step ( int N, float * x, float * x0, float * u, float * v, float diff, float dt );
extern "C" void vel_step ( int N, float * u, float * v, float * u0, float * v0, float visc, float dt );

#pragma warning(disable:4714)
#pragma push_macro("new")
#undef new
#include <Eigen/Dense>
#pragma pop_macro("new")

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

    static void RenderFluidDebugging2D(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode,
        UInt2 dimensions,
        const float* density, const float* velocityU, const float* velocityV, const float* temperature);

    static void RenderFluidDebugging3D(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode,
        UInt3 dimensions,
        const float* density, const float* velocityU, const float* velocityV, const float* temperature);

    void ReferenceFluidSolver2D::RenderDebugging(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode)
    {
        RenderFluidDebugging2D(
            metalContext, parserContext, debuggingMode,
            _pimpl->_dimensions + UInt2(2,2), _pimpl->_density.get(),
            _pimpl->_velU.get(), _pimpl->_velV.get(),
            nullptr);
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

    ReferenceFluidSolver2D::~ReferenceFluidSolver2D()
    {
    }


    ReferenceFluidSolver2D::Settings::Settings()
    {
        _deltaTime = 1.0f/60.f;
        _viscosity = 0.f;
        _diffusionRate = 0.f;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    using VectorX = Eigen::VectorXf;
    using MatrixX = Eigen::MatrixXf;
    using VectorField2D = XLEMath::VectorField2DSeparate<VectorX>;
    using VectorField3D = XLEMath::VectorField3DSeparate<VectorX>;
    using ScalarField2D = XLEMath::ScalarField2D<VectorX>;
    using ScalarField3D = XLEMath::ScalarField3D<VectorX>;

    static ScalarField1D AsScalarField1D(VectorX& v) { return ScalarField1D { v.data(), (unsigned)v.size() }; }


    static std::shared_ptr<PoissonSolver::PreparedMatrix> BuildDiffusionMethod(
        const PoissonSolver& solver, float diffusion, PoissonSolver::Method method,
        unsigned marginFlags, bool wrapEdges)
    {
        return solver.PrepareDiffusionMatrix(diffusion, method, marginFlags, wrapEdges);
    }

    class DiffusionOperation
    {
    public:
        void Execute(
            PoissonSolver& solver, VectorField2D vectorField,
            float diffusionAmount, float deltaTime,
            PoissonSolver::Method method = PoissonSolver::Method::PreconCG, unsigned marginFlags = ~0u, bool wrapEdges = false,
            const char name[] = nullptr);

        void Execute(
            PoissonSolver& solver, ScalarField2D field,
            float diffusionAmount, float deltaTime,
            PoissonSolver::Method method = PoissonSolver::Method::PreconCG,  unsigned marginFlags = ~0u, bool wrapEdges = false,
            const char name[] = nullptr);

        DiffusionOperation();
        ~DiffusionOperation();
    private:
        float _preparedValue;
        unsigned _preparedMarginFlags;
        bool _preparedWrapEdges;
        PoissonSolver::Method _preparedMethod;
        std::shared_ptr<PoissonSolver::PreparedMatrix> _matrix;
    };

    void DiffusionOperation::Execute(
        PoissonSolver& solver, VectorField2D vectorField,
        float diffusionAmount, float deltaTime,
        PoissonSolver::Method method, unsigned marginFlags, bool wrapEdges,
        const char name[])
    {
        if (!diffusionAmount || !deltaTime) return;

        if (    _preparedValue != deltaTime * diffusionAmount || method != _preparedMethod 
            ||  marginFlags != _preparedMarginFlags || wrapEdges != _preparedWrapEdges) {

            _preparedValue = deltaTime * diffusionAmount;
            _preparedMethod = method;
            _preparedMarginFlags = marginFlags;
            _preparedWrapEdges = wrapEdges;
            _matrix = BuildDiffusionMethod(solver, _preparedValue, _preparedMethod, _preparedMarginFlags, _preparedWrapEdges);
        }

        auto iterationsu = solver.Solve(
            AsScalarField1D(*vectorField._u), *_matrix, AsScalarField1D(*vectorField._u), 
            method);
        auto iterationsv = solver.Solve(
            AsScalarField1D(*vectorField._v), *_matrix, AsScalarField1D(*vectorField._v), 
            method);

        if (name)
            LogInfo << name << " diffusion took: (" << iterationsu << ", " << iterationsv << ") iterations.";
    }

    void DiffusionOperation::Execute(
        PoissonSolver& solver, ScalarField2D field,
        float diffusionAmount, float deltaTime,
        PoissonSolver::Method method, unsigned marginFlags, bool wrapEdges, const char name[])
    {
        if (!diffusionAmount || !deltaTime) return;

        if (_preparedValue != deltaTime * diffusionAmount || method != _preparedMethod) {
            _preparedValue = deltaTime * diffusionAmount;
            _preparedMethod = method;
            _preparedMarginFlags = marginFlags;
            _preparedWrapEdges = wrapEdges;
            _matrix = BuildDiffusionMethod(solver, _preparedValue, _preparedMethod, _preparedMarginFlags, _preparedWrapEdges);
        }

        auto iterationsu = solver.Solve(
            AsScalarField1D(*field._u), *_matrix, AsScalarField1D(*field._u), 
            method);

        if (name)
            LogInfo << name << " diffusion took: (" << iterationsu << ") iterations.";
    }

    DiffusionOperation::DiffusionOperation() { _preparedValue = 0.f; _preparedMethod = (PoissonSolver::Method)~0u; }
    DiffusionOperation::~DiffusionOperation() {}

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
        DiffusionOperation _densityDiffusion;
        DiffusionOperation _velocityDiffusion;
        DiffusionOperation _temperatureDiffusion;
        std::shared_ptr<PoissonSolver::PreparedMatrix> _incompressibility;

        unsigned _incompMarginFlags;
        bool _incompWrapEdges;

        void VorticityConfinement(VectorField2D outputField, VectorField2D inputVelocities, float strength, float deltaTime);
    };

    static void EnforceIncompressibility(
        VectorField2D velField,
        const PoissonSolver& solver, const PoissonSolver::PreparedMatrix& A,
        PoissonSolver::Method method, unsigned marginFlags, bool wrapEdges)
    {
        //
        // Following Jos Stam's stable fluids, we'll use Helmholtz-Hodge Decomposition
        // to build a projection operator that will force the velocity field to have
        // zero divergence.
        //
        // This is important for meeting the restrictions from the Naver Stokes equations.
        // 
        // For our input vector field, "w", we can decompose it into two parts:
        //      w = u + del . q        (1)
        // where "u" has zero-divergence (and is our output field). The scalar field,
        // "q" is considered error, and just dropped.
        //
        // We find "q" by multiplying equation (1) by del on both sides, and we get:
        //      del . w = del^2 . q    (2)     (given that del . u is zero)
        //
        // and "u" = w - del . q
        //
        // "b = del^2 . q" is the possion equation; and can be solved in the same way
        // we solve for diffusion (though, the matrix "A" is slightly different). 
        //
        // Following Stam's sample code, we'll do this for both u and v at the same time,
        // with the same solution for "q".
        //
        // Also, here to have to consider how we define the discrete divergence of the
        // field. Stam uses .5f * (f[i+1] - f[i-1]). Depending on how we arrange the physical
        // values with respect to the grid, we could alternatively consider f[i] - f[i-1].
        //

        //
        //  Note -- there may be some benefit in doing this step with a multigrid-like approach
        //          that is, we could solve for incompressibility on a resamples low resolution
        //          grid first; to remove low frequency divergence first... And then successively
        //          use higher resolution grids to remove higher frequency divergence. It might
        //          improve large scale details a bit.
        //

        const auto dims = velField.Dimensions();
        VectorX delW(dims[0] * dims[1]), q(dims[0] * dims[1]);
        q.fill(0.f);
        UInt2 border(1,1);
        if (!(marginFlags & 1<<0)) border[0] = 0u;
        if (!(marginFlags & 1<<1)) border[1] = 0u;
        auto velFieldScale = Float2(float(dims[0]-2*border[0]), float(dims[1]-2*border[1]));

            // note -- default to wrapping on borders without a margin
        for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                const auto i = y*dims[0]+x;
                const auto i0 = y*dims[0]+(x+1)%dims[0];
                const auto i1 = y*dims[0]+(x+dims[0]-1)%dims[0];
                const auto i2 = ((y+1)%dims[1])*dims[0]+x;
                const auto i3 = ((y+dims[1]-1)%dims[1])*dims[0]+x;
                delW[i] = 
                    -0.5f * 
                    (
                            ((*velField._u)[i0] - (*velField._u)[i1]) / velFieldScale[0]
                        + ((*velField._v)[i2] - (*velField._v)[i3]) / velFieldScale[1]
                    );
            }
        SmearBorder2D(delW, dims, marginFlags);

        auto iterations = solver.Solve(
            AsScalarField1D(q), A, AsScalarField1D(delW), 
            method);

        SmearBorder2D(q, dims, marginFlags);
        for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                const auto i = y*dims[0]+x;
                const auto i0 = y*dims[0]+(x+1)%dims[0];
                const auto i1 = y*dims[0]+(x+dims[0]-1)%dims[0];
                const auto i2 = ((y+1)%dims[1])*dims[0]+x;
                const auto i3 = ((y+dims[1]-1)%dims[1])*dims[0]+x;
                (*velField._u)[i] -= .5f*velFieldScale[0] * (q[i0] - q[i1]);
                (*velField._v)[i] -= .5f*velFieldScale[1] * (q[i2] - q[i3]);
            }

        LogInfo << "EnforceIncompressibility took: " << iterations << " iterations.";
    }

    static void EnforceIncompressibility(
        VectorField3D velField,
        const PoissonSolver& solver, const PoissonSolver::PreparedMatrix& A,
        PoissonSolver::Method method)
    {
        const auto dims = velField.Dimensions();
        VectorX delW(dims[0] * dims[1] * dims[2]), q(dims[0] * dims[1] * dims[2]);
        q.fill(0.f);
        const UInt3 border(1,1,1);
        auto velFieldScale = Float3(float(dims[0]-2*border[0]), float(dims[1]-2*border[1]), float(dims[2]-2*border[2]));
        for (unsigned z=border[2]; z<dims[2]-border[2]; ++z)
            for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
                for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                    const auto i = (z*dims[1]+y)*dims[0]+x;
                    delW[i] = 
                        -0.5f * 
                        (
                              ((*velField._u)[i+1]               - (*velField._u)[i-1]) / velFieldScale[0]
                            + ((*velField._v)[i+dims[0]]         - (*velField._v)[i-dims[0]]) / velFieldScale[1]
                            + ((*velField._w)[i+dims[0]*dims[1]] - (*velField._w)[i-dims[0]*dims[1]])  / velFieldScale[2]
                        );
                }

        SmearBorder3D(delW, dims);
        auto iterations = solver.Solve(
            AsScalarField1D(q), A, AsScalarField1D(delW), 
            method);
        SmearBorder3D(q, dims);

        for (unsigned z=border[2]; z<dims[2]-border[2]; ++z)
            for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
                for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                    const auto i = (z*dims[1]+y)*dims[0]+x;
                    (*velField._u)[i] -= .5f*velFieldScale[0] * (q[i+1]                 - q[i-1]);
                    (*velField._v)[i] -= .5f*velFieldScale[1] * (q[i+dims[0]]           - q[i-dims[0]]);
                    (*velField._w)[i] -= .5f*velFieldScale[2] * (q[i+dims[0]*dims[1]]   - q[i-dims[0]*dims[1]]);
                }

        LogInfo << "EnforceIncompressibility took: " << iterations << " iterations.";
    }

    void FluidSolver2D::Pimpl::VorticityConfinement(
        VectorField2D outputField,
        VectorField2D inputVelocities, float strength, float deltaTime)
    {
        //
        // VorticityConfinement amplifies the existing vorticity at each cell.
        // This is intended to add back errors caused by the discrete equations
        // we're using here.
        //
        // The vorticity can be calculated from the velocity field (by taking the
        // cross product with del. In 2D, this produces a scalar value (which is
        // conceptually a vector in the direction of an imaginary Z axis). We also
        // need to find the divergence of this scalar field.
        //
        // See http://web.stanford.edu/class/cs237d/smoke.pdf for details. In that
        // paper, Fedkiw calculates the vorticity at a half-cell offset from the 
        // velocity field. It's not clear why that was done. We will ignore that, 
        // and calculate vorticity exactly on the velocity field.
        //
        // Note --  like EnforceIncompressibility, it might be helpful to do take a
        //          multigrid-like approach for this step. That is, here we're emphasising
        //          very high frequency vorticity features (because these are the features
        //          that are most likely to be lost in the approximations of the model).
        //          But, by using grids of different resolutions, it might be interesting
        //          to emphasise some higher level vorticity features, as well.
        //

            // todo -- support margins fully!
        const auto dims = inputVelocities.Dimensions();
        VectorX vorticity(dims[0]*dims[1]);
        const UInt2 border(1,1);
        for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                auto dvydx = .5f * inputVelocities.Load(UInt2(x+1, y))[1] - inputVelocities.Load(UInt2(x-1, y))[1];
                auto dvxdy = .5f * inputVelocities.Load(UInt2(x, y+1))[0] - inputVelocities.Load(UInt2(x, y-1))[0];
                vorticity[y*dims[0]+x] = dvydx - dvxdy;
            }
        SmearBorder2D(vorticity, dims, ~0u);

        Float2 velFieldScale = deltaTime * strength * Float2(float(dims[0]-2*border[0]), float(dims[1]-2*border[1]));
        for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                    // find the discrete divergence of the absolute vorticity field
                const auto i = y*dims[0]+x;
                Float2 div(
                        .5f * (XlAbs(vorticity[i+1]) - XlAbs(vorticity[i-1])),
                        .5f * (XlAbs(vorticity[i+dims[0]]) - XlAbs(vorticity[i-dims[0]]))
                    );

                float magSq = MagnitudeSquared(div);
                if (magSq > 1e-10f) {
                    div *= XlRSqrt(magSq);

                        // in 2D, the vorticity is in the Z direction. Which means the cross product
                        // with our divergence vector is simple
                    float omega = vorticity[i];
                    auto additionalVel = MultiplyAcross(velFieldScale, Float2(div[1] * omega, -div[0] * omega));
                    outputField.Write(
                        UInt2(x, y),
                        outputField.Load(UInt2(x, y)) + additionalVel);
                }
            }
    }

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

        _pimpl->VorticityConfinement(
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

        unsigned marginFlags = 0;
        marginFlags |= (1<<0) * (settings._borderX == (int)AdvectionBorder::Margin);
        marginFlags |= (1<<1) * (settings._borderY == (int)AdvectionBorder::Margin);
        bool wrapEdges = (settings._borderX == (int)AdvectionBorder::Wrap) || (settings._borderY == (int)AdvectionBorder::Wrap);

        if (marginFlags != _pimpl->_incompMarginFlags || wrapEdges != _pimpl->_incompWrapEdges) {
            _pimpl->_incompMarginFlags = marginFlags;
            _pimpl->_incompWrapEdges = wrapEdges;
            _pimpl->_incompressibility = _pimpl->_poissonSolver.PrepareDivergenceMatrix(
                PoissonSolver::Method::PreconCG, _pimpl->_incompMarginFlags, _pimpl->_incompWrapEdges);
        }

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
            marginFlags, wrapEdges, "Velocity");

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
        EnforceIncompressibility(
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            _pimpl->_poissonSolver, *_pimpl->_incompressibility,
            (PoissonSolver::Method)settings._enforceIncompressibilityMethod,
            marginFlags, wrapEdges);

        SmearBorder2D(densityWorking, _pimpl->_dimsWithBorder, marginFlags);
        _pimpl->_densityDiffusion.Execute(
            _pimpl->_poissonSolver, 
            ScalarField2D(&densityWorking, _pimpl->_dimsWithBorder),
            settings._diffusionRate, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, 
            marginFlags, wrapEdges, "Density");
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
            marginFlags, wrapEdges, "Temperature");
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
        RenderFluidDebugging2D(
            metalContext, parserContext, debuggingMode,
            _pimpl->_dimsWithBorder, _pimpl->_density[1].data(),
            _pimpl->_velU[1].data(), _pimpl->_velV[1].data(),
            _pimpl->_temperature[1].data());
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
        _pimpl->_incompMarginFlags = ~0u;
        _pimpl->_incompWrapEdges = false;
    }

    FluidSolver2D::~FluidSolver2D(){}

    FluidSolver2D::Settings::Settings()
    {
        _viscosity = 0.05f;
        _diffusionRate = 0.05f;
        _tempDiffusion = 2.f;
        _diffusionMethod = 0;
        _advectionMethod = 3;
        _advectionSteps = 4;
        _enforceIncompressibilityMethod = 0;
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
        RenderFluidDebugging3D(
            metalContext, parserContext, debuggingMode,
            _pimpl->_dimsWithBorder, _pimpl->_density[1].data(),
            _pimpl->_velU[1].data(), _pimpl->_velV[1].data(),
            nullptr);
    }

    std::shared_ptr<PoissonSolver::PreparedMatrix> FluidSolver3D::Pimpl::BuildDiffusionMethod(float diffusion)
    {
        return _poissonSolver.PrepareDiffusionMatrix(diffusion, PoissonSolver::Method::PreconCG, ~0u, false);
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
        LogInfo << "Density diffusion took: (" << iterations << ") iterations.";
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
        LogInfo << "Velocity diffusion took: (" << iterationsu << ", " << iterationsv << ") iterations.";
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
            PoissonSolver::Method::PreconCG, ~0u, false);

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

    class CloudsForm2D::Pimpl
    {
    public:
        VectorX _velU[3];
        VectorX _velV[3];

        VectorX _vaporMixingRatio[2];      // qv
        VectorX _condensedMixingRatio[2];   // qc
        VectorX _potentialTemperature[2];   // theta

        UInt2 _dimsWithoutBorder;
        UInt2 _dimsWithBorder;
        unsigned _N;

        PoissonSolver _poissonSolver;
        std::shared_ptr<PoissonSolver::PreparedMatrix> _incompressibility;

        DiffusionOperation _velocityDiffusion;
        DiffusionOperation _vaporDiffusion;
        DiffusionOperation _condensedDiffusion;
        DiffusionOperation _temperatureDiffusion;
    };

    static float PressureAtAltitude(float kilometers)
    {
            //
            //  Returns a value in pascals.
            //  For a given altitude, we can calculate the standard
            //  atmospheric pressure.
            //
            //  Actually the "wetness" of the air should affect the lapse
            //  rate... But we'll ignore that here, and assume all of the 
            //  air is dry for this calculation.
            //
            //  We could build in a humidity constant into the simulation.
            //  This should adjust the lapse rate. Also the T0 temperature
            //  values should be adjustable.
            //
            //  See here for more information:
            //      https://en.wikipedia.org/wiki/Atmospheric_pressure
            //
        const float g = 9.81f / 1000.f; // (in km/second)
        const float p0 = 101325.f;      // (in pascals, 1 standard atmosphere)
        const float T0 = 295.f;         // (in kelvin, base temperature) (about 20 degrees)
        const float Rd = 287.058f;      // (in J . kg^-1 . K^-1. This is the "specific" gas constant for dry air. It is R / M, where R is the gas constant, and M is the ideal gas constant)

            //  We have a few choices for the lape rate here.
            //      we're could use a value close to the "dry adiabatic lapse rate" (around 9.8 kelvin/km)
            //      Or we could use a value around 6 or 7 -- this is the average lapse rate in the troposphere
            //  see https://en.wikipedia.org/wiki/Lapse_rate
            //  The minimum value for lapse rate in the troposhere should be around 4
            //      (see http://www.iac.ethz.ch/edu/courses/bachelor/vertiefung/atmospheric_physics/Slides_2012/buoyancy.pdf)
        const float tempLapseRate = 6.5f;

        // roughly: p0 * std::exp(kilometers * g / (1000.f * T0 * Rd));     (see wikipedia page)
        // see also the "hypsometric equation" equation, similar to above
        return p0 * std::pow(1.f - kilometers * tempLapseRate / T0, g / (tempLapseRate * Rd));
    }

    static float ExnerFunction(float pressure)
    {

            //
            //  The potential temperature is defined based on the atmosphere pressure.
            //  So, we can go backwards as well and get the temperature from the potential
            //  temperature (if we know the pressure)
            //
            //  Note that if "pressure" comes from PressureAtAltitude, we will be 
            //  multiplying by p0, and then dividing it away again.
            //
            //  The Exner function is ratio of the potential temperature and absolute temperature.
            //  So,
            //      temperature = ExnerFunction * potentialTemperature
            //      potentialTemperature = temperature / ExnerFunction
            //

        const float p0 = 101325.f;  // (in pascals, 1 standard atmosphere)
        const float Rd = 287.058f;  // in J . kg^-1 . K^-1. Gas constant for dry air
        const float cp = 1005.f;    // in J . kg^-1 . K^-1. heat capacity of dry air at constant pressure
        const float kappa = Rd/cp;
        return std::pow(pressure/p0, kappa);
    }

    static float KelvinToCelsius(float kelvin) { return kelvin - 273.15f; }
    static float CelsiusToKelvin(float celius) { return celius + 273.15f; }

    void CloudsForm2D::Tick(float deltaTime, const Settings& settings)
    {
        float dt = deltaTime;
        const auto N = _pimpl->_N;
        const auto dims = _pimpl->_dimsWithBorder;

        auto& velUT0 = _pimpl->_velU[0];
        auto& velUT1 = _pimpl->_velU[1];
        auto& velUSrc = _pimpl->_velU[2];
        auto& velUWorking = _pimpl->_velU[2];

        auto& velVT0 = _pimpl->_velV[0];
        auto& velVT1 = _pimpl->_velV[1];
        auto& velVSrc = _pimpl->_velV[2];
        auto& velVWorking = _pimpl->_velV[2];

        auto& potTempT1 = _pimpl->_potentialTemperature[1];
        auto& potTempWorking = _pimpl->_potentialTemperature[0];
        auto& qvT1 = _pimpl->_vaporMixingRatio[1];
        auto& qvSrc = _pimpl->_vaporMixingRatio[0];
        auto& qvWorking = _pimpl->_vaporMixingRatio[0];
        auto& qcT1 = _pimpl->_condensedMixingRatio[1];
        auto& qcWorking = _pimpl->_condensedMixingRatio[0];

            //
            // Following Mark Jason Harris' PhD dissertation,
            // we will simulate condensation and buoyancy of
            // water vapour in the atmosphere.  
            //
            // We use a lot of "mixing ratios" here. The mixing ratio
            // is the ratio of a component in a mixture, relative to all
            // other components. see:
            //  https://en.wikipedia.org/wiki/Mixing_ratio
            //
            // So, if a single component makes up the entirity of a mixture,
            // then the mixing ratio convergences on infinity.
            //

            // Vorticity confinement (additional) force
        // _pimpl->VorticityConfinement(
        //     VectorField2D(&velUSrc, &velVSrc, _pimpl->_dimsWithBorder),
        //     VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),           // last frame results
        //     settings._vorticityConfinement, deltaTime);

            // The strength of buoyancy is proportional to the reciprocal of
            // the referenceVirtualPotentialTemperature. So we can just the 
            // amount of bouyancy by changing this number.
        const auto g = 9.81f / 1000.f;  // (in km/second)
        const auto ambientTemperature = CelsiusToKelvin(23.f);

        const auto zScale = 2.f / float(dims[1]);    // simulating 2 km of atmosphere

            // Buoyancy force
        const UInt2 border(1,1);
        for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {
                
                    //
                    // As described by Harris, we will ignore the effect of local
                    // pressure changes, and use his equation (2.10)
                    //
                const auto i = y*dims[0]+x;
                auto potentialTemp = potTempT1[i];
                auto vapourMixingRatio = qvT1[i];
                auto condensationMixingRatio = qcT1[i];

                    //
                    // In atmosphere thermodynamics, the "virtual temperature" of a parcel of air
                    // is a concept that allows us to simplify some equations. Given a "moist" packet
                    // of air -- that is, a packet with some water vapor -- it should behave the same
                    // as a dry packet of air at some temperature.
                    // That is what the virtual temperature is -- the temperature of a dry packet of air
                    // that would behave the same the given moist packet.
                    //
                    // It seems that the virtual temperature, for realistic vapour mixing ratios, the
                    // virtual temperature is close to linear against the vapour mixing ratio. So we can
                    // use a simple equation to find it.
                    //
                    //  see also -- https://en.wikipedia.org/wiki/Virtual_temperature
                    //
                    // The "potential temperature" of a parcel is proportional to the "temperature"
                    // of that parcel.
                    // and, since
                    //  virtual temperature ~= T . (1 + 0.61qv)
                    // we can apply the same equation to the potential temperature.
                    //
                auto virtualPotentialTemp = 
                    potentialTemp * (1.f + 0.61f * vapourMixingRatio);

                auto altitudeKm = float(y) * zScale;
                auto pressure = PressureAtAltitude(altitudeKm);     // (precalculate these pressures)
                auto exner = ExnerFunction(pressure);
                const auto referenceVirtualPotentialTemperature = 295.f / exner;

                    //
                    // As per Harris, we use the condenstation mixing ratio for the "hydrometeors" mixing
                    // ratio here. Note that this final equation is similar to our simple smoke buoyancy
                    //  --  the force up is linear against temperatures and density of particles 
                    //      in the air
                    // We need to scale the velocity according the scaling system of our grid. In the CFD
                    // system, coordinates are in grid units. 
                    // 
                // auto B = 
                //     g * (virtualPotentialTemp / referenceVirtualPotentialTemperature - condensationMixingRatio);
                // B /= zScale;
                // 
                //     // B is now our buoyant force per unit mass -- so it is just our acceleration.
                // velVSrc[i] -= B;
                (void)virtualPotentialTemp; (void)referenceVirtualPotentialTemperature; (void)condensationMixingRatio;

                const auto temp = potentialTemp * exner;
                auto virtualTemperature = temp * (1.f + vapourMixingRatio);
                auto temperatureBuoyancy = (virtualTemperature - ambientTemperature) / ambientTemperature;
                auto B = g * (temperatureBuoyancy * settings._buoyancyAlpha - condensationMixingRatio * settings._buoyancyBeta);
                velVSrc[i] += B / zScale;
            }

        for (unsigned c=0; c<N; ++c) {
            velUT0[c] = velUT1[c];
            velVT0[c] = velVT1[c];
            velUWorking[c] = velUT1[c] + dt * velUSrc[c];
            velVWorking[c] = velVT1[c] + dt * velVSrc[c];

            qcWorking[c] = qcT1[c]; //  + dt * qcSrc[c];
            qvWorking[c] = qvT1[c] + dt * qvSrc[c];
            potTempWorking[c] = potTempT1[c]; // + dt * temperatureSrc[c];
        }

        _pimpl->_velocityDiffusion.Execute(
            _pimpl->_poissonSolver,
            VectorField2D(&velUWorking, &velVWorking, _pimpl->_dimsWithBorder),
            settings._viscosity, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, ~0u, false, "Velocity");

        AdvectionSettings advSettings {
            (AdvectionMethod)settings._advectionMethod, 
            (AdvectionInterp)settings._interpolationMethod, settings._advectionSteps,
            AdvectionBorder::Margin, AdvectionBorder::Margin, AdvectionBorder::Margin
        };
        PerformAdvection(
            VectorField2D(&velUT1,      &velVT1,        _pimpl->_dimsWithBorder),
            VectorField2D(&velUWorking, &velVWorking,   _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0,      &velVT0,        _pimpl->_dimsWithBorder),
            VectorField2D(&velUWorking, &velVWorking,   _pimpl->_dimsWithBorder),
            deltaTime, advSettings);
        
        ReflectUBorder2D(velUT1, _pimpl->_dimsWithBorder, ~0u);
        ReflectVBorder2D(velVT1, _pimpl->_dimsWithBorder, ~0u);
        EnforceIncompressibility(
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            _pimpl->_poissonSolver, *_pimpl->_incompressibility,
            (PoissonSolver::Method)settings._enforceIncompressibilityMethod,
            ~0u, false);

            // note -- advection of all 3 of these properties should be very
            // similar, since the velocity field doesn't change. Rather that 
            // performing the advection multiple times, we could just do it
            // once and reuse the result for each.
            // Actually, we could even use the same advection result we got
            // while advecting velocity -- it's unclear how that would change
            // the result (especially since that advection happens before we
            // enforce incompressibility).
        _pimpl->_condensedDiffusion.Execute(
            _pimpl->_poissonSolver,
            ScalarField2D(&qcWorking, _pimpl->_dimsWithBorder),
            settings._condensedDiffusionRate, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, ~0u, false, "Condensed");
        PerformAdvection(
            ScalarField2D(&qcT1, _pimpl->_dimsWithBorder),
            ScalarField2D(&qcWorking, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0, &velVT0, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            deltaTime, advSettings);

        _pimpl->_vaporDiffusion.Execute(
            _pimpl->_poissonSolver,
            ScalarField2D(&qvWorking, _pimpl->_dimsWithBorder),
            settings._vaporDiffusionRate, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, ~0u, false, "Vapor");
        PerformAdvection(
            ScalarField2D(&qvT1, _pimpl->_dimsWithBorder),
            ScalarField2D(&qvWorking, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0, &velVT0, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            deltaTime, advSettings);

        _pimpl->_temperatureDiffusion.Execute(
            _pimpl->_poissonSolver,
            ScalarField2D(&potTempWorking, _pimpl->_dimsWithBorder),
            settings._temperatureDiffusionRate, deltaTime, (PoissonSolver::Method)settings._diffusionMethod, ~0u, false, "Temperature");
        PerformAdvection(
            ScalarField2D(&potTempT1, _pimpl->_dimsWithBorder),
            ScalarField2D(&potTempWorking, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT0, &velVT0, _pimpl->_dimsWithBorder),
            VectorField2D(&velUT1, &velVT1, _pimpl->_dimsWithBorder),
            deltaTime, advSettings);

            // Perform condenstation after advection
            // Does it matter much if we do this before or after advection?
        for (unsigned y=border[1]; y<dims[1]-border[1]; ++y)
            for (unsigned x=border[0]; x<dims[0]-border[0]; ++x) {

                    // When the water vapour mixing ratio exceeds a certain point, condensation
                    // may occur. This point is called the saturation point. It depends on
                    // the temperature of the parcel.

                const auto i = y*dims[0]+x;
                auto& potentialTemp = potTempT1[i];
                auto& vapourMixingRatio = qvT1[i];
                auto& condensationMixingRatio = qcT1[i];

                    // There are a lot of constants here... Maybe there is a better
                    // way to express this equation?

                auto altitudeKm = float(y) * zScale;
                auto pressure = PressureAtAltitude(altitudeKm);     // (precalculate these pressures)
                auto exner = ExnerFunction(pressure);
                auto T = KelvinToCelsius(potentialTemp * exner);

                    // We can calculate the partial pressure for water vapor at saturation point.
                    // Note that this pressure value is much smaller than the pressure value
                    // calculated from PressureAtAltitude because it is a "partial" pressure.
                    // It is the pressure for the vapor part only.
                    //
                    // (Here the pressure is calculated assuming all air is dry air, 
                    // but it's not clear if that has a big impact). At this saturation
                    // point, condensation can start to occur. And condensation is what we're
                    // interested in!
                    //
                    // It would be useful if we could find a simplier relationship between
                    // this value and potentialTemp -- by precalculating any terms that are
                    // only dependent on the pressure (which is constant with altitude).
                    //
                    // See https://en.wikipedia.org/wiki/Clausius%E2%80%93Clapeyron_relation
                    // This is called the August-Roche-Magnus formula.
                    //
                    // Note that this is very slightly different from the Harris' paper.
                    // I'm using the equation from wikipedia, which gives a result in hectopascals.
                    //
                    // See also http://www.vaisala.com/Vaisala%20Documents/Application%20notes/Humidity_Conversion_Formulas_B210973EN-F.pdf
                    // ("HUMIDITY CONVERSION FORMULAS") for an alternative formula.
                    //
                    //      (in pascals)
                auto saturationPressure = 100.f * 6.1094f * XlExp((17.625f * T) / (T + 243.04f));

                    // We can use this to calculate the equilibrium point for the vapour mixing
                    // ratio. 
                    // However -- this might be a little inaccurate because it relies on our
                    // calculation of the pressure from PressureAtAltitude. And that doesn't
                    // take into account the humidity (which might be changing as a result of
                    // the condensating occuring?)
                    
                    //      The following is derived from
                    //      ws  = es / (p - es) . Rd/Rv
                    //          = (-p / (es-p) - 1) . Rd/Rv
                    //          = c.(p / (p-es) - 1), where c = Rd/Rv ~= 0.622
                    //      (see http://www.geog.ucsb.edu/~joel/g266_s10/lecture_notes/chapt03/oh10_3_01/oh10_3_01.html)
                    //      It seems that a common approximation is just to ignore the es in
                    //      the denominator of the first equation (since it should be small compared to total
                    //      pressure). But that seems like a minor optimisation? Why not use the full equation?

                const auto Rd = 287.058f;  // in J . kg^-1 . K^-1. Gas constant for dry air
                const auto Rv = 461.495f;  // in J . kg^-1 . K^-1. Gas constant for water vapor
                const auto gasConstantRatio = Rd/Rv;   // ~0.622f;
                float equilibriumMixingRatio = 
                    gasConstantRatio * (pressure / (pressure-saturationPressure) - 1.f);

                    // Once we know our mixing ratio is above the equilibrium point -- how quickly should we
                    // get condensation? Delta time should be a factor here, but the integration isn't very
                    // accurate.
                    // Adjusting mixing ratios like this seems awkward. But I guess that the values tracked
                    // should generally be small relative to the total mixture (ie, ratios should be much
                    // smaller than 1.f). Otherwise changing one ratio effectively changes the meaning of
                    // the other (given that they are ratios against all other substances in the mixture).
                    // But, then again, in this simple model the condensationMixingRatio doesn't effect the 
                    // equilibriumMixingRatio equation. Only the change in temperature (which is adjusted 
                    // here) effects the equilibriumMixingRatio.

                auto difference = vapourMixingRatio - equilibriumMixingRatio;
                auto deltaCondensation = std::min(1.f, deltaTime * settings._condensationSpeed) * difference;
                deltaCondensation = std::max(deltaCondensation, 0.f); // -condensationMixingRatio);
                vapourMixingRatio -= deltaCondensation;
                condensationMixingRatio += deltaCondensation;

                    // Delta condensation should effect the temperature, as well
                    // When water vapour condenses, it releases its latent heat.
                    // Note that the change in temperature will change the equilibrium
                    // mixing ratio (for the next update).

                    // Note -- "latentHeatOfVaporization" value comes from Harris' paper.
                    //          But we should check this. It appears that the units may be
                    //          out in that paper. It maybe should be 2.5 x 10^6, or thereabouts?

                const auto latentHeatOfVaporization = 2260.f * 1000.f;  // in J . kg^-1 for water at 0 degrees celsius
                const auto cp = 1005.f;                                 // in J . kg^-1 . K^-1. heat capacity of dry air at constant pressure
                auto deltaPotTemp = latentHeatOfVaporization / (cp * exner) * deltaCondensation;
                potentialTemp += deltaPotTemp * settings._temperatureChangeSpeed;
            }

        for (unsigned c=0; c<N; ++c) {
            velUSrc[c] = 0.f;
            velVSrc[c] = 0.f;
            qvSrc[c] = 0.f;
        }
    }

    void CloudsForm2D::AddVapor(UInt2 coords, float amount)
    {
        if (coords[0] < _pimpl->_dimsWithoutBorder[0] && coords[1] < _pimpl->_dimsWithoutBorder[1]) {
            unsigned i = (coords[0]+1) + (coords[1]+1) * _pimpl->_dimsWithBorder[0];
            _pimpl->_vaporMixingRatio[0][i] += amount;
        }
    }

    void CloudsForm2D::AddVelocity(UInt2 coords, Float2 vel)
    {
        if (coords[0] < _pimpl->_dimsWithoutBorder[0] && coords[1] < _pimpl->_dimsWithoutBorder[1]) {
            unsigned i = (coords[0]+1) + (coords[1]+1) * _pimpl->_dimsWithBorder[0];
            _pimpl->_velU[2][i] += vel[0];
            _pimpl->_velV[2][i] += vel[1];
        }
    }

    void CloudsForm2D::RenderDebugging(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode)
    {
        RenderFluidDebugging2D(
            metalContext, parserContext, debuggingMode,
            _pimpl->_dimsWithBorder, 
            _pimpl->_condensedMixingRatio[1].data(),
            // _pimpl->_vaporMixingRatio[1].data(),
            _pimpl->_velU[1].data(), _pimpl->_velV[1].data(),
            _pimpl->_potentialTemperature[1].data());
    }

    UInt2 CloudsForm2D::GetDimensions() const { return _pimpl->_dimsWithBorder; }

    CloudsForm2D::CloudsForm2D(UInt2 dimensions)
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

        for (unsigned c=0; c<dimof(_pimpl->_vaporMixingRatio); ++c) {
                // It should be ok to start with constant (or near constant) for potential temperature
                // this means that everything is adiabatic -- and that the temperature varies
                // with altitude in a standard manner.
                // Note that it might be better to start with a noise field -- just so we
                // get some initial randomness into the simulation.
            const float airTemp = CelsiusToKelvin(23.f);
            _pimpl->_potentialTemperature[c] = VectorX(N);
            _pimpl->_potentialTemperature[c].fill(airTemp);

                // we should start with some vapor in the atmosphere. The
                // amount depends on the humidity of the atmosphere.
                // The "specific humidity" is the ratio of the vapor mass to 
                // total mass -- and this is approximate equal to the vapor
                // mixing ratio (because the ratio is very small).
                // So, we can use formulas for specific humidity to calculate a
                // starting point.
            const auto relativeHumidity = .75f; // 75%

            _pimpl->_vaporMixingRatio[c] = VectorX(N);
            const auto& dims = _pimpl->_dimsWithBorder;
            for (unsigned y=0; y<dims[1]; ++y)
                for (unsigned x=0; x<dims[0]; ++x) {
                    unsigned i = x*dims[0]+y;
                    auto potentialTemp = _pimpl->_potentialTemperature[c][i];
                    auto altitudeKm = 2.f * float(y)/float(dims[1]);    // simulating 2 km of atmosphere
                    auto pressure = PressureAtAltitude(altitudeKm);     // (precalculate these pressures)
                    auto exner = ExnerFunction(pressure);
                    auto T = KelvinToCelsius(potentialTemp * exner);
                    auto saturationPressure = 100.f * 6.1094f * XlExp((17.625f * T) / (T + 243.04f));
                    const auto Rd = 287.058f;  // in J . kg^-1 . K^-1. Gas constant for dry air
                    const auto Rv = 461.495f;  // in J . kg^-1 . K^-1. Gas constant for water vapor
                    const auto gasConstantRatio = Rd/Rv;   // ~0.622f;
                    auto equilibriumMixingRatio = 
                        gasConstantRatio * (pressure / (pressure-saturationPressure) - 1.f);
                    // RH = vaporMixingRatio/saturationMixingRatio
                    _pimpl->_vaporMixingRatio[c][i] = relativeHumidity * equilibriumMixingRatio;
                }

                // Starting with zero condensation... But we could initialise
                // with with a noise field; just to get started.
            _pimpl->_condensedMixingRatio[c] = VectorX(N);
            _pimpl->_condensedMixingRatio[c].fill(0.f);
        }

        UInt2 fullDims(dimensions[0]+2, dimensions[1]+2);
        _pimpl->_poissonSolver = PoissonSolver(2, &fullDims[0]);
        _pimpl->_incompressibility = _pimpl->_poissonSolver.PrepareDivergenceMatrix(PoissonSolver::Method::PreconCG, ~0u, false);
    }

    CloudsForm2D::~CloudsForm2D(){}

    CloudsForm2D::Settings::Settings()
    {
        _viscosity = 0.05f;
        _condensedDiffusionRate = 0.f;
        _vaporDiffusionRate = 0.f;
        _temperatureDiffusionRate = 2.f;
        _diffusionMethod = 0;
        _advectionMethod = 3;
        _advectionSteps = 4;
        _enforceIncompressibilityMethod = 0;
        _vorticityConfinement = 0.75f;
        _interpolationMethod = 0;
        _buoyancyAlpha = 1.f;
        _buoyancyBeta = 1.f;
        _condensationSpeed = 60.f;
        _temperatureChangeSpeed = .25f;
    }

}

    // for draw debugging...
#include "SceneEngineUtils.h"
#include "LightingParserContext.h"
#include "../BufferUploads/IBufferUploads.h"
#include "../BufferUploads/DataPacket.h"
#include "../BufferUploads/ResourceLocator.h"
#include "../RenderCore/Metal/DeviceContext.h"
#include "../RenderCore/Metal/Buffer.h"
#include "../RenderCore/Metal/Shader.h"
#include "../RenderCore/Metal/InputLayout.h"
#include "../RenderCore/Metal/ShaderResource.h"
#include "../RenderCore/Techniques/CommonResources.h"
#include "../RenderCore/Techniques/Techniques.h"

namespace SceneEngine
{
    static void RenderFluidDebugging2D(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode,
        UInt2 dimensions,
        const float* density, const float* velocityU, const float* velocityV, const float* temperature)
    {
        TRY {
            using namespace RenderCore;
            using namespace BufferUploads;
            auto& uploads = GetBufferUploads();

            auto dx = dimensions[0], dy = dimensions[1];

            auto desc = CreateDesc(
                BindFlag::ShaderResource,
                0, GPUAccess::Read|GPUAccess::Write,
                TextureDesc::Plain2D(dx, dy, RenderCore::Metal::NativeFormat::R32_FLOAT),
                "fluid");
            auto densityPkt = CreateBasicPacket((dx)*(dy)*sizeof(float), density, TexturePitches((dx)*sizeof(float), (dy)*(dx)*sizeof(float)));
            auto velUPkt = CreateBasicPacket((dx)*(dy)*sizeof(float), velocityU, TexturePitches((dx)*sizeof(float), (dy)*(dx)*sizeof(float)));
            auto velVPkt = CreateBasicPacket((dx)*(dy)*sizeof(float), velocityV, TexturePitches((dx)*sizeof(float), (dy)*(dx)*sizeof(float)));
            auto temperaturePkt = CreateBasicPacket((dx)*(dy)*sizeof(float), temperature, TexturePitches((dx)*sizeof(float), (dy)*(dx)*sizeof(float)));

            auto density = uploads.Transaction_Immediate(desc, densityPkt.get());
            auto velU = uploads.Transaction_Immediate(desc, velUPkt.get());
            auto velV = uploads.Transaction_Immediate(desc, velVPkt.get());
            auto temperature = uploads.Transaction_Immediate(desc, temperaturePkt.get());

            metalContext.BindPS(
                MakeResourceList(
                    Metal::ShaderResourceView(density->GetUnderlying()),
                    Metal::ShaderResourceView(velU->GetUnderlying()),
                    Metal::ShaderResourceView(velV->GetUnderlying()),
                    Metal::ShaderResourceView(temperature->GetUnderlying())));

            const ::Assets::ResChar* pixelShader = "";
            if (debuggingMode == FluidDebuggingMode::Density) {
                pixelShader = "game/xleres/cfd/debug.sh:ps_density:ps_*";
            } else if (debuggingMode == FluidDebuggingMode::Temperature) {
                pixelShader = "game/xleres/cfd/debug.sh:ps_temperature:ps_*";
            } else if (debuggingMode == FluidDebuggingMode::Velocity) {
                pixelShader = "game/xleres/cfd/debug.sh:ps_velocity:ps_*";
            }

            auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>(
                "game/xleres/basic3D.vsh:PT:vs_*", pixelShader);

            Float2 wsDims = dimensions;

            struct Vertex { Float3 position; Float2 texCoord; } 
            vertices[] = 
            {
                { Float3(0.f, 0.f, 0.f), Float2(0.f, 1.f) },
                { Float3(wsDims[0], 0.f, 0.f), Float2(1.f, 1.f) },
                { Float3(0.f, wsDims[1], 0.f), Float2(0.f, 0.f) },
                { Float3(wsDims[0], wsDims[1], 0.f), Float2(1.f, 0.f) }
            };

            Metal::BoundInputLayout inputLayout(Metal::GlobalInputLayouts::PT, shader);
            Metal::BoundUniforms uniforms(shader);
            Techniques::TechniqueContext::BindGlobalUniforms(uniforms);
                
            metalContext.BindPS(MakeResourceList(Techniques::CommonResources()._defaultSampler, Techniques::CommonResources()._linearClampSampler));
            metalContext.Bind(inputLayout);
            uniforms.Apply(metalContext, 
                parserContext.GetGlobalUniformsStream(), Metal::UniformsStream());
            metalContext.Bind(shader);

            metalContext.Bind(MakeResourceList(
                Metal::VertexBuffer(vertices, sizeof(vertices))), sizeof(Vertex), 0);
            metalContext.Bind(Techniques::CommonResources()._cullDisable);
            metalContext.Bind(Metal::Topology::TriangleStrip);
            metalContext.Draw(4);
        } 
        CATCH (const ::Assets::Exceptions::PendingAsset& e) { parserContext.Process(e); }
        CATCH (const ::Assets::Exceptions::InvalidAsset& e) { parserContext.Process(e); }
        CATCH_END
    }

    static void RenderFluidDebugging3D(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode,
        UInt3 dimensions,
        const float* density, const float* velocityU, const float* velocityV, const float* temperature)
    {
        TRY {
            using namespace RenderCore;
            using namespace BufferUploads;
            auto& uploads = GetBufferUploads();

            auto dx = dimensions[0], dy = dimensions[1], dz = dimensions[2];
            auto pktSize = dx*dy*dz*sizeof(float);
            TexturePitches pitches(dx*sizeof(float), dy*dx*sizeof(float));

            auto desc = CreateDesc(
                BindFlag::ShaderResource,
                0, GPUAccess::Read|GPUAccess::Write,
                TextureDesc::Plain3D(dx, dy, dz, RenderCore::Metal::NativeFormat::R32_FLOAT),
                "fluid");
            auto densityPkt = CreateBasicPacket(pktSize, density, pitches);
            auto velUPkt = CreateBasicPacket(pktSize, velocityU, pitches);
            auto velVPkt = CreateBasicPacket(pktSize, velocityV, pitches);
            auto temperaturePkt = CreateBasicPacket(pktSize, temperature, pitches);

            auto density = uploads.Transaction_Immediate(desc, densityPkt.get());
            auto velU = uploads.Transaction_Immediate(desc, velUPkt.get());
            auto velV = uploads.Transaction_Immediate(desc, velVPkt.get());
            auto temperature = uploads.Transaction_Immediate(desc, temperaturePkt.get());

            metalContext.BindPS(
                MakeResourceList(
                    Metal::ShaderResourceView(density->GetUnderlying()),
                    Metal::ShaderResourceView(velU->GetUnderlying()),
                    Metal::ShaderResourceView(velV->GetUnderlying()),
                    Metal::ShaderResourceView(temperature->GetUnderlying())));

            const ::Assets::ResChar* pixelShader = "";
            if (debuggingMode == FluidDebuggingMode::Density)           pixelShader = "game/xleres/cfd/debug3d.sh:ps_density:ps_*";
            else if (debuggingMode == FluidDebuggingMode::Temperature)  pixelShader = "game/xleres/cfd/debug3d.sh:ps_temperature:ps_*";
            else if (debuggingMode == FluidDebuggingMode::Velocity)     pixelShader = "game/xleres/cfd/debug3d.sh:ps_velocity:ps_*";

            auto& shader = ::Assets::GetAssetDep<Metal::ShaderProgram>("game/xleres/basic3D.vsh:PT:vs_*", pixelShader);
            Float2 wsDims = Truncate(dimensions);

            struct Vertex { Float3 position; Float2 texCoord; } 
            vertices[] = 
            {
                { Float3(0.f, 0.f, 0.f), Float2(0.f, 1.f) },
                { Float3(wsDims[0], 0.f, 0.f), Float2(1.f, 1.f) },
                { Float3(0.f, wsDims[1], 0.f), Float2(0.f, 0.f) },
                { Float3(wsDims[0], wsDims[1], 0.f), Float2(1.f, 0.f) }
            };

            Metal::BoundInputLayout inputLayout(Metal::GlobalInputLayouts::PT, shader);
            Metal::BoundUniforms uniforms(shader);
            Techniques::TechniqueContext::BindGlobalUniforms(uniforms);
                
            metalContext.BindPS(MakeResourceList(
                Techniques::CommonResources()._defaultSampler, 
                Techniques::CommonResources()._linearClampSampler));
            metalContext.Bind(inputLayout);
            uniforms.Apply(metalContext, 
                parserContext.GetGlobalUniformsStream(), Metal::UniformsStream());
            metalContext.Bind(shader);

            metalContext.Bind(MakeResourceList(
                Metal::VertexBuffer(vertices, sizeof(vertices))), sizeof(Vertex), 0);
            metalContext.Bind(Techniques::CommonResources()._cullDisable);
            metalContext.Bind(Metal::Topology::TriangleStrip);
            metalContext.Draw(4);
        } 
        CATCH (const ::Assets::Exceptions::PendingAsset& e) { parserContext.Process(e); }
        CATCH (const ::Assets::Exceptions::InvalidAsset& e) { parserContext.Process(e); }
        CATCH_END
    }
}

template<> const ClassAccessors& GetAccessors<SceneEngine::ReferenceFluidSolver2D::Settings>()
{
    using Obj = SceneEngine::ReferenceFluidSolver2D::Settings;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add(u("DeltaTime"), DefaultGet(Obj, _deltaTime),  DefaultSet(Obj, _deltaTime));
        props.Add(u("Viscosity"), DefaultGet(Obj, _viscosity),  DefaultSet(Obj, _viscosity));
        props.Add(u("DiffusionRate"), DefaultGet(Obj, _diffusionRate),  DefaultSet(Obj, _diffusionRate));
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
        props.Add(u("Viscosity"), DefaultGet(Obj, _viscosity),  DefaultSet(Obj, _viscosity));
        props.Add(u("DiffusionRate"), DefaultGet(Obj, _diffusionRate),  DefaultSet(Obj, _diffusionRate));
        props.Add(u("TempDiffusionRate"), DefaultGet(Obj, _tempDiffusion),  DefaultSet(Obj, _tempDiffusion));
        props.Add(u("DiffusionMethod"), DefaultGet(Obj, _diffusionMethod),  DefaultSet(Obj, _diffusionMethod));
        props.Add(u("AdvectionMethod"), DefaultGet(Obj, _advectionMethod),  DefaultSet(Obj, _advectionMethod));
        props.Add(u("AdvectionSteps"), DefaultGet(Obj, _advectionSteps),  DefaultSet(Obj, _advectionSteps));
        props.Add(u("EnforceIncompressibility"), DefaultGet(Obj, _enforceIncompressibilityMethod),  DefaultSet(Obj, _enforceIncompressibilityMethod));
        props.Add(u("BouyancyAlpha"), DefaultGet(Obj, _buoyancyAlpha),  DefaultSet(Obj, _buoyancyAlpha));
        props.Add(u("BouyancyBeta"), DefaultGet(Obj, _buoyancyBeta),  DefaultSet(Obj, _buoyancyBeta));
        props.Add(u("VorticityConfinement"), DefaultGet(Obj, _vorticityConfinement),  DefaultSet(Obj, _vorticityConfinement));
        props.Add(u("InterpolationMethod"), DefaultGet(Obj, _interpolationMethod),  DefaultSet(Obj, _interpolationMethod));
        props.Add(u("BorderX"), DefaultGet(Obj, _borderX),  DefaultSet(Obj, _borderX));
        props.Add(u("BorderY"), DefaultGet(Obj, _borderY),  DefaultSet(Obj, _borderY));
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
        props.Add(u("Viscosity"), DefaultGet(Obj, _viscosity),  DefaultSet(Obj, _viscosity));
        props.Add(u("DiffusionRate"), DefaultGet(Obj, _diffusionRate),  DefaultSet(Obj, _diffusionRate));
        props.Add(u("DiffusionMethod"), DefaultGet(Obj, _diffusionMethod),  DefaultSet(Obj, _diffusionMethod));

        props.Add(u("AdvectionMethod"), DefaultGet(Obj, _advectionMethod),  DefaultSet(Obj, _advectionMethod));
        props.Add(u("AdvectionSteps"), DefaultGet(Obj, _advectionSteps),  DefaultSet(Obj, _advectionSteps));
        props.Add(u("InterpolationMethod"), DefaultGet(Obj, _interpolationMethod),  DefaultSet(Obj, _interpolationMethod));

        props.Add(u("EnforceIncompressibility"), DefaultGet(Obj, _enforceIncompressibilityMethod),  DefaultSet(Obj, _enforceIncompressibilityMethod));
        props.Add(u("VorticityConfinement"), DefaultGet(Obj, _vorticityConfinement),  DefaultSet(Obj, _vorticityConfinement));
        
        init = true;
    }
    return props;
}

template<> const ClassAccessors& GetAccessors<SceneEngine::CloudsForm2D::Settings>()
{
    using Obj = SceneEngine::CloudsForm2D::Settings;
    static ClassAccessors props(typeid(Obj).hash_code());
    static bool init = false;
    if (!init) {
        props.Add(u("Viscosity"), DefaultGet(Obj, _viscosity),  DefaultSet(Obj, _viscosity));
        props.Add(u("CondensedDiffusionRate"), DefaultGet(Obj, _condensedDiffusionRate),  DefaultSet(Obj, _condensedDiffusionRate));
        props.Add(u("VaporDiffusionRate"), DefaultGet(Obj, _vaporDiffusionRate),  DefaultSet(Obj, _vaporDiffusionRate));
        props.Add(u("TemperatureDiffusionRate"), DefaultGet(Obj, _temperatureDiffusionRate),  DefaultSet(Obj, _temperatureDiffusionRate));
        props.Add(u("DiffusionMethod"), DefaultGet(Obj, _diffusionMethod),  DefaultSet(Obj, _diffusionMethod));

        props.Add(u("AdvectionMethod"), DefaultGet(Obj, _advectionMethod),  DefaultSet(Obj, _advectionMethod));
        props.Add(u("AdvectionSteps"), DefaultGet(Obj, _advectionSteps),  DefaultSet(Obj, _advectionSteps));
        props.Add(u("InterpolationMethod"), DefaultGet(Obj, _interpolationMethod),  DefaultSet(Obj, _interpolationMethod));

        props.Add(u("EnforceIncompressibility"), DefaultGet(Obj, _enforceIncompressibilityMethod),  DefaultSet(Obj, _enforceIncompressibilityMethod));
        props.Add(u("VorticityConfinement"), DefaultGet(Obj, _vorticityConfinement),  DefaultSet(Obj, _vorticityConfinement));
        props.Add(u("BuoyancyAlpha"), DefaultGet(Obj, _buoyancyAlpha),  DefaultSet(Obj, _buoyancyAlpha));
        props.Add(u("BuoyancyBeta"), DefaultGet(Obj, _buoyancyBeta),  DefaultSet(Obj, _buoyancyBeta));

        props.Add(u("CondensationSpeed"), DefaultGet(Obj, _condensationSpeed),  DefaultSet(Obj, _condensationSpeed));
        props.Add(u("TemperatureChangeSpeed"), DefaultGet(Obj, _temperatureChangeSpeed),  DefaultSet(Obj, _temperatureChangeSpeed));
        
        init = true;
    }
    return props;
}



