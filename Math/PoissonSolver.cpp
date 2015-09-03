// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PoissonSolver.h"
#include "PoissonSolverDetail.h"
#include "RegularNumberField.h"
#include "./Math.h"
#include "Vector.h"
#include "../ConsoleRig/Log.h"
#include "../Utility/PtrUtils.h"
#include <vector>
#include <assert.h>

#pragma warning(disable:4714)
#pragma push_macro("new")
#undef new
#include <Eigen/Dense>
#pragma pop_macro("new")

#pragma warning(disable:4505)       // 'SceneEngine::CalculateIncompleteCholesky' : unreferenced local function has been removed

namespace XLEMath
{
    using namespace PoissonSolverInternal;

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void RunSOR(ScalarField1D& xv, const AMat& A, const ScalarField1D& b, float relaxationFactor)
    {
        const auto width = GetWidth(A);
        const auto height = GetHeight(A);
        const auto depth = GetDepth(A);
        const auto bor = GetBorders(A);

        if (A._dimensionality==2) {
            for (unsigned y=bor[1]; y<height-bor[1]; ++y) {
                for (unsigned x=bor[0]; x<width-bor[0]; ++x) {
                    const unsigned i = y*width+x;
                    auto v = b[i];

                    v -= A._a1 * xv[i-1];
                    v -= A._a1 * xv[i+1];
                    v -= A._a1 * xv[i-width];
                    v -= A._a1 * xv[i+width];

                    xv[i] = (1.f-relaxationFactor) * xv[i] + relaxationFactor * v / A._a0;
                }
            }
        } else {
            for (unsigned z=bor[2]; z<depth-bor[2]; ++z) {
                for (unsigned y=bor[1]; y<height-bor[1]; ++y) {
                    for (unsigned x=bor[0]; x<width-bor[0]; ++x) {
                        const unsigned i = (z*height+y)*width+x;
                        auto v = b[i];

                        v -= A._a1 * xv[i-width*height];
                        v -= A._a1 * xv[i-width];
                        v -= A._a1 * xv[i-1];
                        v -= A._a1 * xv[i+1];
                        v -= A._a1 * xv[i+width];
                        v -= A._a1 * xv[i+width*height];

                        xv[i] = (1.f-relaxationFactor) * xv[i] + relaxationFactor * v / A._a0;
                    }
                }
            }
        }
    }

