// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(BUILD_INTERPOLATORS)
#define BUILD_INTERPOLATORS

#include "../Transform.h"
#include "../MainGeometry.h"
#include "../Surface.h"
#include "../Lighting/LightDesc.h"		// for LightScreenDest

//////////////////////////////////////////////////////////////////

float3 BuildInterpolator_WORLDPOSITION(VSInput input)
{
	#if defined(GEO_PRETRANSFORMED)
		return float3(input.xy, 0);
	#else
		float3 localPosition = GetLocalPosition(input);
		return mul(LocalToWorld, float4(localPosition,1)).xyz;
	#endif
}

float4 BuildInterpolator_SV_Position(VSInput input)
{
	#if defined(GEO_PRETRANSFORMED)
		return float4(input.xy, 0, 1);
	#else
		float3 worldPosition = BuildInterpolator_WORLDPOSITION(input);
		return mul(WorldToClip, float4(worldPosition,1));
	#endif
}

float4 BuildInterpolator_COLOR0(VSInput input)
{
	#if (GEO_HAS_COLOUR==1) && (MAT_VCOLOR_IS_ANIM_PARAM!=1)
		return GetColour(input);
	#else
		return 1.0.xxxx;
	#endif
}

float3 BuildInterpolator_WORLDVIEWVECTOR(VSInput input)
{
	return WorldSpaceView.xyz - BuildInterpolator_WORLDPOSITION(input);
}

float4 BuildInterpolator_LOCALTANGENT(VSInput input)
{
	return GetLocalTangent(input);
}

float3 BuildInterpolator_LOCALBITANGENT(VSInput input)
{
	return GetLocalBitangent(input);
}

VSOutput BuildInterpolator_VSOutput(VSInput input) : NE_WritesVSOutput
{
	VSOutput output;
	float3 localPosition = GetLocalPosition(input);
	float3 worldPosition = BuildInterpolator_WORLDPOSITION(input);
	float3 worldNormal = LocalToWorldUnitVector(GetLocalNormal(input));

	#if OUTPUT_COLOUR==1
		output.colour = BuildInterpolator_COLOR0(input);
	#endif

	#if OUTPUT_TEXCOORD==1
		output.texCoord = GetTexCoord(input);
	#endif

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

	output.position = BuildInterpolator_SV_Position(input);

	#if OUTPUT_LOCAL_TANGENT_FRAME==1
		output.localTangent = BuildInterpolator_LOCALTANGENT(input);
		output.localBitangent = BuildInterpolator_LOCALBITANGENT(input);
	#endif

	#if (OUTPUT_LOCAL_NORMAL==1)
		output.localNormal = GetLocalNormal(input);
	#endif

	#if OUTPUT_LOCAL_VIEW_VECTOR==1
		output.localViewVector = LocalSpaceView.xyz - localPosition.xyz;
	#endif

	#if OUTPUT_WORLD_VIEW_VECTOR==1
		output.worldViewVector = BuildInterpolator_WORLDVIEWVECTOR(input);
	#endif

	#if OUTPUT_WORLD_POSITION==1
		output.worldPosition = worldPosition.xyz;
	#endif

	#if OUTPUT_FOG_COLOR == 1
		// output.fogColor = CalculateFog(worldPosition.z, WorldSpaceView - worldPosition, NegativeDominantLightDirection);
		output.fogColor = float4(0.0.xxx, 1.f);
	#endif

	#if (OUTPUT_PER_VERTEX_AO==1) && (GEO_HAS_INSTANCE_ID==1)
		output.ambientOcclusion = 1.f; // GetInstanceShadowing(input);
	#endif

	#if (OUTPUT_INSTANCE_ID==1) && (GEO_HAS_INSTANCE_ID==1)
		output.instanceId = input.instanceId;
	#endif

	return output;
}

//////////////////////////////////////////////////////////////////

LightScreenDest BuildSystem_LightScreenDest(VSOutput input, SystemInputs sys)
{
	return LightScreenDest_Create(int2(input.position.xy), GetSampleIndex(sys));
}

SystemInputs BuildSystem_SystemInputs(VSOutput input, SystemInputs sys)
{
	return sys;
}

#endif
