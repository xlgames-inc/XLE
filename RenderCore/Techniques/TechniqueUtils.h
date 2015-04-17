// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderUtils.h"
#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"

namespace RenderCore { namespace Techniques
{

    class CameraDesc
    {
    public:
        Float4x4    _cameraToWorld;
        float       _verticalFieldOfView;
        float       _nearClip, _farClip;

        Float4x4    _temporaryMatrix;       // (temporary placeholder for a hack)

        CameraDesc();
    };

    __declspec(align(16)) class ProjectionDesc
    {
    public:
        Float4x4        _worldToProjection;
        Float4x4        _cameraToProjection;
        Float4x4        _cameraToWorld;
        float           _verticalFov;
        float           _aspectRatio;
        float           _nearClip;
        float           _farClip;

        ProjectionDesc();
    };

    class GlobalTransformConstants
    {
    public:
        Float4x4    _worldToClip;
        Float4      _frustumCorners[4];
        Float3      _worldSpaceView;
        float       _farClip;
        Float4      _minimalProjection;
        Float4x4    _viewToWorld;
    };

    struct LocalTransformConstants
    {
    public:
        Float3x4    _localToWorld;
        Float3      _localSpaceView;
        float       _dummy;
    };

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

    Float4x4 PerspectiveProjection(
        const CameraDesc& sceneCamera, float viewportAspect);

    Float4x4 OrthogonalProjection(
        float l, float t, float r, float b,
        float nearClipPlane, float farClipPlane,
        GeometricCoordinateSpace::Enum coordinateSpace,
        ClipSpaceType::Enum clipSpaceType);

    ClipSpaceType::Enum GetDefaultClipSpaceType();

    std::pair<float, float> CalculateNearAndFarPlane(
        const Float4& minimalProjection, ClipSpaceType::Enum clipSpaceType);
    Float2 CalculateDepthProjRatio_Ortho(
        const Float4& minimalProjection, ClipSpaceType::Enum clipSpaceType);

    std::pair<Float3, Float3> BuildRayUnderCursor(
        Int2 mousePosition, CameraDesc& sceneCamera, 
        const std::pair<Float2, Float2>& viewport);

    std::pair<Float3, Float3> BuildRayUnderCursor(
        Int2 mousePosition, 
        Float3 absFrustumCorners[], 
        const Float3& cameraPosition,
        float nearClip, float farClip,
        const std::pair<Float2, Float2>& viewport);

    GlobalTransformConstants BuildGlobalTransformConstants(const ProjectionDesc& projDesc);

    SharedPkt MakeLocalTransformPacket(const Float4x4& localToWorld, const CameraDesc& camera);
    SharedPkt MakeLocalTransformPacket(const Float4x4& localToWorld, const Float3& worldSpaceCameraPosition);
    LocalTransformConstants MakeLocalTransform(const Float4x4& localToWorld, const Float3& worldSpaceCameraPosition);

}}

