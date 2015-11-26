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

float3 LightResolve_Diffuse(
	GBufferValues sample,
	float3 directionToEye,
	float3 negativeLightDirection,
	LightDesc light)
{
	float rawDiffuse = CalculateDiffuse(
		sample.worldSpaceNormal, directionToEye, negativeLightDirection,
		DiffuseParameters_Roughness(Material_GetRoughness(sample), light.DiffuseWideningMin, light.DiffuseWideningMax));

    float metal = Material_GetMetal(sample);
	float result = Material_GetDiffuseScale(sample) * rawDiffuse * (1.0f - metal);
	result *= sample.cookedLightOcclusion;
	return result * light.Color.diffuse * sample.diffuseAlbedo.rgb;
}

float3 LightResolve_Specular(
	GBufferValues sample,
	float3 directionToEye,
	float3 negativeLightDirection,
	LightDesc light,
	float screenSpaceOcclusion = 1.f,
	bool mirrorSpecular = false)
{
		// Getting lots of problems with specular for normals pointing away
		// from the camera. We have to avoid this case.
	float NdotV = dot(sample.worldSpaceNormal, directionToEye);
	if (NdotV < 0.f) return 0.0.xxx;

	float roughnessValue = Material_GetRoughness(sample);

		////////////////////////////////////////////////

		// In our "metal" lighting model, sample.diffuseAlbedo actually contains
		// per-wavelength F0 values.
	float3 metalF0 = sample.diffuseAlbedo;
	float3 F0_0 = lerp(Material_GetF0_0(sample).xxx, metalF0, Material_GetMetal(sample));

	SpecularParameters param0 = SpecularParameters_RoughF0(roughnessValue, F0_0);
	float3 halfVector = normalize(negativeLightDirection + directionToEye);
	float3 spec0 = CalculateSpecular(
		sample.worldSpaceNormal, directionToEye,
		negativeLightDirection, halfVector,
		param0);

	float specularOcclusion = screenSpaceOcclusion * sample.cookedLightOcclusion;
	const bool viewDependentOcclusion = true;
	if (viewDependentOcclusion) {
		specularOcclusion = TriAceSpecularOcclusion(NdotV, specularOcclusion);
	}

	// float norm = 1.f / (pi * sample.material.roughness * sample.material.roughness);
	float norm = 1.f;

	return (spec0 * specularOcclusion * norm) * light.Color.specular;
}

#endif
