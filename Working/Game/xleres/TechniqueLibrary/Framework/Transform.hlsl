// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(TRANSFORM_H)
#define TRANSFORM_H

#include "../System/Binding.hlsl"

cbuffer GlobalTransform BIND_SEQ_B0
{
	row_major float4x4 WorldToClip;
	float4 FrustumCorners[4];
	float3 WorldSpaceView;
    float FarClip;
	float4 MinimalProjection;
    row_major float4x4 CameraBasis;
}

cbuffer LocalTransform BIND_MAT_B1
{
	row_major float3x4 LocalToWorld;
	float3 LocalSpaceView;
}

cbuffer GlobalState BIND_SEQ_B2
{
	float Time;
	uint GlobalSamplingPassIndex;
	uint GlobalSamplingPassCount;
}

#endif
