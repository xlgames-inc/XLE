// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Transform.h"
#include "../MainGeometry.h"
#include "../Surface.h"
#include "../ShadowProjection.h"
#include "../Vegetation/WindAnim.h"
#include "../Vegetation/InstanceVS.h"
#include "../Utility/ProjectionMath.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

struct RTS_VSOutput
{
    float4 position : POSITION;

    #if OUTPUT_TEXCOORD==1
        float2 texCoord : TEXCOORD0;
    #endif

    #if (OUTPUT_WORLD_POSITION==1)
        float3 worldPosition : WORLDPOSITION;
    #endif
};

void vs_writetris(VSInput input, out RTS_VSOutput output)
{
    float3 localPosition	= GetLocalPosition(input);

    #if GEO_HAS_INSTANCE_ID==1
        float3 objectCentreWorld;
        float3 worldPosition = InstanceWorldPosition(input, objectCentreWorld);
    #else
        float3 worldPosition = mul(LocalToWorld, float4(localPosition,1)).xyz;
        float3 objectCentreWorld = float3(LocalToWorld[0][3], LocalToWorld[1][3], LocalToWorld[2][3]);
    #endif

    #if OUTPUT_TEXCOORD==1
        output.texCoord = GetTexCoord(input);
    #endif

    #if (GEO_HAS_NORMAL==1) || (GEO_HAS_TANGENT_FRAME==1)
        float3 worldNormal = LocalToWorldUnitVector(GetLocalNormal(input));
        #if (GEO_HAS_NORMAL==0) && (GEO_HAS_TANGENT_FRAME==1)
            worldNormal =  BuildWorldSpaceTangentFrame(input).normal;
        #endif

        worldPosition = PerformWindBending(worldPosition, worldNormal, objectCentreWorld, float3(1,0,0), GetColour(input));
    #endif

    // output.position = mul(WorldToClip, float4(worldPosition,1));

    #if SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ARBITRARY
        output.position = ShadowProjection_GetOutput(worldPosition, 0);
    #elif SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ORTHOGONAL
        float3 basePosition = mul(OrthoShadowWorldToProj, float4(worldPosition, 1));
        float3 cascadePos = AdjustForCascade(basePosition, 0);
        output.position = float4(cascadePos, 1.f);
    #endif

    #if OUTPUT_WORLD_POSITION==1
        output.worldPosition = worldPosition.xyz;
    #endif
}

[maxvertexcount(3)]
    void gs_writetris(
        triangle RTS_VSOutput input[3],
        inout TriangleStream<RTS_VSOutput> outputStream)
{
        // Ideally we want the primitive id to be after frustum culling!
        // But we can't get the size of the stream-output buffer from here
        // So, the only way to do that is to use stream-output to collect all
        // of the triangles first, and then rasterize them in a second step.
        // Well, maybe that wouldn't be so bad.
    if (TriInFrustum(input[0].position, input[1].position, input[2].position)
        && BackfaceSign(input[0].position, input[1].position, input[2].position) > 0.f) {

        outputStream.Append(input[0]);
        outputStream.Append(input[1]);
        outputStream.Append(input[2]);
    }
}

void vs_passthrough(float4 position : POSITION, out float4 outPos : SV_Position)
{
    outPos = position;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

struct RTS_GSInput
{
    float4 position : POSITION;
};

struct RTS_GSOutput
{
    float4 position : SV_Position;
    nointerpolation uint triIndex : TRIINDEX;
};

#if 0
[maxvertexcount(3)]
    void gs_passthrough(
        triangle RTS_GSInput input[3], uint primId : SV_PrimitiveID,
        inout TriangleStream<RTS_GSOutput> outputStream)
{
        // this geometry shader just passes SV_PrimitiveID down to the
        // pixel shader...
    RTS_GSOutput zero, one, two;
    zero.position = input[0].position; zero.triIndex = primId;
    one.position = input[1].position; one.triIndex = primId;
    two.position = input[2].position; two.triIndex = primId;

    outputStream.Append(zero);
    outputStream.Append(one);
    outputStream.Append(two);
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

struct ListNode
{
    uint    next;
    uint	triIndex;
};

RWTexture2D<uint>	          ListsHead    : register(u1);
RWStructuredBuffer<ListNode>  LinkedLists  : register(u2);

uint ps_main(float4 pos : SV_Position,
    #if (OUTPUT_PRIM_ID==1)     // (set if we get the primitive id from the geometry shader)
        uint triIndex : PRIMID
    #else
        uint triIndex : SV_PrimitiveID
    #endif
    ) : SV_Target0
{
        // it would be helpful for ListsHead where our bound render target.
        // But we need to both read and write from it... That isn't possible
        // without using a UAV. But it means that the pixel shader output
        // is going to be discarded.

    uint newNodeId = LinkedLists.IncrementCounter();
    uint oldNodeId;
    InterlockedExchange(ListsHead[uint2(pos.xy)], newNodeId+1, oldNodeId);
    // uint oldNodeId = ListsHead[uint2(pos.xy)];
    // ListsHead[uint2(pos.xy)] = newNodeId;

    LinkedLists[newNodeId].triIndex = triIndex;
    LinkedLists[newNodeId].next = oldNodeId;

    // discard;    // perhaps we can write out min/max here (instead of just discard)
    return newNodeId;
}
