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
    void CalculateAbsFrustumCorners(
        Float3 frustumCorners[8],
        const Float4x4& worldToProjection);

    namespace AABBIntersection {
        enum Enum { Culled, Within, Boundary };
    }

    AABBIntersection::Enum TestAABB(
        const Float4x4& localToProjection, 
        const Float3& mins, const Float3& maxs);

    AABBIntersection::Enum TestAABB_Aligned(
        const float localToProjection[], 
        const Float3& mins, const Float3& maxs);

    inline bool CullAABB(
        const Float4x4& localToProjection, 
        const Float3& mins, const Float3& maxs)
    {
        return TestAABB(localToProjection, mins, maxs) 
            == AABBIntersection::Culled;
    }

    inline bool CullAABB_Aligned(
        const float localToProjection[],
        const Float3& mins, const Float3& maxs)
    {
        return TestAABB_Aligned(localToProjection, mins, maxs) 
            == AABBIntersection::Culled;
    }

    Float4 ExtractMinimalProjection(const Float4x4& projectionMatrix);
    bool IsOrthogonalProjection(const Float4x4& projectionMatrix);

///////////////////////////////////////////////////////////////////////////////////////////////////
        //   B U I L D I N G   P R O J E C T I O N   M A T R I C E S
///////////////////////////////////////////////////////////////////////////////////////////////////

    namespace GeometricCoordinateSpace      { enum Enum { LeftHanded,       RightHanded };  }
    namespace ClipSpaceType                 { enum Enum { StraddlingZero,   Positive };     }
    Float4x4 PerspectiveProjection(
        float verticalFOV, float aspectRatio,
        float nearClipPlane, float farClipPlane,
        GeometricCoordinateSpace::Enum coordinateSpace,
        ClipSpaceType::Enum clipSpaceType);

    Float4x4 PerspectiveProjection(
        float l, float t, float r, float b,
        float nearClipPlane, float farClipPlane,
        ClipSpaceType::Enum clipSpaceType);

    Float4x4 OrthogonalProjection(
        float l, float t, float r, float b,
        float nearClipPlane, float farClipPlane,
        GeometricCoordinateSpace::Enum coordinateSpace,
        ClipSpaceType::Enum clipSpaceType);

    Float4x4 OrthogonalProjection(
        float l, float t, float r, float b,
        float nearClipPlane, float farClipPlane,
        ClipSpaceType::Enum clipSpaceType);

///////////////////////////////////////////////////////////////////////////////////////////////////

    std::pair<float, float> CalculateNearAndFarPlane(
        const Float4& minimalProjection, ClipSpaceType::Enum clipSpaceType);
    Float2 CalculateDepthProjRatio_Ortho(
        const Float4& minimalProjection, ClipSpaceType::Enum clipSpaceType);

    std::pair<Float3, Float3> BuildRayUnderCursor(
        Int2 mousePosition, 
        Float3 absFrustumCorners[], 
        const Float3& cameraPosition,
        float nearClip, float farClip,
        const std::pair<Float2, Float2>& viewport);

///////////////////////////////////////////////////////////////////////////////////////////////////

}

