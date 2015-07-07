// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define OUTPUT_TEXCOORD 1
#define VSOUTPUT_EXTRA float2 dhdxy : DHDXY;

#include "TerrainGenerator.h"
#include "HeightsSample.h"
#include "../../MainGeometry.h"
#include "../../Transform.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

struct PatchInputControlPoint
{
    float3 worldPosition : POSITION;
};

PatchInputControlPoint vs_dyntess_main(uint vertexIndex : SV_VertexId)
{
    int x = vertexIndex % 2;
    int y = vertexIndex / 2;
    uint rawHeightValue = LoadRawHeightValue(
        int3(HeightMapOrigin.xy + int2(x,y) * (TileDimensionsInVertices-HeightsOverlap), HeightMapOrigin.z));

    float3 localPosition;
    localPosition.x		 = float(x);
    localPosition.y		 = float(y);
    localPosition.z		 = float(rawHeightValue);

    float3 cellPosition	 = mul( LocalToCell, float4(localPosition, 1)).xyz;
    float3 worldPosition = mul(LocalToWorld, float4( cellPosition, 1));
    worldPosition = AddNoise(worldPosition);

    PatchInputControlPoint output;
    output.worldPosition = worldPosition;
    return output;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

struct HS_ConstantOutput
{
    float Edges[4]        : SV_TessFactor;
    float Inside[2]       : SV_InsideTessFactor;
};

#define ControlPointCount		4

#if DO_EXTRA_SMOOTHING==1
    static const int InterpolationQuality = 2;
    #define MaxTessellation 2*32
#else
    static const int InterpolationQuality = 1;
    #define MaxTessellation 32
#endif
#define MinTessellation 4

uint RemapEdgeIndex(uint hsEdgeIndex)
{
    if (hsEdgeIndex == 0) { return 3; }
    if (hsEdgeIndex == 1) { return 0; }
    if (hsEdgeIndex == 2) { return 1; }
    return 2;
}

float Area(float2 A, float2 B, float2 C, float2 D)
{
    return abs((C.x - A.x) * (D.y - B.y) - (D.x - B.x) * (C.y - A.y));
}

float LengthSq(float2 A) { return dot(A, A); }

    //
    //		PatchConstantFunction
    //      ----------------------------------------------------
    //			-- this is run once per patch. It calculates values that are constant
    //				over the entire patch
    //
HS_ConstantOutput PatchConstantFunction(
    InputPatch<PatchInputControlPoint, ControlPointCount> ip,
    uint PatchID : SV_PrimitiveID)
{
    HS_ConstantOutput output;

    float2 halfViewport = float2(512, 400);
    // const float edgeThreshold = 384.f;
    const float edgeThreshold = 256.f;
    float mult = MaxTessellation / edgeThreshold;

    float2 screenPtsMax[4];
    float2 screenPtsMin[4];
    float3 bias = float3(0,0,20);       // note -- this would ideally be set to some real value based on the data!
    for (uint c2=0; c2<4; ++c2) {
        float4 clipMax = mul(WorldToClip, float4(ip[c2].worldPosition+bias, 1));
        screenPtsMax[c2] = clipMax.xy / clipMax.w * halfViewport;

        float4 clipMin = mul(WorldToClip, float4(ip[c2].worldPosition-bias, 1));
        screenPtsMin[c2] = clipMin.xy / clipMin.w * halfViewport;
    }

        // Edges:
        //  0: u == 0 (pt0 -> pt2)
        //	1: v == 0 (pt0 -> pt1)
        //	2: u == 1 (pt3 -> pt1)
        //	3: v == 1 (pt3 -> pt2)
    uint edgeStartPts[4]	= { 0, 0, 3, 3 };
    uint edgeEndPts[4]		= { 2, 1, 1, 2 };

    for (uint c=0; c<4; ++c) {
            //	Here, we calculate the amount of tessellation for the terrain edge
            //	This is the most important algorithm for terrain.
            //
            //	The current method is just a simple solution. Most of time we might
            //	need something more sophisticated.
            //
            //	In particular, we want to try to detect edges that are most likely
            //	to make up the siloette of details. Often terrain has smooth areas
            //	that don't need a lot of detail... But another area might have rocky
            //	detail with sharp edges -- that type of geometry needs much more detail.
            //
            //	Note that this method is currently producing the wrong results for
            //	tiles that straddle the near clip plane! This can make geometry near
            //	the camera swim around a bit.
#if 0
        float2 startS	= screenPts[edgeStartPts[c]];
        float2 endS		= screenPts[edgeEndPts[c]];

            //	The "extra-smoothing" boosts the maximum tessellation to twice it's
            //	normal value, and enables higher quality interpolation. This way
            //	distant geometry should be the same quality, but we can add extra
            //	vertices in near geometry when we need it.
        float screenSpaceLength = length(startS - endS);
        output.Edges[c] = clamp(screenSpaceLength * mult, MinTessellation, MaxTessellation);
#else
            // this calculation helps protect against bad collapses near the camera
            //      (which are really annoying!)
        float screenSpaceLength = LengthSq(screenPtsMax[edgeStartPts[c]] - screenPtsMax[edgeEndPts[c]]);
        screenSpaceLength = max(LengthSq(screenPtsMax[edgeEndPts[c]] - screenPtsMin[edgeEndPts[c]]), screenSpaceLength);
        screenSpaceLength = max(LengthSq(screenPtsMin[edgeEndPts[c]] - screenPtsMin[edgeStartPts[c]]), screenSpaceLength);
        screenSpaceLength = max(LengthSq(screenPtsMin[edgeStartPts[c]] - screenPtsMax[edgeStartPts[c]]), screenSpaceLength);
        screenSpaceLength = sqrt(screenSpaceLength);
        output.Edges[c] = clamp(screenSpaceLength * mult, MinTessellation, MaxTessellation);
#endif

        // On the LOD interface boundaries, we need to lock the tessellation
        // amounts to something predictable
        const float lodBoundaryTess = MaxTessellation;
        if (NeighbourLodDiffs[RemapEdgeIndex(c)] > 0) {
            output.Edges[c] = lodBoundaryTess;
        } else if (NeighbourLodDiffs[RemapEdgeIndex(c)] < 0) {
            output.Edges[c] = lodBoundaryTess/2;
        }
    }

        //	Could use min, max or average edge
        //	Note that when there are large variations between edge tessellation and
        //	inside tessellation, it can cause some wierd artefacts. We need to be
        //	careful about that.
        //
        // Anything other than max is causing some wierd shuddering near the
        // camera currently. Actually, "max" should be the most stable. Average or
        // min will react more strongly to lod changes (particularly if only one
        // edge is changing a lot)
    output.Inside[0] = max(output.Edges[1], output.Edges[3]);	// v==0 && v==1 edges
    output.Inside[1] = max(output.Edges[0], output.Edges[2]);	// u==0 && u==1 edges

    // output.Inside[0] = lerp(output.Edges[1], output.Edges[3], 0.5f);	// v==0 && v==1 edges
    // output.Inside[1] = lerp(output.Edges[0], output.Edges[2], 0.5f);	// u==0 && u==1 edges

    return output;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

struct PatchOutputControlPoint
{
    float3 worldPosition : POSITION;
};

[domain("quad")]
[partitioning("fractional_even")]
// [partitioning("integer")]
[outputtopology("triangle_cw")]
[patchconstantfunc("PatchConstantFunction")]
[outputcontrolpoints(4)]
[maxtessfactor(MaxTessellation)]
PatchOutputControlPoint hs_main(
    InputPatch<PatchInputControlPoint, ControlPointCount> ip,
    uint i : SV_OutputControlPointID,
    uint PatchID : SV_PrimitiveID )
{
        //	DirectX11 samples suggest that just copying the control points
        //	will activate a "fast pass through mode"
    PatchOutputControlPoint output;
    output.worldPosition = ip[i].worldPosition;
    return output;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

[domain("quad")]
    VSOutput ds_main(	HS_ConstantOutput input, float2 UV : SV_DomainLocation,
                        const OutputPatch<PatchInputControlPoint, ControlPointCount> inputPatch)
{
        //	After the hardware tessellator has run, let's
        //	calculate the positions of the final points. That means finding the
        //	correct location on the patch surface, and reading the height values from
        //	the texture. Let's just go back to patch local coords again.

    // int temp2 = int(UV.y * TileDimensionsInVertices)&1;
    // if (temp2 == 1) {
        //     UV.x += .5f / TileDimensionsInVertices;
        // }

    float rawHeightValue = CustomSample(UV.xy, InterpolationQuality);

        // quick hack to get normal values for the terrain
        //		-- find height values from the source height map
        //			and extract dhdx and dhdy from that
        //		Inside CustomSample, there are many extra interpolation
        //		steps -- that makes it a little inefficient
        //
        //		Note that this is not very accurate around the tile edges.
        //		we need an extra row & column of height values to correctly
        //		calculate the normal values for the edges. This is also needed
        //		to make cubic interpolation more accurate, btw!
    float A = 1.0f/(TileDimensionsInVertices);
    float heightDX = CustomSample(float2(UV.x + A, UV.y), InterpolationQuality);
    float heightDY = CustomSample(float2(UV.x, UV.y + A), InterpolationQuality);

        //	heightDX is the interpolated height change over the distance of a single height map element.
        //	We really want to convert this to world coordinates.
        //		we can simplify this by making assumptions about LocalToCell and LocalToWorld...
        //		let's assume that they both contain scale and translation, but no rotation or skew
        //		This is most likely the case (also they probably contain uniform scale)
    float conversionToWorldUnitsX = 1.0f/(TileDimensionsInVertices-HeightsOverlap) * LocalToCell[0][0] * LocalToWorld[0][0];
    float conversionToWorldUnitsY = 1.0f/(TileDimensionsInVertices-HeightsOverlap) * LocalToCell[1][1] * LocalToWorld[1][1];
    float conversionToWorldUnitsZ = LocalToCell[2][2] * LocalToWorld[2][2];
    float dhdx = (heightDX - rawHeightValue) * conversionToWorldUnitsZ / conversionToWorldUnitsX;
    float dhdy = (heightDY - rawHeightValue) * conversionToWorldUnitsZ / conversionToWorldUnitsY;

    float3 localPosition;
    localPosition.x		 = UV.x;
    localPosition.y		 = UV.y;
    localPosition.z		 = float(rawHeightValue);

    float3 cellPosition	 = mul( LocalToCell, float4(localPosition, 1)).xyz;
    float3 worldPosition = mul(LocalToWorld, float4( cellPosition, 1)).xyz;
    worldPosition = AddNoise(worldPosition);

    const bool showRawTessellationPatch = false;
    if (showRawTessellationPatch) {
        float u0w = (1.f - UV.x) * (1.f - UV.y);
        float u1w = (      UV.x) * (1.f - UV.y);
        float u2w = (1.f - UV.x) * (      UV.y);
        float u3w = (      UV.x) * (      UV.y);

        worldPosition =
              u0w * inputPatch[0].worldPosition
            + u1w * inputPatch[1].worldPosition
            + u2w * inputPatch[2].worldPosition
            + u3w * inputPatch[3].worldPosition
            ;
    }

    float4 clipPosition  = mul( WorldToClip, float4(worldPosition, 1));

    VSOutput output;
    output.position = clipPosition;
    output.texCoord = UV.xy;
    #if (OUTPUT_WORLD_POSITION==1)
        output.worldPosition = worldPosition;
    #endif
        // output height derivatives from domain shader (instead of normals
        //		-- because they will go through the linear interpolators
        //		much better than normals)
    output.dhdxy = float2(dhdx, dhdy);
    return output;
}
