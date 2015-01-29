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

float3 GetLocalViewVector(VSOutput geo)
{
	#if OUTPUT_LOCAL_VIEW_VECTOR==1
		return geo.localViewVector;
	#else
		return 0.0.xxx;
	#endif
}

float3 GetWorldViewVector(VSOutput geo)
{
	#if OUTPUT_WORLD_VIEW_VECTOR==1
		return geo.worldViewVector;
	#else
		return 0.0.xxx;
	#endif
}


float3 TransformVector_Q(float4 q, float3 inputDirectionVector)
{
        //	transform direction vector by quaternion
		//	quaternion by vector:
		//		t = 2 * cross(q.xyz, v)
		//		v' = v + q.w * t + cross(q.xyz, t)
        //
        //  todo -- check this method... someone haven't problems with it here:
        //      http://stackoverflow.com/questions/22497093/faster-quaternion-vector-multiplication-doesnt-work
    float3 t = 2.f * cross(q.xyz, inputDirectionVector);
	return inputDirectionVector + q.w * t + cross(q.xyz, t);
}

float3 TransformPoint_QT(float2x4 qt, float3 inputPosition)
{
		//	transform position by quaternion + translation
	return qt[1].xyz + TransformVector_Q(qt[0], inputPosition);
}

float3 TransformPositionThroughSkinning(VSInput input, float3 position)
{
    #if GEO_HAS_SKIN_WEIGHTS
        float3 result = 0.0.xxx;
        #if SKIN_TRANSFORMS==SKIN_TRANSFORMS_MATRICES
			
			[unroll] for (uint i=0; i<4; ++i) {
				result += input.boneWeights[i] * mul(BoneTransforms[input.boneIndices[i]], float4(position, 1.f));
			}

		#elif SKIN_TRANSFORMS==SKIN_TRANSFORMS_QT

			[unroll] for (uint i=0; i<4; ++i) {
				result += input.boneWeights[i] * TransformPoint_QT(BoneTransforms[input.boneIndices[i]], position);
			}

		#endif
        return result;
    #else
        return position;
    #endif
}

float3 TransformDirectionVectorThroughSkinning(VSInput input, float3 directionVector)
{
    #if GEO_HAS_SKIN_WEIGHTS
        float3 result = 0.0.xxx;
        #if SKIN_TRANSFORMS==SKIN_TRANSFORMS_MATRICES
			
			[unroll] for (uint i=0; i<4; ++i) {
                    // (assuming no scale!)
                const row_major float3x4 jointTransform = BoneTransforms[input.boneIndices[i]];
                const row_major float3x3 rotationPart = float3x3(jointTransform[0].xyz, jointTransform[1].xyz, jointTransform[2].xyz);
				result += input.boneWeights[i] * mul(rotationPart, directionVector);
			}

		#elif SKIN_TRANSFORMS==SKIN_TRANSFORMS_QT

			[unroll] for (uint i=0; i<4; ++i) {
				result += input.boneWeights[i] * TransformVector_Q(BoneTransforms[input.boneIndices[i]][0], directionVector);
			}

		#endif
        return result;
    #else
        return directionVector;
    #endif
}

float3 GetLocalPosition(VSInput input)
{
	#if GEO_HAS_SKIN_WEIGHTS
		return TransformPositionThroughSkinning(input, input.position.xyz);
	#else
		return input.position.xyz;
	#endif
}

#if (OUTPUT_TANGENT_FRAME==1)
    TangentFrameStruct BuildTangentFrameFromGeo(VSOutput geo)
    {
        TangentFrameStruct result;
        result.tangent = normalize(geo.tangent);
        result.bitangent = normalize(geo.bitangent);
        result.normal = normalize(geo.normal);
        return result;
    }
#endif

#if (OUTPUT_LOCAL_TANGENT_FRAME==1)
    TangentFrameStruct BuildLocalTangentFrameFromGeo(VSOutput geo)
    {
        TangentFrameStruct result;
        result.tangent      = normalize(geo.localTangent.xyz);
        result.bitangent    = normalize(geo.localBitangent);
        #if (OUTPUT_LOCAL_NORMAL)
            result.normal   = normalize(geo.localNormal);
        #else
                // note --  it's possible that the tangent and bitangent could
                //          fall out of alignment during edge interpolation. That
                //          could potentially result in a non-unit length normal
                //          (but it would also result in other subtle artefacts in
                //          the normal map. Let's try to cheat and avoid the normalize,
                //          (and just assume it's close to unit length)
            result.normal   = cross(result.tangent, result.bitangent) * geo.localTangent.w;
        #endif
        return result;
    }
#endif

float4 GetLocalTangent(VSInput input)
{
    #if (GEO_HAS_TANGENT_FRAME==1)
        return float4(TransformDirectionVectorThroughSkinning(input, input.tangent.xyz), input.tangent.w);
    #else
        return 0.0.xxxx;
    #endif
}

float3 GetLocalBitangent(VSInput input)
{
    #if (GEO_HAS_TANGENT_FRAME==1)
        return TransformDirectionVectorThroughSkinning(input, input.bitangent.xyz);
    #else
        return 0.0.xxx;
    #endif
}

