// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../TechniqueLibrary/Framework/MainGeometry.hlsl"

float4 GetPosition(VSOUT v) { return v.position; }

struct GSOutput
{
    VSOUT v;
    #if (OUTPUT_PRIM_ID==1)
        nointerpolation uint primId : PRIMID;
    #endif
};

GSOutput MakeVertex(VSOUT current, float2 semiDiagonalAdj, uint primId);

// float SpecialSign(float i) { return sign(i); }
float SpecialSign(float i) { return i >= 0.f ? 1.f : -1.f; }

void GenerateCorner(
    VSOUT prev, VSOUT current, VSOUT next,
    inout GSOutput starterVertex, bool isFirst, uint primId,
    inout TriangleStream<GSOutput> outputStream)
{
    //starterVertex = MakeVertex(current, 0.0.xx);
    //outputStream.Append(starterVertex);
    //return;

    // We want to generate a "skirt" around the triangle,
    // expanding it out so that every pixel it touches is
    // included in rasterisation. The number of vertices we
    // need to generate is determined by the angle between the
    // edges.
    //
    // This emulates behaviour built into newer GPUs.
    //
    // Following http://http.developer.nvidia.com/GPUGems2/gpugems2_chapter42.html,
    // we want to know the quadrant that the input edge and output
    // edges belong to.
    //
    // This is sensitive to the winding order. Only one winding
    // order will work correctly.
    //
    // Note that for geometry that might be touching the near clip
    // plane, we should do a near-clip test here!

    float2 p = GetPosition(prev).xy / GetPosition(prev).w;
    float2 c = GetPosition(current).xy / GetPosition(current).w;
    float2 n = GetPosition(next).xy / GetPosition(next).w;

    float2 e0n = float2(-(p.y - c.y), (p.x - c.x));
    float2 e1n = float2((n.y - c.y), -(n.x - c.x));

    // We can get the semidiagonal using "sign"
    // this will work correct for all cases, except when
    // the input value 0! In that case, there's some ambiguity
    // about the semidiagonal -- we have to choose whether to
    // force the value to be -1 or 1.

    float2 sd0 = float2(SpecialSign(e0n.x), SpecialSign(e0n.y));
    float2 sd1 = float2(SpecialSign(e1n.x), SpecialSign(e1n.y));

    // this part follows GPU Gems 2 closely (but note
    // apparent error in 2 quadrant case)
    // Also; we're generating a triangle strip here, so
    // we have to insert extra points (including some
    // degenerate triangles)
    if (isFirst) {
        starterVertex = MakeVertex(current, sd0, primId);
        outputStream.Append(starterVertex);
    } else {
        outputStream.Append(starterVertex);
        outputStream.Append(MakeVertex(current, sd0, primId));
    }

    float dp = dot(sd0, sd1);
    if (dp > 0) {
            // need a degenerate to get back to the correct winding order
            // (this should be redundant if back face culling is disabled)
        if (isFirst) outputStream.Append(starterVertex);
    } else if (dp == 0) {
        if (!isFirst) outputStream.Append(starterVertex);
        outputStream.Append(MakeVertex(current, sd1, primId));
    } else {
        float2 sd2 = float2(
            sd0.x * sd0.y * sd1.x,
            sd0.y * sd1.x * sd1.y);
        if (!isFirst) outputStream.Append(starterVertex);
        outputStream.Append(MakeVertex(current, sd2, primId));
        outputStream.Append(starterVertex);
        outputStream.Append(MakeVertex(current, sd1, primId));
    }
}

GSOutput MakeVertex(VSOUT current, float2 semiDiagonalAdj, uint primId)
{
    // float2 pixelSize = float2(2.f/64.f, 2.f/64.f);
    float2 pixelSize = float2(2.f/256.f, 2.f/256.f);
    GSOutput result;
    result.v.position = GetPosition(current);
    result.v.position.xy +=
        semiDiagonalAdj * pixelSize * result.v.position.w;
    // todo -- adjust z as well...?
    #if (OUTPUT_PRIM_ID==1)
        result.primId = primId;
    #endif
    return result;
}

#if !(INPUT_RAYTEST_TRIS)

    // (if we could output triangle lists, worse case output would be 9 vertices)
    [maxvertexcount(16)]
        void gs_conservativeRasterization(
            triangle VSOUT input[3],
            #if (OUTPUT_PRIM_ID==1)
                uint primId : SV_PrimitiveID,
            #endif
            inout TriangleStream<GSOutput> outputStream)
    {
        #if !(OUTPUT_PRIM_ID==1)
            uint primId = 0;
        #endif

            // note -- if there are back facing triangle input
            //  sometimes we will get reversed triangles output!
            //  we should always do backface culling before this point
            //  Also, we may need to clipping
            //  Also, currently the 'z' value for the output primitives will
            //  be wrong (because it's not adjusted for the skirt)
        GSOutput starterVertex;
        GenerateCorner(
            input[2], input[0], input[1],
            starterVertex, true, primId,
            outputStream);

        GenerateCorner(
            input[0], input[1], input[2],
            starterVertex, false, primId,
            outputStream);

        GenerateCorner(
            input[1], input[2], input[0],
            starterVertex, false, primId,
            outputStream);
    }

#else

    struct RTSTriangle
    {
        float4 a_v0 : A;
        float2 v1 : B;
        float4 param : C;
        float3 depths : D;
    };

    // (if we could output triangle lists, worse case output would be 9 vertices)
    [maxvertexcount(16)]
        void gs_conservativeRasterization(
            point RTSTriangle input[1],
            #if (OUTPUT_PRIM_ID==1)
                uint primId : SV_PrimitiveID,
            #endif
            inout TriangleStream<GSOutput> outputStream)
    {
        #if !(OUTPUT_PRIM_ID==1)
            uint primId = 0;
        #endif

        VSOUT v[3];
        v[0].position = float4(input[0].a_v0.xy, 0.f, 1.f);
        v[1].position = float4(input[0].a_v0.xy + input[0].a_v0.zw, 0.f, 1.f);
        v[2].position = float4(input[0].a_v0.xy + input[0].v1, 0.f, 1.f);

        GSOutput starterVertex;
        GenerateCorner(
            v[2], v[0], v[1],
            starterVertex, true, primId,
            outputStream);

        GenerateCorner(
            v[0], v[1], v[2],
            starterVertex, false, primId,
            outputStream);

        GenerateCorner(
            v[1], v[2], v[0],
            starterVertex, false, primId,
            outputStream);
    }

#endif
