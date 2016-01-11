// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#undef GEO_HAS_COLOUR

#include "../Transform.h"
#include "../MainGeometry.h"
#include "../TransformAlgorithm.h"
#include "../Surface.h"

VSOutput main(VSInput input)
{
	VSOutput output;
	float3 worldPosition = mul(LocalToWorld, float4(VSIn_GetLocalPosition(input),1));
	output.position		 = mul(WorldToClip, float4(worldPosition,1));

	#if OUTPUT_TEXCOORD==1
		output.texCoord = VSIn_GetTexCoord(input);
	#endif

	return output;
}
