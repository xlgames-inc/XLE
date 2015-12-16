// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "resolveutil.h"
#include "../Colour.h" // for LightingScale

#if HAS_SCREENSPACE_AO==1
    Texture2D<float>	AmbientOcclusion : register(t5);
#endif

export void Setup(
    float4 position,
    float3 viewFrustumVector,
    uint sampleIndex,
    out float3 worldPosition,
    out float worldSpaceDepth,
    out float screenSpaceOcclusion)
{
    int2 pixelCoords = position.xy;
    worldSpaceDepth = GetWorldSpaceDepth(pixelCoords, sampleIndex);
    worldPosition = WorldSpaceView + (worldSpaceDepth / FarClip) * viewFrustumVector;

    #if HAS_SCREENSPACE_AO==1
        screenSpaceOcclusion = LoadFloat1(AmbientOcclusion, pixelCoords, sampleIndex);
    #else
        screenSpaceOcclusion = 1.f;
    #endif
}

export float4 FinalizeResolve(float3 resolvedLight, float shadow)
{
    return float4((LightingScale*shadow)*resolvedLight, 1.f);
}
