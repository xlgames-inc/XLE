// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(BUILD_INTERPOLATORS)
#define BUILD_INTERPOLATORS

#include "../Transform.h"
#include "../MainGeometry.h"
#include "../Surface.h"

//////////////////////////////////////////////////////////////////

float4 BuildInterpolator_SV_Position(VSInput input)
{
	#if defined(INPUT_2D)
		return float4(input.xy, 0, 1);
	#else
		float3 worldPosition = BuildInterpolator_WORLDPOSITION(input);
		return mul(WorldToClip, float4(worldPosition,1));
	#endif
}

float3 BuildInterpolator_WORLDPOSITION(VSInput input)
{
	#if defined(INPUT_2D)
		return float3(input.xy, 0);
	#else
		return mul(LocalToWorld, float4(iPosition,1)).xyz;
	#endif
}

float4 BuildInterpolator_COLOR0(VSInput input)
{
	#if (GEO_HAS_COLOUR==1) && (MAT_VCOLOR_IS_ANIM_PARAM!=1)
		return colour;
	#else
		return 1.0.xxxx;
	#endif
}

float4 BuildInterpolator_WORLDVIEWVECTOR(VSInput input)
{
	return WorldSpaceView.xyz - BuildInterpolator_WORLDPOSITION(input);
}

float4 BuildInterpolator_LOCALTANGENT(VSInput input)
{
	return GetLocalTangent(input);
}

float4 BuildInterpolator_LOCALBITANGENT(VSInput input)
{
	return GetLocalBitangent(input);
}

#endif
