// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../TechniqueLibrary/Framework/SystemUniforms.hlsl"
#include "../TechniqueLibrary/Framework/MainGeometry.hlsl"
#include "../TechniqueLibrary/Framework/Surface.hlsl"
#include "../TechniqueLibrary/SceneEngine/Vegetation/WindAnim.hlsl"
#include "../TechniqueLibrary/SceneEngine/Vegetation/InstanceVS.hlsl"

VSOutput main(VSInput input)
{
	VSOutput output;
	float3 localPosition = VSIn_GetLocalPosition(input);

	#if GEO_HAS_INSTANCE_ID==1
		float3 objectCentreWorld;
		float3 worldNormal;
		float3 worldPosition = InstanceWorldPosition(input, worldNormal, objectCentreWorld);
	#else
		float3 worldPosition = mul(SysUniform_GetLocalToWorld(), float4(localPosition,1)).xyz;
		float3 objectCentreWorld = float3(SysUniform_GetLocalToWorld()[0][3], SysUniform_GetLocalToWorld()[1][3], SysUniform_GetLocalToWorld()[2][3]);
		float3 worldNormal = LocalToWorldUnitVector(VSIn_GetLocalNormal(input));
	#endif

	#if OUTPUT_COLOUR==1
		output.colour 		= VSIn_GetColour(input);
	#endif

	#if OUTPUT_TEXCOORD==1
		output.texCoord 	= VSIn_GetTexCoord(input);
	#endif

	#if GEO_HAS_TEXTANGENT==1
		TangentFrameStruct worldSpaceTangentFrame = VSIn_GetWorldTangentFrame(input);

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

	worldPosition = PerformWindBending(worldPosition, worldNormal, objectCentreWorld, float3(1,0,0), VSIn_GetColour(input).rgb);

	output.position = mul(SysUniform_GetWorldToClip(), float4(worldPosition,1));

	#if OUTPUT_WORLD_POSITION==1
		output.worldPosition = worldPosition;
	#endif

	#if OUTPUT_LOCAL_TANGENT_FRAME==1
		output.localTangent = VSIn_GetLocalTangent(input);
		output.localBitangent = VSIn_GetLocalBitangent(input);
	#endif

	#if (OUTPUT_LOCAL_NORMAL==1)
		output.localNormal = VSIn_GetLocalNormal(input);
	#endif

	#if OUTPUT_LOCAL_VIEW_VECTOR==1
		output.localViewVector = SysUniform_GetLocalSpaceView().xyz - localPosition.xyz;
	#endif

	#if OUTPUT_WORLD_VIEW_VECTOR==1
		output.worldViewVector = SysUniform_GetWorldSpaceView().xyz - worldPosition.xyz;
	#endif

	#if (OUTPUT_PER_VERTEX_AO==1)
		output.ambientOcclusion =  1.f;
		#if (GEO_HAS_PER_VERTEX_AO==1)
			output.ambientOcclusion = input.ambientOcclusion;
		#endif
	#endif

	#if (OUTPUT_PER_VERTEX_MLO==1)
		output.mainLightOcclusion =  1.f;
		#if (GEO_HAS_INSTANCE_ID==1)
			output.mainLightOcclusion *= GetInstanceShadowing(input);
		#endif
	#endif

	return output;
}
