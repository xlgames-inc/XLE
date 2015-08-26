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

#pragma warning(disable:4505)       // 'SceneEngine::CalculateIncompleteCholesky' : unreferenced local function has been removed


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
    
    template<typename Vec>
        static void Multiply(Vec& dst, const std::function<float(unsigned, unsigned)>& A, const Vec& b, unsigned N)
    {
        for (unsigned i=0; i<N; ++i) {
            float d = 0.f;
            for (unsigned j=0; j<N; ++j) {
                d += A(i, j) * b(j);
            }
            dst(i) = d;
        }
    }
        
    struct AMat2D
    {
        unsigned wh;
        float a0;
        float a1;
    };

    template <typename Vec>
        static void Multiply(Vec& dst, AMat2D A, const Vec& b, unsigned N)
    {
        for (unsigned y=1; y<A.wh-1; ++y) {
            for (unsigned x=1; x<A.wh-1; ++x) {
                const unsigned i = y*A.wh + x;

                float v = A.a0 * b(i);
                v += A.a1 * b(i-1);
                v += A.a1 * b(i+1);
                v += A.a1 * b(i-A.wh);
                v += A.a1 * b(i+A.wh);

                dst(i) = v;
            }
        }
    }

    template <typename Vec>
        static void ZeroBorder(Vec& v, unsigned wh)
    {
        for (unsigned i = 1; i < wh - 1; ++i) {
            v(i) = 0.f;             // top
            v(i+(wh-1)*wh) = 0.f;   // bottom
            v(i*wh) = 0.f;          // left
            v(i*wh+(wh-1)) = 0.f;   // right
        }

            // 4 corners
        v(0) = 0.f;
        v(wh-1) = 0.f;
        v((wh-1)*wh) = 0.f;
        v((wh-1)*wh+wh-1) = 0.f;
    }

    template <typename Vec>
        static void ZeroBorder(Vec& v, const AMat2D & A) { ZeroBorder(v, A.wh); }

    template <typename Vec, typename Unknown>
        static void ZeroBorder(Vec&, const Unknown&) {}

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

    class SparseBandedMatrix
    {
    public:
        const int *_bands;
        unsigned _bandCount;
        MatrixX _underlying;

        SparseBandedMatrix() { _bandCount = 0; _bands = nullptr; }
        SparseBandedMatrix(MatrixX&& underlying, const int bands[], unsigned bandCount)
            : _underlying(std::move(underlying))
            { _bands = bands; _bandCount = bandCount; }
        ~SparseBandedMatrix() {}
    };

    template<typename Vec>
        static void SolveLowerTriangular(Vec& x, const SparseBandedMatrix& M, const Vec& b, unsigned N)
    {
            // assuming the last "band" in the matrix is the diagonal aprt
        assert(M._bandCount > 0 && M._bands[M._bandCount-1] == 0);

            // solve: M * dst = b
            // for a lower triangular matrix, using forward substitution
            // this is for a sparse banded matrix, with the bands described by "bands"
            //      -- note that we can improve this further by writing implementations for
            //          common cases (eg, 2D, 3D, etc)
        for (unsigned i=0; i<N; ++i) {
            float d = b(i);
            for (unsigned j=0; j<M._bandCount-1; ++j) {
                int j2 = int(i) + M._bands[j];
                if (j2 >= 0 && j2 < int(i))  // with agressive unrolling, we should avoid this condition
                    d -= M._underlying(i, j) * x(j2);
            }
            x(i) = d / M._underlying(i, M._bandCount-1);
        }
    }

    template<typename Vec>
        static void Multiply(Vec& dst, const SparseBandedMatrix& A, const Vec& b, unsigned N)
    {
        for (unsigned i=0; i<N; ++i) {
            float d = 0.f;
            for (unsigned j=0; j<A._bandCount; ++j) {
                int j2 = int(i) + A._bands[j];
                if (j2 >= 0 && j2 < int(N))  // with agressive unrolling, we should avoid this condition
                    d += A._underlying(i, j) * b(j2);
            }
            dst(i) = d;
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    class Solver_PlainCG
    {
    public:

        template<typename Vec, typename Mat>
            unsigned Execute(Vec& x, const Mat& A, const Vec& b);

        Solver_PlainCG(unsigned N);
        ~Solver_PlainCG();

    protected:
        VectorX _r, _d, _q;
        unsigned _N;
    };

    template<typename Vec, typename Mat>
        unsigned Solver_PlainCG::Execute(Vec& x, const Mat& A, const Vec& b)
    {
            // This is the basic "conjugate gradient" method; with no special thrills
            // returns the number of iterations
        const auto rhoThreshold = 1e-10f;
        const auto maxIterations = 13u;

        Multiply(_r, A, x, _N);
        ZeroBorder(_r, A);
        _r = b - _r;
        _d = _r;
        float rho = _r.dot(_r);

        ZeroBorder(_q, A);
        unsigned k=0;
        if (XlAbs(rho) > rhoThreshold) {
            for (; k<maxIterations; ++k) {
            
                Multiply(_q, A, _d, _N);
                float dDotQ = _d.dot(_q);
                float alpha = rho / dDotQ;
                assert(isfinite(alpha) && !isnan(alpha));
                 x += alpha * _d;
                _r -= alpha * _d;
            
                float rhoOld = rho;
                rho = _r.dot(_r);
                if (XlAbs(rho) < rhoThreshold) break;
                float beta = rho / rhoOld;
                assert(isfinite(beta) && !isnan(beta));
            
                    // we can skip the border for the following...
                    // (but that requires different cases for 2D/3D)
                for (unsigned i=0; i<_N; ++i)
                    _d(i) = _r(i) + beta * _d(i);

            }
        }

        return k;
    }

    Solver_PlainCG::Solver_PlainCG(unsigned N)
    : _r(N), _d(N), _q(N)
    {
        _N = N;
    }

    Solver_PlainCG::~Solver_PlainCG() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class Solver_PreconCG
    {
    public:

        template<typename Vec, typename Mat, typename PreCon>
            unsigned Execute(Vec& x, const Mat& A, const Vec& b, const PreCon& precon);

        Solver_PreconCG(unsigned N);
        ~Solver_PreconCG();

    protected:
        VectorX _r, _d, _q;
        VectorX _s;
        unsigned _N;
    };

    template<typename Vec, typename Mat, typename PreCon>
        unsigned Solver_PreconCG::Execute(Vec& x, const Mat& A, const Vec& b, const PreCon& precon)
    {
            // This is the conjugate gradient method with a preconditioner.
            //
            // Note that for our CFD operations, the preconditioner often comes out very similar
            // to "A" -- so it's not clear whether it will help in any significant way.
            //
            // See http://www.cs.cmu.edu/~quake-papers/painless-conjugate-gradient.pdf 
            // for for detailed description of conjugate gradient methods!
            // 
            // see also reference at http://math.nist.gov/iml++/
        const auto rhoThreshold = 1e-10f;
        const auto maxIterations = 13u;

        Multiply(_r, A, x, _N);    // r = AMat * x
        ZeroBorder(_r, A);
        _r = b - _r;
            
        SolveLowerTriangular(_d, precon, _r, _N);
            
        // #if defined(_DEBUG)
        //     {
        //             // testing "SolveLowerTriangular"
        //         VectorX t(_N);
        //         Multiply(t, precon, _d, _N);
        //         for (unsigned c=0; c<_N; ++c) {
        //             float z = t(c), y = _r(c);
        //             assert(Equivalent(z, y, 1e-1f));
        //         }
        //     }
        // #endif
            
        float rho = _r.dot(_d);
        // float rho0 = rho;

        ZeroBorder(_q, A);
            
        unsigned k=0;
        if (XlAbs(rho) > rhoThreshold) {
            for (; k<maxIterations; ++k) {
            
                    // Note that all of the vectors and matrices
                    // used here are quite sparse! So we need to
                    // simplify the operation shere to take advantage 
                    // of that sparseness.
                    // Multiply by AMat can be replaced with a specialized
                    // operation. Unfortunately the dot products can't be
                    // simplified, because the vectors already have only one
                    // element per cell.
            
                Multiply(_q, A, _d, _N);
                float dDotQ = _d.dot(_q);
                float alpha = rho / dDotQ;
                assert(isfinite(alpha) && !isnan(alpha));
                 x += alpha * _d;
                _r -= alpha * _q;
            
                SolveLowerTriangular(_s, precon, _r, _N);
                float rhoOld = rho;
                rho = _r.dot(_s);
                if (XlAbs(rho) < rhoThreshold) break;
                assert(rho < rhoOld);
                float beta = rho / rhoOld;
                assert(isfinite(beta) && !isnan(beta));
            
                for (unsigned i=0; i<_N; ++i)
                    _d(i) = _s(i) + beta * _d(i);
            }
        }

        return k;
    }

    Solver_PreconCG::Solver_PreconCG(unsigned N)
    : _r(N), _d(N), _q(N), _s(N)
    {
        _N = N;
    }

    Solver_PreconCG::~Solver_PreconCG() {}

///////////////////////////////////////////////////////////////////////////////////////////////////


    class Solver_Multigrid
    {
    public:
        template<typename Vec, typename Mat, typename PreCon>
            unsigned Execute(Vec& x, const Mat& A, const Vec& b, const PreCon& precon);

        Solver_Multigrid(unsigned wh, unsigned levels);
        ~Solver_Multigrid();

    protected:
        std::vector<VectorX> _residual;
        unsigned _N;
        unsigned _wh;
    };

    template<typename Vec, typename Mat, typename PreCon>
        unsigned Solver_Multigrid::Execute(Vec& x, const Mat& A, const Vec& b, const PreCon& precon)
    {
        //
        // Here is our basic V-cycle:
        //  * start with the finest grid
        //  * perform pre-smoothing
        //  * iteratively reduce down:
        //      * "restrict" onto next more coarse grid
        //      * smooth result
        //  * iteratively expand upwards:
        //      * "prolongonate" up to next more fine grid
        //      * smooth result
        //  * do post-smoothing
        //
        //      Note that this is often done in parallel, by dividing the fine
        //      grids across multiple processors.
        //

            // pre-smoothing
        float gamma = 1.25f;                // relaxation factor
        const auto preSmoothIterations = 3u;
        const auto postSmoothIterations = 3u;

        for (unsigned k = 0; k<preSmoothIterations; ++k) {
            for (unsigned y=1; y<wh-1; ++y) {
                for (unsigned x=1; x<wh-1; ++x) {
                    const unsigned i = y*wh+x;
                    float A = b[i];

                    A -= a1 * x[i-1];
                    A -= a1 * x[i+1];
                    A -= a1 * x[i-wh];
                    A -= a1 * x[i+wh];

                    x[i] = (1.f-gamma) * _density[i] + gamma * A / a0;
                }
            }
        }

            // step down
        auto gridCount = int(_residual.size());
        for (int g=0; g<gridCount; ++g) {

        }
            // step up
        for (int g=gridCount-1; g>0; ++g) {
        }

            // post-smoothing
        for (unsigned k = 0; k<postSmoothIterations; ++k) {
            for (unsigned y=1; y<wh-1; ++y) {
                for (unsigned x=1; x<wh-1; ++x) {
                    const unsigned i = y*wh+x;
                    float A = b[i];

                    A -= a1 * x[i-1];
                    A -= a1 * x[i+1];
                    A -= a1 * x[i-wh];
                    A -= a1 * x[i+wh];

                    x[i] = (1.f-gamma) * _density[i] + gamma * A / a0;
                }
            }
        }
    }

    Solver_Multigrid::Solver_Multigrid(unsigned wh, unsigned levels)
    {
        _N = wh*wh;
        _wh = wh;
        unsigned n = _N;
        for (unsigned c=0; c<levels; c++) {
            _residual.push_back(VectorX(n));
            n <<= 1;
        }
    }

    Solver_Multigrid::~Solver_Multigrid() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

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

        VectorX _x, _b;

        int _bands[5];
        SparseBandedMatrix _bandedPrecon;
        std::function<float(unsigned, unsigned)> AMat;

        void DensityDiffusion(const FluidSolver2D::Settings& settings);
    };

    void FluidSolver2D::Pimpl::DensityDiffusion(const FluidSolver2D::Settings& settings)
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

        enum class Diffusion { CG_Precon, PlainCG, ForwardEuler, SOR };
        auto diffusion = (Diffusion)settings._diffusionMethod;
        static bool useGeneralA = false;
        static float estimateFactor = .75f; // maybe we could adapt this based on the amount of noise in the system? In low noise systems, explicit euler seems very close to correct
        static float diffFactor = 5.f;

        auto N = (_dimensions[0]+2) * (_dimensions[1]+2);

        const unsigned wh = _dimensions[0] + 2;

        const float a = diffFactor * settings._deltaTime;
        const float a0 = 1.f + 4.f * a;
        const float a1 = -a;
        const float estA = estimateFactor * a;  // used when calculating a starting estimate

        if (diffusion == Diffusion::PlainCG || diffusion == Diffusion::CG_Precon) {

            for (unsigned y=1; y<wh-1; ++y) {
                for (unsigned x=1; x<wh-1; ++x) {
                    unsigned i = y*wh+x;
                    _b(i) = _density[i];

                        // set an initial estimate using
                        // explicit euler. We'll march forward part of
                        // the timestep, and then refine the estimate
                        // from there using the iterative implicit method.
                    float v = (1.0f - 4.f * estA) * _density[i];
                    v += estA * _density[i-1];
                    v += estA * _density[i+1];
                    v += estA * _density[i-wh];
                    v += estA * _density[i+wh];
                    _x(i) = v;
                }
            }
            ZeroBorder(_x, wh);
            ZeroBorder(_b, wh);

            auto iterations = 0u;
            if (diffusion == Diffusion::PlainCG) {
                Solver_PlainCG solver(N);
                if (useGeneralA) {
                    iterations = solver.Execute(_x, AMat, _b);
                } else {
                    iterations = solver.Execute(_x, AMat2D { wh, a0, a1 }, _b);
                }
            } else {
                Solver_PreconCG solver(N);
                if (useGeneralA) {
                    iterations = solver.Execute(_x, AMat, _b, _bandedPrecon);
                } else {
                    iterations = solver.Execute(_x, AMat2D { wh, a0, a1 }, _b, _bandedPrecon);
                }
            }

            LogInfo << "Diffusion took: " << iterations << " iterations.";
            
            for (unsigned c=0; c<N; ++c) {
                assert(isfinite(_x(c)) && !isnan(_x(c)));
                _density[c] = _x(c);
            }

        } else if (diffusion == Diffusion::ForwardEuler) {
        
                // This is the simpliest integration. We just
                // move forward a single timestep...
            for (unsigned c=0; c<N; ++c) _prevDensity[c] = _density[c];

            for (unsigned y=1; y<wh-1; ++y)
                for (unsigned x=1; x<wh-1; ++x) {
                    const unsigned i = y*wh+x;
                    float A = (1.0f - 4.f * a) * _prevDensity[i];
                    A += a * _prevDensity[i-1];
                    A += a * _prevDensity[i+1];
                    A += a * _prevDensity[i-wh];
                    A += a * _prevDensity[i+wh];
                    
                    _density[i] = A;
                }

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
            const auto iterations = 15u;

            if (useGeneralA) {

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

        _pimpl->DensityDiffusion(settings);

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

    static MatrixX CalculateIncompleteCholesky(std::function<float(unsigned, unsigned)>& mat, unsigned N)
    {
        MatrixX result(N, N);
        result.fill(0.f);
            
        for (unsigned i=0; i<N; ++i) {
            float a = mat(i, i);
            for (unsigned k=0; k<i; ++k) {
                float l = result(i, k);
                a -= l*l;
            }
            a = XlSqrt(a);
            result(i,i) = a;

            if (i != 0) {
                for (unsigned j=i+1; j<N; ++j) {
                    float aij = mat(i, j);
                    for (unsigned k=0; k<i; ++k) {
                        aij -= result(i, k) * result(j, k);
                    }
                    result(j, i) = aij / a;
                }
            }
        }

            // zero-out elements that are zero in the source element
            // (this is an aspect of the "incomplete" Cholesky factorization
            //      -- the result should have the same sparseness as the input)
        // for (unsigned i=0; i<N; ++i) {
        //     for (unsigned j=0; j<N; ++j) {
        //         auto v = mat(i, j);
        //         if (v == 0.f) result(i, j) = 0.f;
        //     }
        // }

        return result * result.transpose();
    }

    static MatrixX CalculateIncompleteCholesky(AMat2D mat, unsigned N)
    {
            //
            //  The final matrix we build will only hold values in
            //  the places where the input matrix also holds values.
            //  However, while building the matrix, we need to store
            //  and calculate values at every address. This means
            //  allocating a very large temporary matrix.
            //  We will return a compressed matrix with the unneeded
            //  bands removed
        MatrixX factorization(N, N);
        factorization.fill(0.f);
            
        for (unsigned i=0; i<N; ++i) {
            float a = mat.a0;
            for (unsigned k=0; k<i; ++k) {
                float l = factorization(i, k);
                a -= l*l;
            }
            a = XlSqrt(a);
            factorization(i,i) = a;

            if (i != 0) {
                for (unsigned j=i+1; j<N; ++j) {
                    float aij = ((j==i+1)||(j==i+mat.wh)) ? mat.a1 : 0.f;
                    for (unsigned k=0; k<i; ++k) {
                        aij -= factorization(i, k) * factorization(j, k);
                    }
                    factorization(j, i) = aij / a;
                }
            }
        }

            // 
            //  Our preconditioner matrix is "factorization" multiplied by
            //  it's transpose
            //

        int bands[] = { -int(mat.wh), -1, 1, mat.wh, 0 };
        MatrixX sparseMatrix(N, dimof(bands));
        for (unsigned i=0; i<N; ++i)
            for (unsigned j=0; j<dimof(bands); ++j) {
                int j2 = int(i) + bands[j];
                if (j2 >= 0 && j2 < int(N)) {

                        // Here, calculate M(i, j), where M
                        // is the factorization multiplied by its transpose
                    float A = 0.f;
                    for (unsigned k=0; k<N; ++k)
                        A += factorization(i, k) * factorization(j2, k);

                    sparseMatrix(i, j) = A;
                } else {
                    sparseMatrix(i, j) = 0.f;
                }
            }

        return std::move(sparseMatrix);
    }

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

        _pimpl->_x = VectorX(N);
        _pimpl->_b = VectorX(N);

        const float dt = 1.0f / 60.f;
        float a = 5.f * dt;

        auto wh = _pimpl->_dimensions[0]+2;
        auto AMat = [wh, a](unsigned i, unsigned j)
            {
                if (i == j) return 1.f + 4.f*a;
                // auto x0 = (i%wh), y0 = i/wh;
                // auto x1 = (j%wh), y1 = j/wh;
                // if (    (std::abs(int(x0)-int(x1)) == 1 && y0 == y1)
                //     ||  (std::abs(int(y0)-int(y1)) == 1 && x0 == x1)) {
                if (j==(i+1) || j==(i-1) || j==(i+wh) || j == (i-wh))
                    return -a;   
                return 0.f;
            };

        _pimpl->AMat = std::function<float(unsigned, unsigned)>(AMat);
        auto bandedPrecon = CalculateIncompleteCholesky( AMat2D { wh, 1.f + 4.f * a, -a }, N );

        _pimpl->_bands[0] = -int(wh);
        _pimpl->_bands[1] =  -1;
        _pimpl->_bands[2] =   1;
        _pimpl->_bands[3] =  wh;
        _pimpl->_bands[4] =   0;

        // #if defined(_DEBUG)
        //     for (unsigned i=0; i<N; ++i)
        //         LogInfo << bandedPrecon(i, 0) << ", " << bandedPrecon(i, 1) << ", " << bandedPrecon(i, 2) << ", " << bandedPrecon(i, 3) << ", " << bandedPrecon(i, 4);
        // 
        //     {
        //         auto fullPrecon = CalculateIncompleteCholesky(_pimpl->AMat, N);
        //         for (unsigned i=0; i<N; ++i) {
        //             int j2[] = { 
        //                 int(i) + _pimpl->_bands[0], 
        //                 int(i) + _pimpl->_bands[1],
        //                 int(i) + _pimpl->_bands[2], 
        //                 int(i) + _pimpl->_bands[3], 
        //                 int(i) + _pimpl->_bands[4]
        //             };
        //             for (unsigned j=0; j<dimof(j2); ++j) {
        //                 if (j2[j] >= 0 && j2[j] < int(N)) {
        //                     float a = bandedPrecon(i, j);
        //                     float b = fullPrecon(i, j2[j]);
        //                     assert(Equivalent(a, b, 1.e-3f));
        //                 }
        //             }
        //         }
        //     }
        // #endif

        _pimpl->_bandedPrecon = SparseBandedMatrix(std::move(bandedPrecon), _pimpl->_bands, dimof(_pimpl->_bands));
    }

    FluidSolver2D::~FluidSolver2D(){}

    FluidSolver2D::Settings::Settings()
    {
        _deltaTime = 1.0f/60.f;
        _viscosity = 0.f;
        _diffusionRate = 0.f;
        _diffusionMethod = 0;
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
        props.Add(u("DiffusionMethod"), DefaultGet(Obj, _diffusionMethod),  DefaultSet(Obj, _diffusionMethod));
        init = true;
    }
    return props;
}

