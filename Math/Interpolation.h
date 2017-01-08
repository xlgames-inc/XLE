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

	// Perform Catmull Rom interpolation along a spline (using spherical methods for rotation types)
	// P0n1 is the point before P0, and P1p1 is the point after P1
	// P0n1t and P1p1t are the "t" values for these points, normalized so that P0 is at t==0 and P1 is at t==1
	// Note; see also "cordal" and "centripedal" versions of Catmull Rom splines; which use the distances between
	// control points to adjusting the spacing of knots.

	float       SphericalCatmullRomInterpolate(float P0n1, float P0, float P1, float P1p1, float P0n1t, float P1p1t, float alpha);
    Float3      SphericalCatmullRomInterpolate(Float3 P0n1, Float3 P0, Float3 P1, Float3 P1p1, float P0n1t, float P1p1t, float alpha);
    Float4      SphericalCatmullRomInterpolate(const Float4& P0n1, const Float4& P0, const Float4& P1, const Float4& P1p1, float P0n1t, float P1p1t, float alpha);
    Float4x4    SphericalCatmullRomInterpolate(const Float4x4& P0n1, const Float4x4& P0, const Float4x4& P1, const Float4x4& P1p1, float P0n1t, float P1p1t, float alpha);
}

