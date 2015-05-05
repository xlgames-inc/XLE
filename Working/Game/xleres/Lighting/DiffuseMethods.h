// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(DIFFUSE_METHODS_H)
#define DIFFUSE_METHODS_H

#if !defined(DIFFUSE_METHOD)
    #define DIFFUSE_METHOD 1
#endif

float Sq(float x) { return x*x; }
float RaiseTo5(float x) { float x2 = x*x; return x2*x2*x; }

float DiffuseMethod_Disney(float3 normal, float3 viewDirection, float3 negativeLightDirection, float roughness)
{
		// This is the Disney diffuse model:
		//		http://disney-animation.s3.amazonaws.com/library/s2012_pbs_disney_brdf_notes_v2.pdf
    //
    // The model is designed to flatten out the diffuse reponse (like Oren-Nayar and Hanrahan-Krueger),
    // while also giving what Disney calls "a retroreflective peak".
    // It's quite nice and also cheaper than Oren-Nayar!
    // It uses 2 fresnel calculations to approximate the focusing of light as it enters and
    // exits the surface.
    //
    // The same "roughness" parameter that is used for specular, is also used there. Higher
    // roughness values should result in a more flattened look (like the Oren-Nayar roughness).

	float cosThetaL = dot(normal, negativeLightDirection);
	float cosThetaV = dot(normal, viewDirection);
	float3 halfVector = lerp(negativeLightDirection, viewDirection, .5f);
  // float3 halfVector = normalize(negativeLightDirection + viewDirection);
	float cosThetaD = dot(halfVector, negativeLightDirection);
	float FD90 = .5f + 2.f * cosThetaD * cosThetaD * roughness;

	return max(0.f, (1.f + (FD90 - 1.f) * RaiseTo5(1.f - cosThetaL)) * (1.f + (FD90 - 1.f) * RaiseTo5(1.f - cosThetaV)) / pi);
}

float DiffuseMethod_Lambert(float3 normal, float3 negativeLightDirection)
{
	return saturate(dot(negativeLightDirection, normal));
}

//////////////////////////////////////////////////////////////////////////
    //   E N T R Y   P O I N T                                      //
//////////////////////////////////////////////////////////////////////////

struct DiffuseParameters
{
    float roughness;
};

DiffuseParameters DiffuseParameters_Roughness(float roughness)
{
  DiffuseParameters result;
  result.roughness = roughness;
  return result;
}

float CalculateDiffuse( float3 normal, float3 viewDirection,
                        float3 negativeLightDirection,
                        DiffuseParameters parameters)
{
    #if DIFFUSE_METHOD==0
        return DiffuseMethod_Lambert(
            normal, negativeLightDirection);
    #elif DIFFUSE_METHOD==1
        return DiffuseMethod_Disney(
            normal, viewDirection, negativeLightDirection,
            parameters.roughness);
    #endif
}

#endif
