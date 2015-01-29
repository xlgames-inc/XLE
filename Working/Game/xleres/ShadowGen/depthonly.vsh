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
	float3 localPosition	= GetLocalPosition(input);

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

	uint count = min(ProjectionCount, OUTPUT_SHADOW_PROJECTION_COUNT);
	result.shadowFrustumFlags = 0;
	for (uint c=0; c<count; ++c) {
		float4 p = ShadowProjection_GetOutput(worldPosition, c);
		float2 z = p.xy / p.w;

		bool	left	= z.x < -1.f,
				right	= z.x >  1.f,
				top		= z.y < -1.f,
				bottom	= z.y >  1.f;

		result.shadowPosition[c] = p;
		result.shadowFrustumFlags |= (left | (right<<1) | (top<<2) | (bottom<<3)) << (c*4);
	}

	return result;
}
