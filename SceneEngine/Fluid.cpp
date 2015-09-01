// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "Fluid.h"
#include "../ConsoleRig/Log.h"
#include "../Math/RegularNumberField.h"
#include "../Math/PoissonSolver.h"
#include "../Math/PoissonSolverDetail.h"
#include "../Utility/Meta/ClassAccessorsImpl.h"

extern "C" void dens_step ( int N, float * x, float * x0, float * u, float * v, float diff, float dt );
extern "C" void vel_step ( int N, float * u, float * v, float * u0, float * v0, float visc, float dt );

#pragma warning(disable:4714)
#pragma push_macro("new")
#undef new
#include <Eigen/Dense>
#pragma pop_macro("new")

using namespace XLEMath::PoissonSolverInternal;

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

    static void RenderFluidDebugging(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode,
        UInt2 dimensions,
        const float* density, const float* velocityU, const float* velocityV, const float* temperature);

    void ReferenceFluidSolver2D::RenderDebugging(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode)
    {
        RenderFluidDebugging(
            metalContext, parserContext, debuggingMode,
            _pimpl->_dimensions, _pimpl->_density.get(),
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
    using ScalarField2D = XLEMath::ScalarField2D<VectorX>;

    static ScalarField1D AsScalarField1D(VectorX& v) { return ScalarField1D { v.data(), (unsigned)v.size() }; }

    class FluidSolver2D::Pimpl
    {
    public:
        VectorX _velU;
        VectorX _velV;
        VectorX _density;
        VectorX _temperature;

        VectorX _prevVelU;
        VectorX _prevVelV;
        VectorX _prevDensity;
        VectorX _prevTemperature;
        UInt2 _dimensions;

        VectorX _workingX, _workingB;

        // int _bands[5];
        // SparseBandedMatrix _bandedPrecon;
        // std::function<float(unsigned, unsigned)> AMat;

        PoissonSolver _densityDiffusionSolver;
        PoissonSolver _velocityDiffusionSolver;
        PoissonSolver _temperatureDiffusionSolver;
        PoissonSolver _incompressibilitySolver;

        void DensityDiffusion(float deltaTime, const FluidSolver2D::Settings& settings);
        void VelocityDiffusion(float deltaTime, const FluidSolver2D::Settings& settings);
        void TemperatureDiffusion(float deltaTime, const FluidSolver2D::Settings& settings);

        template<typename Field, typename VelField>
            void Advect(
                Field dstValues, Field srcValues, 
                VelField velFieldT0, VelField velFieldT1,
                float deltaTime, const FluidSolver2D::Settings& settings);

        template<typename Field>
            void EnforceIncompressibility(
                Field velField,
                const FluidSolver2D::Settings& settings);

        void VorticityConfinement(VectorField2D outputField, VectorField2D inputVelocities, float strength, float deltaTime);
    };

    void FluidSolver2D::Pimpl::DensityDiffusion(float deltaTime, const FluidSolver2D::Settings& settings)
    {
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

        // const float diffFactor = settings._diffusionRate;
        // const float a = diffFactor * deltaTime;
        // const float a0 = 1.f + 4.f * a;
        // const float a1 = -a;

        unsigned iterations = 0;
        const bool useGeneralA = false;
        if (constant_expression<useGeneralA>::result()) {
            // iterations = SolvePoisson(_density, AMat, _density, (PossionSolver)settings._diffusionMethod);
        } else {
            iterations = _densityDiffusionSolver.Solve(
                AsScalarField1D(_density), AsScalarField1D(_density), 
                (PoissonSolver::Method)settings._diffusionMethod);
        }
        LogInfo << "Density diffusion took: (" << iterations << ") iterations.";
    }

    void FluidSolver2D::Pimpl::VelocityDiffusion(float deltaTime, const FluidSolver2D::Settings& settings)
    {
        // const float diffFactor = settings._viscosity;
        // const float a = diffFactor * deltaTime;
        // const float a0 = 1.f + 4.f * a, a1 = -a;

        unsigned iterationsu = 0, iterationsv = 0;
        const bool useGeneralA = false;
        if (constant_expression<useGeneralA>::result()) {
            // iterations = SolvePoisson(_density, AMat, _density, (PossionSolver)settings._diffusionMethod);
        } else {
            iterationsu = _velocityDiffusionSolver.Solve(
                AsScalarField1D(_velU), AsScalarField1D(_velU), 
                (PoissonSolver::Method)settings._diffusionMethod);
            iterationsv += _velocityDiffusionSolver.Solve(
                AsScalarField1D(_velV), AsScalarField1D(_velV), 
                (PoissonSolver::Method)settings._diffusionMethod);
        }
        LogInfo << "Velocity diffusion took: (" << iterationsu << ", " << iterationsv << ") iterations.";
    }

    void FluidSolver2D::Pimpl::TemperatureDiffusion(float deltaTime, const FluidSolver2D::Settings& settings)
    {
        // const float diffFactor = settings._tempDiffusion;
        // const float a = diffFactor * deltaTime;
        // const float a0 = 1.f + 4.f * a;
        // const float a1 = -a;

        unsigned iterations = 0;
        const bool useGeneralA = false;
        if (constant_expression<useGeneralA>::result()) {
            // iterations = SolvePoisson(_temperature, AMat, _temperature, (PossionSolver)settings._diffusionMethod);
        } else {
            iterations = _temperatureDiffusionSolver.Solve(
                AsScalarField1D(_temperature), AsScalarField1D(_temperature), 
                (PoissonSolver::Method)settings._diffusionMethod);
        }
        LogInfo << "Temperature diffusion took: (" << iterations << ") iterations.";
    }

    template<unsigned Interpolation, typename Field>
        static Float2 AdvectRK4(
            const Field& velFieldT0, const Field& velFieldT1,
            UInt2 pt, float velScale)
        {
            const float s = velScale;
            const float halfS = s / 2.f;
    
            Float2 startTap = Float2(float(pt[0]), float(pt[1]));
            auto k1 = velFieldT0.Load(pt);
            auto k2 = .5f * velFieldT0.Sample<Interpolation|RNFSample::Clamp>(startTap + halfS * k1)
                    + .5f * velFieldT1.Sample<Interpolation|RNFSample::Clamp>(startTap + halfS * k1)
                    ;
            auto k3 = .5f * velFieldT0.Sample<Interpolation|RNFSample::Clamp>(startTap + halfS * k2)
                    + .5f * velFieldT1.Sample<Interpolation|RNFSample::Clamp>(startTap + halfS * k2)
                    ;
            auto k4 = velFieldT1.Sample<Interpolation|RNFSample::Clamp>(startTap + s * k3);
    
            return startTap + (s / 6.f) * (k1 + 2.f * k2 + 2.f * k3 + k4);
        }

    template<unsigned Interpolation>
        static Float2 AdvectRK4(
            const VectorField2D& velFieldT0, const VectorField2D& velFieldT1,
            Float2 pt, float velScale)
        {
            const float s = velScale;
            const float halfS = s / 2.f;

                // when using a float point input, we need bilinear interpolation
            auto k1 = velFieldT0.Sample<Interpolation|RNFSample::Clamp>(pt);
            auto k2 = .5f * velFieldT0.Sample<Interpolation|RNFSample::Clamp>(pt + halfS * k1)
                    + .5f * velFieldT1.Sample<Interpolation|RNFSample::Clamp>(pt + halfS * k1)
                    ;
            auto k3 = .5f * velFieldT0.Sample<Interpolation|RNFSample::Clamp>(pt + halfS * k2)
                    + .5f * velFieldT1.Sample<Interpolation|RNFSample::Clamp>(pt + halfS * k2)
                    ;
            auto k4 = velFieldT1.Sample<Interpolation|RNFSample::Clamp>(pt + s * k3);

            return pt + (s / 6.f) * (k1 + 2.f * k2 + 2.f * k3 + k4);
        }

    template<typename Type> Type MaxValue();
    template<> float MaxValue()         { return FLT_MAX; }
    template<> Float2 MaxValue()        { return Float2(FLT_MAX, FLT_MAX); }
    float   Min(float lhs, float rhs)   { return std::min(lhs, rhs); }
    Float2  Min(Float2 lhs, Float2 rhs) { return Float2(std::min(lhs[0], rhs[0]), std::min(lhs[1], rhs[1])); }
    float   Max(float lhs, float rhs)   { return std::max(lhs, rhs); }
    Float2  Max(Float2 lhs, Float2 rhs) { return Float2(std::max(lhs[0], rhs[0]), std::max(lhs[1], rhs[1])); }

    template<unsigned SamplingFlags, typename Field>
        typename Field::ValueType LoadWithNearbyRange(typename Field::ValueType& minNeighbour, typename Field::ValueType& maxNeighbour, const Field& field, Float2 pt)
        {
            Field::ValueType predictorParts[9];
            float predictorWeights[4];
            field.GatherNeighbors(predictorParts, predictorWeights, pt);
            
            minNeighbour =  MaxValue<Field::ValueType>();
            maxNeighbour = -MaxValue<Field::ValueType>();
            for (unsigned c=0; c<9; ++c) {
                minNeighbour = Min(predictorParts[c], minNeighbour);
                maxNeighbour = Max(predictorParts[c], maxNeighbour);
            }

            if (constant_expression<(SamplingFlags & RNFSample::Cubic)==0>::result()) {
                return
                      predictorWeights[0] * predictorParts[0]
                    + predictorWeights[1] * predictorParts[1]
                    + predictorWeights[2] * predictorParts[2]
                    + predictorWeights[3] * predictorParts[3];
            } else {
                return field.Sample<RNFSample::Cubic|RNFSample::Clamp>(pt);
            }
        }

    template<typename Field, typename VelField>
        void FluidSolver2D::Pimpl::Advect(
            Field dstValues, Field srcValues, 
            VelField velFieldT0, VelField velFieldT1,
            float deltaTime, const FluidSolver2D::Settings& settings)
    {
        //
        // This is the advection step. We will use the method of characteristics.
        //
        // We have a few different options for the stepping method:
        //  * basic euler forward integration (ie, just step forward in time)
        //  * forward integration method divided into smaller time steps
        //  * Runge-Kutta integration
        //  * Modified MacCormack methods
        //  * Back and Forth Error Compensation and Correction (BFECC)
        //
        // Let's start without any complex boundary conditions.
        //
        // We have to be careful about how the velocity sample is aligned with
        // the grid cell. Incorrect alignment will produce a bias in the way that
        // we interpolate the field.
        //
        // We could consider offsetting the velocity field by half a cell (see
        // Visual Simulation of Smoke, Fedkiw, et al)
        //
        // Also consider Semi-Lagrangian methods for large timesteps (when the CFL
        // number is larger than 1)
        //

        enum class Advection { ForwardEuler, ForwardEulerDiv, RungeKutta, MacCormackRK4 };
        const auto advectionMethod = (Advection)settings._advectionMethod;
        const auto adjvectionSteps = settings._advectionSteps;

        const unsigned wh = _dimensions[0] + 2;
        const float velFieldScale = float(_dimensions[0]);   // (grid size without borders)

        if (advectionMethod == Advection::ForwardEuler) {

                //  For each cell in the grid, trace backwards
                //  through the velocity field to find an approximation
                //  of where the point was in the previous frame.

            const unsigned SamplingFlags = 0;
            for (unsigned y=1; y<wh-1; ++y)
                for (unsigned x=1; x<wh-1; ++x) {
                    auto startVel = velFieldT1.Load(UInt2(x, y));
                    Float2 tap = Float2(float(x), float(y)) - (deltaTime * velFieldScale) * startVel;
                    tap[0] = Clamp(tap[0], 0.f, float(wh-1) - 1e-5f);
                    tap[1] = Clamp(tap[1], 0.f, float(wh-1) - 1e-5f);
                    dstValues.Write(UInt2(x, y), srcValues.Sample<SamplingFlags>(tap));
                }

        } else if (advectionMethod == Advection::ForwardEulerDiv) {

            float stepScale = deltaTime * velFieldScale / float(adjvectionSteps);
            const unsigned SamplingFlags = 0;
            for (unsigned y=1; y<wh-1; ++y)
                for (unsigned x=1; x<wh-1; ++x) {

                    Float2 tap = Float2(float(x), float(y));
                    for (unsigned s=0; s<adjvectionSteps; ++s) {
                        float a = (adjvectionSteps-1-s) / float(adjvectionSteps-1);
                        auto vel = 
                            LinearInterpolate(
                                velFieldT0.Sample<SamplingFlags>(tap),
                                velFieldT1.Sample<SamplingFlags>(tap),
                                a);

                        tap -= stepScale * vel;
                        tap[0] = Clamp(tap[0], 0.f, float(wh-1) - 1e-5f);
                        tap[1] = Clamp(tap[1], 0.f, float(wh-1) - 1e-5f);
                    }

                    dstValues.Write(UInt2(x, y), srcValues.Sample<SamplingFlags>(tap));
                }

        } else if (advectionMethod == Advection::RungeKutta) {

            if (settings._interpolationMethod == 0) {

                const auto SamplingFlags = 0u;
                for (unsigned y=1; y<wh-1; ++y)
                    for (unsigned x=1; x<wh-1; ++x) {

                            // This is the RK4 version
                            // We'll use the average of the velocity field at t and
                            // the velocity field at t+dt as an estimate of the field
                            // at t+.5*dt

                            // Note that we're tracing the velocity field backwards.
                            // So doing k1 on velField1, and k4 on velFieldT0
                            //      -- hoping this will interact with the velocity diffusion more sensibly
                        const auto tap = AdvectRK4<SamplingFlags>(velFieldT1, velFieldT0, UInt2(x, y), -deltaTime * velFieldScale);
                        dstValues.Write(UInt2(x, y), srcValues.Sample<SamplingFlags|RNFSample::Clamp>(tap));

                    }

            } else {

                const auto SamplingFlags = RNFSample::Cubic;
                for (unsigned y=1; y<wh-1; ++y)
                    for (unsigned x=1; x<wh-1; ++x) {
                        const auto tap = AdvectRK4<SamplingFlags>(velFieldT1, velFieldT0, UInt2(x, y), -deltaTime * velFieldScale);
                        dstValues.Write(UInt2(x, y), srcValues.Sample<SamplingFlags|RNFSample::Clamp>(tap));
                    }

            }

        } else if (advectionMethod == Advection::MacCormackRK4) {

                //
                // This is a modified MacCormack scheme, as described in An Unconditionally
                // Stable MacCormack Method -- Selle & Fedkiw, et al.
                //  http://physbam.stanford.edu/~fedkiw/papers/stanford2006-09.pdf
                //
                // It's also similar to the (oddly long nammed) Back And Forth Error Compensation 
                // and Correction (BFECC).
                //
                // Basically, we want to run an initial predictor step, then run a backwards
                // advection to find an intermediate point. The difference between the value at
                // the initial point and the intermediate point is used as a error term.
                //
                // This way, we get an improved estimate, but with only 2 advection steps.
                //
                // We need to use some advection method for the forward and advection steps. Often
                // a semi-lagrangian method is used (particularly velocities and timesteps are large
                // with respect to the grid size). 
                //
                // But here, we'll use RK4.
                //
                // We also need a way to check for overruns and oscillation cases. Selle & Fedkiw
                // suggest using a normal semi-Lagrangian method in these cases. We'll try a simplier
                // method and just clamp.
                //

            if (settings._interpolationMethod == 0) {

                const auto SamplingFlags = 0u;
                for (unsigned y=1; y<wh-1; ++y)
                    for (unsigned x=1; x<wh-1; ++x) {

                        const auto pt = UInt2(x, y);

                            // advect backwards in time first, to find the predictor
                        const auto predictor = AdvectRK4<SamplingFlags>(velFieldT1, velFieldT0, pt, -deltaTime * velFieldScale);
                            // advect forward again to find the error tap
                        const auto reversedTap = AdvectRK4<SamplingFlags>(velFieldT0, velFieldT1, predictor, deltaTime * velFieldScale);

                        auto originalValue = srcValues.Load(pt);
                        auto reversedValue = srcValues.Sample<SamplingFlags|RNFSample::Clamp>(reversedTap);
                        Field::ValueType finalValue;

                            // Here we clamp the final result within the range of the neighbour cells of the 
                            // original predictor. This prevents the scheme from becoming unstable (by avoiding
                            // irrational values for 0.5f * (originalValue - reversedValue)
                        const bool doRangeClamping = true;
                        if (constant_expression<doRangeClamping>::result()) {
                            Field::ValueType minNeighbour, maxNeighbour;
                            auto predictorValue = LoadWithNearbyRange<SamplingFlags>(minNeighbour, maxNeighbour, srcValues, predictor);
                            finalValue = Field::ValueType(predictorValue + .5f * (originalValue - reversedValue));
                            finalValue = Max(finalValue, minNeighbour);
                            finalValue = Min(finalValue, maxNeighbour);
                        } else {
                            auto predictorValue = srcValues.Sample<SamplingFlags|RNFSample::Clamp>(predictor);
                            finalValue = Field::ValueType(predictorValue + .5f * (originalValue - reversedValue));
                        }

                        dstValues.Write(pt, finalValue);

                    }

            } else {

                const auto SamplingFlags = RNFSample::Cubic;
                for (unsigned y=1; y<wh-1; ++y)
                    for (unsigned x=1; x<wh-1; ++x) {

                        const auto pt = UInt2(x, y);
                        const auto predictor = AdvectRK4<SamplingFlags>(velFieldT1, velFieldT0, pt, -deltaTime * velFieldScale);
                        const auto reversedTap = AdvectRK4<SamplingFlags>(velFieldT0, velFieldT1, predictor, deltaTime * velFieldScale);

                        auto originalValue = srcValues.Load(pt);
                        auto reversedValue = srcValues.Sample<SamplingFlags|RNFSample::Clamp>(reversedTap);

                        Field::ValueType minNeighbour, maxNeighbour;
                        auto predictorValue = LoadWithNearbyRange<SamplingFlags>(minNeighbour, maxNeighbour, srcValues, predictor);
                        auto finalValue = Field::ValueType(predictorValue + .5f * (originalValue - reversedValue));
                        finalValue = Max(finalValue, minNeighbour);
                        finalValue = Min(finalValue, maxNeighbour);

                        dstValues.Write(pt, finalValue);

                    }

            }

        }

    }

    template<typename Field>
        void FluidSolver2D::Pimpl::EnforceIncompressibility(
            Field velField,
            const FluidSolver2D::Settings& settings)
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

        const auto wh = velField._wh;
        VectorX delW(wh * wh), q(wh * wh);
        q.fill(0.f);
        float N = float(wh);
        for (unsigned y=1; y<wh-1; ++y)
            for (unsigned x=1; x<wh-1; ++x)
                delW[y*wh+x] = 
                    -0.5f/N * 
                    (
                          (*velField._u)[y*wh+x+1]   - (*velField._u)[y*wh+x-1]
                        + (*velField._v)[(y+1)*wh+x] - (*velField._v)[(y-1)*wh+x]
                    );

        SmearBorder(delW, wh);
        auto iterations = _incompressibilitySolver.Solve(
            AsScalarField1D(q), AsScalarField1D(delW), 
            (PoissonSolver::Method)settings._enforceIncompressibilityMethod);
        SmearBorder(q, wh);

        for (unsigned y=1; y<wh-1; ++y)
            for (unsigned x=1; x<wh-1; ++x) {
                (*velField._u)[y*wh+x] -= .5f*N * (q[y*wh+x+1]   - q[y*wh+x-1]);
                (*velField._v)[y*wh+x] -= .5f*N * (q[(y+1)*wh+x] - q[(y-1)*wh+x]);
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

        const auto wh = inputVelocities._wh;
        VectorX vorticity(wh*wh);
        for (unsigned y=1; y<wh-1; ++y)
            for (unsigned x=1; x<wh-1; ++x) {
                auto dvydx = .5f * inputVelocities.Load(UInt2(x+1, y))[1] - inputVelocities.Load(UInt2(x-1, y))[1];
                auto dvxdy = .5f * inputVelocities.Load(UInt2(x, y+1))[0] - inputVelocities.Load(UInt2(x, y-1))[0];
                vorticity[y*wh+x] = dvydx - dvxdy;
            }
        SmearBorder(vorticity, wh);

        const float s = deltaTime * strength * float(wh-2);
        for (unsigned y=1; y<wh-1; ++y)
            for (unsigned x=1; x<wh-1; ++x) {
                    // find the discrete divergence of the absolute vorticity field
                Float2 div(
                    .5f * (XlAbs(vorticity[y*wh+x+1]) - XlAbs(vorticity[y*wh+x-1])),
                    .5f * (XlAbs(vorticity[(y+1)*wh+x]) - XlAbs(vorticity[(y-1)*wh+x])));

                float magSq = MagnitudeSquared(div);
                if (magSq > 1e-10f) {
                    div *= XlRSqrt(magSq);

                        // in 2D, the vorticity is in the Z direction. Which means the cross product
                        // with our divergence vector is simple
                    float omega = vorticity[y*wh+x];
                    Float2 additionalVel(s * div[1] * omega, s * -div[0] * omega);

                    outputField.Write(
                        UInt2(x, y),
                        outputField.Load(UInt2(x, y)) + additionalVel);
                }
            }
    }

    void FluidSolver2D::Tick(float deltaTime, const Settings& settings)
    {
        auto D = _pimpl->_dimensions[0];
        assert(_pimpl->_dimensions[1] == _pimpl->_dimensions[0]);

        float dt = deltaTime;
        auto wh = D+2;
        auto N = wh*wh;

        _pimpl->VorticityConfinement(
            VectorField2D(&_pimpl->_prevVelU, &_pimpl->_prevVelV, D+2),
            VectorField2D(&_pimpl->_velU, &_pimpl->_velV, D+2),
            settings._vorticityConfinement, deltaTime);

        for (unsigned c=0; c<N; ++c) {
            _pimpl->_density[c] += dt * _pimpl->_prevDensity[c];
            _pimpl->_velU[c] += dt * _pimpl->_prevVelU[c];
            _pimpl->_velV[c] += dt * _pimpl->_prevVelV[c];
            _pimpl->_temperature[c] += dt * _pimpl->_prevTemperature[c];
        }

            // buoyancy force
        const float buoyancyAlpha = settings._buoyancyAlpha;
        const float buoyancyBeta = settings._buoyancyBeta;
        for (unsigned y=1; y<wh-1; ++y)
            for (unsigned x=1; x<wh-1; ++x) {
                unsigned i=y*wh+x;
                _pimpl->_velV[i] -=     // (upwards is -1 in V)
                     -buoyancyAlpha * _pimpl->_density[i]
                    + buoyancyBeta  * _pimpl->_temperature[i];       // temperature field is just the difference from ambient
            }

        _pimpl->_prevVelU = _pimpl->_velU;
        _pimpl->_prevVelV = _pimpl->_velV;
        _pimpl->VelocityDiffusion(deltaTime, settings);

        VectorX newU(N), newV(N);
        _pimpl->Advect(
            VectorField2D(&newU, &newV, D+2),
            VectorField2D(&_pimpl->_velU, &_pimpl->_velV, D+2),
            VectorField2D(&_pimpl->_prevVelU, &_pimpl->_prevVelV, D+2),
            VectorField2D(&_pimpl->_velU, &_pimpl->_velV, D+2),
            deltaTime, settings);
        
        ReflectUBorder(newU, D+2);
        ReflectVBorder(newV, D+2);
        _pimpl->EnforceIncompressibility(
            VectorField2D(&newU, &newV, D+2),
            settings);
        
        _pimpl->_velU = newU;
        _pimpl->_velV = newV;

        _pimpl->DensityDiffusion(deltaTime, settings);
        auto prevDensity = _pimpl->_density;
        _pimpl->Advect(
            ScalarField2D(&_pimpl->_density, D+2),
            ScalarField2D(&prevDensity, D+2),
            VectorField2D(&_pimpl->_prevVelU, &_pimpl->_prevVelV, D+2),
            VectorField2D(&_pimpl->_velU, &_pimpl->_velV, D+2),
            deltaTime, settings);

        _pimpl->TemperatureDiffusion(deltaTime, settings);
        auto prevTemperature = _pimpl->_temperature;
        _pimpl->Advect(
            ScalarField2D(&_pimpl->_temperature, D+2),
            ScalarField2D(&prevTemperature, D+2),
            VectorField2D(&_pimpl->_prevVelU, &_pimpl->_prevVelV, D+2),
            VectorField2D(&_pimpl->_velU, &_pimpl->_velV, D+2),
            deltaTime, settings);

        for (unsigned c=0; c<N; ++c) {
            _pimpl->_prevVelU[c] = 0.f;
            _pimpl->_prevVelV[c] = 0.f;
            _pimpl->_prevDensity[c] = 0.f;
            _pimpl->_prevTemperature[c] = 0.f;
        }
    }

    void FluidSolver2D::AddDensity(UInt2 coords, float amount)
    {
        if (coords[0] < _pimpl->_dimensions[0] && coords[1] < _pimpl->_dimensions[1]) {
            unsigned i = (coords[0]+1) + (coords[1]+1) * (_pimpl->_dimensions[0] + 2);
            _pimpl->_prevDensity[i] += amount;
        }
    }

    void FluidSolver2D::AddTemperature(UInt2 coords, float amount)
    {
        if (coords[0] < _pimpl->_dimensions[0] && coords[1] < _pimpl->_dimensions[1]) {
            unsigned i = (coords[0]+1) + (coords[1]+1) * (_pimpl->_dimensions[0] + 2);
            // _pimpl->_prevTemperature[i] += amount;

                // heat up to approach this temperature
            auto oldTemp = _pimpl->_temperature[i];
            _pimpl->_temperature[i] = std::max(oldTemp, LinearInterpolate(oldTemp, amount, 0.5f));
        }
    }

    void FluidSolver2D::AddVelocity(UInt2 coords, Float2 vel)
    {
        if (coords[0] < _pimpl->_dimensions[0] && coords[1] < _pimpl->_dimensions[1]) {
            unsigned i = (coords[0]+1) + (coords[1]+1) * (_pimpl->_dimensions[0] + 2);
            _pimpl->_prevVelU[i] += vel[0];
            _pimpl->_prevVelV[i] += vel[1];
        }
    }

    void FluidSolver2D::RenderDebugging(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode)
    {
        RenderFluidDebugging(
            metalContext, parserContext, debuggingMode,
            _pimpl->_dimensions, _pimpl->_density.data(),
            _pimpl->_velU.data(), _pimpl->_velV.data(),
            _pimpl->_temperature.data());
    }

    UInt2 FluidSolver2D::GetDimensions() const { return _pimpl->_dimensions; }

    FluidSolver2D::FluidSolver2D(UInt2 dimensions)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_dimensions = dimensions;
        auto N = (dimensions[0]+2) * (dimensions[1]+2);

        _pimpl->_velU = VectorX(N);
        _pimpl->_velV = VectorX(N);
        _pimpl->_density = VectorX(N);
        _pimpl->_temperature = VectorX(N);
        _pimpl->_prevVelU = VectorX(N);
        _pimpl->_prevVelV = VectorX(N);
        _pimpl->_prevDensity = VectorX(N);
        _pimpl->_prevTemperature = VectorX(N);

        _pimpl->_velU.fill(0.f);
        _pimpl->_velV.fill(0.f);
        _pimpl->_density.fill(0.f);
        _pimpl->_temperature.fill(0.f);
        _pimpl->_prevVelU.fill(0.f);
        _pimpl->_prevVelV.fill(0.f);
        _pimpl->_prevDensity.fill(0.f);
        _pimpl->_prevTemperature.fill(0.f);

        _pimpl->_workingX = VectorX(N);
        _pimpl->_workingB = VectorX(N);

        const float dt = 1.0f / 60.f;
        float a = 5.f * dt;

        auto wh = _pimpl->_dimensions[0]+2;
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

        _pimpl->_densityDiffusionSolver = PoissonSolver(PoissonSolver::AMat2D { wh, 1.f + 4.f * a, -a });
        _pimpl->_velocityDiffusionSolver = PoissonSolver(PoissonSolver::AMat2D { wh, 1.f + 4.f * a, -a });
        _pimpl->_temperatureDiffusionSolver = PoissonSolver(PoissonSolver::AMat2D { wh, 1.f + 4.f * a, -a });
        _pimpl->_incompressibilitySolver = PoissonSolver(PoissonSolver::AMat2D { wh, 4.f, -1.f });
    }

    FluidSolver2D::~FluidSolver2D(){}

    FluidSolver2D::Settings::Settings()
    {
        _viscosity = 0.5f;
        _diffusionRate = 2.f;
        _tempDiffusion = 2.f;
        _diffusionMethod = 0;
        _advectionMethod = 3;
        _advectionSteps = 4;
        _enforceIncompressibilityMethod = 3;
        _buoyancyAlpha = 0.035f;
        _buoyancyBeta = 0.04f;
        _addDensity = 1.f;
        _addTemperature = 0.3f;
        _vorticityConfinement = 0.75f;
        _interpolationMethod = 0;
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
    static void RenderFluidDebugging(
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
                TextureDesc::Plain2D(dx+2, dy+2, RenderCore::Metal::NativeFormat::R32_FLOAT),
                "fluid");
            auto densityPkt = CreateBasicPacket((dx+2)*(dy+2)*sizeof(float), density, TexturePitches((dx+2)*sizeof(float), (dy+2)*(dx+2)*sizeof(float)));
            auto velUPkt = CreateBasicPacket((dx+2)*(dy+2)*sizeof(float), velocityU, TexturePitches((dx+2)*sizeof(float), (dy+2)*(dx+2)*sizeof(float)));
            auto velVPkt = CreateBasicPacket((dx+2)*(dy+2)*sizeof(float), velocityV, TexturePitches((dx+2)*sizeof(float), (dy+2)*(dx+2)*sizeof(float)));
            auto temperaturePkt = CreateBasicPacket((dx+2)*(dy+2)*sizeof(float), temperature, TexturePitches((dx+2)*sizeof(float), (dy+2)*(dx+2)*sizeof(float)));

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
                { Float3(0.f, 0.f, 0.f), Float2(0.f, 0.f) },
                { Float3(wsDims[0], 0.f, 0.f), Float2(1.f, 0.f) },
                { Float3(0.f, wsDims[1], 0.f), Float2(0.f, 1.f) },
                { Float3(wsDims[0], wsDims[1], 0.f), Float2(1.f, 1.f) }
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
        props.Add(u("AddDensity"), DefaultGet(Obj, _addDensity),  DefaultSet(Obj, _addDensity));
        props.Add(u("AddTemperature"), DefaultGet(Obj, _addTemperature),  DefaultSet(Obj, _addTemperature));
        props.Add(u("VorticityConfinement"), DefaultGet(Obj, _vorticityConfinement),  DefaultSet(Obj, _vorticityConfinement));
        props.Add(u("InterpolationMethod"), DefaultGet(Obj, _interpolationMethod),  DefaultSet(Obj, _interpolationMethod));
        init = true;
    }
    return props;
}

