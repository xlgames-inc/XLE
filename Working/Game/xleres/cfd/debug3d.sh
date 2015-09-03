// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Utility/DistinctColors.h"
#include "../CommonResources.h"

Texture3D<float>	Density        : register(t0);
Texture3D<float>	VelocityU      : register(t1);
Texture3D<float>	VelocityV      : register(t2);
Texture3D<float>	Temperature    : register(t3);

float3 GradColor(float input)
{
    input = saturate(input);
    const uint terraceCount = 15;
    float A = input * float(terraceCount);
    uint t = min(uint(floor(A)), terraceCount-1);
    return lerp(GetDistinctFloatColour(t), GetDistinctFloatColour(t+1), frac(A));
}

float3 As3DCoords(float2 input)
{
    const uint tilesX = 4;
    const uint tilesY = 4;
    input *= float2(tilesX, tilesY);
    float tileIndex = floor(input.y)*float(tilesX)+floor(input.x);
    uint3 dims;
    Density.GetDimensions(dims.x, dims.y, dims.z);
    return float3(frac(input), tileIndex/float(dims.z));
}

float4 ps_density(float4 position : SV_Position, float2 coords : TEXCOORD0) : SV_Target0
{
    float value = Density.SampleLevel(ClampingSampler, As3DCoords(coords), 0);
    return float4(GradColor(value), 1.f);
}

float4 ps_temperature(float4 position : SV_Position, float2 coords : TEXCOORD0) : SV_Target0
{
    float value = Temperature.SampleLevel(ClampingSampler, As3DCoords(coords), 0);
    return float4(GradColor(As3DCoords(coords).z), 1.f);
    return float4(GradColor(value), 1.f);
}

float4 ps_velocity(float4 position : SV_Position, float2 coords : TEXCOORD0) : SV_Target0
{
    float2 velocity = float2(
        VelocityU.SampleLevel(ClampingSampler, As3DCoords(coords), 0),
        VelocityV.SampleLevel(ClampingSampler, As3DCoords(coords), 0));
    float2 v = saturate((velocity/1.0f + 1.0.xx) * 0.5.xx);

    return float4(v, 0.f, 1.f);
}
