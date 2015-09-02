// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RegularNumberField.h"

#pragma warning(disable:4714)
#pragma push_macro("new")
#undef new
#include <Eigen/Dense>
#pragma pop_macro("new")

namespace XLEMath
{

///////////////////////////////////////////////////////////////////////////////////////////////////
            //   B I L I N E A R   
///////////////////////////////////////////////////////////////////////////////////////////////////

    template<unsigned SamplingFlags, typename Store>
        static Float2 SampleBilinear(const VectorField2DSeparate<Store>& field, Float2 coord)
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
        
        const auto dims = field.Dimensions();
        if (constant_expression<(SamplingFlags & RNFSample::Clamp)!=0>::result()) {
            x0 = unsigned(Clamp(fx, 0.f, float(dims[0]-1)));
            x1 = std::min(x0+1u, dims[0]-1u);
            y0 = unsigned(Clamp(fy, 0.f, float(dims[1]-1)));
            y1 = std::min(y0+1u, dims[1]-1u);
        } else {
            x0 = unsigned(fx); x1 = x0+1;
            y0 = unsigned(fy); y1 = y0+1;
        }
        assert(x1 < dims[0] && y1 < dims[1]);
        
        float u
            = weights[0] * (*field._u)[y0*dims[0]+x0]
            + weights[1] * (*field._u)[y0*dims[0]+x1]
            + weights[2] * (*field._u)[y1*dims[0]+x0]
            + weights[3] * (*field._u)[y1*dims[0]+x1]
            ;
        float v
            = weights[0] * (*field._v)[y0*dims[0]+x0]
            + weights[1] * (*field._v)[y0*dims[0]+x1]
            + weights[2] * (*field._v)[y1*dims[0]+x0]
            + weights[3] * (*field._v)[y1*dims[0]+x1]
            ;
        return Float2(u, v);
    }

    template<unsigned SamplingFlags, typename Store>
        static float SampleBilinear(const ScalarField2D<Store>& field, Float2 coord)
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
        
        const auto dims = field.Dimensions();
        if (constant_expression<(SamplingFlags & RNFSample::Clamp)!=0>::result()) {
            x0 = unsigned(Clamp(fx, 0.f, float(dims[0]-1)));
            x1 = std::min(x0+1u, dims[0]-1u);
            y0 = unsigned(Clamp(fy, 0.f, float(dims[1]-1)));
            y1 = std::min(y0+1u, dims[1]-1u);
        } else {
            x0 = unsigned(fx); x1 = x0+1;
            y0 = unsigned(fy); y1 = y0+1;
        }
        assert(x1 < dims[0] && y1 < dims[1]);
        
        return 
              weights[0] * (*field._u)[y0*dims[0]+x0]
            + weights[1] * (*field._u)[y0*dims[0]+x1]
            + weights[2] * (*field._u)[y1*dims[0]+x0]
            + weights[3] * (*field._u)[y1*dims[0]+x1]
            ;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
            //   M O N O T O N I C   C U B I C
