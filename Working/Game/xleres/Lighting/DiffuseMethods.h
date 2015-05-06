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

float DiffuseMethod_Disney(
    float3 normal, float3 directionToEye, float3 negativeLightDirection,
    float roughness, float wideningMin, float wideningMax)
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
	float cosThetaV = dot(normal, directionToEye);

	float3 halfVector = lerp(negativeLightDirection, directionToEye, .5f);
	float cosThetaD = max(dot(halfVector, negativeLightDirection), 0);     // (disney calls this the "difference angle")

        // the following factor controls how broad the diffusing effect is
        // at grazing angles. The problem is, with high levels of "roughness"
        // this will end up producing an extra highlight in the opposite direction
        // of the light (eg, in the shadowed area).
        // Disney artists liked it, so they used it. But it seems to cause problems
        // for us, because it also affects the edge of the shadowed area. With basic
        // lambert shading, this edge becomes blurred by the lambert shading falling
        // off. But if that edge is too bright, flaws in the shadowing algorithm can
        // become more obvious.
        // The two factors here, seem to be arbitrarily picked by Disney. So we should
        // make them flexible, and reduce the overall impact slightly.
	float FD90 = lerp(wideningMin, wideningMax, cosThetaD * cosThetaD * roughness);

	float result = (1.f + (FD90 - 1.f) * RaiseTo5(1.f - cosThetaL)) * (1.f + (FD90 - 1.f) * RaiseTo5(1.f - cosThetaV)) / pi;
    return max(result, 0);
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
    float diffuseWideningMin;
    float diffuseWideningMax;
};

DiffuseParameters DiffuseParameters_Roughness(float roughness, float diffuseWideningMin, float diffuseWideningMax)
{
  DiffuseParameters result;
  result.roughness = roughness;
  result.diffuseWideningMin = diffuseWideningMin;
  result.diffuseWideningMax = diffuseWideningMax;
  return result;
}

float CalculateDiffuse( float3 normal, float3 directionToEye,
                        float3 negativeLightDirection,
                        DiffuseParameters parameters)
{
    #if DIFFUSE_METHOD==0
        return DiffuseMethod_Lambert(
            normal, negativeLightDirection);
    #elif DIFFUSE_METHOD==1
        return DiffuseMethod_Disney(
            normal, directionToEye, negativeLightDirection,
            parameters.roughness, parameters.diffuseWideningMin, parameters.diffuseWideningMax);
    #endif
}

#endif
