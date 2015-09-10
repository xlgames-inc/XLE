// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "PoissonSolver.h"
#include "Vector.h"
#include <functional>

namespace XLEMath
{
    
    namespace PoissonSolverInternal
    {
        template<typename Vec, typename Mat>
            static void SolveLowerTriangular(Vec& x, const Mat& M, const Vec& b, unsigned N)
        {
                // solve: M * dst = b
                // for a lower triangular matrix, using forward substitution
            for (unsigned i=0; i<N; ++i) {
                auto d = b(i);
                for (unsigned j=0; j<i; ++j) {
                    d -= M(i, j) * x(j);
                }
                x(i) = d / M(i, i);
            }
        }

        template<typename Mat>
            class SparseBandedMatrix
        {
        public:
            const int *_bands;
            unsigned _bandCount;
            Mat _underlying;

            SparseBandedMatrix() { _bandCount = 0; _bands = nullptr; }
            SparseBandedMatrix(Mat&& underlying, const int bands[], unsigned bandCount)
                : _underlying(std::move(underlying))
                { _bands = bands; _bandCount = bandCount; }
            ~SparseBandedMatrix() {}
        };

        template<typename Vec, typename Mat>
            static void SolveLowerTriangular(Vec& x, const SparseBandedMatrix<Mat>& M, const Vec& b, unsigned N)
        {
                // assuming the last "band" in the matrix is the diagonal aprt
            assert(M._bandCount > 0 && M._bands[M._bandCount-1] == 0);

                // solve: M * dst = b
                // for a lower triangular matrix, using forward substitution
                // this is for a sparse banded matrix, with the bands described by "bands"
                //      -- note that we can improve this further by writing implementations for
                //          common cases (eg, 2D, 3D, etc)
            const bool reflectEdges = true;
            if (constant_expression<!reflectEdges>::result()) {
                for (unsigned i=0; i<N; ++i) {
                    auto d = b(i);
                    for (unsigned j=0; j<M._bandCount-1; ++j) {
                        int j2 = int(i) + M._bands[j];
                        if (j2 >= 0 && j2 < int(i))  // with agressive unrolling, we should avoid this condition
                            d -= M._underlying(i, j) * x[j2];
                    }
                    x[i] = d / M._underlying(i, M._bandCount-1);
                }
            } else {
                for (unsigned i=0; i<N; ++i) {
                    auto d = b(i);
                    for (unsigned j=0; j<M._bandCount-1; ++j) {
                        int j2 = (int(i) + M._bands[j] + int(N))%int(N);
                        d -= M._underlying(i, j) * x[j2];
                    }
                    x[i] = d / M._underlying(i, M._bandCount-1);
                }
            }
        }

        template<typename Vec, typename Mat>
            static void Multiply(Vec& dst, const SparseBandedMatrix<Mat>& A, const Vec& b, unsigned N)
        {
            for (unsigned i=0; i<N; ++i) {
                decltype(dst[0]) d = 0;
                for (unsigned j=0; j<A._bandCount; ++j) {
                    int j2 = int(i) + A._bands[j];
                    if (j2 >= 0 && j2 < int(N))  // with agressive unrolling, we should avoid this condition
                        d += A._underlying(i, j) * b[j2];
                }
                dst[i] = d;
            }
        }

        template<typename Vec>
            static void Multiply(Vec& dst, const std::function<float(unsigned, unsigned)>& A, const Vec& b, unsigned N)
        {
            for (unsigned i=0; i<N; ++i) {
                decltype(dst[0]) d = 0.f;
                for (unsigned j=0; j<N; ++j) {
                    d += A(i, j) * b[j];
                }
                dst[i] = d;
            }
        }

        class AMat
        {
        public:
            UInt3 _dims;
            UInt3 _borders;
            unsigned _dimensionality;
            float _a0, _a1;
            float _a0e, _a0c, _a1e, _a1r;
        };
    
        inline unsigned GetN(const AMat& A)       { return A._dims[0] * A._dims[1] * A._dims[2]; }
        inline unsigned GetWidth(const AMat& A)   { return A._dims[0]; }
        inline unsigned GetHeight(const AMat& A)  { return A._dims[1]; }
        inline unsigned GetDepth(const AMat& A)   { return A._dims[2]; }
        inline UInt3 GetBorders(const AMat& A)    { return A._borders; }

