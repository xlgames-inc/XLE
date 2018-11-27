// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../RenderUtils.h"
#include "../../Math/Vector.h"
#include "../../Math/Matrix.h"
#include "../../Math/ProjectionMath.h"

namespace RenderCore { namespace Techniques
{

    class CameraDesc
    {
    public:
        Float4x4    _cameraToWorld;
        float       _nearClip, _farClip;

        enum class Projection { Perspective, Orthogonal };
        Projection  _projection;

        // perspective settings
        float       _verticalFieldOfView;

        // orthogonal settings
        float       _left, _top;
        float       _right, _bottom;

        CameraDesc();
    };

    class
#if defined(__clang_major__) && __clang_major__ < 4
    // Do not align; it appears that clang 3.9 optimizes the alignment request incorrectly,
    // leading to a segfault when constructing the ProjectionDesc object.
#else
    // Culling tests use an optimized implementation that takes advantage of
    // the fact that this structure is aligned.
    alignas(16)
#endif
    ProjectionDesc
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
        unsigned    _dummy0;
        uint64      _materialGuid;
        unsigned    _dummy1[2];
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    Float4x4 Projection(const CameraDesc& sceneCamera, float viewportAspect);

    ClipSpaceType GetDefaultClipSpaceType();

    std::pair<Float3, Float3> BuildRayUnderCursor(
        Int2 mousePosition, CameraDesc& sceneCamera, 
        const std::pair<Float2, Float2>& viewport);
    
    GlobalTransformConstants BuildGlobalTransformConstants(const ProjectionDesc& projDesc);

    SharedPkt MakeLocalTransformPacket(const Float4x4& localToWorld, const CameraDesc& camera);
    SharedPkt MakeLocalTransformPacket(const Float4x4& localToWorld, const Float3& worldSpaceCameraPosition);
    LocalTransformConstants MakeLocalTransform(const Float4x4& localToWorld, const Float3& worldSpaceCameraPosition);

    bool HasHandinessFlip(const ProjectionDesc& projDesc);

}}