    static void RunSOR(ScalarField1D& xv, std::function<float(unsigned, unsigned)>& A, const ScalarField1D& b, unsigned N, float relaxationFactor)
    {
        for (unsigned i = 0; i < N; ++i) {
            auto v = b[i];

                // these loops work oddly simply in this situation
                // (but of course we can simplify because our matrix is sparse)
            for (unsigned j = 0; j < i; ++j)
                v -= A(i, j) * xv[j];
            for (unsigned j = i+1; j < N; ++j)
                v -= A(i, j) * xv[j];

            xv[i] = (1.f-relaxationFactor) * xv[i] + relaxationFactor * v / A(i, i);
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    
    using VectorX = Eigen::VectorXf;
    using MatrixX = Eigen::MatrixXf;

    static ScalarField1D AsScalarField1D(VectorX& v) { return ScalarField1D { v.data(), (unsigned)v.size() }; }

    template<typename Vec>
        static void ZeroBorder(Vec&x, const AMat& a)
    {
        if (a._dimensionality==2) ZeroBorder2D(x, GetWidth(a));
        else ZeroBorder3D(x, a._dims);
    }

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
        ZeroBorder(_r, A);
        ZeroBorder(_d, A);
        auto rho = _r.dot(_r);

        ZeroBorder(_q, A);
        unsigned k=0;
        if (XlAbs(rho) > rhoThreshold) {
            for (; k<maxIterations; ++k) {
            
                Multiply(_q, A, _d, _N);
                auto dDotQ = _d.dot(_q);
                auto alpha = rho / dDotQ;
                assert(isfinite(alpha) && !isnan(alpha));
                for (unsigned i=0; i<_N; ++i) {
                     x[i] += alpha * _d[i];
                    _r[i] -= alpha * _d[i];
                }
            
                auto rhoOld = rho;
                rho = _r.dot(_r);
                if (XlAbs(rho) < rhoThreshold) break;
                auto beta = rho / rhoOld;
                assert(isfinite(beta) && !isnan(beta));
            
                    // we can skip the border for the following...
                    // (but that requires different cases for 2D/3D)
                for (unsigned i=0; i<_N; ++i)
                    _d[i] = _r[i] + beta * _d[i];

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
        template<typename Mat, typename PreCon>
            unsigned Execute(ScalarField1D& x, const Mat& A, const ScalarField1D& b, const PreCon& precon);

        Solver_PreconCG(unsigned N);
        ~Solver_PreconCG();

    protected:
        VectorX _r, _d, _q;
        VectorX _s;
        unsigned _N;
    };

    template<typename Mat, typename PreCon>
        unsigned Solver_PreconCG::Execute(ScalarField1D& x, const Mat& A, const ScalarField1D& b, const PreCon& precon)
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

        auto rAsField = AsScalarField1D(_r);
        Multiply(rAsField, A, x, _N);    // r = AMat * x
        for (unsigned c=0; c<b._count; ++c) {
            _r[c] = b[c] - _r[c];
        }
        ZeroBorder(_r, A);
            
        SolveLowerTriangular(_d, precon, _r, _N);
            
        // #if defined(_DEBUG)
        //     {
        //             // testing "SolveLowerTriangular"
        //         VectorX t(_N);
        //         Multiply(t, precon, _d, _N);
        //         for (unsigned c=0; c<_N; ++c) {
        //             auto z = t(c), y = _r(c);
        //             assert(Equivalent(z, y, 1e-1f));
        //         }
        //     }
        // #endif
            
        auto rho = _r.dot(_d);
        // auto rho0 = rho;

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
                auto dDotQ = _d.dot(_q);
                auto alpha = rho / dDotQ;
                assert(isfinite(alpha) && !isnan(alpha));
                for (unsigned i=0; i<_N; ++i) {
                     x[i] += alpha * _d[i];
                    _r[i] -= alpha * _q[i];
                }
            
                SolveLowerTriangular(_s, precon, _r, _N);
                auto rhoOld = rho;
                rho = _r.dot(_s);
                if (XlAbs(rho) < rhoThreshold) break;
                assert(rho < rhoOld);
                auto beta = rho / rhoOld;
                assert(isfinite(beta) && !isnan(beta));
            
                for (unsigned i=0; i<_N; ++i)
                    _d[i] = _s[i] + beta * _d[i];
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
        template<typename Mat>
            unsigned Execute(ScalarField1D& x, const Mat& A, const ScalarField1D& b);

        Solver_Multigrid(UInt3 dims, unsigned dimensionality, unsigned levels);
        ~Solver_Multigrid();

    protected:
        std::vector<VectorX> _subResidual;
        std::vector<VectorX> _subB;
        std::vector<UInt3> _subDims;
        unsigned _N;
        unsigned _dimensionality;
    };

    static AMat ChangeResolution(AMat i, unsigned layer)
    {
            // 'a' values are proportion to the square of N
            // N quarters with every layer (width and height half)
        auto scale = std::pow(4.f, float(layer));
        auto result = i;
        result._a0 /= scale;
        result._a1 /= scale;
        return result;
    }

    static void Restrict2D(ScalarField1D& dst, const ScalarField1D& src, UInt2 dstDims, UInt2 srcDims)
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
        for (unsigned y=1; y<dstDims[1]-1; ++y) {
            for (unsigned x=1; x<dstDims[0]-1; ++x) {
                unsigned sx = (x-1)*2+1, sy = (y-1)*2+1;
                dst[y*dstDims[0]+x]
                    = .25f * src[(sy+0)*srcDims[0]+(sx+0)]
                    + .25f * src[(sy+0)*srcDims[0]+(sx+1)]
                    + .25f * src[(sy+1)*srcDims[0]+(sx+0)]
                    + .25f * src[(sy+1)*srcDims[0]+(sx+1)]
                    ;
            }
        }
    }

    static void Restrict3D(ScalarField1D& dst, const ScalarField1D& src, UInt3 dstDims, UInt3 srcDims)
    {
        for (unsigned z=1; z<dstDims[2]-1; ++z) {
            for (unsigned y=1; y<dstDims[1]-1; ++y) {
                for (unsigned x=1; x<dstDims[0]-1; ++x) {
                    unsigned sx = (x-1)*2+1, sy = (y-1)*2+1, sz = (z-1)*2+1;
                    dst[(z*dstDims[1]+y)*dstDims[0]+x]
                        = .125f * src[((sz+0)*srcDims[1]+(sy+0))*srcDims[0]+(sx+0)]
                        + .125f * src[((sz+0)*srcDims[1]+(sy+0))*srcDims[0]+(sx+1)]
                        + .125f * src[((sz+0)*srcDims[1]+(sy+1))*srcDims[0]+(sx+0)]
                        + .125f * src[((sz+0)*srcDims[1]+(sy+1))*srcDims[0]+(sx+1)]
                        + .125f * src[((sz+1)*srcDims[1]+(sy+0))*srcDims[0]+(sx+0)]
                        + .125f * src[((sz+1)*srcDims[1]+(sy+0))*srcDims[0]+(sx+1)]
                        + .125f * src[((sz+1)*srcDims[1]+(sy+1))*srcDims[0]+(sx+0)]
                        + .125f * src[((sz+1)*srcDims[1]+(sy+1))*srcDims[0]+(sx+1)]
                        ;
                }
            }
        }
    }

    static void Prolongate2D(ScalarField1D& dst, const ScalarField1D& src, UInt2 dstDims, UInt2 srcDims)
    {
            // This is the "prolongate" operator.
            // As with the restrict operator, we're going
            // to use a simple bilinear sample, as if each
            // layer was a mipmap.

        for (unsigned y=1; y<dstDims[1]-1; ++y) {
            for (unsigned x=1; x<dstDims[0]-1; ++x) {
                auto sx = (x-1)/2.f + 1.f;
                auto sy = (y-1)/2.f + 1.f;
                auto sx0 = XlFloor(sx), sy0 = XlFloor(sy);
                auto a = sx - sx0, b = sy - sy0;
                decltype(a) weights[] = {
                    (1.0f - a) * (1.0f - b),
                    a * (1.0f - b),
                    (1.0f - a) * b,
                    a * b
                };
                dst[y*dstDims[0]+x]
                    = weights[0] * src[(unsigned(sy0)+0)*srcDims[0]+unsigned(sx0)]
                    + weights[1] * src[(unsigned(sy0)+0)*srcDims[0]+unsigned(sx0)+1]
                    + weights[2] * src[(unsigned(sy0)+1)*srcDims[0]+unsigned(sx0)]
                    + weights[3] * src[(unsigned(sy0)+1)*srcDims[0]+unsigned(sx0)+1]
                    ;
            }
        }
    }

    static void Prolongate3D(ScalarField1D& dst, const ScalarField1D& src, UInt3 dstDims, UInt3 srcDims)
    {
        for (unsigned z=1; z<dstDims[2]-1; ++z) {
            for (unsigned y=1; y<dstDims[1]-1; ++y) {
                for (unsigned x=1; x<dstDims[0]-1; ++x) {
                    auto sx = (x-1)/2.f + 1.f;
                    auto sy = (y-1)/2.f + 1.f;
                    auto sz = (z-1)/2.f + 1.f;
                    auto sx0 = XlFloor(sx), sy0 = XlFloor(sy), sz0 = XlFloor(sz);
                    auto a = sx - sx0, b = sy - sy0, c = sz - sz0;
                    decltype(a) weights[] = {
                        (1.0f - a) * (1.0f - b) * (1.0f - c),
                        a * (1.0f - b) * (1.0f - c),
                        (1.0f - a) * b * (1.0f - c),
                        a * b * (1.0f - c),
                        (1.0f - a) * (1.0f - b) * c,
                        a * (1.0f - b) * c,
                        (1.0f - a) * b * c,
                        a * b * c
                    };
                    dst[(z*dstDims[1]+y)*dstDims[0]+x]
                        = weights[0] * src[((unsigned(sz0)+0)*srcDims[1]+(unsigned(sy0)+0))*srcDims[0]+unsigned(sx0)+0]
                        + weights[1] * src[((unsigned(sz0)+0)*srcDims[1]+(unsigned(sy0)+0))*srcDims[0]+unsigned(sx0)+1]
                        + weights[2] * src[((unsigned(sz0)+0)*srcDims[1]+(unsigned(sy0)+1))*srcDims[0]+unsigned(sx0)+0]
                        + weights[3] * src[((unsigned(sz0)+0)*srcDims[1]+(unsigned(sy0)+1))*srcDims[0]+unsigned(sx0)+1]
                        + weights[4] * src[((unsigned(sz0)+1)*srcDims[1]+(unsigned(sy0)+0))*srcDims[0]+unsigned(sx0)+0]
                        + weights[5] * src[((unsigned(sz0)+1)*srcDims[1]+(unsigned(sy0)+0))*srcDims[0]+unsigned(sx0)+1]
                        + weights[6] * src[((unsigned(sz0)+1)*srcDims[1]+(unsigned(sy0)+1))*srcDims[0]+unsigned(sx0)+0]
                        + weights[7] * src[((unsigned(sz0)+1)*srcDims[1]+(unsigned(sy0)+1))*srcDims[0]+unsigned(sx0)+1]
                        ;
                }
            }
        }
    }

    template<typename Mat>
        unsigned Solver_Multigrid::Execute(ScalarField1D& x, const Mat& A, const ScalarField1D& b)
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
        auto activeDims = A._dims;
        ScalarField1D prevLayer = x;
        ScalarField1D prevB = b;
        auto gridCount = unsigned(_subResidual.size());
        for (unsigned g=0; g<gridCount; ++g) {
            auto prevDims = activeDims;
            activeDims = _subDims[g];
            auto dst = AsScalarField1D(_subResidual[g]);
            auto dstB = AsScalarField1D(_subB[g]);

            if (_dimensionality==2) {
                Restrict2D(dst, prevLayer, Truncate(activeDims), Truncate(prevDims));
                Restrict2D(dstB, prevB, Truncate(activeDims), Truncate(prevDims));   // is it better to downsample B from the top most level each time?
            } else {
                Restrict3D(dst, prevLayer, activeDims, prevDims);
                Restrict3D(dstB, prevB, activeDims, prevDims);   // is it better to downsample B from the top most level each time?
            }

            auto SA = ChangeResolution(A, g+1);
            SA._dims = activeDims;
            for (unsigned k = 0; k<stepSmoothIterations; ++k)
                RunSOR(dst, SA, dstB, gamma);
            iterations += stepSmoothIterations;

            prevLayer = dst;
            prevB = dstB;
        }

            // ---------- step up ----------
        for (unsigned g=gridCount-1; g>0; --g) {
            auto src = AsScalarField1D(_subResidual[g]);
            auto dst = AsScalarField1D(_subResidual[g-1]);
            auto dstB = AsScalarField1D(_subB[g-1]);
            auto srcDims = _subDims[g];
            auto dstDims = _subDims[g-1];

            if (_dimensionality==2) {
                Prolongate2D(dst, src, Truncate(dstDims), Truncate(srcDims));
            } else {
                Prolongate3D(dst, src, dstDims, srcDims);
            }

            auto SA = ChangeResolution(A, g-1+1);
            SA._dims = dstDims;
            for (unsigned k = 0; k<stepSmoothIterations; ++k)
                RunSOR(dst, SA, dstB, gamma);
            iterations += stepSmoothIterations;
        }

            // finally, step back onto 'x'
        if (_dimensionality==2) {
            Prolongate2D(x, AsScalarField1D(_subResidual[0]), Truncate(A._dims), Truncate(_subDims[0]));
        } else {
            Prolongate3D(x, AsScalarField1D(_subResidual[0]), A._dims, _subDims[0]);
        }

            // post-smoothing (SOR method -- can be done in place)
        for (unsigned k = 0; k<postSmoothIterations; ++k)
            RunSOR(x, A, b, gamma);
        iterations += postSmoothIterations;

        return iterations;
    }

