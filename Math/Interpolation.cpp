// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "Interpolation.h"
#include "Transformations.h"

namespace XLEMath
{
    float BezierInterpolate(float P0, float C0, float C1, float P1, float alpha)
    {
        float alpha2 = alpha*alpha;
        float alpha3 = alpha2*alpha;
        float complement = 1.0f-alpha;
        float complement2 = complement*complement;
        float complement3 = complement2*complement;

            //  This the standard Bezier equation (as seen in textbooks everywhere)
        return
                P0 * complement3
            +   3.f * C0 * alpha * complement2
            +   3.f * C1 * alpha2 * complement
            +   P1 * alpha3
            ;
    }

    Float3 BezierInterpolate(Float3 P0, Float3 C0, Float3 C1, Float3 P1, float alpha)
    {
        return Float3(
            BezierInterpolate(P0[0], C0[0], C1[0], P1[0], alpha),
            BezierInterpolate(P0[1], C0[1], C1[1], P1[1], alpha),
            BezierInterpolate(P0[2], C0[2], C1[2], P1[2], alpha) );
    }

    Float4 BezierInterpolate(const Float4& P0, const Float4& C0, const Float4& C1, const Float4& P1, float alpha)
    {
        return Float4(
            BezierInterpolate(P0[0], C0[0], C1[0], P1[0], alpha),
            BezierInterpolate(P0[1], C0[1], C1[1], P1[1], alpha),
            BezierInterpolate(P0[2], C0[2], C1[2], P1[2], alpha),
            BezierInterpolate(P0[3], C0[3], C1[3], P1[3], alpha));
    }

    Float4x4 BezierInterpolate(const Float4x4& P0, const Float4x4& C0, const Float4x4& C1, const Float4x4& P1, float alpha)
    {
        Float4x4 result;
        for (unsigned j=0; j<4; ++j)
            for (unsigned i=0; i<4; ++i)
                result(i,j) = BezierInterpolate(P0(i,j), C0(i,j), C1(i,j), P1(i,j), alpha);
        return result;
    }




    float SphericalInterpolate(float A, float B, float alpha)
    {
        return LinearInterpolate(A, B, alpha);
    }

    Float3 SphericalInterpolate(Float3 A, Float3 B, float alpha)
    {
        return LinearInterpolate(A, B, alpha);
    }

    Float4 SphericalInterpolate(const Float4& A, const Float4& B, float alpha)
    {
            //  Note -- the type of interpolation here depends on the meaning of the values
            //          Is it a rotation axis/angle? Or something else
        return LinearInterpolate(A, B, alpha);
    }
        
    Float4x4 SphericalInterpolate(const Float4x4& A, const Float4x4& B, float alpha)
    {
            //  We're assuming that this input matrix is an affine geometry transform. So we can convert it
            //  into a format that can be slerped!
        auto result = SphericalInterpolate(
            ScaleRotationTranslationQ(A), 
            ScaleRotationTranslationQ(B), alpha);
        return AsFloat4x4(result);
    }
        


        
    float SphericalBezierInterpolate(float P0, float C0, float C1, float P1, float alpha)
    {
            // (just do the non-spherical interpolate, for lazyness)
        return BezierInterpolate(P0, C0, C1, P1, alpha);
    }

    Float3 SphericalBezierInterpolate(Float3 P0, Float3 C0, Float3 C1, Float3 P1, float alpha)
    {
            // (just do the non-spherical interpolate, for lazyness)
        return BezierInterpolate(P0, C0, C1, P1, alpha);
    }

    Float4 SphericalBezierInterpolate(const Float4& P0, const Float4& C0, const Float4& C1, const Float4& P1, float alpha)
    {
            // (just do the non-spherical interpolate, for lazyness)
        return BezierInterpolate(P0, C0, C1, P1, alpha);
    }
        
    Float4x4 SphericalBezierInterpolate(const Float4x4& P0, const Float4x4& C0, const Float4x4& C1, const Float4x4& P1, float alpha)
    {
            // (just do the non-spherical interpolate, for lazyness)
        return BezierInterpolate(P0, C0, C1, P1, alpha);
    }


}