        template <typename Vec>
            static void Multiply(Vec& dst, const AMat& A, const Vec& b, unsigned N)
        {
            const auto width = GetWidth(A), height = GetHeight(A);

            if (A._dimensionality==2) {
                const UInt2 bor(1,1);
                for (unsigned y=bor[1]; y<height-bor[1]; ++y) {
                    for (unsigned x=bor[0]; x<width-bor[0]; ++x) {
                        const unsigned i = y*width + x;

                        auto v = A._a0 * b[i];
                        v += A._a1 * b[i-1];
                        v += A._a1 * b[i+1];
                        v += A._a1 * b[i-width];
                        v += A._a1 * b[i+width];

                        dst[i] = v;
                    }
                }

                    // do the borders, as well --
                    //      4 edges & 4 corners
                const auto w = width, h = height;
                #define XY(x,y) XY_WH(x,y,w)
                for (unsigned i=1; i<w-1; ++i) {
                    dst[XY(i, 0)]       = A._a0e *  b[XY(  i,   0)] 
                                        + A._a1e * (b[XY(  i,   1)] + b[XY(i-1, 0)] + b[XY(i+1, 0)])
                                        + A._a1r * (b[XY(  i, h-1)]);
                    dst[XY(i, h-1)]     = A._a0e *  b[XY(  i, h-1)] 
                                        + A._a1e * (b[XY(  i, h-2)] + b[XY(i-1, h-1)] + b[XY(i+1, h-1)])
                                        + A._a1r * (b[XY(  i,   0)]);
                }

                for (unsigned i=1; i<h-1; ++i) {
                    dst[XY(0, i)]       = A._a0e *  b[XY(  0,   i)] 
                                        + A._a1e * (b[XY(  1,   i)] + b[XY(0, i-1)] + b[XY(0, i+1)])
                                        + A._a1r * (b[XY(w-1,   i)]);
                    dst[XY(w-1, i)]     = A._a0e *  b[XY(w-1,   i)] 
                                        + A._a1e * (b[XY(w-2,   i)] + b[XY(w-1, i-1)] + b[XY(w-1, i+1)])
                                        + A._a1r * (b[XY(  0,   i)]);
                }
                
                dst[XY(0, 0)]           = A._a0c *  b[XY(  0,   0)] 
                                        + A._a1e * (b[XY(  0,   1)] + b[XY(  1,   0)])
                                        + A._a1r * (b[XY(w-1,   0)] + b[XY(  0, h-1)]);
                dst[XY(0, h-1)]         = A._a0c *  b[XY(  0, h-1)] 
                                        + A._a1e * (b[XY(  0, h-2)] + b[XY(  1, h-1)])
                                        + A._a1r * (b[XY(w-1, h-1)] + b[XY(  0,   0)]);

                dst[XY(w-1, 0)]         = A._a0c *  b[XY(w-1,   0)] 
                                        + A._a1e * (b[XY(w-1,   1)] + b[XY(w-2,   0)])
                                        + A._a1r * (b[XY(  0,   0)] + b[XY(w-1, h-1)]);
                dst[XY(w-1, h-1)]       = A._a0c *  b[XY(w-1, h-1)] 
                                        + A._a1e * (b[XY(w-1, h-2)] + b[XY(w-2, h-1)])
                                        + A._a1r * (b[XY(w-1,   0)] + b[XY(  0, h-1)]);
                #undef XY

            } else {
                const UInt3 bor(1,1,1);
                for (unsigned z=bor[2]; z<GetDepth(A)-bor[2]; ++z) {
                    for (unsigned y=bor[1]; y<height-bor[1]; ++y) {
                        for (unsigned x=bor[0]; x<width-bor[0]; ++x) {
                            const unsigned i = (z*height+y)*width + x;

                            auto v = A._a0 * b[i];
                            v += A._a1 * b[i-width*height];
                            v += A._a1 * b[i-width];
                            v += A._a1 * b[i-1];
                            v += A._a1 * b[i+1];
                            v += A._a1 * b[i+width];
                            v += A._a1 * b[i+width*height];

                            dst[i] = v;
                        }
                    }
                }
            }
        }
        
    }
}