///////////////////////////////////////////////////////////////////////////////////////////////////

    static float Sign(float x) { return (x < 0.f) ? -1.f : (x==0.f) ? 0.f : 1.f; }
    static float MonotonicCubic(float xn1, float x0, float x1, float x2, float alpha)
    {
            // See also http://grmanet.sogang.ac.kr/ihm/webpapers/CLMCI.pdf
            // (Controllable Local Monotonic Cubic Interpolation in Fluid Animations)
            // for a much more expensive, but more flexible, monotonic interpolation
            // method.
            // This method reverts to linear in non-monotonic cases. The above paper
            // attempts to find a good curve even on non-monotonic cases
      
        float dk        = (x1 - xn1) / 2.f;
        float dk1       = (x2 - x0) / 2.f;
        float deltak    = x1 - x0;
        if (Sign(dk) != Sign(deltak) || Sign(dk1) != Sign(deltak))
            dk = dk1 = 0.f;

        float a2 = alpha * alpha;
        float a3 = alpha * a2;

            //      original math from Fedkiw has a small error in the first term
            //      (should be -2*deltak, not -delta)
        return 
              (dk + dk1 - 2.f * deltak) * a3
            + (3.f * deltak - 2.f * dk - dk1) * a2
            + dk * alpha
            + x0;
    }

    template<typename Store>
        float SampleMonotonicCubic(const ScalarField2D<Store>& field, Float2 coord)
    {
        float fx = XlFloor(coord[0]);
        float fy = XlFloor(coord[1]);
        float a = coord[0] - fx, b = coord[1] - fy;

        const auto dims = field.Dimensions();
        unsigned x0 = unsigned(Clamp(fx, 0.f, float(dims[0]-1)));
        unsigned x1 = std::min(x0+1u, dims[0]-1u);
        unsigned y0 = unsigned(Clamp(fy, 0.f, float(dims[1]-1)));
        unsigned y1 = std::min(y0+1u, dims[1]-1u);
        assert(x1 < dims[0] && y1 < dims[1]);

        unsigned x2 = std::min(x1+1u, dims[0]-1u);
        unsigned y2 = std::min(y1+1u, dims[1]-1u);
        unsigned xn1 = std::max(x0, 1u) - 1u;
        unsigned yn1 = std::max(y0, 1u) - 1u;
        
        float u[] = 
        {
            (*field._u)[yn1*dims[0]+xn1],
            (*field._u)[yn1*dims[0]+ x0],
            (*field._u)[yn1*dims[0]+ x1],
            (*field._u)[yn1*dims[0]+ x2],

            (*field._u)[ y0*dims[0]+xn1],
            (*field._u)[ y0*dims[0]+ x0],
            (*field._u)[ y0*dims[0]+ x1],
            (*field._u)[ y0*dims[0]+ x2],

            (*field._u)[ y1*dims[0]+xn1],
            (*field._u)[ y1*dims[0]+ x0],
            (*field._u)[ y1*dims[0]+ x1],
            (*field._u)[ y1*dims[0]+ x2],

            (*field._u)[ y2*dims[0]+xn1],
            (*field._u)[ y2*dims[0]+ x0],
            (*field._u)[ y2*dims[0]+ x1],
            (*field._u)[ y2*dims[0]+ x2]
        };

        float un1 = MonotonicCubic(u[ 0], u[ 1], u[ 2], u[ 3], a);
        float u0  = MonotonicCubic(u[ 4], u[ 5], u[ 6], u[ 7], a);
        float u1  = MonotonicCubic(u[ 8], u[ 9], u[10], u[11], a);
        float u2  = MonotonicCubic(u[12], u[13], u[14], u[15], a);
        return MonotonicCubic(un1, u0, u1, u2, b);
    }

    template<typename Store>
        Float2 SampleMonotonicCubic(const VectorField2DSeparate<Store>& field, Float2 coord)
    {
        float fx = XlFloor(coord[0]);
        float fy = XlFloor(coord[1]);
        float a = coord[0] - fx, b = coord[1] - fy;

        const auto dims = field.Dimensions();
        unsigned x0 = unsigned(Clamp(fx, 0.f, float(dims[0]-1)));
        unsigned x1 = std::min(x0+1u, dims[0]-1u);
        unsigned y0 = unsigned(Clamp(fy, 0.f, float(dims[1]-1)));
        unsigned y1 = std::min(y0+1u, dims[1]-1u);
        assert(x1 < dims[0] && y1 < dims[1]);

        unsigned x2 = std::min(x1+1u, dims[0]-1u);
        unsigned y2 = std::min(y1+1u, dims[1]-1u);
        unsigned xn1 = std::max(x0, 1u) - 1u;
        unsigned yn1 = std::max(y0, 1u) - 1u;
        
        float u[] = 
        {
            (*field._u)[yn1*dims[0]+xn1],
            (*field._u)[yn1*dims[0]+ x0],
            (*field._u)[yn1*dims[0]+ x1],
            (*field._u)[yn1*dims[0]+ x2],

            (*field._u)[ y0*dims[0]+xn1],
            (*field._u)[ y0*dims[0]+ x0],
            (*field._u)[ y0*dims[0]+ x1],
            (*field._u)[ y0*dims[0]+ x2],

            (*field._u)[ y1*dims[0]+xn1],
            (*field._u)[ y1*dims[0]+ x0],
            (*field._u)[ y1*dims[0]+ x1],
            (*field._u)[ y1*dims[0]+ x2],

            (*field._u)[ y2*dims[0]+xn1],
            (*field._u)[ y2*dims[0]+ x0],
            (*field._u)[ y2*dims[0]+ x1],
            (*field._u)[ y2*dims[0]+ x2]
        };

        float v[] = 
        {
            (*field._v)[yn1*dims[0]+xn1],
            (*field._v)[yn1*dims[0]+ x0],
            (*field._v)[yn1*dims[0]+ x1],
            (*field._v)[yn1*dims[0]+ x2],

            (*field._v)[ y0*dims[0]+xn1],
            (*field._v)[ y0*dims[0]+ x0],
            (*field._v)[ y0*dims[0]+ x1],
            (*field._v)[ y0*dims[0]+ x2],

            (*field._v)[ y1*dims[0]+xn1],
            (*field._v)[ y1*dims[0]+ x0],
            (*field._v)[ y1*dims[0]+ x1],
            (*field._v)[ y1*dims[0]+ x2],

            (*field._v)[ y2*dims[0]+xn1],
            (*field._v)[ y2*dims[0]+ x0],
            (*field._v)[ y2*dims[0]+ x1],
            (*field._v)[ y2*dims[0]+ x2]
        };

            // Unfortunately, to get correct cubic interpolation in 2D, we
            // have to do a lot of interpolations (similar to the math used
            // in refining cubic surfaces)
            //
            // We can optimise this by only doing the monotonic interpolation
            // of the U parameter in the U direction (and the V parameter in
            // the V direction). Maybe we could fall back to linear in the 
            // cross-wise direction.
            //
            // It's hard to know what effect that optimisation would have on
            // the advection operation.
        float un1 = MonotonicCubic(u[ 0], u[ 1], u[ 2], u[ 3], a);
        float u0  = MonotonicCubic(u[ 4], u[ 5], u[ 6], u[ 7], a);
        float u1  = MonotonicCubic(u[ 8], u[ 9], u[10], u[11], a);
        float u2  = MonotonicCubic(u[12], u[13], u[14], u[15], a);
        float fu =  MonotonicCubic(un1, u0, u1, u2, b);

        float vn1 = MonotonicCubic(v[ 0], v[ 1], v[ 2], v[ 3], a);
        float v0  = MonotonicCubic(v[ 4], v[ 5], v[ 6], v[ 7], a);
        float v1  = MonotonicCubic(v[ 8], v[ 9], v[10], v[11], a);
        float v2  = MonotonicCubic(v[12], v[13], v[14], v[15], a);
        float fv =  MonotonicCubic(vn1, v0, v1, v2, b);
        return Float2(fu, fv);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
            //   G A T H E R   N E I G H B O R S
///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Store>
        void GatherNeighbors(Float2 neighbours[8], float weights[4], const VectorField2DSeparate<Store>& field, Float2 coord)
    {
        float fx = XlFloor(coord[0]);
        float fy = XlFloor(coord[1]);
        float a = coord[0] - fx, b = coord[1] - fy;
        weights[0] = (1.f - a) * (1.f - b);
        weights[1] = a * (1.f - b);
        weights[2] = (1.f - a) * b;
        weights[3] = a * b;

        const auto dims = field.Dimensions();
        unsigned x0, x1, y0, y1;
        x0 = unsigned(Clamp(fx, 0.f, float(dims[0]-1)));
        x1 = std::min(x0+1u, dims[0]-1u);
        y0 = unsigned(Clamp(fy, 0.f, float(dims[1]-1)));
        y1 = std::min(y0+1u, dims[1]-1u);

        unsigned xx = std::max(x0, 1u) - 1u;
        unsigned yx = std::max(y0, 1u) - 1u;

        neighbours[0][0] = (*field._u)[y0*dims[0]+x0];
        neighbours[1][0] = (*field._u)[y0*dims[0]+x1];
        neighbours[2][0] = (*field._u)[y1*dims[0]+x0];
        neighbours[3][0] = (*field._u)[y1*dims[0]+x1];

        neighbours[4][0] = (*field._u)[yx*dims[0]+xx];
        neighbours[5][0] = (*field._u)[yx*dims[0]+x0];
        neighbours[6][0] = (*field._u)[yx*dims[0]+x1];
        neighbours[7][0] = (*field._u)[y0*dims[0]+xx];
        neighbours[8][0] = (*field._u)[y1*dims[0]+xx];

        neighbours[0][1] = (*field._v)[y0*dims[0]+x0];
        neighbours[1][1] = (*field._v)[y0*dims[0]+x1];
        neighbours[2][1] = (*field._v)[y1*dims[0]+x0];
        neighbours[3][1] = (*field._v)[y1*dims[0]+x1];

        neighbours[4][1] = (*field._v)[yx*dims[0]+xx];
        neighbours[5][1] = (*field._v)[yx*dims[0]+x0];
        neighbours[6][1] = (*field._v)[yx*dims[0]+x1];
        neighbours[7][1] = (*field._v)[y0*dims[0]+xx];
        neighbours[8][1] = (*field._v)[y1*dims[0]+xx];
    }

    template<typename Store>
        void GatherNeighbors(float neighbours[8], float weights[4], const ScalarField2D<Store>& field, Float2 coord)
    {
        float fx = XlFloor(coord[0]);
        float fy = XlFloor(coord[1]);
        float a = coord[0] - fx, b = coord[1] - fy;
        weights[0] = (1.f - a) * (1.f - b);
        weights[1] = a * (1.f - b);
        weights[2] = (1.f - a) * b;
        weights[3] = a * b;

        const auto dims = field.Dimensions();
        unsigned x0, x1, y0, y1;
        x0 = unsigned(Clamp(fx, 0.f, float(dims[0]-1)));
        x1 = std::min(x0+1u, dims[0]-1u);
        y0 = unsigned(Clamp(fy, 0.f, float(dims[1]-1)));
        y1 = std::min(y0+1u, dims[1]-1u);

        unsigned xx = std::max(x0, 1u) - 1u;
        unsigned yx = std::max(y0, 1u) - 1u;

        neighbours[0] = (*field._u)[y0*dims[0]+x0];
        neighbours[1] = (*field._u)[y0*dims[0]+x1];
        neighbours[2] = (*field._u)[y1*dims[0]+x0];
        neighbours[3] = (*field._u)[y1*dims[0]+x1];

        neighbours[4] = (*field._u)[yx*dims[0]+xx];
        neighbours[5] = (*field._u)[yx*dims[0]+x0];
        neighbours[6] = (*field._u)[yx*dims[0]+x1];
        neighbours[7] = (*field._u)[y0*dims[0]+xx];
        neighbours[8] = (*field._u)[y1*dims[0]+xx];
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Store>
        float ScalarField2D<Store>::Load(Coord coord) const
    {
        assert(coord[0] < _dims[0] && coord[1] < _dims[1]);
        return (*_u)[coord[1] * _dims[0] + coord[0]];
    }

    template<typename Store>
        void ScalarField2D<Store>::Write(Coord coord, ValueType value)
    {
        assert(coord[0] < _dims[0] && coord[1] < _dims[1]);
        (*_u)[coord[1] * _dims[0] + coord[0]] = value;
        assert(isfinite(value) && !isnan(value));
    }

    template<typename Store>
    template<unsigned SamplingFlags>
        auto ScalarField2D<Store>::Sample(FloatCoord c) const -> ValueType
    {
        if (constant_expression<(SamplingFlags & RNFSample::Cubic)!=0>::result())
            return SampleMonotonicCubic(*this, c);
        return SampleBilinear<SamplingFlags>(*this, c);
    }

    template<typename Store>
        void ScalarField2D<Store>::GatherNeighbors(ValueType neighbours[8], float weights[4], FloatCoord coord) const
    {
        return XLEMath::GatherNeighbors(neighbours, weights, *this, coord);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Store>
        Float2 VectorField2DSeparate<Store>::Load(Coord coord) const
    {
        assert(coord[0] < _dims[0] && coord[1] < _dims[1]);
        return Float2(
            (*_u)[coord[1] * _dims[0] + coord[0]],
            (*_v)[coord[1] * _dims[0] + coord[0]]);
    }

    template<typename Store>
        void VectorField2DSeparate<Store>::Write(Coord coord, ValueType value)
    {
        assert(coord[0] < _dims[0] && coord[1] < _dims[1]);
        (*_u)[coord[1] * _dims[0] + coord[0]] = value[0];
        (*_v)[coord[1] * _dims[0] + coord[0]] = value[1];
        assert(isfinite(value[0]) && !isnan(value[0]));
        assert(isfinite(value[1]) && !isnan(value[1]));
    }

    template<typename Store>
    template<unsigned SamplingFlags>
        auto VectorField2DSeparate<Store>::Sample(FloatCoord c) const -> ValueType
    {
        if (constant_expression<(SamplingFlags & RNFSample::Cubic)!=0>::result())
            return SampleMonotonicCubic(*this, c);
        return SampleBilinear<SamplingFlags>(*this, c);
    }

    template<typename Store>
        void VectorField2DSeparate<Store>::GatherNeighbors(ValueType neighbours[8], float weights[4], FloatCoord coord) const
    {
        return XLEMath::GatherNeighbors(neighbours, weights, *this, coord);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Field>
        unsigned InstantiateField()
        {
            using SampleFn = typename Field::ValueType(Field::*)(typename Field::FloatCoord)const;
            SampleFn t0 = &Field::Sample<0>; (void)t0;
            SampleFn t1 = &Field::Sample<RNFSample::Clamp>; (void)t1;
            SampleFn t2 = &Field::Sample<RNFSample::Cubic>; (void)t2;
            SampleFn t3 = &Field::Sample<RNFSample::Cubic|RNFSample::Clamp>; (void)t3;
            return 0;
        }

    static const unsigned s_i[] = 
    {
        InstantiateField<ScalarField2D<Eigen::VectorXf>>(),
        InstantiateField<VectorField2DSeparate<Eigen::VectorXf>>()
    };

    template class ScalarField2D<Eigen::VectorXf>;
    template class VectorField2DSeparate<Eigen::VectorXf>;

}

