// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(DIRECTIONAL_RESOLVE_H)
#define DIRECTIONAL_RESOLVE_H

#include "SpecularMethods.h"
#include "DiffuseMethods.h"
#include "LightDesc.h"

#include "materialquery.h"
#include "../gbuffer.h"
#include "../Utility/MathConstants.h"

float3 LightResolve_Diffuse_NdotL(
	GBufferValues sample,
	float3 directionToEye,
	float3 negativeLightDirection,
	float NdotL,
	LightDesc light)
{
		// If we use specular light from both sides, we must also use diffuse from both sides
		// Otherwise we can get situations where a pixel gets specular light, but no diffuse
		//		-- which ends up appearing gray for diaeletrics
	#if MAT_DOUBLE_SIDED_LIGHTING
		NdotL = abs(NdotL);
	#else
		NdotL = saturate(NdotL);
	#endif

	float rawDiffuse = CalculateDiffuse(
		sample.worldSpaceNormal, directionToEye, negativeLightDirection,
		DiffuseParameters_Roughness(Material_GetRoughness(sample), light.DiffuseWideningMin, light.DiffuseWideningMax));

    float metal = Material_GetMetal(sample);
	float result = Material_GetDiffuseScale(sample) * rawDiffuse * (1.0f - metal);
	result *= sample.cookedLightOcclusion;
	result *= NdotL;
	return result * light.Diffuse.rgb * sample.diffuseAlbedo.rgb;
}

float3 LightResolve_Diffuse(
	GBufferValues sample,
	float3 directionToEye,
	float3 negativeLightDirection,
	LightDesc light)
{
	float NdotL = dot(sample.worldSpaceNormal, negativeLightDirection);
	return LightResolve_Diffuse_NdotL(sample, directionToEye, negativeLightDirection, NdotL, light);
}

float3 LightResolve_Specular(
	GBufferValues sample,
	float3 directionToEye,
	float3 negativeLightDirection,
	LightDesc light,
	float screenSpaceOcclusion = 1.f,
	bool mirrorSpecular = false)
{
		// HACK! preventing problems at very low roughness values
	float roughnessValue = max(0.03f, Material_GetRoughness(sample));

		////////////////////////////////////////////////

		// In our "metal" lighting model, sample.diffuseAlbedo actually contains
		// per-wavelength F0 values.
	float3 metalF0 = sample.diffuseAlbedo;
	float3 F0_0 = lerp(Material_GetF0_0(sample).xxx, metalF0, Material_GetMetal(sample));

	SpecularParameters param0 = SpecularParameters_RoughF0Transmission(
		roughnessValue, F0_0, sample.transmission);

	// todo -- 	Consider not normalizing the half vector for lower quality modes
	//			we could also consider calculating the half vector at a lower
	//			granularity (particularly for distant objects). Calculating on
	//			a per-vertex level might not be beneficial in the long run, but
	//			perhaps on a per-object level for distant objects and distant lights...?
	float3 halfVector = normalize(negativeLightDirection + directionToEye);
	float3 spec0 = CalculateSpecular(
		sample.worldSpaceNormal, directionToEye,
		negativeLightDirection, halfVector,
		param0);

	float specularOcclusion = screenSpaceOcclusion * sample.cookedLightOcclusion;
	const bool viewDependentOcclusion = true;
	if (viewDependentOcclusion) {
		float NdotV = dot(sample.worldSpaceNormal, directionToEye);
		#if MAT_DOUBLE_SIDED_LIGHTING
			NdotV *= sign(dot(sample.worldSpaceNormal, negativeLightDirection));
		#endif
		specularOcclusion = TriAceSpecularOcclusion(NdotV, specularOcclusion);
	}

	// float norm = 1.f / (pi * sample.material.roughness * sample.material.roughness);
	float norm = 1.f;

	// note -- specular occlusion is going to apply to both reflected and transmitted specular
	return spec0 * (specularOcclusion * norm) * light.Specular;
}

#endif
