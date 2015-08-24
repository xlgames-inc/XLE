// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Utility/DistinctColors.h"

Texture2D<float>	HardMaterials : register(t2);
Texture2D<float>	SoftMaterials : register(t3);

float3 GradColor(float input)
{
    input = saturate(input);
    const uint terraceCount = 15;
    float A = input * float(terraceCount);
    uint t = min(uint(floor(A)), terraceCount-1);
    return lerp(GetDistinctFloatColour(t), GetDistinctFloatColour(t+1), frac(A));
}

float4 ps_hardMaterials(float4 position : SV_Position, float2 coords : TEXCOORD0) : SV_Target0
{
    float value = HardMaterials.Load(uint3(coords.xy, 0));
    value /= 500.f;
    return float4(GradColor(value), 1.f);
}

float4 ps_softMaterials(float4 position : SV_Position, float2 coords : TEXCOORD0) : SV_Target0
{
    float hard = HardMaterials.Load(uint3(coords.xy, 0));
    float soft = SoftMaterials.Load(uint3(coords.xy, 0));

    float3 hardCol = float3(1.f, 0.25f, 0.25f) * GradColor(hard / 500.f);
    return float4(hardCol + soft.xxx, 1.f);
}
