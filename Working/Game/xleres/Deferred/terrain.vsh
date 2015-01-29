// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define OUTPUT_TEXCOORD 1
#define OUTPUT_NORMAL 1
#define OUTPUT_COLOUR 0

#include "../Transform.h"
#include "../MainGeometry.h"
#include "../Terrain.h"
#include "../TerrainSurface.h"
#include "../Surface.h"
	
VSOutput main(VSInput input)
{
	VSOutput output;
	float3 localPosition = GetTerrainLocalPosition(input);
	float3 worldPosition = mul(LocalToWorld, float4(localPosition,1));
	#if OUTPUT_WORLDSPACE==1
		output.position		 = float4(worldPosition,1);
	#else
		output.position		 = mul(WorldToClip, float4(worldPosition,1));
	#endif

	output.texCoord 	 = BuildBaseTexCoord(worldPosition.xyz);
		// DavidJ -- note --	we could just use a "SNORM" vertex format instead of this unpack.. 
		//						but it requires changing CPU side code
	output.normal		 = LocalToWorldUnitVector(GetLocalNormal(input));
	#if (OUTPUT_COLOUR==1)
		output.colour 		 = 1.0.xxxx;
	#endif

	#if OUTPUT_WORLD_VIEW_VECTOR==1
		output.worldViewVector = WorldSpaceView.xyz - worldPosition.xyz;
	#endif

	return output;
}
