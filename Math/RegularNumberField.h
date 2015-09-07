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
        UInt2 _dims;

        using ValueType = Float2;
        using Coord = UInt2;
        using FloatCoord = Float2;

        inline UInt2 Dimensions() const { return _dims; }

        ValueType Load(Coord c) const;
        void Write(Coord x, ValueType value);

        template<unsigned SamplingFlags = RNFSample::Clamp>
            ValueType Sample(FloatCoord c) const;

        static const unsigned NeighborCount = 9;
        static const unsigned BilinearWeightCount = 4;
        void GatherNeighbors(ValueType neighbours[9], float weights[4], FloatCoord coord) const;

        VectorField2DSeparate() : _u(nullptr), _v(nullptr), _dims(0, 0) {}
        VectorField2DSeparate(Store* u, Store* v, UInt2 dims) : _u(u), _v(v), _dims(dims) {}
    };

    template<typename Store>
        class VectorField3DSeparate
    {
    public:
        Store* _u, *_v, *_w; 
        UInt3 _dims;

        using ValueType = Float3;
        using Coord = UInt3;
        using FloatCoord = Float3;

        inline UInt3 Dimensions() const { return _dims; }

        ValueType Load(Coord c) const;
        void Write(Coord x, ValueType value);

        template<unsigned SamplingFlags = RNFSample::Clamp>
            ValueType Sample(FloatCoord c) const;

        static const unsigned NeighborCount = 27;
        static const unsigned BilinearWeightCount = 8;
        void GatherNeighbors(ValueType neighbours[27], float weights[8], FloatCoord coord) const;

        VectorField3DSeparate() : _u(nullptr), _v(nullptr), _dims(0, 0, 0) {}
        VectorField3DSeparate(Store* u, Store* v, Store* w, UInt3 dims) : _u(u), _v(v), _w(w), _dims(dims) {}
    };

    template<typename Store>
        class ScalarField2D
    {
    public:
        Store* _u;
        UInt2 _dims;

        using ValueType = float;
        using Coord = UInt2;
        using FloatCoord = Float2;

        inline UInt2 Dimensions() const { return _dims; }

        ValueType Load(Coord c) const;
        void Write(Coord x, ValueType value);

        template<unsigned SamplingFlags = RNFSample::Clamp>
            ValueType Sample(FloatCoord c) const;

        static const unsigned NeighborCount = 9;
        static const unsigned BilinearWeightCount = 4;
        void GatherNeighbors(ValueType neighbours[9], float weights[4], FloatCoord coord) const;

        ScalarField2D() : _u(nullptr), _dims(0, 0) {}
        ScalarField2D(Store* u, UInt2 dims) : _u(u), _dims(dims) {}
    };

    template<typename Store>
        class ScalarField3D
    {
    public:
        Store* _u;
        UInt3 _dims;

        using ValueType = float;
        using Coord = UInt3;
        using FloatCoord = Float3;

        inline UInt3 Dimensions() const { return _dims; }

        ValueType Load(Coord c) const;
        void Write(Coord x, ValueType value);

        template<unsigned SamplingFlags = RNFSample::Clamp>
            ValueType Sample(FloatCoord c) const;

        static const unsigned NeighborCount = 27;
        static const unsigned BilinearWeightCount = 8;
        void GatherNeighbors(ValueType neighbours[27], float weights[8], FloatCoord coord) const;

        ScalarField3D() : _u(nullptr), _dims(0, 0, 0) {}
        ScalarField3D(Store* u, UInt3 dims) : _u(u), _dims(dims) {}
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

    template <typename Vec>
        static void CopyBorder2D(Vec& dst, const Vec& src, unsigned wh)
    {
        for (unsigned i = 1; i < wh - 1; ++i) {
            dst[i] = src[i];
            dst[i+(wh-1)*wh] = src[i+(wh-1)*wh];
            dst[i*wh] = src[i*wh];
            dst[i*wh+(wh-1)] = src[i*wh+(wh-1)];
        }

            // 4 corners
        dst[0] = src[0];
        dst[wh-1]= src[wh-1];
        dst[(wh-1)*wh] = src[(wh-1)*wh];
        dst[(wh-1)*wh+wh-1] = src[(wh-1)*wh+wh-1];
    }

    inline unsigned XY(unsigned x, unsigned y, unsigned wh)                 { return y*wh+x; }
    inline unsigned XYZ(unsigned x, unsigned y, unsigned z, UInt3 dims)     { return (z*dims[1]+y)*dims[0]+x; }
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

    template <typename Vec>
        static void ZeroBorder3D(Vec& v, UInt3 dims)
    {
        auto LX = dims[0]-1, LY = dims[1]-1, LZ = dims[2]-1;
        #define XYZ(x,y,z) XYZ(x,y,z,dims)

            // 6 faces
        for (unsigned y=1; y<dims[1]-1; ++y)
            for (unsigned x=1; x<dims[0]-1; ++x) {
                v[XYZ(x,y, 0)]  = 0.f;
                v[XYZ(x,y,LZ)]  = 0.f;
            }

        for (unsigned z=1; z<dims[2]-1; ++z)
            for (unsigned x=1; x<dims[0]-1; ++x) {
                v[XYZ(x, 0,z)]  = 0.f;
                v[XYZ(x,LY,z)]  = 0.f;
            }

        for (unsigned z=1; z<dims[2]-1; ++z)
            for (unsigned y=1; y<dims[1]-1; ++y) {
                v[XYZ( 0,y,z)]  = 0.f;
                v[XYZ(LX,y,z)]  = 0.f;
            }

            // 12 edges
        for (unsigned x=1; x<dims[0]-1; ++x) {
            v[XYZ(x, 0, 0)]     = 0.f;
            v[XYZ(x, 0,LZ)]     = 0.f;
            v[XYZ(x,LY, 0)]     = 0.f;
            v[XYZ(x,LY,LZ)]     = 0.f;
        }

        for (unsigned y=1; y<dims[1]-1; ++y) {
            v[XYZ( 0,y, 0)]     = 0.f;
            v[XYZ( 0,y,LZ)]     = 0.f;
            v[XYZ(LX,y, 0)]     = 0.f;
            v[XYZ(LX,y,LZ)]     = 0.f;
        }

        for (unsigned z=1; z<dims[2]-1; ++z) {
            v[XYZ( 0, 0,z)]     = 0.f;
            v[XYZ( 0,LY,z)]     = 0.f;
            v[XYZ(LX, 0,z)]     = 0.f;
            v[XYZ(LX,LY,z)]     = 0.f;
        }

            // 8 corners
        v[XYZ( 0,  0,  0)] = 0.f;
        v[XYZ(LX,  0,  0)] = 0.f;
        v[XYZ( 0, LY,  0)] = 0.f;
        v[XYZ(LX, LY,  0)] = 0.f;

        v[XYZ( 0,  0, LZ)] = 0.f;
        v[XYZ(LX,  0, LZ)] = 0.f;
        v[XYZ( 0, LY, LZ)] = 0.f;
        v[XYZ(LX, LY, LZ)] = 0.f;
        #undef XY
    }

    template <typename Vec>
        static void CopyBorder3D(Vec& dst, const Vec& src, UInt3 dims)
    {
        auto LX = dims[0]-1, LY = dims[1]-1, LZ = dims[2]-1;
        #define XYZ(x,y,z) XYZ(x,y,z,dims)

            // 6 faces
        for (unsigned y=1; y<dims[1]-1; ++y)
            for (unsigned x=1; x<dims[0]-1; ++x) {
                dst[XYZ(x,y, 0)]  = src[XYZ(x,y, 0)];
                dst[XYZ(x,y,LZ)]  = src[XYZ(x,y,LZ)];
            }

        for (unsigned z=1; z<dims[2]-1; ++z)
            for (unsigned x=1; x<dims[0]-1; ++x) {
                dst[XYZ(x, 0,z)]  = src[XYZ(x, 0,z)];
                dst[XYZ(x,LY,z)]  = src[XYZ(x,LY,z)];
            }

        for (unsigned z=1; z<dims[2]-1; ++z)
            for (unsigned y=1; y<dims[1]-1; ++y) {
                dst[XYZ( 0,y,z)]  = src[XYZ( 0,y,z)];
                dst[XYZ(LX,y,z)]  = src[XYZ(LX,y,z)];
            }

            // 12 edges
        for (unsigned x=1; x<dims[0]-1; ++x) {
            dst[XYZ(x, 0, 0)]     = src[XYZ(x, 0, 0)];
            dst[XYZ(x, 0,LZ)]     = src[XYZ(x, 0,LZ)];
            dst[XYZ(x,LY, 0)]     = src[XYZ(x,LY, 0)];
            dst[XYZ(x,LY,LZ)]     = src[XYZ(x,LY,LZ)];
        }

        for (unsigned y=1; y<dims[1]-1; ++y) {
            dst[XYZ( 0,y, 0)]     = src[XYZ( 0,y, 0)];
            dst[XYZ( 0,y,LZ)]     = src[XYZ( 0,y,LZ)];
            dst[XYZ(LX,y, 0)]     = src[XYZ(LX,y, 0)];
            dst[XYZ(LX,y,LZ)]     = src[XYZ(LX,y,LZ)];
        }

        for (unsigned z=1; z<dims[2]-1; ++z) {
            dst[XYZ( 0, 0,z)]     = src[XYZ( 0, 0,z)];
            dst[XYZ( 0,LY,z)]     = src[XYZ( 0,LY,z)];
            dst[XYZ(LX, 0,z)]     = src[XYZ(LX, 0,z)];
            dst[XYZ(LX,LY,z)]     = src[XYZ(LX,LY,z)];
        }

            // 8 corners
        dst[XYZ( 0,  0,  0)] = src[XYZ( 0,  0,  0)];
        dst[XYZ(LX,  0,  0)] = src[XYZ(LX,  0,  0)];
        dst[XYZ( 0, LY,  0)] = src[XYZ( 0, LY,  0)];
        dst[XYZ(LX, LY,  0)] = src[XYZ(LX, LY,  0)];

        dst[XYZ( 0,  0, LZ)] = src[XYZ( 0,  0, LZ)];
        dst[XYZ(LX,  0, LZ)] = src[XYZ(LX,  0, LZ)];
        dst[XYZ( 0, LY, LZ)] = src[XYZ( 0, LY, LZ)];
        dst[XYZ(LX, LY, LZ)] = src[XYZ(LX, LY, LZ)];
        #undef XY
    }

    template <typename Vec>
        static void ReflectBorder3D(Vec& v, UInt3 dims, unsigned reflectionAxis)
    {
        auto LX = dims[0]-1, LY = dims[1]-1, LZ = dims[2]-1;
        #define XYZ(x,y,z) XYZ(x,y,z,dims)

            // 6 faces
        auto reflect = (reflectionAxis==2)?-1.f:1.f;
        for (unsigned y=1; y<dims[1]-1; ++y)
            for (unsigned x=1; x<dims[0]-1; ++x) {
                v[XYZ(x,y,0)]   = reflect * v[XYZ(x,y,1)];
                v[XYZ(x,y,LZ)]  = reflect * v[XYZ(x,y,LZ-1)];
            }

        reflect = (reflectionAxis==1)?-1.f:1.f;
        for (unsigned z=1; z<dims[2]-1; ++z)
            for (unsigned x=1; x<dims[0]-1; ++x) {
                v[XYZ(x,0,z)]   = reflect * v[XYZ(x,1,z)];
                v[XYZ(x,LY,z)]  = reflect * v[XYZ(x,LY-1,z)];
            }

        reflect = (reflectionAxis==0)?-1.f:1.f;
        for (unsigned z=1; z<dims[2]-1; ++z)
            for (unsigned y=1; y<dims[1]-1; ++y) {
                v[XYZ(0,y,z)]   = reflect * v[XYZ(1,y,z)];
                v[XYZ(LX,y,z)]  = reflect * v[XYZ(LX-1,y,z)];
            }

            // 12 edges
        for (unsigned x=1; x<dims[0]-1; ++x) {
            v[XYZ(x, 0, 0)]     = 0.5f*(v[XYZ(x,   1, 0)]   + v[XYZ(x, 0,   1)]);
            v[XYZ(x, 0,LZ)]     = 0.5f*(v[XYZ(x,   1,LZ)]   + v[XYZ(x, 0,LZ-1)]);
            v[XYZ(x,LY, 0)]     = 0.5f*(v[XYZ(x,LY-1, 0)]   + v[XYZ(x,LY,   1)]);
            v[XYZ(x,LY,LZ)]     = 0.5f*(v[XYZ(x,LY-1,LZ)]   + v[XYZ(x,LY,LZ-1)]);
        }

        for (unsigned y=1; y<dims[0]-1; ++y) {
            v[XYZ( 0,y, 0)]     = 0.5f*(v[XYZ(   1,y, 0)]   + v[XYZ( 0,y,   1)]);
            v[XYZ( 0,y,LZ)]     = 0.5f*(v[XYZ(   1,y,LZ)]   + v[XYZ( 0,y,LZ-1)]);
            v[XYZ(LX,y, 0)]     = 0.5f*(v[XYZ(LX-1,y, 0)]   + v[XYZ(LX,y,   1)]);
            v[XYZ(LX,y,LZ)]     = 0.5f*(v[XYZ(LX-1,y,LZ)]   + v[XYZ(LX,y,LZ-1)]);
        }

        for (unsigned z=1; z<dims[2]-1; ++z) {
            v[XYZ( 0, 0,z)]     = 0.5f*(v[XYZ(   1, 0,z)]   + v[XYZ( 0,   1,z)]);
            v[XYZ( 0,LY,z)]     = 0.5f*(v[XYZ(   1,LY,z)]   + v[XYZ( 0,LY-1,z)]);
            v[XYZ(LX, 0,z)]     = 0.5f*(v[XYZ(LX-1, 0,z)]   + v[XYZ(LX,   1,z)]);
            v[XYZ(LX,LY,z)]     = 0.5f*(v[XYZ(LX-1,LY,z)]   + v[XYZ(LX,LY-1,z)]);
        }

            // 8 corners
        v[XYZ( 0, 0, 0)]        = (v[XYZ(   1, 0, 0)] + v[XYZ( 0,   1, 0)] + v[XYZ( 0, 0,   1)])/3.f;
        v[XYZ(LX, 0, 0)]        = (v[XYZ(LX-1, 0, 0)] + v[XYZ(LX,   1, 0)] + v[XYZ(LX, 0,   1)])/3.f;

        v[XYZ( 0, LY, 0)]       = (v[XYZ(   1,LY, 0)] + v[XYZ( 0,LY-1, 0)] + v[XYZ( 0,LY,   1)])/3.f;
        v[XYZ(LX, LY, 0)]       = (v[XYZ(LX-1,LY, 0)] + v[XYZ(LX,LY-1, 0)] + v[XYZ(LX,LY,   1)])/3.f;

        v[XYZ( 0, 0, LZ)]       = (v[XYZ(   1, 0,LZ)] + v[XYZ( 0,   1,LZ)] + v[XYZ( 0, 0,LZ-1)])/3.f;
        v[XYZ(LX, 0, LZ)]       = (v[XYZ(LX-1, 0,LZ)] + v[XYZ(LX,   1,LZ)] + v[XYZ(LX, 0,LZ-1)])/3.f;

        v[XYZ( 0, LY, LZ)]      = (v[XYZ(   1,LY,LZ)] + v[XYZ( 0,LY-1,LZ)] + v[XYZ( 0,LY,LZ-1)])/3.f;
        v[XYZ(LX, LY, LZ)]      = (v[XYZ(LX-1,LY,LZ)] + v[XYZ(LX,LY-1,LZ)] + v[XYZ(LX,LY,LZ-1)])/3.f;
        
        #undef XY
    }

    template <typename Vec>
        static void SmearBorder3D(Vec& v, UInt3 dims)
    {
        ReflectBorder3D(v, dims, 4);        // (same as ReflectBorder with an invalid reflection axis)
    }

}

