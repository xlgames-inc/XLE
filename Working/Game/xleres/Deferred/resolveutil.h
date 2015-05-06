// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(RESOLVE_UTIL_H)
#define RESOLVE_UTIL_H

#include "materialquery.h"
#include "../gbuffer.h"
#include "../Utility/MathConstants.h"
#include "../Colour.h"
#include "../TextureAlgorithm.h"
#include "../System/LoadGBuffer.h"
#include "../Lighting/LightingAlgorithm.h"
#include "../Lighting/SpecularMethods.h"
#include "../Lighting/DiffuseMethods.h"
#include "../TransformAlgorithm.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////

Texture2D_MaybeMS<float>	DepthTexture	 	: register(t4);

float GetLinear0To1Depth(int2 pixelCoords, uint sampleIndex)
{
	return NDCDepthToLinear0To1(LoadFloat1(DepthTexture, pixelCoords.xy, sampleIndex));
}

float GetWorldSpaceDepth(int2 pixelCoords, uint sampleIndex)
{
	return NDCDepthToWorldSpace(LoadFloat1(DepthTexture, pixelCoords.xy, sampleIndex));
}

float3 CalculateWorldPosition(int2 pixelCoords, uint sampleIndex, float3 viewFrustumVector)
{
	return CalculateWorldPosition(
		viewFrustumVector, GetLinear0To1Depth(pixelCoords, sampleIndex),
		WorldSpaceView);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

struct LightColors
{
	float3 diffuse;
	float3 specular;
	float nonMetalSpecularBrightness;
};

cbuffer LightBuffer
{
	float3 		NegativeLightDirection;
	float		LightRadius;
	LightColors LightColor;
	float		LightPower;
	float		DiffuseWideningMin;
	float		DiffuseWideningMax;
}

float3 LightResolve_Diffuse(
	GBufferValues sample,
	float3 directionToEye,
	float3 negativeLightDirection,
	LightColors lightColor)
{
	float rawDiffuse = CalculateDiffuse(
		sample.worldSpaceNormal, directionToEye, negativeLightDirection,
		DiffuseParameters_Roughness(Material_GetRoughness(sample), DiffuseWideningMin, DiffuseWideningMax));

  float metal = Material_GetMetal(sample);
	float light = MO_DiffuseScale * rawDiffuse * (1.0f - metal);
	return light * lightColor.diffuse * sample.diffuseAlbedo.rgb;
}

float3 LightResolve_Specular(
	GBufferValues sample,
	float3 directionToEye,
	float3 negativeLightDirection,
	LightColors lightColor)
{
	float roughnessValue = Material_GetRoughness(sample);

		////////////////////////////////////////////////

	float F0_0 = Material_GetF0_0(sample);
	SpecularParameters param0 = SpecularParameters_RoughF0(roughnessValue, F0_0);
	float spec0 = Material_GetSpecularScale0(sample)
		* CalculateSpecular(
			sample.worldSpaceNormal, directionToEye,
			negativeLightDirection, param0);

		////////////////////////////////////////////////

	// float F0_1 = Material_GetF0_1(sample);
	// float3 specularColor1 = 1.0.xxx;
	// SpecularParameters param1 = SpecularParameters_RoughF0(3.f * roughnessValue, F0_1);
	// float spec1 = Material_GetSpecularScale1(sample)
	// 	* CalculateSpecular(
	// 		sample.worldSpaceNormal, directionToEye,
	// 		negativeLightDirection, param1);

		////////////////////////////////////////////////

	float scale = sample.cookedAmbientOcclusion;
	float3 result =
		(saturate(spec0) * scale)
		* lerp(lightColor.nonMetalSpecularBrightness.xxx * sample.diffuseAlbedo, lightColor.specular, Material_GetMetal(sample));
	// result += (saturate(spec1) * scale) * specularColor1;
	return result;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

static const float attenuationScalar = 1.f; // 4.f  * pi;

float DistanceAttenuation0(float distanceSq)
{
	float attenuationFactor = 4.f * pi * distanceSq;
	float attenuation = 5000000.f / attenuationFactor;
	return attenuation;
}

float PowerForHalfRadius(float halfRadius, float powerFraction)
{
		// attenuation = power / (distanceSq+1);
		// attenuation * (distanceSq+1) = power
		// (power*0.5f) * (distanceSq+1) = power
		// .5f*power = distanceSq+1
		// power = (distanceSq+1) / .5f
	return (attenuationScalar*(halfRadius*halfRadius)+1.f) * (1.f/(1.f-powerFraction));
}

float DistanceAttenuation(float distanceSq, float power)
{
	float attenuation = power / (attenuationScalar*distanceSq+1);
	return attenuation;
}

#endif
