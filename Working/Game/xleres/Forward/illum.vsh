// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Transform.h"
#include "../MainGeometry.h"
#include "../Surface.h"
#include "../TransformAlgorithm.h"

	// HACK -- disabling this fog functionality for the time being
#if OUTPUT_FOG_COLOR == 1
	// #include "../../xleres_cry/Lighting/Atmosphere.h"
#endif

#if GEO_HAS_INSTANCE_ID==1
	StructuredBuffer<float4> InstanceOffsets : register(t15);
#endif

VSOutput main(VSInput input)
{
	VSOutput output;
	float3 localPosition	= GetLocalPosition(input);

	#if GEO_HAS_INSTANCE_ID==1
		float3 worldPosition = InstanceOffsets[input.instanceId] + localPosition;
	#else
		float3 worldPosition = mul(LocalToWorld, float4(localPosition,1)).xyz;
	#endif

	output.position			= mul(WorldToClip, float4(worldPosition,1));
	
	#if OUTPUT_COLOUR==1
		output.colour = GetColour(input);
	#endif
	
	#if OUTPUT_TEXCOORD==1
		output.texCoord = GetTexCoord(input);
	#endif

	#if GEO_HAS_TANGENT_FRAME==1
		TangentFrameStruct worldSpaceTangentFrame = BuildWorldSpaceTangentFrame(input);
		
		#if OUTPUT_TANGENT_FRAME==1
			output.tangent 		= worldSpaceTangentFrame.tangent;
			output.bitangent 	= worldSpaceTangentFrame.bitangent;
		#endif
		
		#if (OUTPUT_TANGENT_FRAME==1) || (OUTPUT_NORMAL==1)
			output.normal 		= worldSpaceTangentFrame.normal;
		#endif

	#elif (OUTPUT_NORMAL==1)  

		output.normal = mul(GetLocalToWorldUniformScale(), GetLocalNormal(input));

	#endif

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

	#if OUTPUT_WORLD_POSITION==1
		output.worldPosition = worldPosition.xyz;
	#endif

	#if OUTPUT_FOG_COLOR == 1
		// output.fogColor = CalculateFog(worldPosition.z, WorldSpaceView - worldPosition, NegativeDominantLightDirection);
		output.fogColor = 0.0.xxxx;
	#endif

	#if (OUTPUT_PER_VERTEX_AO==1) && (GEO_HAS_INSTANCE_ID==1)
		output.ambientOcclusion = InstanceOffsets[input.instanceId].w;
	#endif
	
	return output;
}
