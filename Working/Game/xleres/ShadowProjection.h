// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(SHADOW_PROJECTION_H)
#define SHADOW_PROJECTION_H

#define SHADOW_CASCADE_MODE_ARBITRARY 1
#define SHADOW_CASCADE_MODE_ORTHOGONAL 2

static const uint ShadowMaxSubProjections = 6;

cbuffer ArbitraryShadowProjection
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

cbuffer OrthogonalShadowProjection
{
	row_major float3x4 OrthoShadowWorldToProj;
    float4 OrthoShadowMinimalProjection;
    uint OrthoShadowSubProjectionCount;    
    float3 OrthoShadowCascadeScale[ShadowMaxSubProjections];
    float3 OrthoShadowCascadeTrans[ShadowMaxSubProjections];
}

cbuffer ScreenToShadowProjection
{
    row_major float4x4 CameraToShadow[ShadowMaxSubProjections];
    float4 OriginalProjectionScale;
    row_major float4x4 CameraToWorld;
}

uint GetShadowSubProjectionCount()
{
    #if SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ARBITRARY
		return ShadowSubProjectionCount;
	#elif SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ORTHOGONAL
		return OrthoShadowSubProjectionCount;
	#endif
}

float3 AdjustForCascade(float3 basePosition, uint cascadeIndex)
{
    #if SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ORTHOGONAL
        return float3(	
			basePosition.x * OrthoShadowCascadeScale[cascadeIndex].x,
			basePosition.y * OrthoShadowCascadeScale[cascadeIndex].y,
			basePosition.z * OrthoShadowCascadeScale[cascadeIndex].z)
			+ OrthoShadowCascadeTrans[cascadeIndex];
    #else
        return basePosition;
    #endif
}

float4 ShadowProjection_GetOutput(float3 position, uint cascadeIndex)
{
	#if SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ARBITRARY
        return mul(ShadowWorldToProj[projectIndex], float4(position,1));
    #elif SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ORTHOGONAL
        float3 a = AdjustForCascade(mul(OrthoShadowWorldToProj, Float4(position, 1)));
        return float4(a, 1.f);
    #endif
}

float4 ShadowProjection_GetOutput(VSInput geo, uint cascadeIndex)
{
	return ShadowProjection_GetOutput(geo.position, cascadeIndex);
}


#endif
