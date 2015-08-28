// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Utility/DistinctColors.h"
#include "../CommonResources.h"

Texture2D<float>	Density : register(t0);
Texture2D<float>	VelocityU : register(t1);
Texture2D<float>	VelocityV : register(t2);

float3 GradColor(float input)
{
    input = saturate(input);
    const uint terraceCount = 15;
    float A = input * float(terraceCount);
    uint t = min(uint(floor(A)), terraceCount-1);
    return lerp(GetDistinctFloatColour(t), GetDistinctFloatColour(t+1), frac(A));
}

float4 ps_density(float4 position : SV_Position, float2 coords : TEXCOORD0) : SV_Target0
{
    float value = Density.SampleLevel(ClampingSampler, coords, 0);
    return float4(GradColor(value), 1.f);
}

float4 ps_velocity(float4 position : SV_Position, float2 coords : TEXCOORD0) : SV_Target0
{
    float2 velocity = float2(
        VelocityU.SampleLevel(ClampingSampler, coords, 0),
        VelocityV.SampleLevel(ClampingSampler, coords, 0));
    float2 v = saturate((velocity/1.0f + 1.0.xx) * 0.5.xx);

    return float4(v, 0.f, 1.f);
}
