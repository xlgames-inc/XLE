// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../CommonResources.h"
#include "../Transform.h"

Texture2D<float> DepthTexture;

cbuffer Settings
{
    row_major float4x4 ProjectionMatrix;
    float3 ViewSpaceSunPosition;
}

float4 ps_sunflare(float4 pos : SV_Position, float3 startViewSpace : VIEWSPACEPOSITION) : SV_Target0
{
    // Walk between this point and the sun in view space, and find where the
    // sun is occluded in the depth buffer. This will result in a fixed number
    // of depth buffer samples per pixel

    const uint steps = 8;
    for (uint c=0; c<steps; ++c) {
        float3 testPosition = lerp(startViewSpace, ViewSpaceSunPosition, c / float(steps));
        float4 projected = mul(ProjectionMatrix, float4(testPosition, 1));
        float2 tc = float2(.5f + .5f * projected.x / projected.w, .5f - .5f * projected.y / projected.w);
        float depth = DepthTexture.SampleLevel(PointClampSampler, tc, 0);
        if (depth < 1.f) return 0.0.xxxx;
    }

    return 1.0.xxxx;
}

void vs_sunflare(uint vertexId : SV_VertexID, out float4 oPosition : SV_Position, out float3 oViewSpace : VIEWSPACEPOSITION)
{
    float2 coord = float2((float)(vertexId / 2), (float)(vertexId % 2));
    oViewSpace = FrustumCorners[vertexId].xyz;
    oPosition = float4(2.f * coord.x - 1.f, -2.f * coord.y + 1.f, 0.f, 1.f);
}
