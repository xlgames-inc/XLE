// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(VOLUMETRIC_FOG_H)
#define VOLUMETRIC_FOG_H

cbuffer VolumetricFogConstants
{
    float ESM_C;        //  = .25f * 80.f;
    float ShadowsBias;  // = 0.00000125f
    float ShadowDepthScale;

    float JitteringAmount; // = 0.5f;

    float Density;
    float NoiseDensityScale;
    float NoiseSpeed;

    float HeightStart;
    float HeightEnd;

    float3 ForwardColour;
    float3 BackColour;

    float3 ReciprocalGridDimensions;
    float WorldSpaceGridDepth;
}

#include "../Colour.h"
#include "../ShadowProjection.h"
#include "../TransformAlgorithm.h"

float MakeComparisonDistance(float shadowBufferDepth, int slice)
{
    float4 miniProj = ShadowProjection_GetMiniProj(slice);
    if (ShadowsPerspectiveProjection) {
        return NDCDepthToWorldSpace_Perspective(shadowBufferDepth, AsMiniProjZW(miniProj)) * ShadowDepthScale;
    } else {
        return NDCDepthToWorldSpace_Ortho(shadowBufferDepth, AsMiniProjZW(miniProj)) * ShadowDepthScale;
    }
}

#endif
