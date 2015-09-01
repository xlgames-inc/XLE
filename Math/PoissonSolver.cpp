// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PoissonSolver.h"
#include "PoissonSolverDetail.h"
#include "./Math.h"
#include <vector>
#include <assert.h>

#pragma warning(disable:4714)
#pragma push_macro("new")
#undef new
#include <Eigen/Dense>
#pragma pop_macro("new")


namespace XLEMath
{
    using namespace PoissonSolverInternal;

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Vec>
        static void RunSOR(Vec& xv, PoissonSolver::AMat2D A, const Vec& b, float relaxationFactor)
    {
        for (unsigned y=1; y<A._wh-1; ++y) {
            for (unsigned x=1; x<A._wh-1; ++x) {
                const unsigned i = y*A._wh+x;
                float v = b[i];

                v -= A._a1 * xv[i-1];
                v -= A._a1 * xv[i+1];
                v -= A._a1 * xv[i-A._wh];
                v -= A._a1 * xv[i+A._wh];

                xv[i] = (1.f-relaxationFactor) * xv[i] + relaxationFactor * v / A._a0;
            }
        }
    }

    template<typename Vec>
        static void RunSOR(Vec& xv, std::function<float(unsigned, unsigned)>& A, const Vec& b, float relaxationFactor)
    {
        for (unsigned i = 0; i < N; ++i) {
            float v = b[i];

                // these loops work oddly simply in this situation
                // (but of course we can simplify because our matrix is sparse)
            for (unsigned j = 0; j < i; ++j)
                v -= A(i, j) * x[j];
            for (unsigned j = i+1; j < N; ++j)
                v -= A(i, j) * x[j];

            xv[i] = (1.f-relaxationFactor) * x[i] + relaxationFactor * v / A(i, i);
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    using VectorX = Eigen::VectorXf;

    static ScalarField1D AsScalarField1D(VectorX& v) { return ScalarField1D { v.data(), (unsigned)v.size() }; }

    class Solver_PlainCG
    {
    public:
        template<typename Mat>
            unsigned Execute(ScalarField1D& x, const Mat& A, const ScalarField1D& b);

        Solver_PlainCG(unsigned N);
        ~Solver_PlainCG();

    protected:
        VectorX _r, _d, _q;
        unsigned _N;
    };

    template<typename Mat>
        unsigned Solver_PlainCG::Execute(ScalarField1D& x, const Mat& A, const ScalarField1D& b)
    {
            // This is the basic "conjugate gradient" method; with no special thrills
            // returns the number of iterations
        const auto rhoThreshold = 1e-10f;
        const auto maxIterations = 13u;

        auto rAsField = AsScalarField1D(_r);
        Multiply(rAsField, A, x, _N);
        for (unsigned c=0; c<b._count; ++c) {
            _r[c] = b[c] - _r[c];
            _d[c] = _r[c];
        }
        ZeroBorder(_r, GetWH(A));
        ZeroBorder(_d, GetWH(A));
        float rho = _r.dot(_r);

        ZeroBorder(_q, GetWH(A));
        unsigned k=0;
        if (XlAbs(rho) > rhoThreshold) {
            for (; k<maxIterations; ++k) {
            
                Multiply(_q, A, _d, _N);
                float dDotQ = _d.dot(_q);
                float alpha = rho / dDotQ;
                assert(isfinite(alpha) && !isnan(alpha));
                for (unsigned i=0; i<_N; ++i) {
                     x[i] += alpha * _d[i];
                    _r[i] -= alpha * _d[i];
                }
            
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
#if 0
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
        template<typename Vec, typename Mat>
            unsigned Execute(Vec& x, const Mat& A, const Vec& b);

        Solver_Multigrid(unsigned wh, unsigned levels);
        ~Solver_Multigrid();

    protected:
        std::vector<VectorX> _subResidual;
        std::vector<VectorX> _subB;
        std::vector<unsigned> _subWh;
        unsigned _N;
        unsigned _wh;
    };

    static PoissonSolver::AMat2D ChangeResolution(PoissonSolver::AMat2D i, unsigned layer)
    {
            // 'a' values are proportion to the square of N
            // N quarters with every layer (width and height half)
        float scale = std::pow(4.f, float(layer));
        auto result = i;
        result._a0 /= scale;
        result._a1 /= scale;
        return result;
    }

    template<typename Vec>
        static void Restrict(Vec& dst, const Vec& src, unsigned dstWh, unsigned srcWh)
    {
            // This is the "restrict" operator
            // There are many possible methods for this
            // We're going to start with a simple method that
            // assumes that the sample values are at the corners
            // of the grid. This way we can just use the box
            // mipmap operator; as so...
            // If we have more complex boundary conditions, we
            // might want to move the sames to the center of the 
            // grid cells; which would mean that we should 
            // use a more complex operator here
        for (unsigned y=1; y<dstWh-1; ++y) {
            for (unsigned x=1; x<dstWh-1; ++x) {
                unsigned sx = (x-1)*2+1, sy = (y-1)*2+1;
                dst[y*dstWh+x]
                    = .25f * src[(sy+0)*srcWh+(sx+0)]
                    + .25f * src[(sy+0)*srcWh+(sx+1)]
                    + .25f * src[(sy+1)*srcWh+(sx+0)]
                    + .25f * src[(sy+1)*srcWh+(sx+1)]
                    ;
            }
        }
    }

    template<typename Vec>
        static void Prolongate(Vec& dst, const Vec& src, unsigned dstWh, unsigned srcWh)
    {
            // This is the "prolongate" operator.
            // As with the restrict operator, we're going
            // to use a simple bilinear sample, as if each
            // layer was a mipmap.

        for (unsigned y=1; y<dstWh-1; ++y) {
            for (unsigned x=1; x<dstWh-1; ++x) {
                float sx = (x-1)/2.f + 1.f;
                float sy = (y-1)/2.f + 1.f;
                float sx0 = XlFloor(sx), sy0 = XlFloor(sy);
                float a = sx - sx0, b = sy - sy0;
                float weights[] = {
                    (1.0f - a) * (1.0f - b),
                    a * (1.0f - b),
                    (1.0f - a) * b,
                    a * b
                };
                dst[y*dstWh+x]
                    = weights[0] * src[(unsigned(sy0)+0)*srcWh+unsigned(sx0)]
                    + weights[1] * src[(unsigned(sy0)+0)*srcWh+unsigned(sx0)+1]
                    + weights[2] * src[(unsigned(sy0)+1)*srcWh+unsigned(sx0)]
                    + weights[3] * src[(unsigned(sy0)+1)*srcWh+unsigned(sx0)+1]
                    ;
            }
        }
    }

    template<typename Vec, typename Mat>
        unsigned Solver_Multigrid::Execute(Vec& x, const Mat& A, const Vec& b)
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

        float gamma = 1.25f;                // relaxation factor
        const auto preSmoothIterations = 3u;
        const auto postSmoothIterations = 3u;
        const auto stepSmoothIterations = 1u;
        auto iterations = 0u;

            // pre-smoothing (SOR method -- can be done in place)
        for (unsigned k = 0; k<preSmoothIterations; ++k)
            RunSOR(x, A, b, gamma);
        iterations += preSmoothIterations;

            // ---------- step down ----------
        unsigned activeWh = A.wh;
        auto* prevLayer = &x;
        auto* prevB = &b;
        auto gridCount = unsigned(_subResidual.size());
        for (unsigned g=0; g<gridCount; ++g) {
            auto prevWh = activeWh;
            activeWh = _subWh[g];
            auto& dst = _subResidual[g];
            auto& dstB = _subB[g];

            Restrict(dst, *prevLayer, activeWh, prevWh);
            Restrict(dstB, *prevB, activeWh, prevWh);   // is it better to downsample B from the top most level each time?

            auto SA = ChangeResolution(A, g+1);
            SA.wh = activeWh;
            for (unsigned k = 0; k<stepSmoothIterations; ++k)
                RunSOR(dst, SA, dstB, gamma);
            iterations += stepSmoothIterations;

            prevLayer = &dst;
            prevB = &dstB;
        }

            // ---------- step up ----------
        for (unsigned g=gridCount-1; g>0; --g) {
            auto& src = _subResidual[g];
            auto& dst = _subResidual[g-1];
            auto& dstB = _subB[g-1];
            unsigned srcWh = _subWh[g];
            unsigned dstWh = _subWh[g-1];

            Prolongate(dst, src, dstWh, srcWh);

            auto SA = ChangeResolution(A, g-1+1);
            SA.wh = dstWh;
            for (unsigned k = 0; k<stepSmoothIterations; ++k)
                RunSOR(dst, SA, dstB, gamma);
            iterations += stepSmoothIterations;
        }

            // finally, step back onto 'x'
        Prolongate(x, _subResidual[0], A.wh, _subWh[0]);

            // post-smoothing (SOR method -- can be done in place)
        for (unsigned k = 0; k<postSmoothIterations; ++k)
            RunSOR(x, A, b, gamma);
        iterations += postSmoothIterations;

        return iterations;
    }

    Solver_Multigrid::Solver_Multigrid(unsigned wh, unsigned levels)
    {
            // todo -- need to consider the border for "wh"
        _N = wh*wh;
        _wh = wh;
        for (unsigned c=0; c<levels; c++) {
            wh = ((wh-2) >> 1) + 2;
            unsigned n = wh*wh;
            VectorX subr(n); subr.fill(0.f);
            _subResidual.push_back(std::move(subr));
            VectorX subb(n); subb.fill(0.f);
            _subB.push_back(std::move(subb));
            _subWh.push_back(wh);
        }
    }

    Solver_Multigrid::~Solver_Multigrid() {}

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PoissonSolver::Pimpl
    {
    public:
        AMat2D _A;
        VectorX _tempBuffer;
    };

    static PoissonSolver::AMat2D GetEstimate(const PoissonSolver::AMat2D& A, float estimationFactor)
    {
        const float estA1 = estimationFactor * A._a1;  // used when calculating a starting estimate
        return PoissonSolver::AMat2D { A._wh, 1.f - 4.f * estA1, estA1 };
    }

    unsigned PoissonSolver::Solve(ScalarField1D x, const ScalarField1D& b, Method solver)
    {
        //
        // Here is our basic solver for Poisson equations (such as the heat equation).
        // It's a complex partial differential equation, so the solution is complex.
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
        //  * conjugate gradient methods with complex preconditioners
        //      - (such as using a parallel multi-grid as a preconditioner for
        //          the conjugate gradient method)
        //
        // See: https://www.math.ucla.edu/~jteran/papers/MST10.pdf for a method that
        // uses a multigrid preconditioner for the conjugate gradient method (which
        // can be parallelized) with complex boundary conditions support.
        //
        // We must also consider the boundary conditions in this step.
        // 

            // maybe we could adapt this based on the amount of noise in the system? 
            // In low noise systems, explicit euler seems very close to correct
        static float estimateFactor = .75f; 
        const auto& A = _pimpl->_A;
        auto estMatA = GetEstimate(A, estimateFactor);
        const auto N = GetN(A);
        const auto wh = GetWH(A);

        assert(x._count == N);
        assert(b._count == N);

            // if b is an alias of x, we need to copy the data into
            // a safe place
        ScalarField1D workingB = b;
        if (workingB._u == x._u) {
            for (unsigned i=0; i<N; ++i)
                _pimpl->_tempBuffer[i] = b._u[i];
            workingB._u = _pimpl->_tempBuffer.data();
        }

        if (solver == Method::PlainCG || solver == Method::CG_Precon || solver == Method::Multigrid) {

            for (unsigned qy=1; qy<wh-1; ++qy) {
                for (unsigned qx=1; qx<wh-1; ++qx) {
                    auto i = qy*wh+qx;

                        // set an initial estimate using
                        // explicit euler. We'll march forward part of
                        // the timestep, and then refine the estimate
                        // from there using the iterative implicit method.
                    float v = estMatA._a0 * b[i];
                    v += estMatA._a1 * b[i-1];
                    v += estMatA._a1 * b[i+1];
                    v += estMatA._a1 * b[i-wh];
                    v += estMatA._a1 * b[i+wh];
                    x._u[i] = v;
                }
            }

            ZeroBorder(x._u, wh);

            auto iterations = 0u;
            if (solver == Method::PlainCG) {
                Solver_PlainCG solver(N);
                iterations = solver.Execute(x, A, workingB);
            } /*else if (solver == Method::CG_Precon) {
                Solver_PreconCG solver(N);
                iterations = solver.Execute(x, A, workingB, _bandedPrecon);
            } else if (solver == Method::Multigrid) {
                Solver_Multigrid solver(wh, 2);
                iterations = solver.Execute(x, A, workingB);
            }*/

            return iterations;

        } else if (solver == Method::ForwardEuler) {
        
                // This is the simpliest integration. We just
                // move forward a single timestep...
            float a0 = 1.f - 4.f * -A._a1;
            float a1 = -A._a1;
            for (unsigned iy=1; iy<wh-1; ++iy)
                for (unsigned ix=1; ix<wh-1; ++ix) {
                    const unsigned i = iy*wh+ix;
                    float v = a0 * workingB[i];
                    v += a1 * workingB[i-1];
                    v += a1 * workingB[i+1];
                    v += a1 * workingB[i-wh];
                    v += a1 * workingB[i+wh];
                    x._u[i] = v;
                }

            ZeroBorder(x._u, wh);
            return 1;

        } else if (solver == Method::SOR) {

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

            float gamma = 1.25f;    // relaxation factor
            const auto iterations = 15u;

            for (unsigned k = 0; k<iterations; ++k)
                RunSOR(x, A, workingB, gamma);

            ZeroBorder(x._u, wh);
            return iterations;

        }

        return 0;
    }


    PoissonSolver::PoissonSolver(AMat2D A)
    {
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_A = A;
        _pimpl->_tempBuffer = VectorX(A._wh*A._wh);
    }

    PoissonSolver::PoissonSolver(PoissonSolver&& moveFrom)
    : _pimpl(std::move(moveFrom._pimpl)) {}

    PoissonSolver& PoissonSolver::operator=(PoissonSolver&& moveFrom)
    {
        _pimpl = std::move(moveFrom._pimpl);
        return *this;
    }

    PoissonSolver::PoissonSolver() {}
    PoissonSolver::~PoissonSolver() {}
}

