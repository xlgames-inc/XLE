// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(SPECULAR_METHODS_H)
#define SPECULAR_METHODS_H

#include "optimized-ggx.h"
#include "LightingAlgorithm.h"

#if !defined(SPECULAR_METHOD)
    #define SPECULAR_METHOD 1
#endif

    //////////////////////////////////////////////////////////////////////////
        //   C O O K   T O R R E N C E                                  //
    //////////////////////////////////////////////////////////////////////////

float CookTorrenceSpecular(float NdotL, float NdotH, float NdotV, float VdotH, float roughnessSquared)
{
		// using D3D book wiki for reference for Cook-Torrence specular equation:

	float geo_numerator   = 2.0f * NdotH;
    float geo_denominator = VdotH;
 
    float geo_b = (geo_numerator * NdotV) / geo_denominator;
    float geo_c = (geo_numerator * NdotL) / geo_denominator;
    float geo   = min(1.0f, min(geo_b, geo_c));

	float finalRoughness;
	{
			// beckmann distribution
		float roughness_a = 1.0f / ( 4.0f * roughnessSquared * pow( NdotH, 4 ) );
        float roughness_b = NdotH * NdotH - 1.0f;
        float roughness_c = roughnessSquared * NdotH * NdotH;
        finalRoughness = roughness_a * exp( roughness_b / roughness_c );
	}

	float Rs_numerator    = (geo * finalRoughness);
    float Rs_denominator  = NdotV * NdotL;
    float Rs              = Rs_numerator/ Rs_denominator;

	return saturate(NdotL) * Rs;
}

float CalculateSpecular_CookTorrence(float3 normal, float3 viewDirection, float3 negativeLightDirection, float roughness, float F0)
{
    float3 worldSpaceReflection = reflect(-viewDirection, normal);
	float3 halfVector = normalize(negativeLightDirection + viewDirection);
	float fresnel = SchlickFresnelF0(viewDirection, halfVector, F0);

	float NdotL = saturate(dot(negativeLightDirection, normal.xyz));
	float NdotH = saturate(dot(halfVector, normal.xyz));
	float NdotV = saturate(dot(viewDirection, normal.xyz));
	float VdotH = saturate(dot(halfVector, viewDirection));

    return fresnel * CookTorrenceSpecular(NdotL, NdotH, NdotV, VdotH, roughness*roughness);
}

    //////////////////////////////////////////////////////////////////////////
        //   G G X                                                      //
    //////////////////////////////////////////////////////////////////////////

float CalculateSpecular_GGX(float3 normal, float3 viewDirection, float3 negativeLightDirection, float roughness, float F0)
{
    // return LightingFuncGGX_REF(normal, viewDirection, negativeLightDirection, roughness, F0);
    return LightingFuncGGX_OPT5(normal, viewDirection, negativeLightDirection, roughness, F0);
}

    //////////////////////////////////////////////////////////////////////////
        //   E N T R Y   P O I N T                                      //
    //////////////////////////////////////////////////////////////////////////

struct SpecularParameters
{
    float roughness;
    float F0;
};

SpecularParameters SpecularParameters_Init(float roughness, float refractiveIndex)
{
    SpecularParameters result;
    result.roughness = roughness;
    result.F0 = RefractiveIndexToF0(refractiveIndex);
    return result;
}

SpecularParameters SpecularParameters_RoughF0(float roughness, float F0)
{
    SpecularParameters result;
    result.roughness = roughness;
    result.F0 = F0;
    return result;
}

float CalculateSpecular(float3 normal, float3 viewDirection, 
                        float3 negativeLightDirection, 
                        SpecularParameters parameters)
{
    #if SPECULAR_METHOD==0
        return CalculateSpecular_CookTorrence(
            normal, viewDirection, negativeLightDirection, 
            parameters.roughness, parameters.F0);
    #elif SPECULAR_METHOD==1
        return CalculateSpecular_GGX(
            normal, viewDirection, negativeLightDirection, 
            parameters.roughness, parameters.F0);
    #endif
}


#endif


