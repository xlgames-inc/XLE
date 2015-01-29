// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(SHADOW_PROJECTION_H)
#define SHADOW_PROJECTION_H

static const uint ShadowMaxProjections = 6;

cbuffer ShadowProjection
{
	uint ProjectionCount;
	float4 ShadowProjRatio[ShadowMaxProjections];
	row_major float4x4 ShadowProjection[ShadowMaxProjections];
}

cbuffer ScreenToShadowProjection
{
    row_major float4x4 CameraToShadow[ShadowMaxProjections];
    float4 OriginalProjectionScale;
    row_major float4x4 CameraToWorld;
}

float4 ShadowProjection_GetOutput(VSInput geo, uint projectIndex)
{
	return mul(ShadowProjection[projectIndex], float4(geo.position,1));
}

float4 ShadowProjection_GetOutput(float3 position, uint projectIndex)
{
	return mul(ShadowProjection[projectIndex], float4(position,1));
}


#endif
