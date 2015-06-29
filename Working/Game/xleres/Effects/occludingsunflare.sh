// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../CommonResources.h"
#include "../Transform.h"
#include "../TextureAlgorithm.h"
#include "../Utility/MathConstants.h"

Texture2D<float> InputTexture;

cbuffer Settings
{
    float2 ProjSpaceSunPosition;
    float2 AspectCompensation;
}

float4 ps_sunflare_directblur(float4 pos : SV_Position, float2 projPos : PROJPOS) : SV_Target0
{
        // Walk between this point and the sun in view space, and find where the
        // sun is occluded in the depth buffer. This will result in a fixed number
        // of depth buffer samples per pixel
        // In the other version, we do a blur in radial coordinates first. This
        // version doesn't require that blur (which will change the performance)
        // but the result is more blocky and less smooth.

    float DSq = length(projPos.xy - ProjSpaceSunPosition);
    if (DSq > 0.75f)
        return 0.0.xxxx;

    const uint steps = 32;
    for (uint c=0; c<steps; ++c) {
        float4 testPosition = float4(lerp(projPos.xy, ProjSpaceSunPosition, (c) / float(steps)), 0, 1);
        float2 tc = float2(.5f + .5f * testPosition.x / testPosition.w, .5f - .5f * testPosition.y / testPosition.w);

        float4 depths = InputTexture.Gather(PointClampSampler, tc);
        // float A = dot(depths.xy, depths.zw);
        // float T = 1.97f;
        // if (A > T) return float4(10.0.xxx, (1.f-(DSq/.75f)) * saturate((A-T) / (2-T)));

        float t = dot(depths == 1.f, 1.0.xxxx);
        if (t > 0.f) return float4(10.0.xxx, (1.f-(DSq/.75f)) * (t/4.f));
    }

    return 0.0.xxxx;
}

float4 ps_sunflare(float4 pos : SV_Position, float2 projPos : PROJPOS) : SV_Target0
{
    float2 off = projPos.xy - ProjSpaceSunPosition;
    off /= AspectCompensation;

    float d = length(off);
    if (d > 1.f) discard;

    float a = atan2(off.y, off.x);
    return float4(10.0.xxx, (1.f - d) * InputTexture.SampleLevel(WrapUSampler, float2(a / (2.f * pi), d), 0));
}

void vs_sunflare(uint vertexId : SV_VertexID, out float4 oPosition : SV_Position, out float2 oProjSpace : PROJPOS)
{
    float2 coord = float2((float)(vertexId / 2), (float)(vertexId % 2));
    oPosition = float4(2.f * coord.x - 1.f, -2.f * coord.y + 1.f, 0.f, 1.f);
    oProjSpace = oPosition.xy;
}

float2 ToCartesianTC(float2 radialTC)
{
    float2 cs;
    sincos(2.f * pi * radialTC.x, cs.y, cs.x);
    float2 source = ProjSpaceSunPosition + AspectCompensation * radialTC.y * cs;
    return float2(.5f + .5f * source.x, .5f - .5f * source.y);
}

float ps_toradial(float4 pos : SV_Position, float2 projPos : PROJPOS) : SV_Target0
{
    float2 Q;
    Q.x = .5f + .5f * projPos.x;
    Q.y = .5f - .5f * projPos.y;

    float4 depths = InputTexture.Gather(PointClampSampler, ToCartesianTC(Q));
    return dot(depths == 1.f, 1.0.xxxx) / 4.f;
}

float ps_blur(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    float result = 0.f;
    uint baseY = 0; // uint(position.x)%4;
    const uint sampleCount = 16;

    float2 recipOutputDims = texCoord.xy / position.xy;

    [unroll] for (uint c=0; c<sampleCount; c++) {
        uint2 p;
        p.x = position.x;
        // p.x += float(c%4) / 2.f - 1.f;
        p.y = baseY + uint(c / float(sampleCount) * float(position.y) + 0.5f);

        #if (SINGLE_PASS==0)
            float texSample = LoadFloat1(InputTexture, p, 0);
        #else
            float4 depths = InputTexture.Gather(PointClampSampler, ToCartesianTC(p * recipOutputDims));
            float texSample = dot(depths == 1.f, 1.0.xxxx) / 4.f;
        #endif

        result += texSample;
    }

    return saturate(2.f * result / float(sampleCount));
    // return saturate(result);
}
