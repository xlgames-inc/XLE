// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(RESOLVE_CASCADE_H)
#define RESOLVE_CASCADE_H

#include "../ShadowProjection.h"

void FindCascade_FromWorldPosition(
    out uint finalCascadeIndex, out float4 frustumCoordinates,
    float3 worldPosition)
{
        // find the first frustum we're within
    uint projectionCount = min(GetShadowSubProjectionCount(), ShadowMaxSubProjections);

    finalCascadeIndex = projectionCount;

    #if SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ORTHOGONAL
        float3 basePosition = mul(OrthoShadowWorldToProj, float4(worldPosition, 1));
    #endif

    for (uint c=0; c<projectionCount; c++) {
        float wPart = 1.f;
        #if SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ARBITRARY
            frustumCoordinates = mul(ShadowWorldToProj[c], float4(worldPosition, 1));
            wPart = frustumCoordinates.w;
        #elif SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ORTHOGONAL
            frustumCoordinates = float4(AdjustForCascade(basePosition, c), 1.f);
        #else
            frustumCoordinates = float4(2.0.xxx, 1.f);
        #endif

        if (max(max(abs(frustumCoordinates.x), abs(frustumCoordinates.y)),
            max(frustumCoordinates.z, wPart-frustumCoordinates.z)) < wPart) {

            finalCascadeIndex = c;
            break;
        }
    }
}

float4 CameraCoordinateToShadow(float2 camCoordinate, float worldSpaceDepth, float4x4 camToShadow)
{
    const float cameraCoordinateScale = worldSpaceDepth; // (linear0To1Depth * FarClip);

        //
        //	Accuracy of this transformation is critical...
        //		We'll be comparing to values in the shadow buffer, so we
        //		should try to use the most accurate transformation method
        //
    float4x3 cameraToShadow4x3 = float4x3(
        camToShadow[0].xyz,
        camToShadow[1].xyz,
        camToShadow[2].xyz,
        camToShadow[3].xyz);
    float4 offset = mul(cameraToShadow4x3, float3(camCoordinate, -1.f));
    offset *= cameraCoordinateScale;	// try doing this scale here (maybe increase accuracy a bit?)

    float4 translatePart = float4(camToShadow[0].w, camToShadow[1].w, camToShadow[2].w, camToShadow[3].w);
    return offset + translatePart;
}

void FindCascade_CameraToShadowMethod(
    out uint finalCascadeIndex, out float4 frustumCoordinates,
    float2 texCoord, float worldSpaceDepth)
{
    const float2 camCoordinate = float2(
        ( 2.f * texCoord.x - 1.f) / MinimalProjection.x,
        (-2.f * texCoord.y + 1.f) / MinimalProjection.y);

    uint projectionCount = min(GetShadowSubProjectionCount(), ShadowMaxSubProjections);
    finalCascadeIndex = projectionCount;

        // 	Find the first frustum we're within
        //	This first loop is kept separate and simple
        //	even though it means we need another comparison
        //	below. This is just to try to keep the generated
        //	shader code simplier.
        //
        //	Note that in order to unroll this first loop, we
        //	must make the loop terminator a compile time constant.
        //	Normally, the number of cascades is passed in a shader
        //	constant (ie, not available at compile time).
        //	However, if the cascade loop is simple, it may be better
        //	to unroll, even if it means a few extra redundant checks
        //	at the end.
        //
        //	It looks like these 2 tweaks (separating the first loop,
        //	and unrolling it) reduces the number of temporary registers
        //	required by 4 (but obvious increases the instruction count).
        //	That seems like a good improvement.

    #if SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ORTHOGONAL

            // in ortho mode, this is much simplier... Here is a
            // separate implementation to take advantage of that case!

        float3 baseCoord = CameraCoordinateToShadow(camCoordinate, worldSpaceDepth, OrthoCameraToShadow).xyz;

            // all cascades have the same near/far clip plane. So we can reject based on depth early
            // then we only need to look at the XY part of each cascade
        if (max(baseCoord.z, 1.f-baseCoord.z) <= 1.f) {
            [unroll] for (uint c=0; c<ShadowMaxSubProjections; c++) {
                float3 t = AdjustForCascade(baseCoord, c);
                if (max(abs(t.x), abs(t.y)) < 1.f) {
                    finalCascadeIndex = c;
                    frustumCoordinates = float4(t, 1.f);
                    break;
                }
            }
        }

    #else
        for (uint c=0; c<projectionCount; c++) {
            frustumCoordinates = CameraCoordinateToShadow(
                camCoordinate, worldSpaceDepth, CameraToShadow[c]);

            float wPart = frustumCoordinates.w;
            if (max(max(abs(frustumCoordinates.x), abs(frustumCoordinates.y)),
                max(frustumCoordinates.z, wPart-frustumCoordinates.z)) < wPart) {

                finalCascadeIndex = c;
                break;
            }
        }
    #endif
}

#endif