float3 GetLocalNormal(VSInput input)
{
    #if GEO_HAS_NORMAL==1
        #if GEO_V_NORMAL_UNSIGNED==1
            return input.normal * 2.0.xxx - 1.0.xxx;
        #else
            return input.normal;
        #endif
    #elif GEO_HAS_TANGENT_FRAME==1
            //  if the tangent and bitangent are unit-length and perpendicular, then we 
            //  shouldn't have to normalize here. Since the inputs are coming from the
            //  vertex buffer, let's assume it's ok
        float4 localTangent = GetLocalTangent(input);
        float3 localBitangent = GetLocalBitangent(input);
        return cross(localTangent.xyz, localBitangent.xyz) * localTangent.w;
    #else
        return float3(0,0,1);
    #endif
}

float3 LocalToWorldUnitVector(float3 localSpaceVector)
{
    float3 result = mul(GetLocalToWorldUniformScale(), localSpaceVector);
    #if !defined(NO_SCALE)
        result = normalize(result); // store scale value in constant...?
    #endif
    return result;
}

#if (GEO_HAS_TANGENT_FRAME==1)
    TangentFrameStruct BuildWorldSpaceTangentFrame(VSInput input)
    {
        	//	If we can guarantee no scale on local-to-world, we can skip normalize of worldtangent/worldbitangent
        float4 localTangent     = GetLocalTangent(input);
		float3 worldTangent 	= LocalToWorldUnitVector(localTangent.xyz);
		float3 worldBitangent 	= LocalToWorldUnitVector(GetLocalBitangent(input));

            //  There's some issues here. If local-to-world has a flip on it, it might flip
            //  the direction we get from the cross product here... That's probably not
            //  what's expected. 
            //  (worldNormal shouldn't need to be normalized, so long as worldTangent 
            //  and worldNormal are perpendicular to each other)
		float3 worldNormal		= cross(worldTangent, worldBitangent) * localTangent.w;
        #if LOCAL_TO_WORLD_HAS_FLIP==1
            worldNormal             = -worldNormal;
        #endif
        return BuildTangentFrame(worldTangent, worldBitangent, worldNormal);
    }
#endif

float3 GetNormal(VSOutput geo)
{
    #if defined(RES_HAS_NORMAL_MAP_DXT)
        bool dxtNormalMap = RES_HAS_NORMAL_MAP_DXT==1;
    #else
        bool dxtNormalMap = false;
    #endif

	#if OUTPUT_TANGENT_FRAME==1
	
		#if (RES_HAS_NORMAL_MAP==1) && (OUTPUT_TEXCOORD==1)
            return NormalMapAlgorithm(
                NormalsTexture, DefaultSampler, dxtNormalMap, 
                geo.texCoord, BuildTangentFrameFromGeo(geo));
		#else
			return normalize(geo.normal);
		#endif

    #elif OUTPUT_LOCAL_TANGENT_FRAME==1

        #if (RES_HAS_NORMAL_MAP==1) && (OUTPUT_TEXCOORD==1)
            float3 localNormal = NormalMapAlgorithm(
                NormalsTexture, DefaultSampler, dxtNormalMap, 
                geo.texCoord, BuildLocalTangentFrameFromGeo(geo));

                // note --  Problems when there is a scale on LocalToWorld here.
                //          There are many objects with uniform scale values, and they require a normalize here.
                //          Ideally we'd have a LocalToWorld matrix with the scale removed,
                //          or at least a "uniform scale" scalar to remove the scaling
            return normalize(mul(GetLocalToWorldUniformScale(), localNormal));
		#else
			return normalize(mul(GetLocalToWorldUniformScale(), BuildLocalTangentFrameFromGeo(geo).normal));
		#endif
		
	#elif (OUTPUT_NORMAL==1) && (RES_HAS_NORMAL_MAP==1) && (OUTPUT_TEXCOORD==1) && (OUTPUT_WORLD_VIEW_VECTOR==1)

	    float3x3 normalsTextureToWorld = AutoCotangentFrame(normalize(geo.normal), GetWorldViewVector(geo), geo.texCoord);
		float3 normalTextureSample = SampleNormalMap(NormalsTexture, DefaultSampler, dxtNormalMap, geo.texCoord);
			// Note -- matrix multiply opposite from normal (so we can initialise normalsTextureToWorld easily)
		return mul(normalTextureSample, normalsTextureToWorld);

    #elif (OUTPUT_LOCAL_NORMAL==1) && (RES_HAS_NORMAL_MAP==1) && (OUTPUT_TEXCOORD==1) && (OUTPUT_LOCAL_VIEW_VECTOR==1)

		float3x3 normalsTextureToWorld = AutoCotangentFrame(normalize(geo.localNormal), GetLocalViewVector(geo), geo.texCoord);
		float3 normalTextureSample = SampleNormalMap(NormalsTexture, DefaultSampler, dxtNormalMap, geo.texCoord);
			// Note -- matrix multiply opposite from normal (so we can initialise normalsTextureToWorld easily)
		return mul(normalTextureSample, normalsTextureToWorld);
	
    #elif (OUTPUT_NORMAL==1)

        return normalize(geo.normal);

	#else
		return 1.0.xxx;
	#endif
}
	
void DoAlphaTest(VSOutput geo)
{
	#if (OUTPUT_TEXCOORD==1) && (MAT_ALPHA_TEST==1)
        AlphaTestAlgorithm(DiffuseTexture, DefaultSampler, geo.texCoord, 0.5f);
	#endif
}

#endif