    Solver_Multigrid::Solver_Multigrid(UInt3 dims, unsigned dimensionality, unsigned levels)
    {
        _dimensionality = dimensionality;
        _N = dims[0]*dims[1]*dims[2];
        for (unsigned c=0; c<levels; c++) {
            dims[0] = (unsigned)std::max(1, ((int(dims[0])-2) >> 1)) + 2u;
            dims[1] = (unsigned)std::max(1, ((int(dims[1])-2) >> 1)) + 2u;
            dims[2] = (unsigned)std::max(1, ((int(dims[2])-2) >> 1));
            if (dims[2] > 1) dims[2] += 2;

            unsigned n = dims[0]*dims[1]*dims[2];
            VectorX subr(n); subr.fill(0.f);
            _subResidual.push_back(std::move(subr));
            VectorX subb(n); subb.fill(0.f);
            _subB.push_back(std::move(subb));
            _subDims.push_back(dims);
        }
    }

    Solver_Multigrid::~Solver_Multigrid() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    class PoissonSolver::Pimpl
    {
    public:
        VectorX _tempBuffer;
        UInt3 _dimensionsWithBorders;
        UInt3 _borders;
        unsigned _dimensionality;

        std::unique_ptr<Solver_PlainCG> _plainCGSolver;
        std::unique_ptr<Solver_PreconCG> _preconCGSolver;
        std::unique_ptr<Solver_Multigrid> _multigridSolver;
    };

