// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Colour.h"

Texture2D	Input : register(t0);

cbuffer Material
{
    float2 RRoughnessRange;
    float2 RSpecularRange;
    float2 RMetalRange;

    float2 GRoughnessRange;
    float2 GSpecularRange;
    float2 GMetalRange;

    float2 BRoughnessRange;
    float2 BSpecularRange;
    float2 BMetalRange;

    float2 ARoughnessRange;
    float2 ASpecularRange;
    float2 AMetalRange;
}

float4 copy(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    return Input.Load(int3(position.xy, 0));
}

Texture2D DiffuseTexture;
Texture2D SpecularColor;
Texture2D MaterialMask;

struct PerPixelMaterialParam
{
    float   roughness;
    float   specular;
    float   metal;
};

PerPixelMaterialParam DecodeParametersTexture_ColoredSpecular(float4 paramTexSample, float4 diffuseSample)
{
    PerPixelMaterialParam result;
    float specLum = saturate(SRGBLuminance(paramTexSample.rgb));
    result.roughness = 0.f;
    result.specular = specLum;

    // float specLength = length(paramTexSample.rgb);
    // float3 specDir = paramTexSample.rgb / specLength;
    // float d = dot(specDir, diffuseSample.rgb);
    // float3 diffuseDiverge = diffuseSample.rgb - d * specDir;
    // float div = dot(diffuseDiverge, diffuseDiverge);
    // result.metal = lerp(MetalMin, MetalMax, saturate(32.f * div * div));

    float dH = RGB2HSV(diffuseSample.rgb).x;
    float sH = RGB2HSV(paramTexSample.rgb).x;
    float diff = abs(dH - sH);
    if (diff > 180) diff = 360 - diff;
    result.metal = diff / 180.f;
    result.metal *= specLum;
    result.metal *= result.metal;
    result.metal = saturate(result.metal);
    return result;
}

PerPixelMaterialParam ScaleByRange(
    PerPixelMaterialParam input,
    float2 rRange, float2 sRange, float2 mRange)
{
    PerPixelMaterialParam result;
    result.roughness = lerp(rRange.x, rRange.y, input.roughness);
    result.specular = lerp(sRange.x, sRange.y, input.specular);
    result.metal = lerp(mRange.x, mRange.y, input.metal);
    return result;
}

PerPixelMaterialParam Scale(PerPixelMaterialParam input, float factor)
{
    PerPixelMaterialParam result = input;
    result.roughness *= factor;
    result.specular *= factor;
    result.metal *= factor;
    return result;
}

PerPixelMaterialParam Add(PerPixelMaterialParam lhs, PerPixelMaterialParam rhs)
{
    PerPixelMaterialParam result;
    result.roughness = lhs.roughness + rhs.roughness;
    result.specular = lhs.specular + rhs.specular;
    result.metal = lhs.metal + rhs.metal;
    return result;
}

float4 AsParametersTexture(PerPixelMaterialParam param)
{
    return float4(param.roughness, param.specular, param.metal, 1.f);
}

float4 SpecularWithMask(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    float4 diffuse = DiffuseTexture.Load(int3(position.xy, 0));
    float4 specColor = SpecularColor.Load(int3(position.xy, 0));
    float4 materialMask = MaterialMask.Load(int3(position.xy, 0));
    materialMask.a = 1.0f - materialMask.a;       // (alpha weight is inverted)

    PerPixelMaterialParam param = DecodeParametersTexture_ColoredSpecular(specColor, diffuse);
    PerPixelMaterialParam rSample = ScaleByRange(param, RRoughnessRange, RSpecularRange, RMetalRange);
    PerPixelMaterialParam gSample = ScaleByRange(param, GRoughnessRange, GSpecularRange, GMetalRange);
    PerPixelMaterialParam bSample = ScaleByRange(param, BRoughnessRange, BSpecularRange, BMetalRange);
    PerPixelMaterialParam aSample = ScaleByRange(param, ARoughnessRange, ASpecularRange, AMetalRange);

    float weightSum = materialMask.r + materialMask.g + materialMask.b + materialMask.a;
    return
        AsParametersTexture(
            Add(Scale(rSample, materialMask.r/weightSum),
            Add(Scale(gSample, materialMask.g/weightSum),
            Add(Scale(bSample, materialMask.b/weightSum),
                Scale(aSample, materialMask.a/weightSum)))));
}
