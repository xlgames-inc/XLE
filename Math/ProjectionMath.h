// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "Vector.h"
#include "Matrix.h"

namespace Math
{
    void CalculateAbsFrustumCorners(
        Float3 frustumCorners[8],
        const Float4x4& worldToProjection);

    namespace AABBIntersection {
        enum Enum { Culled, Within, Boundary };
    }

    AABBIntersection::Enum TestAABB_Basic(
        const Float4x4& localToProjection, 
        const Float3& mins, const Float3& maxs);

    AABBIntersection::Enum TestAABB_SSE2(
        __declspec(align(16)) const float localToProjection[], 
        const Float3& mins, const Float3& maxs);

    AABBIntersection::Enum TestAABB_SSE3(
        __declspec(align(16)) const float localToProjection[], 
        const Float3& mins, const Float3& maxs);

    AABBIntersection::Enum TestAABB_SSE4(
        __declspec(align(16)) const float localToProjection[], 
        const Float3& mins, const Float3& maxs);

    AABBIntersection::Enum TestAABB(
        const Float4x4& localToProjection, 
        const Float3& mins, const Float3& maxs);

    inline bool CullAABB(
        const Float4x4& localToProjection, 
        const Float3& mins, const Float3& maxs)
    {
        return TestAABB(localToProjection, mins, maxs) 
            == AABBIntersection::Culled;
    }
}