    class PoissonSolver::PreparedMatrix
    {
    public:
        AMat _amat;
        std::vector<int> _bands;
        SparseBandedMatrix<MatrixX> _bandedPrecon;
    };

    static AMat GetEstimate(const AMat& A, float estimationFactor)
    {
        const auto estA1 = estimationFactor * A._a1;  // used when calculating a starting estimate
        return AMat { A._dims, A._borders, A._dimensionality, 1.f - 4.f * estA1, estA1 };
    }

    unsigned PoissonSolver::Solve(
        ScalarField1D x, 
        const PreparedMatrix& A, 
        const ScalarField1D& b, Method solver) const
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
        const auto& matA = A._amat;
        auto estMatA = GetEstimate(matA, estimateFactor);
        const auto N = GetN(matA);

        const auto width = GetWidth(matA);
        const auto height = GetHeight(matA);
        const auto depth = GetDepth(matA);
        const auto bor = GetBorders(matA);

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

        if (solver == Method::PlainCG || solver == Method::PreconCG || solver == Method::Multigrid) {

            for (unsigned qz=bor[2]; qz<depth-bor[2]; ++qz) {
                for (unsigned qy=bor[1]; qy<height-bor[1]; ++qy) {
                    for (unsigned qx=bor[0]; qx<width-bor[0]; ++qx) {
                        auto i = (qz*height+qy)*width+qx;

                            // set an initial estimate using
                            // explicit euler. We'll march forward part of
                            // the timestep, and then refine the estimate
                            // from there using the iterative implicit method.
                        auto v = estMatA._a0 * b[i];
                        v += estMatA._a1 * b[i-1];
                        v += estMatA._a1 * b[i+1];
                        v += estMatA._a1 * b[i-width];
                        v += estMatA._a1 * b[i+width];
                        x._u[i] = v;
                    }
                }
            }

            ZeroBorder(x, matA);

            auto iterations = 0u;
            if (solver == Method::PlainCG) {
                if (!_pimpl->_plainCGSolver)
                    _pimpl->_plainCGSolver = std::make_unique<Solver_PlainCG>(N);
                iterations = _pimpl->_plainCGSolver->Execute(x, matA, workingB);
            } else if (solver == Method::PreconCG) {
                if (!_pimpl->_preconCGSolver)
                    _pimpl->_preconCGSolver = std::make_unique<Solver_PreconCG>(N);
                iterations = _pimpl->_preconCGSolver->Execute(x, matA, workingB, A._bandedPrecon);
            } else if (solver == Method::Multigrid) {
                if (!_pimpl->_multigridSolver)
                    _pimpl->_multigridSolver = std::make_unique<Solver_Multigrid>(_pimpl->_dimensionsWithBorders, _pimpl->_dimensionality, 2);
                iterations = _pimpl->_multigridSolver->Execute(x, matA, workingB);
            }

            return iterations;

        } else if (solver == Method::ForwardEuler) {
        
                // This is the simpliest integration. We just
                // move forward a single timestep...
            auto a0 = 1.f - 4.f * -matA._a1;
            auto a1 = -matA._a1;
            for (unsigned iz=bor[2]; iz<depth-bor[2]; ++iz)
                for (unsigned iy=bor[1]; iy<height-bor[1]; ++iy)
                    for (unsigned ix=bor[0]; ix<width-bor[0]; ++ix) {
                        const unsigned i = (iz*height+iy)*width+ix;
                        auto v = a0 * workingB[i];
                        v += a1 * workingB[i-1];
                        v += a1 * workingB[i+1];
                        v += a1 * workingB[i-width];
                        v += a1 * workingB[i+width];
                        x._u[i] = v;
                    }

            ZeroBorder(x, matA);
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
                RunSOR(x, matA, workingB, gamma);

            ZeroBorder(x, matA);
            return iterations;

        }

