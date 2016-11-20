// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(SURFACE_H)
#define SURFACE_H

#include "CommonResources.h"
#include "MainGeometry.h"
#include "SurfaceAlgorithm.h"
#include "Transform.h"
#include "Animation\SkinTransform.h"
#include "Binding.h"

Texture2D		DiffuseTexture          TEXTURE_BOUND1_0;
Texture2D		NormalsTexture          TEXTURE_BOUND1_1;

	//	Accessors for properties in MainGeometry structs

///////////////////////////////////////////////////////////////////////////////////////////////////
	//		VSInput			//
///////////////////////////////////////////////////////////////////////////////////////////////////

float3 VSIn_GetLocalPosition(VSInput input)
{
	#if GEO_HAS_SKIN_WEIGHTS
		return TransformPositionThroughSkinning(input, input.position.xyz);
	#else
		return input.position.xyz;
	#endif
}

float4 VSIn_GetLocalTangent(VSInput input)
{
    #if (GEO_HAS_TANGENT_FRAME==1)
        return float4(TransformDirectionVectorThroughSkinning(input, input.tangent.xyz), input.tangent.w);
    #else
        return 0.0.xxxx;
    #endif
}

#if GEO_HAS_NORMAL==1
	float3 VSIn_GetLocalNormal(VSInput input)
	{
		#if GEO_V_NORMAL_UNSIGNED==1
            return TransformDirectionVectorThroughSkinning(input, input.normal * 2.0.xxx - 1.0.xxx);
        #else
            return TransformDirectionVectorThroughSkinning(input, input.normal);
        #endif
	}
#endif

float3 VSIn_GetLocalBitangent(VSInput input)
{
	#if (GEO_HAS_BITANGENT==1)
		return TransformDirectionVectorThroughSkinning(input, input.bitangent.xyz);
    #elif (GEO_HAS_TANGENT_FRAME==1) && (GEO_HAS_NORMAL==1)
		float4 tangent = VSIn_GetLocalTangent(input);
		float3 normal = VSIn_GetLocalNormal(input);
		return cross(tangent.xyz, normal) * GetWorldTangentFrameHandiness(tangent);
    #else
        return 0.0.xxx;
    #endif
}

#if GEO_HAS_NORMAL!=1
	float3 VSIn_GetLocalNormal(VSInput input)
	{
	    #if GEO_HAS_TANGENT_FRAME==1
	            //  if the tangent and bitangent are unit-length and perpendicular, then we
	            //  shouldn't have to normalize here. Since the inputs are coming from the
	            //  vertex buffer, let's assume it's ok
	        float4 localTangent = VSIn_GetLocalTangent(input);
	        float3 localBitangent = VSIn_GetLocalBitangent(input);
	        return NormalFromTangents(localTangent.xyz, localBitangent.xyz, GetWorldTangentFrameHandiness(localTangent));
	    #else
	        return float3(0,0,1);
	    #endif
	}
#endif

#if (GEO_HAS_TANGENT_FRAME==1)
    TangentFrameStruct VSIn_GetWorldTangentFrame(VSInput input)
    {
        	//	If we can guarantee no scale on local-to-world, we can skip normalize of worldtangent/worldbitangent
        float4 localTangent     = VSIn_GetLocalTangent(input);
		float3 worldTangent 	= LocalToWorldUnitVector(localTangent.xyz);
		float3 worldBitangent 	= LocalToWorldUnitVector(VSIn_GetLocalBitangent(input));
		float handiness 		= GetWorldTangentFrameHandiness(localTangent);

            //  There's some issues here. If local-to-world has a flip on it, it might flip
            //  the direction we get from the cross product here... That's probably not
            //  what's expected.
            //  (worldNormal shouldn't need to be normalized, so long as worldTangent
            //  and worldNormal are perpendicular to each other)
		#if GEO_HAS_NORMAL==1
			float3 worldNormal	= LocalToWorldUnitVector(VSIn_GetLocalNormal(input));
		#else
			float3 worldNormal  = NormalFromTangents(worldTangent, worldBitangent, handiness);
		#endif
        return BuildTangentFrame(worldTangent, worldBitangent, worldNormal, handiness);
    }
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
	//		VSOutput			//
///////////////////////////////////////////////////////////////////////////////////////////////////

float2 GetTexCoord(VSOutput geo)
{
	#if OUTPUT_TEXCOORD==1 /////////////////////////////////////////////
		return geo.texCoord;
	#else
	    return 0.0.xx;
	#endif //////////////////////////////////////////////////////////////
}

float4 GetColor(VSOutput geo)
{
	#if OUTPUT_COLOUR>=2 ////////////////////////////////////////////////
	    return float4(geo.colour.rgb, 1.f);
	#elif OUTPUT_COLOUR>=1
		return geo.colour;
	#else
	    return 1.0.xxxx;
	#endif //////////////////////////////////////////////////////////////
}

float3 GetWorldViewVector(VSOutput geo)
{
	#if OUTPUT_WORLD_VIEW_VECTOR==1
		return geo.worldViewVector;
	#elif OUTPUT_WORLD_POSITION==1
		return WorldSpaceView.xyz - geo.worldPosition;	// if we have either the world-view-world or world-position it's a bit redundant to have the other
	#else
		return 0.0.xxx;
	#endif
}

float3 GetLocalViewVector(VSOutput geo)
{
	#if OUTPUT_LOCAL_VIEW_VECTOR==1
		return geo.localViewVector;
	#else
		return 0.0.xxx;
	#endif
}

