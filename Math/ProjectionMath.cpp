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

    static inline void TestAABB_SSE_TransCorner(
        __m128 corner0, __m128 corner1, 
        __m128& A0, __m128& A1, __m128& A2,
        float* dst)
    {
        // still need many registers --
        // A0, A1, A2
        // x0, y0, z2
        // x1, y1, z1
        // abuv, cz11
        auto x0 = _mm_dp_ps(A0, corner0, (0xF<<4)|(1<<0));      // L: ~12, T: 0 (varies for different processors)
        auto y0 = _mm_dp_ps(A1, corner0, (0xF<<4)|(1<<1));      // L: ~12, T: 0 (varies for different processors)
        auto z0 = _mm_dp_ps(A2, corner0, (0xF<<4)|(1<<2));      // L: ~12, T: 0 (varies for different processors)

        auto x1 = _mm_dp_ps(A0, corner1, (0xF<<4)|(1<<0));      // L: ~12, T: 0 (varies for different processors)
        auto y1 = _mm_dp_ps(A1, corner1, (0xF<<4)|(1<<1));      // L: ~12, T: 0 (varies for different processors)
        auto z1 = _mm_dp_ps(A2, corner1, (0xF<<4)|(1<<2));      // L: ~12, T: 0 (varies for different processors)

        auto clipSpaceXYZ0 = _mm_add_ps(x0, y0);                // L: 3, T: 1
        auto clipSpaceXYZ1 = _mm_add_ps(x1, y1);                // L: 3, T: 1
        clipSpaceXYZ0 = _mm_add_ps(z0, clipSpaceXYZ0);          // L: 3, T: 1
        clipSpaceXYZ1 = _mm_add_ps(z1, clipSpaceXYZ1);          // L: 3, T: 1
        _mm_store_ps(dst, clipSpaceXYZ0);
        _mm_store_ps(dst + 4, clipSpaceXYZ1);
    }

    __declspec(align(16)) static const unsigned g_zeroZWComponentsInit[] = { 0xffffffff, 0xffffffff, 0, 0 };
    static const auto g_zeroZWComponents = _mm_load_ps((const float*)g_zeroZWComponentsInit);
    static const auto g_signMask = _mm_set1_ps(-0.f);       // -0.f = 1 << 31
        
    static inline void TestAABB_SSE_CalcFlags(
        const float* clipSpaceXYZMem, const float* clipSpaceWMem,
        __m128& andUpper, __m128& andLower,
        __m128& orUpperLower)
    {
        assert((size_t(clipSpaceXYZMem)&0xF) == 0);
        assert((size_t(clipSpaceWMem)&0xF) == 0);

        auto xyz = _mm_load_ps(clipSpaceXYZMem);
        auto w = _mm_load_ps(clipSpaceWMem);

        auto cmp0 = _mm_cmpgt_ps(xyz, w);               // L: 3, T: -

        auto negW = _mm_xor_ps(w, g_signMask);          // L: 1, T: ~0.33 (this will flip the sign of w)
        negW = _mm_and_ps(negW, g_zeroZWComponents);    // L: 1, T: 1

        auto cmp1 = _mm_cmplt_ps(xyz, negW);            // L: 3, T: -

            // apply bitwise "and" and "or" as required...
        andUpper = _mm_and_ps(andUpper, cmp0);          // L: 1, T: ~1
        andLower = _mm_and_ps(andLower, cmp1);          // L: 1, T: ~1

        orUpperLower = _mm_or_ps(orUpperLower, cmp0);   // L: 1, T: .33
        orUpperLower = _mm_or_ps(orUpperLower, cmp1);   // L: 1, T: .33
    }

    static AABBIntersection::Enum TestAABB_SSE(
        const float localToProjection[], 
        const Float3& mins, const Float3& maxs)
    {
        // Perform projection into culling space...

        // We can perform the matrix * vector multiply in three ways:
        //      1. using "SSE4.1" dot product instruction "_mm_dp_ps"
        //      2. using SSE3 vector multiply and horizontal add instructions
        //      2. using "FMA" vector multiply and fused vector add
        //
        // FMA is not supported on Intel chips earlier than Haswell. That's a
        // bit frustrating.
        //
        // The dot production instruction has low throughput but very
        // high latency. That means we need to interleave a number of 
        // transforms in order to get the best performance. Actually, compiler
        // generated optimization should be better for doing that. But 
        // I'm currently using a compiler that doesn't seem to generate that
        // instruction (so, doing it by hand).
        //
        // We can separate the test for each point into 2 parts;
        //      1. the matrix * vector multiply
        //      2. comparing the result against the edges of the frustum
        //
        // The 1st part has a high latency. But the latency values for the
        // second part are much smaller. The second part is much more compact
        // and easier to optimise. It makes sense to do 2 points in parallel,
        // to cover the latency of the 1st part with the calculations from the
        // 2nd part.
        //
        // However, we have a bit of problem with register counts! We need a lot
        // of registers. Visual Studio 2010 is only using 8 xmm registers, which is
        // not really enough. We need 16 registers to do this well. But it seems that
        // we can only have 16 register in x64 mode.
        //
        //       0-3  : matrix
        //       4xyz : clipSpaceXYZ
        //       5xyz : clipSpaceWWW (then -clipSpaceWWW)
        //       6    : utility
        //       7xyz : andUpper
        //       8xyz : andLower
        //       9xyz : orUpperAndLower
        //      10    : abuv
        //      11    : cw11
        //
        // If we want to do multiple corners at the same time, it's just going to
        // increase the registers we need.
        //
        //  What this means is the idea situation depends on the hardware we're
        //  targeting!
        //      1. x86 Haswell+ --> fused-multiply-add
        //      2. x64 --> dot product with 16 xmm registers
        //      3. otherwise, 2-step, transform corners and then clip test
        //
        // One solution is to do the transformation first, and write the result to
        // memory. But this makes it more difficult to cover the latency in the
        // dot product.

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

        assert((size_t(localToProjection) & 0xf) == 0);
        auto abuv = _mm_set_ps(maxs[1], maxs[0], mins[1], mins[0]);   // (note; using WZYX order)
        auto cw11 = _mm_set_ps(1.f, 1.f, maxs[2], mins[2]);

        __m128 corners[8];
        corners[0] = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 1, 0));
        corners[1] = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 1, 2));
        corners[2] = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 3, 0));
        corners[3] = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 3, 2));
        corners[4] = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 1, 0));
        corners[5] = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 1, 2));
        corners[6] = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 3, 0));
        corners[7] = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 3, 2));

        auto A0 = _mm_load_ps(localToProjection +  0);
        auto A1 = _mm_load_ps(localToProjection +  4);
        auto A2 = _mm_load_ps(localToProjection +  8);
        auto A3 = _mm_load_ps(localToProjection + 12);

        __declspec(align(16)) Float4 cornerClipSpaceXYZ[8];
        __declspec(align(16)) Float4 cornerClipW[8];

            //  We want to interleave projection calculations for multiple vectors in 8 registers
            //  We have very few registers. So we need to separate the calculation of the "W" part
            //  This will mean having to duplicate the "shuffle" instruction. But this is a very
            //  cheap instruction.

        TestAABB_SSE_TransCorner(
            corners[0], corners[1],
            A0, A1, A2, &cornerClipSpaceXYZ[0][0]);
        TestAABB_SSE_TransCorner(
            corners[2], corners[3],
            A0, A1, A2, &cornerClipSpaceXYZ[2][0]);
        TestAABB_SSE_TransCorner(
            corners[3], corners[4],
            A0, A1, A2, &cornerClipSpaceXYZ[4][0]);
        TestAABB_SSE_TransCorner(
            corners[5], corners[6],
            A0, A1, A2, &cornerClipSpaceXYZ[6][0]);

            //  Now do the "W" parts.. Do 4 at a time to try to cover latency
        {
            auto abc1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 1, 0));
            auto w0 = _mm_dp_ps(A3, abc1, (0xF<<4)|( 0xF));
            auto ubc1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 1, 2));
            auto w1 = _mm_dp_ps(A3, ubc1, (0xF<<4)|( 0xF));
            auto avc1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 3, 0));
            auto w2 = _mm_dp_ps(A3, avc1, (0xF<<4)|( 0xF));
            auto uvc1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 0, 3, 2));
            auto w3 = _mm_dp_ps(A3, uvc1, (0xF<<4)|( 0xF));

            _mm_store_ps(&cornerClipW[0][0], w0);
            _mm_store_ps(&cornerClipW[1][0], w1);
            _mm_store_ps(&cornerClipW[2][0], w2);
            _mm_store_ps(&cornerClipW[3][0], w3);

            auto abw1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 1, 0));
            auto w4 = _mm_dp_ps(A3, abw1, (0xF<<4)|( 0xF));
            auto ubw1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 1, 2));
            auto w5 = _mm_dp_ps(A3, ubw1, (0xF<<4)|( 0xF));
            auto avw1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 3, 0));
            auto w6 = _mm_dp_ps(A3, avw1, (0xF<<4)|( 0xF));
            auto uvw1 = _mm_shuffle_ps(abuv, cw11, _MM_SHUFFLE(2, 1, 3, 2));
            auto w7 = _mm_dp_ps(A3, uvw1, (0xF<<4)|( 0xF));

            _mm_store_ps(&cornerClipW[4][0], w4);
            _mm_store_ps(&cornerClipW[5][0], w5);
            _mm_store_ps(&cornerClipW[6][0], w6);
            _mm_store_ps(&cornerClipW[7][0], w7);
        }

            // Now compare with screen edges and calculate the bit masks

        __declspec(align(16)) unsigned andInitializer[] = { 0xffffffff, 0xffffffff, 0xffffffff, 0 };
        assert((size_t(andInitializer) & 0xf) == 0);

        auto andUpper = _mm_load_ps((const float*)andInitializer);
        auto andLower = _mm_load_ps((const float*)andInitializer);
        auto orUpperLower = _mm_setzero_ps();

        TestAABB_SSE_CalcFlags(&cornerClipSpaceXYZ[0][0], &cornerClipW[0][0], andUpper, andLower, orUpperLower);
        TestAABB_SSE_CalcFlags(&cornerClipSpaceXYZ[1][0], &cornerClipW[1][0], andUpper, andLower, orUpperLower);
        TestAABB_SSE_CalcFlags(&cornerClipSpaceXYZ[2][0], &cornerClipW[2][0], andUpper, andLower, orUpperLower);
        TestAABB_SSE_CalcFlags(&cornerClipSpaceXYZ[3][0], &cornerClipW[3][0], andUpper, andLower, orUpperLower);
        TestAABB_SSE_CalcFlags(&cornerClipSpaceXYZ[4][0], &cornerClipW[4][0], andUpper, andLower, orUpperLower);
        TestAABB_SSE_CalcFlags(&cornerClipSpaceXYZ[5][0], &cornerClipW[5][0], andUpper, andLower, orUpperLower);
        TestAABB_SSE_CalcFlags(&cornerClipSpaceXYZ[6][0], &cornerClipW[6][0], andUpper, andLower, orUpperLower);
        TestAABB_SSE_CalcFlags(&cornerClipSpaceXYZ[7][0], &cornerClipW[7][0], andUpper, andLower, orUpperLower);

            // Get the final result...

        andUpper        = _mm_hadd_ps(andLower,     andUpper);
        orUpperLower    = _mm_hadd_ps(orUpperLower, orUpperLower);
        andUpper        = _mm_hadd_ps(andUpper,     andUpper);
        orUpperLower    = _mm_hadd_ps(orUpperLower, orUpperLower);
        andUpper        = _mm_hadd_ps(andUpper,     andUpper);

        __declspec(align(16)) unsigned andResult[4];
        __declspec(align(16)) unsigned orUpperLowerResult[4];
        assert((size_t(andResult) & 0xf) == 0);
        assert((size_t(orUpperLowerResult) & 0xf) == 0);

        _mm_store_ps((float*)orUpperLowerResult, orUpperLower);
        _mm_store_ps((float*)andResult, andUpper);

        if (andResult[0])           { return AABBIntersection::Culled; }
        if (orUpperLowerResult[0])  { return AABBIntersection::Boundary; }
        return AABBIntersection::Within;
    }

    static inline Float4 XYZProj(const Float4x4& localToProjection, const Float3 input)
    {
        return localToProjection * Expand(input, 1.f);
    }

    static AABBIntersection::Enum TestAABB_Basic(const Float4x4& localToProjection, const Float3& mins, const Float3& maxs)
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
            return AABBIntersection::Culled;
        }
        if (leftOr | rightOr | topOr | bottomOr | nearOr | farOr) {
            return AABBIntersection::Boundary;
        }
        return AABBIntersection::Within;
    }

    AABBIntersection::Enum TestAABB(
        const Float4x4& localToProjection, 
        const Float3& mins, const Float3& maxs)
    {
        return TestAABB_Basic(localToProjection, mins, maxs);
    }

    AABBIntersection::Enum TestAABB_Aligned(
        const float localToProjection[], 
        const Float3& mins, const Float3& maxs)
    {
        return TestAABB_SSE(localToProjection, mins, maxs);
    }
}