        return 0;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

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

        return result * result.transpose();
    }

    static MatrixX CalculateIncompleteCholesky(const AMat& mat, unsigned N, unsigned bandOptimization)
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

        const auto width = GetWidth(mat);
        const auto height = GetHeight(mat);
        const unsigned thirdTest = (mat._dimensionality==2)?0:(width*height);
            
        if (bandOptimization == 0) {

            for (unsigned i=0; i<N; ++i) {
                float a = mat._a0;
                for (unsigned k=0; k<i; ++k) {
                    float l = factorization(i, k);
                    a -= l*l;
                }
                a = XlSqrt(a);
                factorization(i,i) = a;

                if (i != 0) {
                    for (unsigned j=i+1; j<N; ++j) {
                        float aij = ((j==i+1)||(j==i+width)||(j==i+thirdTest)) ? mat._a1 : 0.f;
                        for (unsigned k=0; k<i; ++k)
                            aij -= factorization(i, k) * factorization(j, k);
                        factorization(j, i) = aij / a;
                    }
                }
            }

                // 
                //  Our preconditioner matrix is "factorization" multiplied by
                //  it's transpose
                //

            int bands2D[] = { -int(width), -1, 1, width, 0 };
            int bands3D[] = { -int(width*height), -int(width), -1, 1, width, width*height, 0 };
            unsigned bandCount; int* bands;
            if (mat._dimensionality == 2) {
                bandCount = dimof(bands2D);
                bands = bands2D;
            } else {
                bandCount = dimof(bands3D);
                bands = bands3D;
            }
            MatrixX sparseMatrix(N, bandCount);
            for (unsigned i=0; i<N; ++i)
                for (unsigned j=0; j<bandCount; ++j) {
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

        } else {

            // Generating the Cholesky factorization is actually really expensive!
            // But we can optimise it because the input matrix is banded.
            // We will assume the values in the factorization fall off to zero 
            // within a certain number of cells from the bands. This happens naturally,
            // and we can adjust the number of cells to adjust the accuracy we want.
            // (also note that some of the small details in the matrix will be lost
            // when we generate the preconditioner matrix -- because all off-band cells
            // in the final matrix are zero, anyway).
            //
            // Actually, it seems like the values off the main bands may not have any
            // effect on the final preconditioner matrix we generate? (given that the
            // final matrix is sparse, and has zeroes off the main bands).

            assert(bandOptimization < width);   // if "bandOptimisation" is very big, the math will be incorrect (and anyway, it will run slowly)
            const int magicOffset0 = width; // +2;
            const int magicOffset1 = width; // -2;

            for (unsigned i=0; i<N; ++i) {
                float a = mat._a0;
                for (unsigned k=0; k<i; ++k) {
                    float l = factorization(i, k);
                    a -= l*l;
                }
                a = XlSqrt(a);
                factorization(i,i) = a;

                if (i != 0) {

                    int kband0Start, kband0End;
                    if (mat._dimensionality==2) {
                        kband0Start = kband0End = 0;
                    } else {
                        kband0Start = std::max(0,   int(i)-int(width*height)-int(bandOptimization));
                        kband0End =                 int(i)-int(width*height)+int(bandOptimization)+1;
                    }

                    int kband1Start = std::max(0,   int(i)-magicOffset0-int(bandOptimization));
                    int kband1End =                 int(i)-magicOffset0+int(bandOptimization)+1;

                    int kband2Start = std::max(0,   int(i)-1-int(bandOptimization));

                    for (unsigned j=i+1; j<std::min(i+1+bandOptimization+1, N); ++j) {
                        float aij = ((j==i+1)||(j==i+width)||(j==i+thirdTest)) ? mat._a1 : 0.f;

                            // there are only some cases of "k" that can possibly have data
                            // it must be within the widened bands of both i and k. It's awkward
                            // to find an overlap, so let's just check the bands of i
                        for (int k=kband0Start; k<kband0End; ++k)
                            aij -= factorization(i, k) * factorization(j, k);
                        for (int k=kband1Start; k<kband1End; ++k)
                            aij -= factorization(i, k) * factorization(j, k);
                        for (int k=kband2Start; k<int(i); ++k)
                            aij -= factorization(i, k) * factorization(j, k);

                        factorization(j, i) = aij / a;
                    }
                    
                    for (unsigned j=i+magicOffset1-bandOptimization; j<std::min(i+magicOffset1+bandOptimization+1, N); ++j) {
                        float aij = ((j==i+1)||(j==i+width)||(j==i+thirdTest)) ? mat._a1 : 0.f;
                        for (int k=kband0Start; k<kband0End; ++k)
                            aij -= factorization(i, k) * factorization(j, k);
                        for (int k=kband1Start; k<kband1End; ++k)
                            aij -= factorization(i, k) * factorization(j, k);
                        for (int k=kband2Start; k<int(i); ++k)
                            aij -= factorization(i, k) * factorization(j, k);
                        factorization(j, i) = aij / a;
                    }

                    if (mat._dimensionality!=2) {
                        for (unsigned j=i+width*height-bandOptimization; j<std::min(i+width*height+bandOptimization+1, N); ++j) {
                            float aij = ((j==i+1)||(j==i+width)||(j==i+thirdTest)) ? mat._a1 : 0.f;
                            for (int k=kband0Start; k<kband0End; ++k)
                                aij -= factorization(i, k) * factorization(j, k);
                            for (int k=kband1Start; k<kband1End; ++k)
                                aij -= factorization(i, k) * factorization(j, k);
                            for (int k=kband2Start; k<int(i); ++k)
                                aij -= factorization(i, k) * factorization(j, k);
                            factorization(j, i) = aij / a;
                        }
                    }
                }
            }

                // 
                //  Our preconditioner matrix is "factorization" multiplied by
                //  it's transpose
                //

            int bands2D[] = { -int(width), -1, 1, width, 0 };
            int bands3D[] = { -int(width*height), -int(width), -1, 1, width, width*height, 0 };
            unsigned bandCount; int* bands;
            if (mat._dimensionality == 2) {
                bandCount = dimof(bands2D);
                bands = bands2D;
            } else {
                bandCount = dimof(bands3D);
                bands = bands3D;
            }
            MatrixX sparseMatrix(N, bandCount);
            for (unsigned i=0; i<N; ++i) {

                int kband0Start = std::max(0,       int(i)-magicOffset0-int(bandOptimization));
                int kband0End   = std::max(0,       int(i)-magicOffset0+int(bandOptimization)+1);
                int kband1Start = std::max(0,       int(i)-1-int(bandOptimization));
                int kband1End   = std::min(int(N),  int(i)+1+int(bandOptimization)+1);
                int kband2Start = std::min(int(N),  int(i)+magicOffset1-int(bandOptimization));
                int kband2End   = std::min(int(N),  int(i)+magicOffset1+int(bandOptimization)+1);

                int kband3Start, kband3End, kband4Start, kband4End;
                if (mat._dimensionality==2) {
                    kband3Start =  kband3End =  kband4Start = kband4End = 0;
                } else {
                    kband3Start =  std::max(0,      int(i)-int(width*height)-int(bandOptimization));
                    kband3End   =  std::max(0,      int(i)-int(width*height)+int(bandOptimization)+1);
                    kband4Start =  std::min(int(N), int(i)+int(width*height)-int(bandOptimization));
                    kband4End   =  std::min(int(N), int(i)+int(width*height)+int(bandOptimization)+1);
                }

                for (unsigned j=0; j<bandCount; ++j) {
                    int j2 = int(i) + bands[j];
                    if (j2 >= 0 && j2 < int(N)) {

                            // Here, calculate M(i, j), where M
                            // is the factorization multiplied by its transpose
                        float A = 0.f;
                        for (int k=kband0Start; k<kband0End; ++k)
                            A += factorization(i, k) * factorization(j2, k);
                        for (int k=kband1Start; k<kband1End; ++k)
                            A += factorization(i, k) * factorization(j2, k);
                        for (int k=kband2Start; k<kband2End; ++k)
                            A += factorization(i, k) * factorization(j2, k);
                        for (int k=kband3Start; k<kband3End; ++k)
                            A += factorization(i, k) * factorization(j2, k);
                        for (int k=kband4Start; k<kband4End; ++k)
                            A += factorization(i, k) * factorization(j2, k);

                        sparseMatrix(i, j) = A;
                    } else {
                        sparseMatrix(i, j) = 0.f;
                    }
                }
            }

            return std::move(sparseMatrix);

        }
        
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    PoissonSolver::PoissonSolver(unsigned dimensionality, unsigned dimensions[])
    {
        assert(dimensionality==2 || dimensionality == 3);
        dimensionality = std::min(dimensionality, 3u);
        _pimpl = std::make_unique<Pimpl>();
        _pimpl->_dimensionsWithBorders = UInt3(1,1,1);
        _pimpl->_borders = UInt3(0,0,0);
        _pimpl->_dimensionality = dimensionality;
        for (unsigned c=0; c<dimensionality; ++c) {
            _pimpl->_dimensionsWithBorders[c] = dimensions[c];
            _pimpl->_borders[c] = 1u;
        }

        const auto N = 
              _pimpl->_dimensionsWithBorders[0]
            * _pimpl->_dimensionsWithBorders[1]
            * _pimpl->_dimensionsWithBorders[2];
        
        _pimpl->_tempBuffer = VectorX(N);
        _pimpl->_tempBuffer.fill(0.f);

        #if defined(_DEBUG)
            {
                const auto diffusion = 0.1f;
                const auto a0 = 1.f + 6.f * diffusion;
                const auto a1 = -diffusion;
                AMat A = { 
                    UInt3(8, 8, 8), 
                    UInt3(1, 1, 1),
                    3,
                    a0, a1 };
                auto precon0 = CalculateIncompleteCholesky(A, 8*8*8, 1);
                auto precon1 = CalculateIncompleteCholesky(A, 8*8*8, 0);

                const auto rows = precon0.rows();
                for (unsigned i=0; i<rows; ++i) {
                    LogInfo << "[" << i << "] " 
                        << precon0(i, 0) << ", "
                        << precon0(i, 1) << ", "
                        << precon0(i, 2) << ", "
                        << precon0(i, 3) << ", "
                        << precon0(i, 4) << ", "
                        << precon0(i, 5) << ", "
                        << precon0(i, 6) << " ("
                        << precon1(i, 0) << ", "
                        << precon1(i, 1) << ", "
                        << precon1(i, 2) << ", "
                        << precon1(i, 3) << ", "
                        << precon1(i, 4) << ", "
                        << precon1(i, 5) << ", "
                        << precon1(i, 6) << ")";
                }

                (void)precon1;
            }
        #endif
    }

    auto PoissonSolver::PrepareDiffusionMatrix(
            float centralWeight, float adjWeight, Method method) -> std::shared_ptr<PreparedMatrix>
    {
        AMat A = { 
            _pimpl->_dimensionsWithBorders, 
            _pimpl->_borders,
            _pimpl->_dimensionality,
            centralWeight, adjWeight };
        const auto N = _pimpl->_dimensionsWithBorders[0] * _pimpl->_dimensionsWithBorders[1] * _pimpl->_dimensionsWithBorders[2];

        auto result = std::make_shared<PreparedMatrix>();
        result->_amat = A;

        const bool needPrecon = method == Method::PreconCG;
        if (needPrecon) {
            static unsigned bandOptimisation = 3;
            auto precon = CalculateIncompleteCholesky(A, N, bandOptimisation);
            const auto width = _pimpl->_dimensionsWithBorders[0];
            const auto height = _pimpl->_dimensionsWithBorders[1];

            if (_pimpl->_dimensionality==2) {
                    // ----- 2D case -----
                result->_bands.resize(5);
                result->_bands[0] =  -int(width);
                result->_bands[1] =  -1;
                result->_bands[2] =   1;
                result->_bands[3] =   width;
                result->_bands[4] =   0;
            } else {
                    // ----- 3D case -----
                result->_bands.resize(7);
                result->_bands[0] =  -int(width*height);
                result->_bands[1] =  -int(width);
                result->_bands[2] =  -1;
                result->_bands[3] =   1;
                result->_bands[4] =   width;
                result->_bands[5] =   width*height;
                result->_bands[6] =   0;
            }

            result->_bandedPrecon = SparseBandedMatrix<MatrixX>(
                std::move(precon), 
                AsPointer(result->_bands.cbegin()), (unsigned)result->_bands.size());
        }

        return std::move(result);
    }

    auto PoissonSolver::PrepareDivergenceMatrix(Method method) -> std::shared_ptr<PreparedMatrix>
    {
        AMat A = { _pimpl->_dimensionsWithBorders, _pimpl->_borders, _pimpl->_dimensionality, 4.f, -1.f };
        const auto N = _pimpl->_dimensionsWithBorders[0] * _pimpl->_dimensionsWithBorders[1] * _pimpl->_dimensionsWithBorders[2];

        auto result = std::make_shared<PreparedMatrix>();
        result->_amat = A;

        const bool needPrecon = method == Method::PreconCG;
        if (needPrecon) {
            static unsigned bandOptimisation = 3;
            auto precon = CalculateIncompleteCholesky(A, N, bandOptimisation);
            const auto width = _pimpl->_dimensionsWithBorders[0];
            const auto height = _pimpl->_dimensionsWithBorders[1];

            if (_pimpl->_dimensionality==2) {
                    // ----- 2D case -----
                result->_bands.resize(5);
                result->_bands[0] =  -int(width);
                result->_bands[1] =  -1;
                result->_bands[2] =   1;
                result->_bands[3] =   width;
                result->_bands[4] =   0;
            } else {
                    // ----- 3D case -----
                result->_bands.resize(7);
                result->_bands[0] =  -int(width*height);
                result->_bands[1] =  -int(width);
                result->_bands[2] =  -1;
                result->_bands[3] =   1;
                result->_bands[4] =   width;
                result->_bands[5] =   width*height;
                result->_bands[6] =   0;
            }

            result->_bandedPrecon = SparseBandedMatrix<MatrixX>(
                std::move(precon), 
                AsPointer(result->_bands.cbegin()), (unsigned)result->_bands.size());
        }

        return std::move(result);
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

