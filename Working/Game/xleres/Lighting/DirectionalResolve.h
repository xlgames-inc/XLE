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
	LightDesc light)
{
	float rawDiffuse = CalculateDiffuse(
		sample.worldSpaceNormal, directionToEye, light.NegativeDirection,
		DiffuseParameters_Roughness(Material_GetRoughness(sample), light.DiffuseWideningMin, light.DiffuseWideningMax));

    float metal = Material_GetMetal(sample);
	float result = Material_GetDiffuseScale(sample) * rawDiffuse * (1.0f - metal);
	result *= sample.cookedLightOcclusion;
	return result * light.Color.diffuse * sample.diffuseAlbedo.rgb;
}

float3 LightResolve_Specular(
	GBufferValues sample,
	float3 directionToEye,
	LightDesc light,
	bool mirrorSpecular = false)
{
		// Getting lots of problems with specular for normals pointing away
		// from the camera. We have to avoid this case.
	if (dot(sample.worldSpaceNormal, directionToEye) < 0.f) return 0.0.xxx;

	float roughnessValue = Material_GetRoughness(sample);

		////////////////////////////////////////////////

	float rawDiffuse = CalculateDiffuse(
		sample.worldSpaceNormal, directionToEye, light.NegativeDirection,
		DiffuseParameters_Roughness(Material_GetRoughness(sample), light.DiffuseWideningMin, light.DiffuseWideningMax));

		// note that we have to compenstate for the normalization factor built into
		// the CalculateDiffuse call.
	const float normalizationFactor = recipocalPi;
	rawDiffuse /= normalizationFactor;

		// note -- we have to be careful here, because when using the lambert diffuse
		//		model, "rawDiffuse" already has the 1.0f/pi normalization factor built
		//		in... but CalculateSpecular expects it to just by NdotL. To prevent
		//		problems we need to make sure USE_DISNEY_EQUATOR is disabled when
		//		using lambert diffuse.

	float F0_0 = Material_GetF0_0(sample);

		// in our "metal" lighting model, sample.diffuseAlbedo actually contains
		// per-wavelength F0 values. In theory, we could do the specular calculation
		// 3 times (once for each wavelength)... But it's a bit too crazy. We can
		// make an approximation by taking the average F0 and then scaling by
		// the colour afterwards...
	float3 metalF0 = sample.diffuseAlbedo;
	F0_0 = lerp(F0_0, 0.5f * (metalF0.r + metalF0.g + metalF0.b), Material_GetMetal(sample));

	SpecularParameters param0 = SpecularParameters_RoughF0(roughnessValue, F0_0);
	float spec0 = Material_GetSpecularScale0(sample)
		* CalculateSpecular(
			sample.worldSpaceNormal, directionToEye,
			light.NegativeDirection, param0, rawDiffuse);

		////////////////////////////////////////////////

	// float F0_1 = Material_GetF0_1(sample);
	// float3 specularColor1 = 1.0.xxx;
	// SpecularParameters param1 = SpecularParameters_RoughF0(3.f * roughnessValue, F0_1);
	// float spec1 = Material_GetSpecularScale1(sample)
	// 	* CalculateSpecular(
	// 		sample.worldSpaceNormal, directionToEye,
	// 		negativeLightDirection, param1);

		////////////////////////////////////////////////

	float3 result = (spec0 * sample.cookedLightOcclusion) * light.Color.specular;

		// second part of our metal model approximation (see above)
	result *= lerp(1.0.xxx, sample.diffuseAlbedo, Material_GetMetal(sample));

	// result += (saturate(spec1) * scale) * specularColor1;
	return result;
}

#endif
