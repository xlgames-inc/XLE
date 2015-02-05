// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "GeometryConfiguration.h"
#include "../Transform.h"
#include "../MainGeometry.h"
#include "../TransformAlgorithm.h"
#include "../Surface.h"
#include "../ShadowProjection.h"

#if GEO_HAS_INSTANCE_ID==1
	StructuredBuffer<float4> InstanceOffsets : register(t15);
#endif

VSShadowOutput main(VSInput input)
{
	float3 localPosition = GetLocalPosition(input);

	#if GEO_HAS_INSTANCE_ID==1
		// float3 worldPosition = input.instanceOffset.xyz + localPosition;
		float3 worldPosition = InstanceOffsets[input.instanceId] + localPosition;
	#else
		float3 worldPosition = mul(LocalToWorld, float4(localPosition,1)).xyz;
	#endif

	VSShadowOutput result;
	result.position = worldPosition.xyz;

	#if OUTPUT_TEXCOORD==1
		result.texCoord = input.texCoord;
	#endif

	result.shadowFrustumFlags = 0;
	
	uint count = min(GetShadowSubProjectionCount(), OUTPUT_SHADOW_PROJECTION_COUNT);

	#if SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ARBITRARY
///////////////////////////////////////////////////////////////////////////////////////////////////

		for (uint c=0; c<count; ++c) {
			float4 p = ShadowProjection_GetOutput(worldPosition, c);
			bool	left	= p.x < -p.w,
					right	= p.x >  p.w,
					top		= p.y < -p.w,
					bottom	= p.y >  p.w;

			result.shadowPosition[c] = p;
			result.shadowFrustumFlags |= (left | (right<<1) | (top<<2) | (bottom<<3)) << (c*4);
		}

///////////////////////////////////////////////////////////////////////////////////////////////////
	#elif SHADOW_CASCADE_MODE==SHADOW_CASCADE_MODE_ORTHOGONAL
///////////////////////////////////////////////////////////////////////////////////////////////////

		float3 basePosition = mul(OrthoShadowProjection, float4(worldPosition, 1));
		result.baseShadowPosition = basePosition;

		uint count = min(OrthoShadowSubProjectionCount, OUTPUT_SHADOW_PROJECTION_COUNT);
		for (uint c=0; c<count; ++c) {
			float3 cascade = AdjustForCascade(basePosition, c);
			bool	left	= cascade.x < -1.f,
					right	= cascade.x >  1.f,
					top		= cascade.y < -1.f,
					bottom	= cascade.y >  1.f;

			result.shadowPosition[c] = cascade;
			result.shadowFrustumFlags |= 
				(left | (right<<1) | (top<<2) | (bottom<<3)) << (c*4);
		}

///////////////////////////////////////////////////////////////////////////////////////////////////
	#endif

	return result;
}


