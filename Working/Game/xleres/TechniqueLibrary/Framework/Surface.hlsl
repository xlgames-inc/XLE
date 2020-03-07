// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(SURFACE_H)
#define SURFACE_H

#include "CommonResources.hlsl"
#include "MainGeometry.hlsl"
#include "SystemUniforms.hlsl"
#include "../Math/SurfaceAlgorithm.hlsl"

	//	Accessors for properties in MainGeometry structs

///////////////////////////////////////////////////////////////////////////////////////////////////
	//		VSIN			//
///////////////////////////////////////////////////////////////////////////////////////////////////

float3 VSIN_GetLocalPosition(VSIN input)
{
	return input.position.xyz;
}

float4 VSIN_GetLocalTangent(VSIN input)
{
	#if (GEO_HAS_TEXTANGENT==1)
		return input.tangent.xyzw;
	#else
		return 0.0.xxxx;
	#endif
}

#if GEO_HAS_NORMAL==1
	float3 VSIN_GetLocalNormal(VSIN input)
	{
		return input.normal;
	}
#endif

float3 VSIN_GetLocalBitangent(VSIN input)
{
	#if (GEO_HAS_TEXBITANGENT==1)
		return input.bitangent.xyz;
	#elif (GEO_HAS_TEXTANGENT==1) && (GEO_HAS_NORMAL==1)
		float4 tangent = VSIN_GetLocalTangent(input);
		float3 normal = VSIN_GetLocalNormal(input);
		return cross(tangent.xyz, normal) * GetWorldTangentFrameHandiness(tangent);
	#else
		return 0.0.xxx;
	#endif
}

#if GEO_HAS_NORMAL!=1
	float3 VSIN_GetLocalNormal(VSIN input)
	{
		#if GEO_HAS_TEXTANGENT==1
				//  if the tangent and bitangent are unit-length and perpendicular, then we
				//  shouldn't have to normalize here. Since the inputs are coming from the
				//  vertex buffer, let's assume it's ok
			float4 localTangent = VSIN_GetLocalTangent(input);
			float3 localBitangent = VSIN_GetLocalBitangent(input);
			return NormalFromTangents(localTangent.xyz, localBitangent.xyz, GetWorldTangentFrameHandiness(localTangent));
		#else
			return float3(0,0,1);
		#endif
	}
#endif

#if (GEO_HAS_TEXTANGENT==1)
	TangentFrame VSIN_GetWorldTangentFrame(VSIN input)
	{
			//	If we can guarantee no scale on local-to-world, we can skip normalize of worldtangent/worldbitangent
		float4 localTangent     = VSIN_GetLocalTangent(input);
		float3 worldTangent 	= LocalToWorldUnitVector(localTangent.xyz);
		float3 worldBitangent 	= LocalToWorldUnitVector(VSIN_GetLocalBitangent(input));
		float handiness 		= GetWorldTangentFrameHandiness(localTangent);

			//  There's some issues here. If local-to-world has a flip on it, it might flip
			//  the direction we get from the cross product here... That's probably not
			//  what's expected.
			//  (worldNormal shouldn't need to be normalized, so long as worldTangent
			//  and worldNormal are perpendicular to each other)
		#if GEO_HAS_NORMAL==1
			float3 worldNormal	= LocalToWorldUnitVector(VSIN_GetLocalNormal(input));
		#else
			float3 worldNormal  = NormalFromTangents(worldTangent, worldBitangent, handiness);
		#endif
		return BuildTangentFrame(worldTangent, worldBitangent, worldNormal, handiness);
	}
#endif

CompressedTangentFrame VSIN_GetCompressedTangentFrame(VSIN input)
{
	CompressedTangentFrame result;
	#if GEO_HAS_TEXTANGENT==1
		float4 localTangent = VSIN_GetLocalTangent(input);
		float3 localBitangent = VSIN_GetLocalBitangent(input);
		result.basisVector0 = localTangent.xyz;
		result.basisVector1 = localBitangent;
		result.handiness = localTangent.w;
	#else
		result.basisVector0 = 0.0.xxx;
		result.basisVector1 = 0.0.xxx;
		result.handiness = 0.0;
	#endif
	return result;
}

CompressedTangentFrame TransformLocalToWorld(CompressedTangentFrame inputFrame)
{
	CompressedTangentFrame result;
	result.basisVector0 = LocalToWorldUnitVector(inputFrame.basisVector0);
	result.basisVector1 = LocalToWorldUnitVector(inputFrame.basisVector1);
	#if LOCAL_TO_WORLD_HAS_FLIP==1
		result.handiness = sign(-inputFrame.handiness);
	#else
		result.handiness = sign(inputFrame.handiness);
	#endif
	return result;
}

