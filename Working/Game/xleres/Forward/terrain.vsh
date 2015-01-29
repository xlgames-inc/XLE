// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#define GEO_HAS_TEXCOORD 1
#define GEO_HAS_NORMAL 1
#define OUTPUT_TEXCOORD 1
#define OUTPUT_NORMAL 1
#define OUTPUT_COLOUR 0

#include "../Transform.h"
#include "../MainGeometry.h"
#include "../Terrain.h"
#include "../TerrainSurface.h"
#include "../Surface.h"
#include "../Lighting/Atmosphere.h"

VSOutput main(VSInput input)
{
	VSOutput output;
	float3 localPosition = float3(input.position.xy, input.texCoord.x);
	float3 worldPosition = mul(LocalToWorld, float4(localPosition,1));
	output.position		 = mul(WorldToClip, float4(worldPosition,1));
	output.texCoord 	 = BuildBaseTexCoord(worldPosition.xyz);
	output.normal		 = LocalToWorldUnitVector(GetLocalNormal(input));
	#if OUTPUT_COLOUR >= 1
		output.colour 		 = 1.0.xxxx;
	#endif
	#if OUTPUT_FOG_COLOR == 1
		output.fogColor = CalculateFog(worldPosition.z, WorldSpaceView - worldPosition, NegativeDominantLightDirection);
	#endif
	return output;
}