float3 GetWorldPosition(VSOutput geo)
{
	#if OUTPUT_WORLD_POSITION==1
		return geo.worldPosition;
	#elif OUTPUT_WORLD_VIEW_VECTOR==1
		return WorldSpaceView.xyz - geo.worldViewVector;	// if we have either the world-view-world or world-position it's a bit redundant to have the other
	#else
		return 0.0.xxx;
	#endif
}

#if (OUTPUT_TANGENT_FRAME==1)
    TangentFrameStruct GetWorldTangentFrame(VSOutput geo)
    {
        TangentFrameStruct result;
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

#if (OUTPUT_LOCAL_TANGENT_FRAME==1)
    TangentFrameStruct GetLocalTangentFrame(VSOutput geo)
    {
        TangentFrameStruct result;
        result.tangent	 	= normalize(geo.localTangent.xyz);
        result.bitangent    = normalize(geo.localBitangent);
		result.handiness 	= GetWorldTangentFrameHandiness(geo.localTangent);
        #if (OUTPUT_LOCAL_NORMAL)
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

float3 GetVertexNormal(VSOutput geo)
{
	#if OUTPUT_TANGENT_FRAME==1
		return normalize(geo.normal);
	#elif OUTPUT_LOCAL_TANGENT_FRAME==1
		return GetLocalTangentFrame(geo).normal;
	#elif (OUTPUT_NORMAL==1)
		return normalize(geo.normal);
	#else
		return 0.0.xxx;
	#endif
}

float3 SampleDefaultNormalMap(VSOutput geo)
{
	#if defined(RES_HAS_NormalsTexture_DXT)
		bool dxtNormalMap = RES_HAS_NormalsTexture_DXT==1;
	#else
		bool dxtNormalMap = false;
	#endif

	return SampleNormalMap(NormalsTexture, DefaultSampler, dxtNormalMap, GetTexCoord(geo));
}

float3 TransformNormalMapToWorld(float3 normalTextureSample, VSOutput geo)
{
	#if OUTPUT_TANGENT_FRAME==1

		TangentFrameStruct tangentFrame = GetWorldTangentFrame(geo);
		float3x3 normalsTextureToWorld = float3x3(tangentFrame.tangent.xyz, tangentFrame.bitangent, tangentFrame.normal);
		return mul(normalTextureSample, normalsTextureToWorld);

    #elif OUTPUT_LOCAL_TANGENT_FRAME==1

		TangentFrameStruct localTangentFrame = GetLocalTangentFrame(geo);
		float3x3 normalsTextureToLocal = float3x3(localTangentFrame.tangent.xyz, localTangentFrame.bitangent, localTangentFrame.normal);
		float3 localNormal = mul(normalTextureSample, normalsTextureToLocal);

            // note --  Problems when there is a scale on LocalToWorld here.
            //          There are many objects with uniform scale values, and they require a normalize here.
            //          Ideally we'd have a LocalToWorld matrix with the scale removed,
            //          or at least a "uniform scale" scalar to remove the scaling
        return normalize(mul(GetLocalToWorldUniformScale(), localNormal));

	#elif (OUTPUT_NORMAL==1) && ((OUTPUT_WORLD_VIEW_VECTOR==1) || (OUTPUT_WORLD_VIEW_VECTOR==1))

	    float3x3 normalsTextureToWorld = AutoCotangentFrame(normalize(geo.normal), GetWorldViewVector(geo), geo.texCoord);
			// Note -- matrix multiply opposite from normal (so we can initialise normalsTextureToWorld easily)
		return mul(normalTextureSample, normalsTextureToWorld);

    #elif (OUTPUT_LOCAL_NORMAL==1) && (OUTPUT_LOCAL_VIEW_VECTOR==1)

		float3x3 normalsTextureToWorld = AutoCotangentFrame(normalize(geo.localNormal), GetLocalViewVector(geo), geo.texCoord);
			// Note -- matrix multiply opposite from normal (so we can initialise normalsTextureToWorld easily)
		return mul(normalTextureSample, normalsTextureToWorld);

    #elif (OUTPUT_NORMAL==1)

        return normalize(geo.normal);

	#else

		return 0.0.xxx;

	#endif
}

float3 GetNormal(VSOutput geo)
{
	#if (RES_HAS_NormalsTexture==1) && (OUTPUT_TEXCOORD==1)
		return TransformNormalMapToWorld(SampleDefaultNormalMap(geo), geo);
	#elif (OUTPUT_NORMAL==1)
		return normalize(geo.normal);
	#elif OUTPUT_LOCAL_TANGENT_FRAME==1
		return normalize(mul(GetLocalToWorldUniformScale(), GetLocalTangentFrame(geo).normal));
	#else
		return 0.0.xxx;
	#endif
}

void DoAlphaTest(VSOutput geo, float alphaThreshold)
{
	#if (OUTPUT_TEXCOORD==1) && ((MAT_ALPHA_TEST==1)||(MAT_ALPHA_TEST_PREDEPTH==1))
		#if (USE_CLAMPING_SAMPLER_FOR_DIFFUSE==1)
			AlphaTestAlgorithm(DiffuseTexture, ClampingSampler, geo.texCoord, alphaThreshold);
		#else
        	AlphaTestAlgorithm(DiffuseTexture, MaybeAnisotropicSampler, geo.texCoord, alphaThreshold);
		#endif
	#endif
}

#endif
