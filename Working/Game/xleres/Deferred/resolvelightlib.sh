// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Lighting/LightShapes.h"
#include "resolveutil.h"
#include "../System/LoadGBuffer.h"
#include "../Colour.h" // for LightingScale

#if HAS_SCREENSPACE_AO==1
    Texture2D<float>	AmbientOcclusion : register(t5);
#endif

cbuffer LightBuffer
{
    LightDesc Light;
}

export float3 DoResolve_Directional(
    float4 position, float3 viewFrustumVector,
    float3 worldPosition, uint sampleIndex)
{
    SystemInputs sys = SystemInputs_SampleIndex(sampleIndex);

    int2 pixelCoords = position.xy;
    GBufferValues sample = LoadGBuffer(position.xy, sys);

    LightSampleExtra sampleExtra;
    sampleExtra.screenSpaceOcclusion = 1.f;
    #if HAS_SCREENSPACE_AO==1
        sampleExtra.screenSpaceOcclusion = LoadFloat1(AmbientOcclusion, pixelCoords, GetSampleIndex(sys));
    #endif

    LightScreenDest screenDest;
    screenDest.pixelCoords = pixelCoords;
    screenDest.sampleIndex = GetSampleIndex(sys);

    return Resolve_Directional(sample, sampleExtra, Light, worldPosition, normalize(-viewFrustumVector), screenDest);
}

export void CalculateWorldPosition(
    float4 position,
    float3 viewFrustumVector,
    uint sampleIndex,
    out float3 worldPosition,
    out float worldSpaceDepth)
{
    int2 pixelCoords = position.xy;
    worldSpaceDepth = GetWorldSpaceDepth(pixelCoords, sampleIndex);
    worldPosition = WorldSpaceView + (worldSpaceDepth / FarClip) * viewFrustumVector;
}

export float4 FinalizeResolve(float3 resolvedLight)
{
    return float4((LightingScale)*resolvedLight, 1.f);
}
