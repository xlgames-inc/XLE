// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define _SCL_SECURE_NO_WARNINGS

#include "Fluid.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/Meta/ClassAccessorsImpl.h"

extern "C" void dens_step ( int N, float * x, float * x0, float * u, float * v, float diff, float dt );
extern "C" void vel_step ( int N, float * u, float * v, float * u0, float * v0, float visc, float dt );

#pragma warning(disable:4714)
#pragma push_macro("new")
#undef new
#include <Eigen/Dense>
#pragma pop_macro("new")


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
            unsigned i = coords[0] + coords[1] * (_pimpl->_dimensions[0] + 2);
            _pimpl->_prevDensity[i] += amount;
        }
    }

    void ReferenceFluidSolver2D::AddVelocity(UInt2 coords, Float2 vel)
    {
        if (coords[0] < _pimpl->_dimensions[0] && coords[1] < _pimpl->_dimensions[1]) {
            unsigned i = coords[0] + coords[1] * (_pimpl->_dimensions[0] + 2);
            _pimpl->_prevVelU[i] += vel[0];
            _pimpl->_prevVelV[i] += vel[1];
        }
    }

    static void RenderFluidDebugging(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode,
        UInt2 dimensions,
        const float* density, const float* velocityU, const float* velocityV);

    void ReferenceFluidSolver2D::RenderDebugging(
        RenderCore::Metal::DeviceContext& metalContext,
        LightingParserContext& parserContext,
        FluidDebuggingMode debuggingMode)
    {
        RenderFluidDebugging(
            metalContext, parserContext, debuggingMode,
            _pimpl->_dimensions, _pimpl->_density.get(),
            _pimpl->_velU.get(), _pimpl->_velV.get());
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

    using MatrixX = Eigen::MatrixXf;
    using VectorX = Eigen::VectorXf;

    class FluidSolver2D::Pimpl
    {
    public:
        std::unique_ptr<float[]> _velU;
        std::unique_ptr<float[]> _velV;
        std::unique_ptr<float[]> _density;

        std::unique_ptr<float[]> _prevVelU;
        std::unique_ptr<float[]> _prevVelV;
        std::unique_ptr<float[]> _prevDensity;
        UInt2 _dimensions;

        VectorX x, r;
        VectorX d, s, q;
        MatrixX precon;
        std::function<float(unsigned, unsigned)> AMat;

        void DensityDiffusion(float dt);
    };

    template<typename Vec>
        static void Multiply(Vec& dst, std::function<float(unsigned, unsigned)>& A, const Vec& b, unsigned N)
    {
        for (unsigned i=0; i<N; ++i) {
            float d = 0.f;
            for (unsigned j=0; j<N; ++j) {
                d += A(i, j) * b(j);
            }
            dst(i) = d;
        }
    }

    template<typename Vec, typename Mat>
        static void SolveLowerTriangular(Vec& x, const Mat& M, const Vec& b, unsigned N)
    {
            // solve: M * dst = b
            // for a lower triangular matrix, using forward substitution
        for (unsigned i=0; i<N; ++i) {
            float d = b(i);
            for (unsigned j=0; j<i; ++j) {
                d -= M(i, j) * x(j);
            }
            x(i) = d / M(i, i);
        }
    }

    void FluidSolver2D::Pimpl::DensityDiffusion(float dt)
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
        // There are many methods to solve this equation. We want a method that
        // is:
        //  * stable
        //  * parallelizable
        //  * sparse in memory usage
        // 
        // Some methods (including the methods below) produce oscillation at high
        // time steps (or large distances between cells). We need to be careful to
        // to avoid that type of oscillation.
        //
        // As suggested in Jos Stam's Stable Fluids, we'll use an integration scheme
        // based on an implicit euler method. There are a number of variations on this
        // basic method (such as the Crank-Nicolson method).
        //
        // Note that when we want a periodic boundary condition (such as wrapping around
        // on the edges), then we can consider solutions other than the ones provided
        // here.
        //
        // These methods produce a system of linear equations. In 1D, this system is
        // tridiagonal, and can be solved with the tridiagonal matrix algorithm.
        //
        // But in 2D, we must use more complex methods. There are many options here:
        //  * Jacobi relaxation (or sucessive over-relaxation, or similar)
        //      -   this type of method is very convenient because the implementation is
        //          simple with our type of banded matrix. But it is not as efficient 
        //          or accurate as other methods.
        //  * Conjugate Gradient methods
        //  * Multi-grid methods
        //  * Parallel methods
        //      -   (such as dividing the matrix into many smaller parts).
        //
        // We must also consider the boundary conditions in this step.
        // 

        enum class Diffusion { CG_Cholseky, PlainCG, ForwardEuler, SOR };
        static auto diffusion = Diffusion::SOR;

        auto N = (_dimensions[0]+2) * (_dimensions[1]+2);

        if (diffusion == Diffusion::CG_Cholseky) {

                // Calculate the incomplete cholesky factorization first
                // (this will be used as a preconditioner to the 

                // "precon" should now hold our preconditioner for the 
                // conjugate gradient.
                //
                // See http://www.cs.cmu.edu/~quake-papers/painless-conjugate-gradient.pdf 
                // for for detailed description of conjugate gradient methods!
                // 
                // see also reference at http://math.nist.gov/iml++/
            
                // init 'x' to some initial estimate
                //      Perhaps run basic euler integration to get the starting estimate?
                //  Or, maybe there's a better way to initialize 'r' for this calculation?
            // std::copy(_density.get(), _density.get() + N, x.data());
            for (unsigned c=0; c<N; ++c) x(c) = _density[c];
            const auto& b = x;

            Multiply(r, AMat, x, N);    // r = AMat * x
            r = b - r;
            
            SolveLowerTriangular(d, precon, r, N);
            
            {
                    // testing "SolveLowerTriangular"
                VectorX t = precon * d;
                for (unsigned c=0; c<N; ++c)
                    assert(XlAbs(t(c)-r(c)) <= 1e-5f);
            }
            
            float rho = r.dot(d);
            // float rho0 = rho;
            
            if (rho != 0.f) {
                const auto iterations = 15u;
                unsigned k=0;
                for (; k<iterations; ++k) {
            
                        // Note that all of the vectors and matrices
                        // used here are quite sparse! So we need to
                        // simplify the operation shere to take advantage 
                        // of that sparseness.
                        // Multiply by AMat can be replaced with a specialized
                        // operation. Unfortunately the dot products can't be
                        // simplified, because the vectors already have only one
                        // element per cell.
            
                    Multiply(q, AMat, d, N);
                    float dDotQ = d.dot(q);
                    float alpha = rho / dDotQ;
                    assert(isfinite(alpha) && !isnan(alpha));
                    x += alpha * d;
                    r -= alpha * q;
            
                    SolveLowerTriangular(s, precon, r, N);
                    float rhoOld = rho;
                    rho = r.dot(s);
                    assert(rho < rhoOld);
                    float beta = rho / rhoOld;
                    assert(isfinite(beta) && !isnan(beta));
            
                    for (unsigned i=0; i<N; ++i)
                        d(i) = s(i) + beta * d(i);
            
                    // if (rho < e^2*rho0) break;
                    if (XlAbs(rho) < 1e-8f) break;
                    // if (rho == 0.f) break;
                }

                LogInfo << "Diffusion took: " << k << " iterations.";
            }
            
            // std::copy(x.data(), x.data() + N, _density.get());
            for (unsigned c=0; c<N; ++c) {
                    // We sometimes get negative values coming in,
                    // because of noise in the calculation. But that shouldn't
                    // be happening, if everything was perfectly accurate.
                assert(isfinite(x(c)) && !isnan(x(c)));
                _density[c] = x(c);
            }

        } else if (diffusion == Diffusion::PlainCG) {

            for (unsigned c=0; c<N; ++c) x(c) = _density[c];
            const auto& b = x;

            Multiply(r, AMat, x, N);    // r = AMat * x
            r = b - r;
            d = r;

            float rho = r.dot(r);
            if (rho != 0.f) {
                const auto iterations = 15u;
                unsigned k=0;
                for (; k<iterations; ++k) {
            
                    Multiply(q, AMat, d, N);
                    float dDotQ = d.dot(q);
                    float alpha = rho / dDotQ;
                    assert(isfinite(alpha) && !isnan(alpha));
                    x += alpha * d;
                    r -= alpha * d;
            
                    float rhoOld = rho;
                    rho = r.dot(r);
                    float beta = rho / rhoOld;
                    assert(isfinite(beta) && !isnan(beta));
            
                    for (unsigned i=0; i<N; ++i)
                        d(i) = r(i) + beta * d(i);
            
                    // if (rho < e^2*rho0) break;
                    if (XlAbs(rho) < 1e-10f) break;
                    // if (rho == 0.f) break;
                }

                LogInfo << "Diffusion took: " << k << " iterations.";
            }
            
            for (unsigned c=0; c<N; ++c) {
                assert(isfinite(x(c)) && !isnan(x(c)));
                _density[c] = x(c);
            }

        } else if (diffusion == Diffusion::ForwardEuler) {
        
                // This is the simpliest integration. We just
                // move forward a single timestep...
            for (unsigned c=0; c<N; ++c) x(c) = _density[c];
            // Multiply(r, AMat, x, N);
            for (unsigned c=0; c<N; ++c) _density[c] = r(c);

        } else if (diffusion == Diffusion::SOR) {

                // This is successive over relaxation. It's a iterative method similar
                // to Gauss-Seidel. But we have an extra factor, the relaxation factor, 
                // that can be used to adjust the way in which the system converges. 
                //
                // The choice of relaxation factor has an effect on the rate of convergence.
                // However, it's not clear how we should pick the relaxation factor.
                //
                // An advantage of this method is it can be done in-place... It doesn't
                // require any extra space.
                //
                // One possibility is that we should allow the relaxation factor to evolve
                // over several frames. That is, we increase or decrease the factor every
                // frame (within the range of 0 to 2) to improve the convergence of the 
                // next frame.
                //
                // We can calculate the ideal relaxation factor for a (positive definite)
                // tridiagonal matrix. Even though our matrix doesn't meet this restriction
                // the relaxation factor many be close to ideal for us. To calculate that,
                // we need the spectral radius of the associated Jacobi matrix.

                // We should start with an approximate result. We can just start with the
                // previous frame's result -- but maybe there is a better starting point?
                // (maybe stepping forward 3/4 of a timestep would be a good starting point?)

            for (unsigned i=0; i<N; ++i)
                _prevDensity[i] = _density[i];
                
            float gamma = 1.25f;    // relaxation factor

            static bool useGeneralA = false;
            if (useGeneralA) {

                const unsigned iterations = 15;
                for (unsigned k = 0; k<iterations; ++k) {
                    for (unsigned i = 0; i < N; ++i) {
                        float A = _prevDensity[i];

                            // these loops work oddly simply in this situation
                            // (but of course we can simplify because our matrix is sparse)
                        for (unsigned j = 0; j < i; ++j)
                            A -= AMat(i, j) * _density[j];
                        for (unsigned j = i+1; j < N; ++j)
                            A -= AMat(i, j) * _density[j];

                        _density[i] = (1.f-gamma) * _density[i] + gamma * A / AMat(i, i);
                    }
                }

            } else {

                const unsigned wh = _dimensions[0] + 2;

                const float dt = 1.0f / 60.f;
                const float a = 5.f * dt;
                const float a0 = 1.f + 4.f * a;
                const float a1 = -a;

                const unsigned iterations = 15;
                for (unsigned k = 0; k<iterations; ++k) {

                    for (unsigned y=1; y<wh-1; ++y) {
                        for (unsigned x=1; x<wh-1; ++x) {
                            const unsigned i = y*wh+x;
                            float A = _prevDensity[i];

                            A -= a1 * _density[i-1];
                            A -= a1 * _density[i+1];
                            A -= a1 * _density[i-wh];
                            A -= a1 * _density[i+wh];

                            _density[i] = (1.f-gamma) * _density[i] + gamma * A / a0;
                        }
                    }

                }

            }

        }
    }

    void FluidSolver2D::Tick(const Settings& settings)
    {
        auto D = _pimpl->_dimensions[0];
        assert(_pimpl->_dimensions[1] == _pimpl->_dimensions[0]);

        auto N = (D+2) * (D+2);
        for (unsigned c=0; c<N; ++c) {
            _pimpl->_density[c] += _pimpl->_prevDensity[c];
            _pimpl->_velU[c] += _pimpl->_prevVelU[c];
            _pimpl->_velV[c] += _pimpl->_prevVelV[c];
        }

        _pimpl->DensityDiffusion(settings._deltaTime);

        for (unsigned c=0; c<N; ++c) {
            _pimpl->_prevVelU[c] = 0.f;
            _pimpl->_prevVelV[c] = 0.f;
            _pimpl->_prevDensity[c] = 0.f;
        }
    }

    void FluidSolver2D::AddDensity(UInt2 coords, float amount)
    {
        if (coords[0] < _pimpl->_dimensions[0] && coords[1] < _pimpl->_dimensions[1]) {
            unsigned i = coords[0] + coords[1] * (_pimpl->_dimensions[0] + 2);
            _pimpl->_prevDensity[i] += amount;
        }
    }

    void FluidSolver2D::AddVelocity(UInt2 coords, Float2 vel)
    {
        if (coords[0] < _pimpl->_dimensions[0] && coords[1] < _pimpl->_dimensions[1]) {
            unsigned i = coords[0] + coords[1] * (_pimpl->_dimensions[0] + 2);
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
            _pimpl->_dimensions, _pimpl->_density.get(),
            _pimpl->_velU.get(), _pimpl->_velV.get());
    }

    UInt2 FluidSolver2D::GetDimensions() const { return _pimpl->_dimensions; }

    FluidSolver2D::FluidSolver2D(UInt2 dimensions)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_dimensions = dimensions;
        auto N = (dimensions[0]+2) * (dimensions[1]+2);
        _pimpl->_velU = std::make_unique<float[]>(N);
        _pimpl->_velV = std::make_unique<float[]>(N);
        _pimpl->_density = std::make_unique<float[]>(N);
        _pimpl->_prevVelU = std::make_unique<float[]>(N);
        _pimpl->_prevVelV = std::make_unique<float[]>(N);
        _pimpl->_prevDensity = std::make_unique<float[]>(N);

        for (unsigned c=0; c<N; ++c) {
            _pimpl->_velU[c] = 0.f;
            _pimpl->_velV[c] = 0.f;
            _pimpl->_density[c] = 0.f;
            _pimpl->_prevVelU[c] = 0.f;
            _pimpl->_prevVelV[c] = 0.f;
            _pimpl->_prevDensity[c] = 0.f;
        }

        _pimpl->x = VectorX(N);
        _pimpl->r = VectorX(N);
        _pimpl->d = VectorX(N);
        _pimpl->s = VectorX(N);
        _pimpl->q = VectorX(N);

        const float dt = 1.0f / 60.f;
        float a = 5.f * dt;

        auto w = _pimpl->_dimensions[0]+2;
        auto AMat = [w, a](unsigned i, unsigned j)
            {
                if (i == j) return 1.f + 4.f*a;
                auto x0 = (i%w), y0 = i/w;
                auto x1 = (j%w), y1 = j/w;
                if (    (std::abs(int(x0)-int(x1)) == 1 && y0 == y1)
                    ||  (std::abs(int(y0)-int(y1)) == 1 && x0 == x1)) {
                        // Compare with the Crank-Nicolson method, which
                        // blends values at t-1 with values at t
                    return -a;   
                }
                return 0.f;
            };

        _pimpl->precon = MatrixX(N, N);
        _pimpl->precon.fill(0.f);
            
        for (unsigned i=0; i<N; ++i) {
            float a = AMat(i, i);
            for (unsigned k=0; k<i; ++k) {
                float l = _pimpl->precon(i, k);
                a -= l*l;
            }
            _pimpl->precon(i,i) = XlSqrt(a);

            if (i != 0) {
                for (unsigned j=i+1; j<N; ++j) {
                    float aij = AMat(i, j);
                    for (unsigned k=0; k<i-1; ++k) {
                        aij -= _pimpl->precon(i, k) * _pimpl->precon(j, k);
                    }
                    _pimpl->precon(j, i) = aij / a;
                }
            }
        }

        _pimpl->AMat = std::function<float(unsigned, unsigned)>(AMat);
    }

    FluidSolver2D::~FluidSolver2D(){}

    FluidSolver2D::Settings::Settings()
    {
        _deltaTime = 1.0f/60.f;
        _viscosity = 0.f;
        _diffusionRate = 0.f;
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
        const float* density, const float* velocityU, const float* velocityV)
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

            auto density = uploads.Transaction_Immediate(desc, densityPkt.get());
            auto velU = uploads.Transaction_Immediate(desc, velUPkt.get());
            auto velV = uploads.Transaction_Immediate(desc, velVPkt.get());

            metalContext.BindPS(
                MakeResourceList(
                    Metal::ShaderResourceView(density->GetUnderlying()),
                    Metal::ShaderResourceView(velU->GetUnderlying()),
                    Metal::ShaderResourceView(velV->GetUnderlying())));

            const ::Assets::ResChar* pixelShader;
            if (debuggingMode == FluidDebuggingMode::Density) {
                pixelShader = "game/xleres/cfd/debug.sh:ps_density:ps_*";
            } else {
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
        props.Add(u("DeltaTime"), DefaultGet(Obj, _deltaTime),  DefaultSet(Obj, _deltaTime));
        props.Add(u("Viscosity"), DefaultGet(Obj, _viscosity),  DefaultSet(Obj, _viscosity));
        props.Add(u("DiffusionRate"), DefaultGet(Obj, _diffusionRate),  DefaultSet(Obj, _diffusionRate));
        init = true;
    }
    return props;
}

