// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

// warnings from CML
#pragma warning(disable:4267)       //  warning C4267: 'initializing' : conversion from 'size_t' to 'int', possible loss of data

#include "ProjectionMath.h"
#include "../Core/Prefix.h"
#include <assert.h>
#include <intrin.h>

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

    static const float* AsFloatArray(const Float4x4& m) { return &m(0,0); }

    static void TestAABB_SSE_Pt(
        __m128 pt, 
        __m128& A0, __m128& A1, __m128& A2, __m128& A3,
        __m128& andUpper, __m128& andLower,
        __m128& orUpper, __m128& orLower,
        __m128& zeroZWComponents)
    {
        auto x = _mm_dp_ps(A0, pt, (0xF<<4)|(1<<0));  // L: ~12, T: 0 (varies for different processors)
        auto y = _mm_dp_ps(A1, pt, (0xF<<4)|(1<<1));  // L: ~12, T: 0 (varies for different processors)
        auto z = _mm_dp_ps(A2, pt, (0xF<<4)|(1<<2));  // L: ~12, T: 0 (varies for different processors)
        auto w = _mm_dp_ps(A3, pt, (0xF<<4)|( 0xF));  // L: ~12, T: 0 (varies for different processors)

        auto clipSpaceXYZ = _mm_add_ps(x, y);           // L: 3, T: 1
        clipSpaceXYZ = _mm_add_ps(z, clipSpaceXYZ);     // L: 3, T: 1

        const auto sign_mask = _mm_set1_ps(-0.f); // -0.f = 1 << 31

        //      SSE absolute using bit mask --
        // w = _mm_andnot_ps(sign_mask, w);

        // clipSpaceXYZ = _mm_mul_ps(clipSpaceXYZ, w);     // L: 5, T: ~1
        // const auto upperCompare = _mm_set1_ps(1.f);
        // const auto lowerCompare = _mm_set_ps(-1.f, -1.f, 0.f, 0.f);   // (In DirectX clip space, z=0.f is the near plane)

        auto negW = _mm_xor_ps(w, sign_mask);           // L: 1, T: ~0.33 (this will flip the sign of w)

            // In DirectX clip space, z=0.f is the near plane
            // we need to set negW.z to 0. It's easiest using a bitwise and...
        negW = _mm_and_ps(negW, zeroZWComponents); // L: 1, T: 1

        auto cmp0 = _mm_cmpgt_ps(clipSpaceXYZ, w);      // L: 3, T: -
        auto cmp1 = _mm_cmplt_ps(clipSpaceXYZ, negW);   // L: 3, T: -

        andUpper = _mm_and_ps(andUpper, cmp0);          // L: 1, T: ~1
        andLower = _mm_and_ps(andLower, cmp1);          // L: 1, T: ~1

        orUpper = _mm_or_ps(orUpper, cmp0);             // L: 1, T: .33
        orLower = _mm_or_ps(orLower, cmp1);             // L: 1, T: .33
    }

    AABBIntersection::Enum TestAABB_SSE(const Float4x4& localToProjection, const Float3& mins, const Float3& maxs)
    {
        // We can use SSE "shuffle" to load the vectors for each corner.
        //
        //  abc = mins[0,1,2]
        //  uvw = maxs[0,1,2]
        //
        //  r0 = abuv
        //  r1 = cw,1,1
        //
        //  abc, abw
        //  ubc, ubw
        //  avc, avw
        //  uvc, uvw
        //
        //  abc1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 1, 0));
        //  ubc1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 1, 2));
        //  avc1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 3, 0));
        //  uvc1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 3, 2));
        //
        //  abw1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 1, 0));
        //  ubw1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 1, 2));
        //  avw1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 3, 0));
        //  uvw1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 3, 2));
        //
        // Now we need to do the matrix multiply & perspective divide
        // then we have to compare the results against 0 and 1 and 
        // do some binary comparisons.

        auto abuv = _mm_set_ps(maxs[1], maxs[0], mins[1], mins[0]);   // (note; using WZYX order)
        auto cw11 = _mm_set_ps(1.f, 1.f, maxs[2], mins[2]);

        auto A0 = _mm_loadu_ps(AsFloatArray(localToProjection) +  0);
        auto A1 = _mm_loadu_ps(AsFloatArray(localToProjection) +  4);
        auto A2 = _mm_loadu_ps(AsFloatArray(localToProjection) +  8);
        auto A3 = _mm_loadu_ps(AsFloatArray(localToProjection) + 12);

        __declspec(align(16)) unsigned andInitializer[] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
        assert((size_t(andInitializer) & 0xf) == 0);

        __declspec(align(16)) unsigned zeroZWComponentsInit[] = { 0xffffffff, 0xffffffff, 0, 0 };
        assert((size_t(zeroZWComponentsInit) & 0xf) == 0);

        auto zeroZWComponents = _mm_load_ps((const float*)zeroZWComponentsInit);
        auto andUpper = _mm_load_ps((const float*)andInitializer);
        auto andLower = _mm_load_ps((const float*)andInitializer);
        auto orUpper = _mm_setzero_ps();
        auto orLower = _mm_setzero_ps();

            // Perform projection into culling space...

            // We can perform the matrix * vector multiply in two ways:
            //      1. using "SSE4.1" dot product instruction "_mm_dp_ps"
            //      2. using "SSE" and "FMA" vector multiply and fused vector add
            //
            // The dot production instruction has low throughput by very
            // high latency. That means we need to interleave a number of 
            // transforms in order to get the best performance. Actually, compiler
            // generated optimization should be better for doing that. But 
            // I'm currently using a compiler that doesn't seem to generate that
            // instruction (so, doing it by hand).

        ////////////////////////////////////////
        TestAABB_SSE_Pt(
            _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 1, 0)),
            A0, A1, A2, A3,
            andUpper, andLower, orUpper, orLower, zeroZWComponents);

        TestAABB_SSE_Pt(
            _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 1, 2)),
            A0, A1, A2, A3,
            andUpper, andLower, orUpper, orLower, zeroZWComponents);

        TestAABB_SSE_Pt(
            _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 3, 0)),
            A0, A1, A2, A3,
            andUpper, andLower, orUpper, orLower, zeroZWComponents);

        TestAABB_SSE_Pt(
            _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 3, 2)),
            A0, A1, A2, A3,
            andUpper, andLower, orUpper, orLower, zeroZWComponents);

        TestAABB_SSE_Pt(
            _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 1, 0)),
            A0, A1, A2, A3,
            andUpper, andLower, orUpper, orLower, zeroZWComponents);

        TestAABB_SSE_Pt(
            _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 1, 2)),
            A0, A1, A2, A3,
            andUpper, andLower, orUpper, orLower, zeroZWComponents);

        TestAABB_SSE_Pt(
            _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 3, 0)),
            A0, A1, A2, A3,
            andUpper, andLower, orUpper, orLower, zeroZWComponents);

        TestAABB_SSE_Pt(
            _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 3, 2)),
            A0, A1, A2, A3,
            andUpper, andLower, orUpper, orLower, zeroZWComponents);
        ////////////////////////////////////////

        __declspec(align(16)) unsigned andUpperResult[4];
        __declspec(align(16)) unsigned andLowerResult[4];
        __declspec(align(16)) unsigned  orUpperResult[4];
        __declspec(align(16)) unsigned  orLowerResult[4];
        assert((size_t(andInitializer) & 0xf) == 0);
        assert((size_t(andLowerResult) & 0xf) == 0);
        assert((size_t( orUpperResult) & 0xf) == 0);
        assert((size_t( orLowerResult) & 0xf) == 0);

        _mm_store_ps((float*)andUpperResult, andUpper);
        _mm_store_ps((float*)andLowerResult, andLower);
        _mm_store_ps((float*)orUpperResult, orUpper);
        _mm_store_ps((float*)orLowerResult, orLower);

        if (andUpperResult[0] | andUpperResult[1] | andUpperResult[2] | andLowerResult[0] | andLowerResult[1] | andLowerResult[2]) {
            return AABBIntersection::Culled;
        }
        if (orUpperResult[0] | orUpperResult[1] | orUpperResult[2] | orLowerResult[0] | orLowerResult[1] | orLowerResult[2]) {
            return AABBIntersection::Boundary;
        }
        return AABBIntersection::Within;
    }

    static inline Float4 XYZProj(const Float4x4& localToProjection, const Float3 input)
    {
        Float4 p = localToProjection * Expand(input, 1.f);
        // return Truncate(p) * 1.f / /*XlAbs*/ (p[3]);
        return p;
    }

    AABBIntersection::Enum TestAABB(const Float4x4& localToProjection, const Float3& mins, const Float3& maxs)
    {
        const auto compareResult = TestAABB_SSE(localToProjection, mins, maxs);
        (void)compareResult;

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

        Float4 projectedCorners[8];
        projectedCorners[0] = XYZProj(localToProjection, corners[0]);
        projectedCorners[1] = XYZProj(localToProjection, corners[1]);
        projectedCorners[2] = XYZProj(localToProjection, corners[2]);
        projectedCorners[3] = XYZProj(localToProjection, corners[3]);
        projectedCorners[4] = XYZProj(localToProjection, corners[4]);
        projectedCorners[5] = XYZProj(localToProjection, corners[5]);
        projectedCorners[6] = XYZProj(localToProjection, corners[6]);
        projectedCorners[7] = XYZProj(localToProjection, corners[7]);

        bool leftAnd = true, rightAnd = true, topAnd = true, bottomAnd = true, nearAnd = true, farAnd = true;
        bool leftOr = false, rightOr = false, topOr = false, bottomOr = false, nearOr = false, farOr = false;
        for (unsigned c=0; c<8; ++c) {
            leftAnd     &= (projectedCorners[c][0] < -projectedCorners[c][3]);
            rightAnd    &= (projectedCorners[c][0] >  projectedCorners[c][3]);
            topAnd      &= (projectedCorners[c][1] < -projectedCorners[c][3]);
            bottomAnd   &= (projectedCorners[c][1] >  projectedCorners[c][3]);
            nearAnd     &= (projectedCorners[c][2] <  0.f);
            farAnd      &= (projectedCorners[c][2] >  projectedCorners[c][3]);

            leftOr      |= (projectedCorners[c][0] < -projectedCorners[c][3]);
            rightOr     |= (projectedCorners[c][0] >  projectedCorners[c][3]);
            topOr       |= (projectedCorners[c][1] < -projectedCorners[c][3]);
            bottomOr    |= (projectedCorners[c][1] >  projectedCorners[c][3]);
            nearOr      |= (projectedCorners[c][2] <  0.f);
            farOr       |= (projectedCorners[c][2] >  projectedCorners[c][3]);
        }
        
        if (leftAnd | rightAnd | topAnd | bottomAnd | nearAnd | farAnd) {
            assert(compareResult == AABBIntersection::Culled);
            return AABBIntersection::Culled;
        }
        if (leftOr | rightOr | topOr | bottomOr | nearOr | farOr) {
            assert(compareResult == AABBIntersection::Boundary);
            return AABBIntersection::Boundary;
        }
        assert(compareResult == AABBIntersection::Within);
        return AABBIntersection::Within;
    }
}

