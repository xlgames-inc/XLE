// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(VOLUMETRIC_FOG_H)
#define VOLUMETRIC_FOG_H

cbuffer VolumetricFogConstants
{
    float3 ForwardColour;
    float3 BackColour;

    float3 ReciprocalGridDimensions;
    float WorldSpaceGridDepth;

    float ESM_C;        //  = .25f * 80.f;
    float ShadowsBias;  // = 0.00000125f
    float ShadowDepthScale;

    float JitteringAmount; // = 0.5f;

    float Density;
    float NoiseDensityScale;
    float NoiseSpeed;

    float HeightStart;
    float HeightEnd;
}

#define MONOCHROME_INSCATTER 1

#include "../Colour.h"
#include "../ShadowProjection.h"
#include "../TransformAlgorithm.h"
#include "../Utility/MathConstants.h"
#include "../Lighting/ShadowsResolve.h" // for ShadowsPerspectiveProjection

float MakeComparisonDistance(float shadowBufferDepth, int slice)
{
    #if ESM_SHADOW_MAPS==1
        float4 miniProj = ShadowProjection_GetMiniProj(slice);
        if (ShadowsPerspectiveProjection) {
            return NDCDepthToWorldSpace_Perspective(shadowBufferDepth, AsMiniProjZW(miniProj)) * ShadowDepthScale;
        } else {
            return NDCDepthToWorldSpace_Ortho(shadowBufferDepth, AsMiniProjZW(miniProj)) * ShadowDepthScale;
        }
    #else
        return shadowBufferDepth;
    #endif
}

static const float DepthPower = 3.f;

float DepthBiasEq(float depth0To1)
{
    return pow(max(0.f, depth0To1), DepthPower);
}

float DepthBiasInvEq(float depth0To1)
{
    return pow(max(0.f, depth0To1), 1.f/DepthPower);
}

float MonochromeRaleighScattering(float cosTheta)
{
    return 3.f / (16.f * pi) * (1.f + cosTheta * cosTheta);
}

#endif
