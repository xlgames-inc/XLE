// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

// warnings from CML
#pragma warning(disable:4267)       //  warning C4267: 'initializing' : conversion from 'size_t' to 'int', possible loss of data

#include "ProjectionMath.h"

namespace Math
{
    static Float4x4 InvertWorldToProjection(const Float4x4& input, bool useAccurateInverse)
    {
        if (useAccurateInverse) {
            return cml::detail::inverse_f<Float4x4, 0>()(input);
        } else {
            return Inverse(input);
        }
    }

    void CalculateAbsFrustumCorners(Float3 frustumCorners[8], const Float4x4& worldToProjection)
    {
            //  So long as we can invert the world to projection matrix accurately, we can 
            //  extract the frustum corners easily. We just need to pass the coordinates
            //  of the corners of clip space through the inverse matrix.
            //
            //  If the matrix inversion is not accurate enough, we can do this by going back to
            //  the source components that built the worldToProjection matrix.
            //  We can easily get the projection top/left/right/bottom from the raw projection matrix
            //  and we can also get the near and far clip from that. The world to view matrix can be
            //  inverted accurately with InvertOrthonormalTransform (and normally we should have the
            //  world to view matrix calculated at higher points in the pipeline). 
            //  So by using those source components, we can calculate the corners without and extra
            //  matrix inversion operations.
        static bool useAccurateInverse = true;      // the normal cml invert is pretty accurate. But sometimes it seems we get a better result with this
        auto projectionToWorld = InvertWorldToProjection(worldToProjection, useAccurateInverse);
        Float4 v0 = projectionToWorld * Float4(-1.f,  1.f, 0.f, 1.f);
        Float4 v1 = projectionToWorld * Float4(-1.f, -1.f, 0.f, 1.f);
        Float4 v2 = projectionToWorld * Float4( 1.f,  1.f, 0.f, 1.f);
        Float4 v3 = projectionToWorld * Float4( 1.f, -1.f, 0.f, 1.f);

        Float4 v4 = projectionToWorld * Float4(-1.f,  1.f, 1.f, 1.f);
        Float4 v5 = projectionToWorld * Float4(-1.f, -1.f, 1.f, 1.f);
        Float4 v6 = projectionToWorld * Float4( 1.f,  1.f, 1.f, 1.f);
        Float4 v7 = projectionToWorld * Float4( 1.f, -1.f, 1.f, 1.f);

        frustumCorners[0] = Truncate(v0) / v0[3];
        frustumCorners[1] = Truncate(v1) / v1[3];
        frustumCorners[2] = Truncate(v2) / v2[3];
        frustumCorners[3] = Truncate(v3) / v3[3];

        frustumCorners[4] = Truncate(v4) / v4[3];
        frustumCorners[5] = Truncate(v5) / v5[3];
        frustumCorners[6] = Truncate(v6) / v6[3];
        frustumCorners[7] = Truncate(v7) / v7[3];
    }


    AABBIntersection::Enum TestAABB(const Float4x4& localToProjection, const Float3& mins, const Float3& maxs)
    {
            //  for the box to be culled, all points must be outside of the same bounding box
            //  plane... We can do this in clip space (assuming we can do a fast position transform on
            //  the CPU). We can also do this in world space by finding the planes of the frustum, and
            //  comparing each corner point to each plane.
        Float3 corners[8] = 
        {
            Float3(mins[0], mins[1], mins[2]),
            Float3(maxs[0], mins[1], mins[2]),
            Float3(mins[0], maxs[1], mins[2]),
            Float3(maxs[0], maxs[1], mins[2]),

            Float3(mins[0], mins[1], maxs[2]),
            Float3(maxs[0], mins[1], maxs[2]),
            Float3(mins[0], maxs[1], maxs[2]),
            Float3(maxs[0], maxs[1], maxs[2])
        };
        
        Float3 projectedCorners[8];
        for (unsigned c=0; c<8; ++c) {
            Float4 p = localToProjection * Float4(corners[c], 1.f);
            float recipAbsW = 1.f/XlAbs(p[3]);
            projectedCorners[c] = Float3(p[0]*recipAbsW, p[1]*recipAbsW, p[2]*recipAbsW);
        }

        bool leftAnd = true, rightAnd = true, topAnd = true, bottomAnd = true, nearAnd = true, farAnd = true;
        bool leftOr = false, rightOr = false, topOr = false, bottomOr = false, nearOr = false, farOr = false;
        for (unsigned c=0; c<8; ++c) {
            leftAnd     &= (projectedCorners[c][0] < -1.f);
            rightAnd    &= (projectedCorners[c][0] >  1.f);
            topAnd      &= (projectedCorners[c][1] < -1.f);
            bottomAnd   &= (projectedCorners[c][1] >  1.f);
            nearAnd     &= (projectedCorners[c][2] <  0.f);
            farAnd      &= (projectedCorners[c][2] >  1.f);

            leftOr      |= (projectedCorners[c][0] < -1.f);
            rightOr     |= (projectedCorners[c][0] >  1.f);
            topOr       |= (projectedCorners[c][1] < -1.f);
            bottomOr    |= (projectedCorners[c][1] >  1.f);
            nearOr      |= (projectedCorners[c][2] <  0.f);
            farOr       |= (projectedCorners[c][2] >  1.f);
        }
        
        if (leftAnd | rightAnd | topAnd | bottomAnd | nearAnd | farAnd) {
            return AABBIntersection::Culled;
        }
        if (leftOr | rightOr | topOr | bottomOr | nearOr | farOr) {
            return AABBIntersection::Boundary;
        }
        return AABBIntersection::Within;
    }
}