float3 GetNormal(CompressedTangentFrame inputFrame)
{
	return NormalFromTangents(inputFrame.basisVector0, inputFrame.basisVector1, inputFrame.handiness);
}

TangentFrame AsTangentFrame(CompressedTangentFrame inputFrame)
{
	float3 normal = NormalFromTangents(inputFrame.basisVector0, inputFrame.basisVector1, inputFrame.handiness);
	return BuildTangentFrame(inputFrame.basisVector0, inputFrame.basisVector1, normal, inputFrame.handiness);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
	//		VSOUT			//
///////////////////////////////////////////////////////////////////////////////////////////////////

float2 VSOUT_GetTexCoord0(VSOUT geo)
{
	#if VSOUT_HAS_TEXCOORD>=1 /////////////////////////////////////////////
		return geo.texCoord;
	#else
		return 0.0.xx;
	#endif //////////////////////////////////////////////////////////////
}

float4 VSOUT_GetColor0(VSOUT geo)
{
	#if VSOUT_HAS_COLOR>=2 ////////////////////////////////////////////////
		return float4(geo.color.rgb, 1.f);
	#elif VSOUT_HAS_COLOR>=1
		return geo.color;
	#else
		return 1.0.xxxx;
	#endif //////////////////////////////////////////////////////////////
}

float3 VSOUT_GetWorldViewVector(VSOUT geo)
{
	#if VSOUT_HAS_WORLD_VIEW_VECTOR==1
		return geo.worldViewVector;
	#elif VSOUT_HAS_WORLD_POSITION==1
		return SysUniform_GetWorldSpaceView().xyz - geo.worldPosition;	// if we have either the world-view-world or world-position it's a bit redundant to have the other
	#else
		return 0.0.xxx;
	#endif
}

float3 VSOUT_GetLocalViewVector(VSOUT geo)
{
	#if VSOUT_HAS_LOCAL_VIEW_VECTOR==1
		return geo.localViewVector;
	#else
		return 0.0.xxx;
	#endif
}

float3 VSOUT_GetWorldPosition(VSOUT geo)
{
	#if VSOUT_HAS_WORLD_POSITION==1
		return geo.worldPosition;
	#elif VSOUT_HAS_WORLD_VIEW_VECTOR==1
		return SysUniform_GetWorldSpaceView().xyz - geo.worldViewVector;	// if we have either the world-view-world or world-position it's a bit redundant to have the other
	#else
		return 0.0.xxx;
	#endif
}

#if (VSOUT_HAS_TANGENT_FRAME==1)
	TangentFrame VSOUT_GetWorldTangentFrame(VSOUT geo)
	{
		TangentFrame result;
		result.tangent = geo.tangent.xyz;
		result.bitangent = geo.bitangent;
		result.normal = geo.normal;

		// note -- 	The denormalization caused by per vertex interpolation
		// 			is fairly subtle. We could perhaps skip this on all but
		//			the highest quality modes..?
		//			Also, there are other options:
		//				- higher order interpolation across the triangle using geometry shaders
		//				- using cotangent stuff particularly with derivative maps
		const bool doRenormalize = true;
		if (doRenormalize) {
			result.tangent = normalize(result.tangent);
			result.bitangent = normalize(result.bitangent);
			result.normal = normalize(result.normal);
		}
		result.handiness = 1.f; // (handiness value is lost in this case)
		return result;
	}
#endif

#if (VSOUT_HAS_LOCAL_TANGENT_FRAME==1)
	TangentFrame VSOUT_GetLocalTangentFrame(VSOUT geo)
	{
		TangentFrame result;
		result.tangent	 	= normalize(geo.localTangent.xyz);
		result.bitangent    = normalize(geo.localBitangent);
		result.handiness 	= GetWorldTangentFrameHandiness(geo.localTangent);
		#if (VSOUT_HAS_LOCAL_NORMAL)
			result.normal   = normalize(geo.localNormal);
		#else
				// note --  it's possible that the tangent and bitangent could
				//          fall out of alignment during edge interpolation. That
				//          could potentially result in a non-unit length normal
				//          (but it would also result in other subtle artefacts in
				//          the normal map. Let's try to cheat and avoid the normalize,
				//          (and just assume it's close to unit length)
			result.normal   = NormalFromTangents(result.tangent, result.bitangent, result.handiness);
		#endif
		return result;
	}
#endif

float3 VSOUT_GetVertexNormal(VSOUT geo)
{
	#if VSOUT_HAS_TANGENT_FRAME==1
		return normalize(geo.normal);
	#elif VSOUT_HAS_LOCAL_TANGENT_FRAME==1
		return VSOUT_GetLocalTangentFrame(geo).normal;
	#elif (VSOUT_HAS_NORMAL==1)
		return normalize(geo.normal);
	#else
		return 0.0.xxx;
	#endif
}

#endif
