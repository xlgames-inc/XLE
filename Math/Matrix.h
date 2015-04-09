// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Math.h"
#include "Vector.h"

#if MATHLIBRARY_ACTIVE == MATHLIBRARY_CML
    #pragma warning(push)
    #pragma warning(disable:4512)       // assignment operator could not be generated
    #include <cml/matrix.h>
    #pragma warning(pop)
#endif

namespace XLEMath
{
    #if MATHLIBRARY_ACTIVE == MATHLIBRARY_CML

        typedef cml::matrix< float, cml::fixed<3,3>, cml::col_basis>      Float3x3;
        typedef cml::matrix< float, cml::fixed<3,4>, cml::col_basis>      Float3x4;
        typedef cml::matrix< float, cml::fixed<4,4>, cml::col_basis>      Float4x4;

        Float4x4        Expand(const Float3x3& rotationScalePart, const Float3& translationPart);

    #endif

    inline Float4x4 LinearInterpolate(const Float4x4& lhs, const Float4x4& rhs, float alpha)
    {
        Float4x4 result;
        for (unsigned i=0; i<4; ++i)
            for (unsigned j=0; j<4; ++j)
                result(i,j) = LinearInterpolate(lhs(i,j), rhs(i,j), alpha);
        return result;
    }

    inline bool Equivalent(const Float4x4& a, const Float4x4& b, Float4x4::value_type tolerance)
    {
        for (unsigned i=0; i<4; ++i)
            for (unsigned j=0; j<4; ++j)
                if (!Equivalent(a(i, j), b(i, j), tolerance))
                    return false;
        return true;
    }

    Float3x3        Inverse(const Float3x3& input);
    Float4x4        Inverse(const Float4x4& input);
    Float3x3        Determinant(const Float3x3& input);
    Float4x4        Determinant(const Float4x4& input);
    Float3x3        Transpose(const Float3x3& input);
    Float4x4        Transpose(const Float4x4& input);
 
    inline Float3x4 Truncate(const Float4x4& input)
    {
        Float3x4 result;
        for (unsigned i=0; i<3; i++)
            for (unsigned j=0; j<4; j++)
                result(i,j) = input(i,j);
        return result;
    }

    inline const float* AsFloatArray(const Float4x4& m)    { return &m(0,0); }
    inline float* AsFloatArray(Float4x4& m)                { return &m(0,0); }
    Float4x4 AsFloat4x4(const float a[]);

    Float4x4 MakeFloat4x4(
        float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33);

}

