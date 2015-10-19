// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(COMMON_BRUSHES_H)
#define COMMON_BRUSHES_H

#include "Interfaces.h"
#include "../../CommonResources.h"

class SolidFill : IBrush
{
    float4 Calculate(DebuggingShapesCoords coords, float4 baseColor, float2 fillDerivatives) { return baseColor; }
    bool NeedsDerivatives() { return false; }
};

class WhiteOutline : IBrush
{
    float4 Calculate(DebuggingShapesCoords coords, float4 baseColor, float2 fillDerivatives) { return 1.0.xxxx; }
    bool NeedsDerivatives() { return false; }
};

class CrossHatchFill : IBrush
{
    float4 Calculate(
        DebuggingShapesCoords coords,
        float4 baseColor,
        float2 fillDerivatives)
    {
        float4 color = baseColor;

            // cross hatch pattern -- (bright:dark == 1:1)
        uint p = uint(coords.position.x) + uint(coords.position.y);
        if (((p/4) % 2) != 0) {
            color.rgb *= 0.66f;
        }

        return color;
    }
    bool NeedsDerivatives() { return false; }
};

Texture2D RefractionsBuffer : register(t12);

static const float SqrtHalf = 0.70710678f;
static const float3 BasicShapesLightDirection = normalize(float3(SqrtHalf, SqrtHalf, -0.25f));

class RaisedRefactiveFill : IBrush
{
    float4 Calculate(
        DebuggingShapesCoords coords,
        float4 baseColor,
        float2 dhdp)
    {
        float3 u = float3(1.f, 0.f, dhdp.x);
        float3 v = float3(0.f, 1.f, dhdp.y);
        float3 normal = normalize(cross(v, u));

        float d = saturate(dot(BasicShapesLightDirection, normal));
        float A = 7.5f * pow(d, 2.f);

        float3 result = float4(A * baseColor + 0.1.xxx, 1.f);

        result.rgb += RefractionsBuffer.SampleLevel(ClampingSampler, GetRefractionCoords(coords), 0).rgb;
        return float4(result.rgb, 1.f);
    }

    bool NeedsDerivatives() { return true; }
};

#endif
