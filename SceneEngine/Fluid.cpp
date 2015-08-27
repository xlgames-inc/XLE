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

    template<typename Vec>
        static void RunSOR(Vec& xv, AMat2D A, const Vec& b, float relaxationFactor)
    {
        for (unsigned y=1; y<A.wh-1; ++y) {
            for (unsigned x=1; x<A.wh-1; ++x) {
                const unsigned i = y*A.wh+x;
                float v = b[i];

                v -= A.a1 * xv[i-1];
                v -= A.a1 * xv[i+1];
                v -= A.a1 * xv[i-A.wh];
                v -= A.a1 * xv[i+A.wh];

                xv[i] = (1.f-relaxationFactor) * xv[i] + relaxationFactor * v / A.a0;
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

    static AMat2D ChangeResolution(AMat2D i, unsigned layer)
    {
            // 'a' values are proportion to the square of N
            // N quarters with every layer (width and height half)
        float scale = std::pow(4.f, float(layer));
        AMat2D result = i;
        result.a0 /= scale;
        result.a1 /= scale;
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

///////////////////////////////////////////////////////////////////////////////////////////////////

    class FluidSolver2D::Pimpl
    {
    public:
        VectorX _velU;
        VectorX _velV;
        VectorX _density;

        VectorX _prevVelU;
        VectorX _prevVelV;
        VectorX _prevDensity;
        UInt2 _dimensions;

        VectorX _workingX, _workingB;

        int _bands[5];
        SparseBandedMatrix _bandedPrecon;
        std::function<float(unsigned, unsigned)> AMat;

        void DensityDiffusion(const FluidSolver2D::Settings& settings);

        template<typename Field>
            void VelocityAdvect(
                Field dstValues, Field prevValues,
                const FluidSolver2D::Settings& settings);

        enum class PossionSolver { CG_Precon, PlainCG, ForwardEuler, SOR, Multigrid };

        template<typename Vec, typename AMatType>
            unsigned SolvePoisson(Vec& x, AMatType A, const Vec& b, PossionSolver solver);

        template<typename Field>
            void EnforceIncompressibility(
                Field velField,
                const FluidSolver2D::Settings& settings);
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

        static bool useGeneralA = false;
        static float diffFactor = 5.f;
        const float a = diffFactor * settings._deltaTime;
        const float a0 = 1.f + 4.f * a;
        const float a1 = -a;

        auto method = (PossionSolver)settings._diffusionMethod;

        unsigned iterations = 0;
        if (useGeneralA) {
            // iterations = SolvePoisson(_density, AMat, _density, method);
        } else {
            iterations = SolvePoisson(_density, AMat2D { _dimensions[0]+2, a0, a1 }, _density, method);
        }
        LogInfo << "Diffusion took: " << iterations << " iterations.";
    }

    static unsigned GetN(const AMat2D& A) { return A.wh * A.wh; }
    static unsigned GetWH(const AMat2D& A) { return A.wh; }
    static AMat2D GetEstimate(const AMat2D& A, float estimationFactor)
    {
        const float estA1 = estimationFactor * A.a1;  // used when calculating a starting estimate
        return AMat2D { A.wh, 1.f - 4.f * estA1, estA1 };
    }

    template<typename Vec, typename AMatType>
        unsigned FluidSolver2D::Pimpl::SolvePoisson(Vec& x, AMatType A, const Vec& b, PossionSolver solver)
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

        static float estimateFactor = .75f; // maybe we could adapt this based on the amount of noise in the system? In low noise systems, explicit euler seems very close to correct
        auto estMatA = GetEstimate(A, estimateFactor);

        const auto N = GetN(A);
        const auto wh = GetWH(A);

        if (solver == PossionSolver::PlainCG || solver == PossionSolver::CG_Precon || solver == PossionSolver::Multigrid) {

            for (unsigned y=1; y<wh-1; ++y) {
                for (unsigned x=1; x<wh-1; ++x) {
                    auto i = y*wh+x;
                    _workingB(i) = b[i];

                        // set an initial estimate using
                        // explicit euler. We'll march forward part of
                        // the timestep, and then refine the estimate
                        // from there using the iterative implicit method.
                    float v = estMatA.a0 * b[i];
                    v += estMatA.a1 * b[i-1];
                    v += estMatA.a1 * b[i+1];
                    v += estMatA.a1 * b[i-wh];
                    v += estMatA.a1 * b[i+wh];
                    _workingX(i) = v;
                }
            }
            ZeroBorder(_workingX, wh);
            ZeroBorder(_workingB, wh);

            auto iterations = 0u;
            if (solver == PossionSolver::PlainCG) {
                Solver_PlainCG solver(N);
                iterations = solver.Execute(_workingX, A, _workingB);
            } else if (solver == PossionSolver::CG_Precon) {
                Solver_PreconCG solver(N);
                iterations = solver.Execute(_workingX, A, _workingB, _bandedPrecon);
            } else if (solver == PossionSolver::Multigrid) {
                Solver_Multigrid solver(wh, 2);
                iterations = solver.Execute(_workingX, A, _workingB);
            }
            
            for (unsigned c=0; c<N; ++c) {
                assert(isfinite(_workingX(c)) && !isnan(_workingX(c)));
                x[c] = _workingX(c);
            }

            return iterations;

        } else if (solver == PossionSolver::ForwardEuler) {
        
                // This is the simpliest integration. We just
                // move forward a single timestep...
            for (unsigned c=0; c<N; ++c) _workingX[c] = b[c];

            float a0 = 1.f - 4.f * -A.a1;
            float a1 = -A.a1;
            for (unsigned iy=1; iy<wh-1; ++iy)
                for (unsigned ix=1; ix<wh-1; ++ix) {
                    const unsigned i = iy*wh+ix;
                    float v = a0 * _workingX[i];
                    v += a1 * _workingX[i-1];
                    v += a1 * _workingX[i+1];
                    v += a1 * _workingX[i-wh];
                    v += a1 * _workingX[i+wh];
                    x[i] = v;
                }

            return 1;

        } else if (solver == PossionSolver::SOR) {

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

            auto* bCopy = &b;
            if (&x == bCopy) {
                for (unsigned i=0; i<N; ++i)
                    _workingB[i] = b[i];
                bCopy = &_workingB;
            }

            for (unsigned k = 0; k<iterations; ++k)
                RunSOR(x, A, *bCopy, gamma);

            return iterations;

        }

        return 0;
    }

    class VelocityField2D
    {
    public:
        VectorX* _u, *_v; 
        unsigned _wh;
    };

    template<bool DoClamp>
        static Float2 LoadBilinear(const VelocityField2D& field, Float2 coord)
    {
        float fx = XlFloor(coord[0]);
        float fy = XlFloor(coord[1]);
        float a = coord[0] - fx, b = coord[1] - fy;
        float weights[] = 
        {
            (1.f - a) * (1.f - b),
            a * (1.f - b),
            (1.f - a) * b,
            a * b
        };
        unsigned x0, x1, y0, y1;
        
        if (constant_expression<DoClamp>::result()) {
            x0 = unsigned(Clamp(fx, 0.f, float(field._wh-1)));
            x1 = std::min(x0+1u, field._wh-1u);
            y0 = unsigned(Clamp(fy, 0.f, float(field._wh-1)));
            y1 = std::min(y0+1u, field._wh-1u);
        } else {
            x0 = unsigned(fx); x1 = x0+1;
            y0 = unsigned(fy); y1 = y0+1;
        }
        assert(x1 < field._wh && y1 < field._wh);
        
        float u
            = weights[0] * (*field._u)[y0*field._wh+x0]
            + weights[1] * (*field._u)[y0*field._wh+x1]
            + weights[2] * (*field._u)[y1*field._wh+x0]
            + weights[3] * (*field._u)[y1*field._wh+x1]
            ;
        float v
            = weights[0] * (*field._v)[y0*field._wh+x0]
            + weights[1] * (*field._v)[y0*field._wh+x1]
            + weights[2] * (*field._v)[y1*field._wh+x0]
            + weights[3] * (*field._v)[y1*field._wh+x1]
            ;
        return Float2(u, v);
    }

    static Float2 Load(const VelocityField2D& field, UInt2 coord)
    {
        assert(coord[0] < field._wh && coord[1] < field._wh);
        return Float2(
            (*field._u)[coord[1] * field._wh + coord[0]],
            (*field._v)[coord[1] * field._wh + coord[0]]);
    }

    static void Write(VelocityField2D& field, UInt2 coord, Float2 value)
    {
        assert(coord[0] < field._wh && coord[1] < field._wh);
        (*field._u)[coord[1] * field._wh + coord[0]] = value[0];
        (*field._v)[coord[1] * field._wh + coord[0]] = value[1];
        assert(isfinite(value[0]) && !isnan(value[0]));
        assert(isfinite(value[1]) && !isnan(value[1]));
    }

    template<typename Field>
        void FluidSolver2D::Pimpl::VelocityAdvect(
            Field dstValues, Field prevValues,
            const FluidSolver2D::Settings& settings)
    {
        //
        // This is the advection step. We will use the method of characteristics.
        //
        // We have a few different options for the stepping method:
        //  * basic euler forward integration (ie, just step forward in time)
        //  * forward integration method divided into smaller time steps
        //  * Runge-Kutta integration
        //  * Modified MacCormick methods
        //  * Back and Forth Error Compensation and Correction (BFECC)
        //
        // Let's start without any complex boundary conditions.
        //
        // We have to be careful about how the velocity sample is aligned with
        // the grid cell. Incorrect alignment will produce a bias in the way that
        // we interpolate the field.
        //

        enum class Advection { ForwardEuler, ForwardEulerDiv, RungeKutta };
        const auto advectionMethod = (Advection)settings._advectionMethod;
        const auto adjvectionSteps = settings._advectionSteps;

        const unsigned wh = _dimensions[0] + 2;
        const float deltaTime = settings._deltaTime;
        const float velFieldScale = 1.f; // float(_dimensions[0]*_dimensions[1]);   // (grid size without borders)

        VelocityField2D velFieldT0 { &_prevVelU, &_prevVelV, _dimensions[0]+2 };
        VelocityField2D velFieldT1 { &_velU, &_velV, _dimensions[0]+2 };

        if (advectionMethod == Advection::ForwardEuler) {

                //  For each cell in the grid, trace backwards
                //  through the velocity field to find an approximation
                //  of where the point was in the previous frame.

            for (unsigned y=1; y<wh-1; ++y)
                for (unsigned x=1; x<wh-1; ++x) {
                    auto startVel = Load(velFieldT0, UInt2(x, y));
                    Float2 tap = Float2(float(x), float(y)) - (deltaTime * velFieldScale) * startVel;
                    tap[0] = Clamp(tap[0], 0.f, float(wh-1) - 1e-5f);
                    tap[1] = Clamp(tap[1], 0.f, float(wh-1) - 1e-5f);
                    Write(
                        dstValues, UInt2(x, y),
                        LoadBilinear<false>(prevValues, tap));
                }

        } else if (advectionMethod == Advection::ForwardEulerDiv) {

            float stepScale = deltaTime * velFieldScale / float(adjvectionSteps);

            for (unsigned y=1; y<wh-1; ++y)
                for (unsigned x=1; x<wh-1; ++x) {

                    Float2 tap = Float2(float(x), float(y));
                    for (unsigned s=0; s<adjvectionSteps; ++s) {
                        float a = (adjvectionSteps-1-s) / float(adjvectionSteps-1);
                        auto vel = 
                            LinearInterpolate(
                                LoadBilinear<false>(velFieldT0, tap),
                                LoadBilinear<false>(velFieldT1, tap),
                                a);

                        tap -= stepScale * vel;
                        tap[0] = Clamp(tap[0], 0.f, float(wh-1) - 1e-5f);
                        tap[1] = Clamp(tap[1], 0.f, float(wh-1) - 1e-5f);
                    }

                    Write(
                        dstValues, UInt2(x, y),
                        LoadBilinear<false>(prevValues, tap));
                }

        } else if (advectionMethod == Advection::RungeKutta) {

            for (unsigned y=1; y<wh-1; ++y)
                for (unsigned x=1; x<wh-1; ++x) {

                        // This is the RK4 version
                        // We'll use the average of the velocity field at t and
                        // the velocity field at t+dt as an estimate of the field
                        // at t+.5*dt

                    float s = deltaTime * velFieldScale;
                    float halfS = .5f * s;

                    Float2 startTap = Float2(float(x), float(y));

                        // Note that we're tracing the velocity field backwards.
                        // So doing k1 on velField1, and k4 on velFieldT0
                        //      -- hoping this will interact with the velocity diffusion more sensibly
                    auto k1 = Load(velFieldT1, UInt2(x, y));
                    auto k2 = .5f * LoadBilinear<true>(velFieldT0, startTap - halfS * k1)
                            + .5f * LoadBilinear<true>(velFieldT1, startTap - halfS * k1)
                            ;
                    auto k3 = .5f * LoadBilinear<true>(velFieldT0, startTap - halfS * k2)
                            + .5f * LoadBilinear<true>(velFieldT1, startTap - halfS * k2)
                            ;
                    auto k4 = LoadBilinear<true>(velFieldT0, startTap - s * k3);

                    auto tap = startTap - (s / 6.f) * (k1 + 2.f * k2 + 2.f * k3 + k4);
                    Write(
                        dstValues, UInt2(x, y),
                        LoadBilinear<true>(prevValues, tap));

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

        unsigned wh = _dimensions[0] + 2;
        VectorX delW(wh * wh);
        VectorX q(wh * wh);

        for (unsigned y=1; y<wh-1; ++y)
            for (unsigned x=1; x<wh-1; ++x)
                delW[y*wh+x] = 
                    -0.5f * 
                    (
                          (*velField._u)[y*wh+x+1] - (*velField._u)[y*wh+x-1]
                        + (*velField._v)[(y+1)*wh+x] - (*velField._v)[(y-1)*wh+x]
                    );

        ZeroBorder(delW, wh);
        ZeroBorder(q, wh);
        auto iterations = SolvePoisson(
            q, AMat2D { wh, 1.f + 4.f, 1.f }, 
            delW, (PossionSolver)settings._enforceIncompressibilityMethod);

        for (unsigned y=1; y<wh-1; ++y)
            for (unsigned x=1; x<wh-1; ++x) {
                (*velField._u)[y*wh+x] -= .5f * (delW[y*wh+x+1] - delW[y*wh+x-1]);
                (*velField._v)[y*wh+x] -= .5f * (delW[(y+1)*wh+x] - delW[(y-1)*wh+x]);
            }

        LogInfo << "EnforceIncompressibility took: " << iterations << " iterations.";
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

        // _pimpl->_prevVelU = _pimpl->_velU;
        // _pimpl->_prevVelV = _pimpl->_velV;
        // VectorX newU(N), newV(N);
        // _pimpl->VelocityAdvect(
        //     VelocityField2D { &newU, &newV, D+2 },
        //     VelocityField2D { &_pimpl->_prevVelU, &_pimpl->_prevVelV, D+2 },
        //     settings);
        // 
        // ZeroBorder(newU, D+2);
        // ZeroBorder(newV, D+2);
        // _pimpl->EnforceIncompressibility(
        //     VelocityField2D { &newU, &newV, D+2 },
        //     settings);
        // 
        // _pimpl->_velU = newU;
        // _pimpl->_velV = newV;

        _pimpl->EnforceIncompressibility(
            VelocityField2D { &_pimpl->_velU, &_pimpl->_velV, D+2 },
            settings);

        for (unsigned c=0; c<N; ++c) {
            _pimpl->_prevVelU[c] = 0.f;
            _pimpl->_prevVelV[c] = 0.f;
            _pimpl->_prevDensity[c] = 0.f;
        }
    }

    void FluidSolver2D::AddDensity(UInt2 coords, float amount)
    {
        if (coords[0] < _pimpl->_dimensions[0] && coords[1] < _pimpl->_dimensions[1]) {
            unsigned i = (coords[0]+1) + (coords[1]+1) * (_pimpl->_dimensions[0] + 2);
            _pimpl->_prevDensity[i] += amount;
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
            _pimpl->_velU.data(), _pimpl->_velV.data());
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

        _pimpl->_velU = VectorX(N);
        _pimpl->_velV = VectorX(N);
        _pimpl->_density = VectorX(N);
        _pimpl->_prevVelU = VectorX(N);
        _pimpl->_prevVelV = VectorX(N);
        _pimpl->_prevDensity = VectorX(N);

        _pimpl->_velU.fill(0.f);
        _pimpl->_velV.fill(0.f);
        _pimpl->_density.fill(0.f);
        _pimpl->_prevVelU.fill(0.f);
        _pimpl->_prevVelV.fill(0.f);
        _pimpl->_prevDensity.fill(0.f);

        _pimpl->_workingX = VectorX(N);
        _pimpl->_workingB = VectorX(N);

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

        _pimpl->_bands[0] =  -int(wh);
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
        _advectionMethod = 0;
        _advectionSteps = 4;
        _enforceIncompressibilityMethod = 0;
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
        props.Add(u("AdvectionMethod"), DefaultGet(Obj, _advectionMethod),  DefaultSet(Obj, _advectionMethod));
        props.Add(u("AdvectionSteps"), DefaultGet(Obj, _advectionSteps),  DefaultSet(Obj, _advectionSteps));
        props.Add(u("EnforceIncompressibility"), DefaultGet(Obj, _enforceIncompressibilityMethod),  DefaultSet(Obj, _enforceIncompressibilityMethod));
        init = true;
    }
    return props;
}

