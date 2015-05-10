// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(BASIC_RESOLVE_H)
#define BASIC_RESOLVE_H

#include "SpecularMethods.h"
#include "DiffuseMethods.h"
#include "LightDesc.h"

#include "materialquery.h"
#include "../gbuffer.h"

float3 LightResolve_Diffuse(
	GBufferValues sample,
	float3 directionToEye,
	float3 negativeLightDirection,
	LightColors lightColor,
    float wideningMin, float wideningMax)
{
	float rawDiffuse = CalculateDiffuse(
		sample.worldSpaceNormal, directionToEye, negativeLightDirection,
		DiffuseParameters_Roughness(Material_GetRoughness(sample), wideningMin, wideningMax));

    float metal = Material_GetMetal(sample);
	float light = Material_GetDiffuseScale(sample) * rawDiffuse * (1.0f - metal);
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

#endif
