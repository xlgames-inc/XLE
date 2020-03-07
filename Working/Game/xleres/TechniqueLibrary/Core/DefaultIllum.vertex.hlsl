// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Framework/SystemUniforms.hlsl"
#include "../Framework/MainGeometry.hlsl"
#include "../Framework/DeformVertex.hlsl"
#include "../Framework/Surface.hlsl"
#include "../SceneEngine/Lighting/resolvefog.hlsl"

VSOUT DefaultIllumVertex(
	DeformedVertex deformedVertex,
	VSIN input)
{
	float3 worldPosition;
	TangentFrame worldSpaceTangentFrame;

	if (deformedVertex.coordinateSpace == 0) {
		worldPosition = mul(SysUniform_GetLocalToWorld(), float4(deformedVertex.position,1)).xyz;
	 	worldSpaceTangentFrame = AsTangentFrame(TransformLocalToWorld(deformedVertex.tangentFrame));
	} else {
		worldPosition = deformedVertex.position;
		worldSpaceTangentFrame = AsTangentFrame(deformedVertex.tangentFrame);
	}

	VSOUT output;
	output.position = mul(SysUniform_GetWorldToClip(), float4(worldPosition,1));

	#if VSOUT_HAS_COLOR>=1
		output.color = VSIN_GetColor0(input);
	#endif

	#if VSOUT_HAS_TEXCOORD>=1
		output.texCoord = VSIN_GetTexCoord0(input);
	#endif

	#if (VSOUT_HAS_NORMAL==1)
		output.normal = worldSpaceTangentFrame.normal;
	#endif

	#if GEO_HAS_TEXTANGENT==1
		#if VSOUT_HAS_TANGENT_FRAME==1
			output.tangent = worldSpaceTangentFrame.tangent;
			output.bitangent = worldSpaceTangentFrame.bitangent;
		#endif
	#endif

	#if VSOUT_HAS_WORLD_POSITION==1
		output.worldPosition = worldPosition;
	#endif

	#if VSOUT_HAS_LOCAL_TANGENT_FRAME==1
		output.localTangent = VSIN_GetLocalTangent(input);
		output.localBitangent = VSIN_GetLocalBitangent(input);
	#endif

	#if (VSOUT_HAS_LOCAL_NORMAL==1)
		output.localNormal = VSIN_GetLocalNormal(input);
	#endif

	#if VSOUT_HAS_LOCAL_VIEW_VECTOR==1
		output.localViewVector = SysUniform_GetLocalSpaceView().xyz - deformedVertex.localPosition.xyz;
	#endif

	#if VSOUT_HAS_WORLD_VIEW_VECTOR==1
		output.worldViewVector = SysUniform_GetWorldSpaceView().xyz - worldPosition.xyz;
	#endif

	#if (VSOUT_HAS_PER_VERTEX_AO==1)
		output.ambientOcclusion = 1.f;
		#if (GEO_HAS_PER_VERTEX_AO==1)
			output.ambientOcclusion = input.ambientOcclusion;
		#endif
	#endif

	#if (VSOUT_HAS_PER_VERTEX_MLO==1)
		output.mainLightOcclusion = 1.f;
		#if (GEO_HAS_INSTANCE_ID==1)
			output.mainLightOcclusion *= GetInstanceShadowing(input);
		#endif
	#endif

	#if VSOUT_HAS_FOG_COLOR == 1
		output.fogColor = ResolveOutputFogColor(worldPosition.xyz, SysUniform_GetWorldSpaceView().xyz);
	#endif

	return output;
}
