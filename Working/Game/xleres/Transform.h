// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(TRANSFORM_H)
#define TRANSFORM_H

#include "Binding.h"

cbuffer GlobalTransform CB_BOUND0_0
{
	row_major float4x4 WorldToClip;
	float4 FrustumCorners[4];
	float3 WorldSpaceView;
    float FarClip;
	float4 MinimalProjection;
    row_major float4x4 CameraBasis;
}

cbuffer LocalTransform CB_BOUND1_1
{
	row_major float3x4 LocalToWorld;
	float3 LocalSpaceView;
	uint2 MaterialGuid;
}

cbuffer GlobalState CB_BOUND0_2
{
	float Time;
	uint GlobalSamplingPassIndex;
	uint GlobalSamplingPassCount;
}

#endif
