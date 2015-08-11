// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(RESOLVE_RT_SHADOWS_H)
#define RESOLVE_RT_SHADOWS_H

struct RTSListNode
{
    uint    next;
    uint	triIndex;
};

struct RTSTriangle
{
    float4 corners[3];
};

Texture2D<uint>	                RTSListsHead;
StructuredBuffer<RTSListNode>   RTSLinkedLists;
StructuredBuffer<RTSTriangle>   RTSTriangles;

float DirSign2D(float2 p1, float2 p2, float2 p3)
{
  return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

bool IsPointInTri2D(float2 pt, float2 a, float2 b, float2 c)
{
    bool t0 = DirSign2D(pt, a, b) < 0.0f;
    bool t1 = DirSign2D(pt, b, c) < 0.0f;
    bool t2 = DirSign2D(pt, c, a) < 0.0f;
    return ((t0 == t1) && (t1 == t2));
}

float3 Barycentric2D(float2 pt, float2 a, float2 b, float2 c)
{
        // precalculate {
    float2 v0 = b - a, v1 = c - a;
    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float invDenom = 1.0 / (d00 * d11 - d01 * d01);
        // }

    float2 v2 = pt - a;
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);

    float3 result;
    result.x = (d11 * d20 - d01 * d21) * invDenom;
    result.y = (d00 * d21 - d01 * d20) * invDenom;
    result.z = 1.0f - result.x - result.y;
    return result;
}

bool IsShadowedByTriangle(float3 postDivideCoord, uint triIndex)
{
    RTSTriangle tri = RTSTriangles[triIndex];
    float2 A = tri.corners[0].xy / tri.corners[0].w;
    float2 B = tri.corners[1].xy / tri.corners[1].w;
    float2 C = tri.corners[2].xy / tri.corners[2].w;
    if (IsPointInTri2D(postDivideCoord.xy, A, B, C)) {
        float3 bary = Barycentric2D(postDivideCoord.xy, A, B, C);
        float d = dot(bary,
            float3(
                tri.corners[0].z/tri.corners[0].w,
                tri.corners[1].z/tri.corners[1].w,
                tri.corners[2].z/tri.corners[2].w));
        return d < postDivideCoord.z;
    }
    return false;
}

float ResolveRTShadows(float3 postDivideCoord, int2 randomizerValue)
{
    uint2 dims;
    RTSListsHead.GetDimensions(dims.x, dims.y);
    uint2 gridIndex = uint2(postDivideCoord.xy * dims);

        // todo -- check min/max here

    uint i = RTSListsHead[gridIndex];
    while (i!=0) {
        if (IsShadowedByTriangle(postDivideCoord, RTSLinkedLists[i].triIndex))
            return 0.f;
        i = RTSLinkedLists[i].next;
    }
    return 1.f;
}

#endif
