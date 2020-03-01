// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(SURFACE_ALGORITHM_H)
#define SURFACE_ALGORITHM_H

#include "../Framework/SystemUniforms.hlsl"

struct TangentFrameStruct
{
    float3 tangent, bitangent, normal;
    float handiness;
};

TangentFrameStruct BuildTangentFrame(float3 tangent, float3 bitangent, float3 normal, float handiness)
{
    TangentFrameStruct result;
    result.tangent = tangent;
    result.bitangent = bitangent;
    result.normal = normal;
    result.handiness = handiness;
    return result;
}

float GetWorldTangentFrameHandiness(float4 tangent)
{
	#if LOCAL_TO_WORLD_HAS_FLIP==1
		return sign(-tangent.w);
	#else
		return sign(tangent.w);
	#endif
}

float3 NormalFromTangents(float3 tangent, float3 bitangent, float handiness)
{
		// Note -- the order of this cross product
		// 		depends on whether we have a right handed or
		//		left handed tangent frame. That should depend
		//		on how the tangent frame is generated, and
		//		whether there is a flip on any transforms applied.
	return cross(bitangent, tangent) * handiness;
}

float3 SampleNormalMap(Texture2D normalMap, SamplerState samplerObject, bool dxtFormatNormalMap, float2 texCoord)
{
	if (dxtFormatNormalMap) {
		float3 rawNormal = normalMap.Sample(samplerObject, texCoord).xyz * 2.f - 1.0.xxx;
            // note... we may have to do a normalize here; because the normal can
            // become significantly denormalized after filtering.
        rawNormal = normalize(rawNormal);
        return rawNormal;
    } else {
		float2 result = normalMap.Sample(samplerObject, texCoord).xy * 2.f - 1.0.xx;

            // The following seems to give the best results on the "nyra" model currently...
            // It seems that maybe that model is using a wierd coordinate scheme in the normal map?
        float2 coordTwiddle = float2(result.x, result.y);
        #if (MAT_HACK_TWIDDLE_NORMAL_MAP==1)
            coordTwiddle = float2(result.x, -result.y);
        #endif
		return float3(coordTwiddle, sqrt(saturate(1.f + dot(result.xy, -result.xy))));
        // return normalize(float3(coordTwiddle, 1.f - saturate(dot(result.xy, result.xy))));
    }
}

float3 NormalMapAlgorithm(  Texture2D normalMap, SamplerState samplerObject, bool dxtFormatNormalMap,
                            float2 texCoord, TangentFrameStruct tangentFrame)
{
    row_major float3x3 normalsTextureToWorld = float3x3(tangentFrame.tangent.xyz, tangentFrame.bitangent, tangentFrame.normal);
    float3 normalTextureSample = SampleNormalMap(normalMap, samplerObject, dxtFormatNormalMap, texCoord);
		// Note -- matrix multiply opposite from typical
        //          (so we can initialise normalsTextureToWorld easily)
	return mul(normalTextureSample, normalsTextureToWorld);
    //return
    //      normalTextureSample.x * tangentFrame.tangent.xyz
    //    + normalTextureSample.y * tangentFrame.bitangent
    //    + normalTextureSample.z * tangentFrame.normal;
}

void AlphaTestAlgorithm(Texture2D textureObject, SamplerState samplerObject,
                        float2 texCoord, float alphaThreshold)
{
	if (textureObject.Sample(samplerObject, texCoord).a < alphaThreshold) {
		clip(-1);
	}
}

float3x3 GetLocalToWorldUniformScale()
{
        // note -- here, we assume that local-to-world doesn't have a nonuniform
        // scale.
	return (float3x3)SysUniform_GetLocalToWorld();
}

float3 LocalToWorldUnitVector(float3 localSpaceVector)
{
    float3 result = mul(GetLocalToWorldUniformScale(), localSpaceVector);
    #if !defined(NO_SCALE)
        result = normalize(result); // store scale value in constant...?
    #endif
    return result;
}

float3x3 AutoCotangentFrame(float3 inputNormal, float3 negativeViewVector, float2 texCoord)
{
		// get edge vectors of the pixel triangle
	float3 dp1	= ddx(negativeViewVector);
	float3 dp2	= ddy(negativeViewVector);
	float2 duv1	= ddx(texCoord);
	float2 duv2	= ddy(texCoord);

		// solve the linear system
	float3 dp2perp = cross(dp2, inputNormal);
	float3 dp1perp = cross(inputNormal, dp1);
	float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

		// construct a scale-invariant frame
	float invmax = rsqrt( max( dot(T,T), dot(B,B) ) );
	return float3x3(T * invmax, B * invmax, inputNormal);
}

#endif
