// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Transform.h"
#include "../MainGeometry.h"
#include "../Surface.h"
#include "../Vegetation/WindAnim.h"
#include "../Vegetation/InstanceVS.h"

VSOutput main(VSInput input)
{
	VSOutput output;
	float3 localPosition = GetLocalPosition(input);

	#if GEO_HAS_INSTANCE_ID==1
		float3 objectCentreWorld;
		float3 worldPosition = InstanceWorldPosition(input, objectCentreWorld);
	#else
		float3 worldPosition = mul(LocalToWorld, float4(localPosition,1)).xyz;
		float3 objectCentreWorld = float3(LocalToWorld[0][3], LocalToWorld[1][3], LocalToWorld[2][3]);
	#endif

	#if OUTPUT_COLOUR==1
		output.colour 		= GetColour(input);
	#endif

	#if OUTPUT_TEXCOORD==1
		output.texCoord 	= GetTexCoord(input);
	#endif

	float3 worldNormal = LocalToWorldUnitVector(GetLocalNormal(input));
	#if GEO_HAS_TANGENT_FRAME==1
		TangentFrameStruct worldSpaceTangentFrame = BuildWorldSpaceTangentFrame(input);

		#if OUTPUT_TANGENT_FRAME==1
			output.tangent = worldSpaceTangentFrame.tangent;
			output.bitangent = worldSpaceTangentFrame.bitangent;
		#endif

		#if GEO_HAS_NORMAL==0
			worldNormal = worldSpaceTangentFrame.normal;
		#endif
	#endif

	#if (OUTPUT_NORMAL==1)
		output.normal = worldNormal;
	#endif

	worldPosition = PerformWindBending(worldPosition, worldNormal, objectCentreWorld, float3(1,0,0), GetColour(input));

	output.position = mul(WorldToClip, float4(worldPosition,1));

	#if OUTPUT_LOCAL_TANGENT_FRAME==1
		output.localTangent = GetLocalTangent(input);
		output.localBitangent = GetLocalBitangent(input);
	#endif

	#if (OUTPUT_LOCAL_NORMAL==1)
		output.localNormal = GetLocalNormal(input);
	#endif

	#if OUTPUT_LOCAL_VIEW_VECTOR==1
		output.localViewVector = LocalSpaceView.xyz - localPosition.xyz;
	#endif

	#if OUTPUT_WORLD_VIEW_VECTOR==1
		output.worldViewVector = WorldSpaceView.xyz - worldPosition.xyz;
	#endif

	#if (OUTPUT_PER_VERTEX_AO==1) && (GEO_HAS_INSTANCE_ID==1)
		output.ambientOcclusion =  GetInstanceShadowing(input);
	#endif

	return output;
}
