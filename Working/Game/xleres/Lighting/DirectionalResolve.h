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

float3 CalculateHt(float3 i, float3 o, float iorIncident, float iorOutgoing)
{
	return -normalize(iorIncident * i + iorOutgoing * o);
}

#include "Testing/WalterTrans.sh"

float3 LightResolve_Diffuse_NdotL(
	GBufferValues sample,
	float3 directionToEye,
	float3 negativeLightDirection,
	float NdotL,
	LightDesc light)
{
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
	float NdotL = saturate(dot(sample.worldSpaceNormal, negativeLightDirection));
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
		// Getting lots of problems with specular for normals pointing away
		// from the camera. We have to avoid this case.
	float NdotV = dot(sample.worldSpaceNormal, directionToEye);
	if (NdotV < 0.f) return 0.0.xxx;

		// HACK! preventing problems at very low roughness values
	float roughnessValue = max(0.03f, Material_GetRoughness(sample));

		////////////////////////////////////////////////

		// In our "metal" lighting model, sample.diffuseAlbedo actually contains
		// per-wavelength F0 values.
	float3 metalF0 = sample.diffuseAlbedo;
	float3 F0_0 = lerp(Material_GetF0_0(sample).xxx, metalF0, Material_GetMetal(sample));

	SpecularParameters param0 = SpecularParameters_RoughF0(roughnessValue, F0_0);

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
	spec0 = saturate(spec0);

	float specularOcclusion = screenSpaceOcclusion * sample.cookedLightOcclusion;
	const bool viewDependentOcclusion = true;
	if (viewDependentOcclusion) {
		specularOcclusion = TriAceSpecularOcclusion(NdotV, specularOcclusion);
	}

	// Calculate specular light transmitted through
	// For most cases of transmission, there should actually be 2 interfaces
	//		-- 	when the light enters the material, and when it exits it.
	// The light will bend at each interface. So, to calculate the refraction
	// properly, we really need to know the thickness of the object. That will
	// determine how much the light actually bends. If we know the thickness,
	// we can calculate an approximate bending due to refaction. But for now, ignore
	// thank.
	//
	// It may be ok to consider the microfacet distribution only on a single
	// interface.
	// Walter's implementation is based solving for a surface pointing away from
	// the camera. And, as he mentions in the paper, the higher index of refraction
	// should be inside the material (ie, on the camera side). Infact, this is
	// required to get the transmission half vector, ht, pointing in the right direction.
	// So, in effect we're solving for the microfacets on an imaginary back face where
	// the light first entered the object on it's way to the camera.
	//
	// In theory, we could do the fresnel calculation for r, g & b separately. But we're
	// just going to ignore that and only do a single channel. This might produce an
	// incorrect result for metals; but why would we get a large amount of transmission
	// through metals?
	float refracted = 0.f;
	const float iorIncident = 1.f;
	const float iorOutgoing = 1.33f;
	WalterTrans(
		param0.F0.g, iorIncident, iorOutgoing, param0.roughness,
		negativeLightDirection.xyz, directionToEye.xyz, -sample.worldSpaceNormal.xyz,
		refracted);
	refracted = saturate(refracted);

	// float norm = 1.f / (pi * sample.material.roughness * sample.material.roughness);
	float norm = 1.f;
	return (spec0 * specularOcclusion * norm + refracted) * light.Specular;
}

#endif
