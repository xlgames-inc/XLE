// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(SHADOW_PROJECTION_H)
#define SHADOW_PROJECTION_H

#include "MainGeometry.h"
#include "Binding.h"

static const uint ShadowMaxSubProjections = 6;

cbuffer ArbitraryShadowProjection BIND_SEQ_B4
{
        // note --
        //      I've used this order to try to reduce the
        //      about of unused space at the top of CB when
        //      we read from fewer projections than
        //      "ShadowMaxSubProjections" We could maybe get
        //      a better result by combining the projection
        //      and minimal projection into a struct, I guess.
	uint ShadowSubProjectionCount;
    float4 ShadowMinimalProjection[ShadowMaxSubProjections];
    row_major float4x4 ShadowWorldToProj[ShadowMaxSubProjections];
}

cbuffer OrthogonalShadowProjection BIND_SEQ_B5
{
	row_major float3x4 OrthoShadowWorldToProj;
    float4 OrthoShadowMinimalProjection;
    uint OrthoShadowSubProjectionCount;
    float3 OrthoShadowCascadeScale[ShadowMaxSubProjections];
    float3 OrthoShadowCascadeTrans[ShadowMaxSubProjections];
	row_major float3x4 OrthoNearCascade;
	float4 OrthoShadowNearMinimalProjection;
}

cbuffer ScreenToShadowProjection
{
    row_major float4x4 CameraToShadow[ShadowMaxSubProjections];
	row_major float4x4 OrthoCameraToShadow;	// the "definition" projection for cascades in ortho mode
	float2 XYScale;
	float2 XYTrans;
	row_major float4x4 OrthoNearCameraToShadow;
}

#if defined(SHADOW_CASCADE_MODE)
	uint GetShadowCascadeMode() { return SHADOW_CASCADE_MODE; }
#endif

uint GetShadowSubProjectionCount(uint cascadeMode)
{
	if (cascadeMode == SHADOW_CASCADE_MODE_ARBITRARY) 	return ShadowSubProjectionCount;
	if (cascadeMode == SHADOW_CASCADE_MODE_ORTHOGONAL) 	return OrthoShadowSubProjectionCount;
    return 0;
}

float3 AdjustForOrthoCascade(float3 basePosition, uint cascadeIndex)
{
    return float3(
		basePosition.x * OrthoShadowCascadeScale[cascadeIndex].x + OrthoShadowCascadeTrans[cascadeIndex].x,
		basePosition.y * OrthoShadowCascadeScale[cascadeIndex].y + OrthoShadowCascadeTrans[cascadeIndex].y,
		basePosition.z);
}

float4 ShadowProjection_GetOutput(float3 position, uint cascadeIndex, uint cascadeMode)
{
	if (cascadeMode==SHADOW_CASCADE_MODE_ARBITRARY) {
        return mul(ShadowWorldToProj[cascadeIndex], float4(position,1));
    } else if (cascadeMode==SHADOW_CASCADE_MODE_ORTHOGONAL) {
        float3 a = AdjustForOrthoCascade(mul(OrthoShadowWorldToProj, float4(position, 1)), cascadeIndex);
        return float4(a, 1.f);
    } else {
        return 0.0.xxxx;
    }
}

float4 ShadowProjection_GetOutput(VSInput geo, uint cascadeIndex, uint cascadeMode)
{
	return ShadowProjection_GetOutput(geo.position, cascadeIndex, cascadeMode);
}

float4 ShadowProjection_GetMiniProj_NotNear(uint cascadeIndex, uint cascadeMode)
{
	if (cascadeMode==SHADOW_CASCADE_MODE_ARBITRARY) {
		return ShadowMinimalProjection[cascadeIndex];
	} else if (cascadeMode==SHADOW_CASCADE_MODE_ORTHOGONAL) {
		float4 result = OrthoShadowMinimalProjection;
		result.xy = OrthoShadowCascadeScale[cascadeIndex].xy;
		return result;
	} else {
		return 1.0.xxxx;
	}
}

float4 ShadowProjection_GetMiniProj(uint cascadeIndex, uint cascadeMode)
{
    if (	cascadeMode==SHADOW_CASCADE_MODE_ORTHOGONAL
		&& 	cascadeIndex == OrthoShadowSubProjectionCount)
		return OrthoShadowNearMinimalProjection;
	return ShadowProjection_GetMiniProj_NotNear(cascadeIndex, cascadeMode);
}

#endif
