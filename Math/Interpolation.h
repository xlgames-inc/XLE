// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Vector.h"
#include "Matrix.h"

namespace XLEMath
{
    float       BezierInterpolate(float P0, float C0, float C1, float P1, float alpha);
    Float3      BezierInterpolate(Float3 P0, Float3 C0, Float3 C1, Float3 P1, float alpha);
    Float4      BezierInterpolate(const Float4& P0, const Float4& C0, const Float4& C1, const Float4& P1, float alpha);
    Float4x4    BezierInterpolate(const Float4x4& P0, const Float4x4& C0, const Float4x4& C1, const Float4x4& P1, float alpha);

    float       SphericalInterpolate(float A, float B, float alpha);
    Float3      SphericalInterpolate(Float3 A, Float3 B, float alpha);
    Float4      SphericalInterpolate(const Float4& A, const Float4& B, float alpha);
    Float4x4    SphericalInterpolate(const Float4x4& A, const Float4x4& B, float alpha);

    float       SphericalBezierInterpolate(float P0, float C0, float C1, float P1, float alpha);
    Float3      SphericalBezierInterpolate(Float3 P0, Float3 C0, Float3 C1, Float3 P1, float alpha);
    Float4      SphericalBezierInterpolate(const Float4& P0, const Float4& C0, const Float4& C1, const Float4& P1, float alpha);
    Float4x4    SphericalBezierInterpolate(const Float4x4& P0, const Float4x4& C0, const Float4x4& C1, const Float4x4& P1, float alpha);
}

