// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../TechniqueLibrary/Framework/SystemUniforms.hlsl"
#include "../TechniqueLibrary/Framework/MainGeometry.hlsl"
#include "../TechniqueLibrary/Framework/Surface.hlsl"
#include "../TechniqueLibrary/Math/TransformAlgorithm.hlsl"
#include "../TechniqueLibrary/SceneEngine/Vegetation/WindAnim.hlsl"
#include "../TechniqueLibrary/SceneEngine/Vegetation/InstanceVS.hlsl"
#include "resolvefog.hlsl"

VSOUT main(VSIN input)
{
	VSOUT output;
	float3 localPosition	= VSIN_GetLocalPosition(input);

	#if GEO_HAS_INSTANCE_ID==1
		float3 objectCentreWorld;
		float3 worldNormal;
		float3 worldPosition = InstanceWorldPosition(input, worldNormal, objectCentreWorld);
	#else
		float3 worldPosition = mul(SysUniform_GetLocalToWorld(), float4(localPosition,1)).xyz;
		float3 objectCentreWorld = float3(SysUniform_GetLocalToWorld()[0][3], SysUniform_GetLocalToWorld()[1][3], SysUniform_GetLocalToWorld()[2][3]);
		float3 worldNormal = LocalToWorldUnitVector(VSIN_GetLocalNormal(input));
	#endif

	#if VSOUT_HAS_COLOR>=1
		output.color = VSIN_GetColor0(input);
	#endif

	#if VSOUT_HAS_TEXCOORD==1
		output.texCoord = VSIN_GetTexCoord0(input);
	#endif

	#if GEO_HAS_TEXTANGENT==1
		TangentFrameStruct worldSpaceTangentFrame = VSIN_GetWorldTangentFrame(input);

		#if VSOUT_HAS_TANGENT_FRAME==1
			output.tangent = worldSpaceTangentFrame.tangent;
			output.bitangent = worldSpaceTangentFrame.bitangent;
		#endif

		#if GEO_HAS_NORMAL==0
			worldNormal = worldSpaceTangentFrame.normal;
		#endif
	#endif

	float3 worldViewVector = SysUniform_GetWorldSpaceView().xyz - worldPosition.xyz;
	float3 localNormal = VSIN_GetLocalNormal(input);

	// Flip the normal here, if we have to. Note that we only flip the normal, not the
	// tangent/bitangent. This will have a different effect to if we flip the final normal,
	// after reading the normal map. In effect, the shape described by double sided normals
	// maps is made slightly different.
	// Also, we could get some wierd effects on "smooth" shaded geometry with MAT_DOUBLE_SIDED_LIGHTING
	// enabled, because the flipping point will move with the camera.
	#if (MAT_DOUBLE_SIDED_LIGHTING==1)
		if (dot(worldNormal, worldViewVector) < 0.f) {
			worldNormal *= -1.f;
			localNormal *= -1.f;
		}
	#endif

	#if (VSOUT_HAS_NORMAL==1)
		output.normal = worldNormal;
	#endif

	worldPosition = PerformWindBending(worldPosition, worldNormal, objectCentreWorld, float3(1,0,0), VSIN_GetColor0(input).rgb);

	output.position = mul(SysUniform_GetWorldToClip(), float4(worldPosition,1));

	#if VSOUT_HAS_LOCAL_TANGENT_FRAME==1
		output.localTangent = VSIN_GetLocalTangent(input);
		output.localBitangent = VSIN_GetLocalBitangent(input);
	#endif

	#if (VSOUT_HAS_LOCAL_NORMAL==1)
		output.localNormal = localNormal;
	#endif

	#if VSOUT_HAS_LOCAL_VIEW_VECTOR==1
		output.localViewVector = SysUniform_GetLocalSpaceView().xyz - localPosition.xyz;
	#endif

	#if VSOUT_HAS_WORLD_VIEW_VECTOR==1
		output.worldViewVector = worldViewVector;
	#endif

	#if VSOUT_HAS_WORLD_POSITION==1
		output.worldPosition = worldPosition.xyz;
	#endif

	#if VSOUT_HAS_FOG_COLOR == 1
		output.fogColor = ResolveOutputFogColor(worldPosition.xyz, SysUniform_GetWorldSpaceView().xyz);
	#endif

	#if (VSOUT_HAS_PER_VERTEX_MLO==1) && (GEO_HAS_INSTANCE_ID==1)
		output.mainLightOcclusion = GetInstanceShadowing(input);
	#endif

	#if (VSOUT_HAS_INSTANCE_ID==1) && (GEO_HAS_INSTANCE_ID==1)
		output.instanceId = input.instanceId;
	#endif

	return output;
}
