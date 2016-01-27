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
#include "../TextureAlgorithm.h"		// for SystemInputs
#include "../Lighting/LightDesc.h"		// for LightScreenDest

//////////////////////////////////////////////////////////////////

float3 BuildInterpolator_WORLDPOSITION(VSInput input)
{
	#if defined(GEO_PRETRANSFORMED)
		return VSIn_GetLocalPosition(input).xyz;
	#else
		float3 localPosition = VSIn_GetLocalPosition(input);
		return mul(LocalToWorld, float4(localPosition,1)).xyz;
	#endif
}

float4 BuildInterpolator_COLOR(VSInput input)
{
	#if (MAT_VCOLOR_IS_ANIM_PARAM!=1)
		return VSIn_GetColour(input);
	#else
		return 1.0.xxxx;
	#endif
}

float2 BuildInterpolator_TEXCOORD(VSInput input) { return VSIn_GetTexCoord(input); }

float4 BuildInterpolator_COLOR0(VSInput input) { return BuildInterpolator_COLOR(input); }
float2 BuildInterpolator_TEXCOORD0(VSInput input) { return BuildInterpolator_TEXCOORD(input); }

float3 BuildInterpolator_WORLDVIEWVECTOR(VSInput input)
{
	return WorldSpaceView.xyz - BuildInterpolator_WORLDPOSITION(input);
}

float4 BuildInterpolator_LOCALTANGENT(VSInput input)
{
	return VSIn_GetLocalTangent(input);
}

float3 BuildInterpolator_LOCALBITANGENT(VSInput input)
{
	return VSIn_GetLocalBitangent(input);
}

VSOutput BuildInterpolator_VSOutput(VSInput input) : NE_WritesVSOutput
{
	VSOutput output;
	float3 localPosition = VSIn_GetLocalPosition(input);
	float3 worldPosition = BuildInterpolator_WORLDPOSITION(input);
	float3 worldNormal = LocalToWorldUnitVector(VSIn_GetLocalNormal(input));

	#if OUTPUT_COLOUR==1
		output.colour = BuildInterpolator_COLOR0(input);
	#endif

	#if OUTPUT_TEXCOORD==1
		output.texCoord = VSIn_GetTexCoord(input);
	#endif

	#if GEO_HAS_TANGENT_FRAME==1
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

	#if defined(GEO_PRETRANSFORMED)
		output.position = float4(VSIn_GetLocalPosition(input).xyz, 1);
	#else
		output.position = mul(WorldToClip, float4(worldPosition,1));
	#endif

	#if OUTPUT_LOCAL_TANGENT_FRAME==1
		output.localTangent = BuildInterpolator_LOCALTANGENT(input);
		output.localBitangent = BuildInterpolator_LOCALBITANGENT(input);
	#endif

	#if (OUTPUT_LOCAL_NORMAL==1)
		output.localNormal = VSIn_GetLocalNormal(input);
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

//////////////////////////////////////////////////////////////////

#if SHADER_NODE_EDITOR_CHART==1
	float2 NormInterp(float2 a) { return 0.5.xx + 0.5f * a.xx; }
#else
	float2 NormInterp(float2 a) { return 0.5.xx + 0.5f * a; }
#endif

float InterpolateVariable_AutoAxis(float lhs, float rhs, float2 a)
{
	return lerp(lhs.x, rhs.x, NormInterp(a).x);
}

float2 InterpolateVariable_AutoAxis(float2 lhs, float2 rhs, float2 a)
{
	return float2(lerp(lhs.x, rhs.x, NormInterp(a).x), lerp(lhs.y, rhs.y, NormInterp(a).y));
}

float3 InterpolateVariable_AutoAxis(float3 lhs, float3 rhs, float2 a)
{
	return float3(lerp(lhs.x, rhs.x, NormInterp(a).x), lerp(lhs.y, rhs.y, NormInterp(a).y), lerp(lhs.z, rhs.z, NormInterp(a).x));
}

float4 InterpolateVariable_AutoAxis(float4 lhs, float4 rhs, float2 a)
{
	return float4(lerp(lhs.x, rhs.x, NormInterp(a).x), lerp(lhs.y, rhs.y, NormInterp(a).y), lerp(lhs.z, rhs.z, NormInterp(a).x), lerp(lhs.w, rhs.w, NormInterp(a).y));
}

float  InterpolateVariable_XAxis(float  lhs, float  rhs, float2 a) { return lerp(lhs, rhs, NormInterp(a).x); }
float2 InterpolateVariable_XAxis(float2 lhs, float2 rhs, float2 a) { return lerp(lhs, rhs, NormInterp(a).x); }
float3 InterpolateVariable_XAxis(float3 lhs, float3 rhs, float2 a) { return lerp(lhs, rhs, NormInterp(a).x); }
float4 InterpolateVariable_XAxis(float4 lhs, float4 rhs, float2 a) { return lerp(lhs, rhs, NormInterp(a).x); }
float  InterpolateVariable_YAxis(float  lhs, float  rhs, float2 a) { return lerp(lhs, rhs, NormInterp(a).y); }
float2 InterpolateVariable_YAxis(float2 lhs, float2 rhs, float2 a) { return lerp(lhs, rhs, NormInterp(a).y); }
float3 InterpolateVariable_YAxis(float3 lhs, float3 rhs, float2 a) { return lerp(lhs, rhs, NormInterp(a).y); }
float4 InterpolateVariable_YAxis(float4 lhs, float4 rhs, float2 a) { return lerp(lhs, rhs, NormInterp(a).y); }

float3 BuildRefractionNormal(VSOutput geo, SystemInputs sys) { return -GetNormal(geo); }
float3 BuildRefractionIncident(VSOutput geo, SystemInputs sys) { return normalize(float3(1.0f, 0.33f, 0.0f)); }
float3 BuildRefractionOutgoing(VSOutput geo, SystemInputs sys)
{
	float3 worldSpacePosition = GetNormal(geo);
	return normalize(WorldSpaceView.xyz - worldSpacePosition);
}

#endif
