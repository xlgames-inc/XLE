// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Vector.h"

namespace XLEMath
{
    namespace RNFSample
    { 
        const unsigned Clamp = 1<<0; 
        const unsigned Cubic = 1<<1; 
    }

    /// <summary>A 2D field of 2D vectors</summary>
    /// This is an abstraction of a field of vectors. It provides
    /// a simple interface that is independent of the underlying storage
    /// implementation. This allows us to implement general math operations
    /// on fields of data without too many restriction on how the data is
    /// stored in memory (and whether the calling code is using vector
    /// implementations from Eigen, CML, or some other library).
    ///
    /// This implementation is a square grid of vectors. X and Y components
    /// of the vectors are not interleaved (ie, they are stored separately
    /// in memory)
    template<typename Store>
        class VectorField2DSeparate
    {
    public:
        Store* _u, *_v; 
        unsigned _wh;

        using ValueType = Float2;
        using Coord = UInt2;
        using FloatCoord = Float2;

        inline unsigned Width() const { return _wh; }
        inline unsigned Height() const { return _wh; }

        ValueType Load(Coord c) const;
        void Write(Coord x, ValueType value);

        template<unsigned SamplingFlags = RNFSample::Clamp>
            ValueType Sample(FloatCoord c) const;

        void GatherNeighbors(ValueType neighbours[8], float weights[4], FloatCoord coord) const;

        VectorField2DSeparate() : _u(nullptr), _v(nullptr), _wh(0) {}
        VectorField2DSeparate(Store* u, Store* v, unsigned wh) : _u(u), _v(v), _wh(wh) {}
    };

    template<typename Store>
        class ScalarField2D
    {
    public:
        Store* _u;
        unsigned _wh;

        using ValueType = float;
        using Coord = UInt2;
        using FloatCoord = Float2;

        inline unsigned Width() const { return _wh; }
        inline unsigned Height() const { return _wh; }

        ValueType Load(Coord c) const;
        void Write(Coord x, ValueType value);

        template<unsigned SamplingFlags = RNFSample::Clamp>
            ValueType Sample(FloatCoord c) const;

        void GatherNeighbors(ValueType neighbours[8], float weights[4], FloatCoord coord) const;

        ScalarField2D() : _u(nullptr), _wh(0) {}
        ScalarField2D(Store* u, unsigned wh) : _u(u), _wh(wh) {}
    };

///////////////////////////////////////////////////////////////////////////////////////////////////
        //      U T I L I I T I E S
///////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename Vec>
        static void ZeroBorder2D(Vec& v, unsigned wh)
    {
        for (unsigned i = 1; i < wh - 1; ++i) {
            v[i] = 0.f;             // top
            v[i+(wh-1)*wh] = 0.f;   // bottom
            v[i*wh] = 0.f;          // left
            v[i*wh+(wh-1)] = 0.f;   // right
        }

            // 4 corners
        v[0] = 0.f;
        v[wh-1]= 0.f;
        v[(wh-1)*wh] = 0.f;
        v[(wh-1)*wh+wh-1] = 0.f;
    }

    inline unsigned XY(unsigned x, unsigned y, unsigned wh) { return y*wh+x; }
    template <typename Vec>
        static void ReflectUBorder2D(Vec& v, unsigned wh)
    {
        #define XY(x,y) XY(x,y,wh)
        for (unsigned i = 1; i < wh - 1; ++i) {
            v[XY(0, i)]     = -v[XY(1,i)];
            v[XY(wh-1, i)]  = -v[XY(wh-2,i)];
            v[XY(i, 0)]     =  v[XY(i, 1)];
            v[XY(i, wh-1)]  =  v[XY(i, wh-2)];
        }

            // 4 corners
        v[XY(0,0)]          = 0.5f*(v[XY(1,0)]          + v[XY(0,1)]);
        v[XY(0,wh-1)]       = 0.5f*(v[XY(1,wh-1)]       + v[XY(0,wh-2)]);
        v[XY(wh-1,0)]       = 0.5f*(v[XY(wh-2,0)]       + v[XY(wh-1,1)]);
        v[XY(wh-1,wh-1)]    = 0.5f*(v[XY(wh-2,wh-1)]    + v[XY(wh-1,wh-2)]);
        #undef XY
    }

    template <typename Vec>
        static void ReflectVBorder2D(Vec& v, unsigned wh)
    {
        #define XY(x,y) XY(x,y,wh)
        for (unsigned i = 1; i < wh - 1; ++i) {
            v[XY(0, i)]     =  v[XY(1,i)];
            v[XY(wh-1, i)]  =  v[XY(wh-2,i)];
            v[XY(i, 0)]     = -v[XY(i, 1)];
            v[XY(i, wh-1)]  = -v[XY(i, wh-2)];
        }

            // 4 corners
        v[XY(0,0)]          = 0.5f*(v[XY(1,0)]          + v[XY(0,1)]);
        v[XY(0,wh-1)]       = 0.5f*(v[XY(1,wh-1)]       + v[XY(0,wh-2)]);
        v[XY(wh-1,0)]       = 0.5f*(v[XY(wh-2,0)]       + v[XY(wh-1,1)]);
        v[XY(wh-1,wh-1)]    = 0.5f*(v[XY(wh-2,wh-1)]    + v[XY(wh-1,wh-2)]);
        #undef XY
    }

    template <typename Vec>
        static void SmearBorder2D(Vec& v, unsigned wh)
    {
        #define XY(x,y) XY(x,y,wh)
        for (unsigned i = 1; i < wh - 1; ++i) {
            v[XY(0, i)]     =  v[XY(1,i)];
            v[XY(wh-1, i)]  =  v[XY(wh-2,i)];
            v[XY(i, 0)]     =  v[XY(i, 1)];
            v[XY(i, wh-1)]  =  v[XY(i, wh-2)];
        }

            // 4 corners
        v[XY(0,0)]          = 0.5f*(v[XY(1,0)]          + v[XY(0,1)]);
        v[XY(0,wh-1)]       = 0.5f*(v[XY(1,wh-1)]       + v[XY(0,wh-2)]);
        v[XY(wh-1,0)]       = 0.5f*(v[XY(wh-2,0)]       + v[XY(wh-1,1)]);
        v[XY(wh-1,wh-1)]    = 0.5f*(v[XY(wh-2,wh-1)]    + v[XY(wh-1,wh-2)]);
        #undef XY
    }

}

