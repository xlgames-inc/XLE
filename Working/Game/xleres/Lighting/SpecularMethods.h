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

float CalculateSpecular_CookTorrence(float3 normal, float3 directionToEye, float3 negativeLightDirection, float roughness, float F0)
{
    float3 worldSpaceReflection = reflect(-directionToEye, normal);
	float3 halfVector = normalize(negativeLightDirection + directionToEye);
	float fresnel = SchlickFresnelF0(directionToEye, halfVector, F0);

	float NdotL = saturate(dot(negativeLightDirection, normal.xyz));
	float NdotH = saturate(dot(halfVector, normal.xyz));
	float NdotV = saturate(dot(directionToEye, normal.xyz));
	float VdotH = saturate(dot(halfVector, directionToEye));

    return fresnel * CookTorrenceSpecular(NdotL, NdotH, NdotV, VdotH, roughness*roughness);
}

    //////////////////////////////////////////////////////////////////////////
        //   G G X                                                      //
    //////////////////////////////////////////////////////////////////////////

float SmithG(float NdotV, float alpha)
{
    // Filmic worlds uses Smith-Schlick implementation.
    // It's a little bit simplier...
    // Epic course notes suggest k = a/2
    // return 1.0f/(dotNV*(1.0f-k)+k);

    float a = alpha * alpha;
    float b = NdotV * NdotV;
    return 1.f/(NdotV + sqrt(lerp(b, 1, a)));
    // return 1.f/(NdotV + sqrt(a + (1.f-a) * b));
    // return 1.f/(NdotV + sqrt(a + b - a*b));
}

float TrowReitzD(float NdotH, float alpha)
{
    // Note that the Disney model generalizes this
    // a little further by making the denomination power
    // variable.
    // They call it "GTR"
    float alphaSqr = alpha * alpha;
    float denom = 1.f + (alphaSqr - 1.f) * NdotH * NdotH;
    return alphaSqr / (pi * denom * denom);
}

float3 ReferenceSpecularGGX(
    float3 normal,
    float3 directionToEye,
    float3 negativeLightDirection,
    float3 halfVector,
    float roughness, float3 F0,
    bool mirrorSurface)
{
    // This is reference implementation of "GGX" specular
    // It's very close to the Disney lighting model implementation

    // Our basic microfacet specular equation is:
    //
    //   D(thetah) * F(thetad) * G(thetal, thetav)
    // ---------------------------------------------
    //            4cos(thetal)cos(thetav)

    // D is our microfacet distribution function
    // F is fresnel
    // G is the shadowing factor (geometric attenuation)

    float NdotL = dot(normal, negativeLightDirection);
    float NdotV = dot(normal, directionToEye);
    float NdotH = dot(normal, halfVector);
    // if (NdotL < 0 || NdotV < 0) return 0.0.xxx;
    NdotL = saturate(NdotL);
    NdotV = saturate(NdotV);
    NdotH = saturate(NdotH);

    /////////// Shadowing factor ///////////
        // As per the Disney model, rescaling roughness to
        // values 0.5f -> 1.f for SmithG alpha, and squaring
    float alphag = roughness*.5+.5;
    alphag *= alphag;
    float G = SmithG(NdotL, alphag) * SmithG(NdotV, alphag);

    /////////// Fresnel ///////////
    float3 F;
    if (!mirrorSurface) {
        F = SchlickFresnelF0(directionToEye, halfVector, F0);
    } else {
        F = SchlickFresnelF0(directionToEye, normal, F0);
    }

    /////////// Microfacet ///////////
        // Mapping alpha to roughness squared (as per Disney
        // model and Filmic worlds implementation)
    float D = TrowReitzD(NdotH, roughness * roughness);

    // note that the NdotL part here is for scaling down the incident light
    // (and so not part of the general BRDF equation)
    return NdotL * (G * D) * F;
}

float3 CalculateSpecular_GGX(
    float3 normal, float3 directionToEye, float3 negativeLightDirection,
    float3 halfVector,
    float roughness, float3 F0, bool mirrorSurface)
{
    float aveF0 = 0.3333f * (F0.r + F0.g + F0.b);

    #if 1 // (FORCE_GGX_REF == 1)
        return ReferenceSpecularGGX(
            normal, directionToEye, negativeLightDirection, halfVector,
            roughness, F0, mirrorSurface);

        return LightingFuncGGX_REF(normal, directionToEye, negativeLightDirection, roughness, aveF0).xxx;
    #endif

    if (!mirrorSurface) {
        return LightingFuncGGX_OPT5(normal, directionToEye, negativeLightDirection, roughness, aveF0).xxx;
    } else {
        return LightingFuncGGX_OPT5_Mirror(normal, directionToEye, negativeLightDirection, roughness, aveF0).xxx;
    }
}

    //////////////////////////////////////////////////////////////////////////
        //   E N T R Y   P O I N T                                      //
    //////////////////////////////////////////////////////////////////////////

struct SpecularParameters
{
    float   roughness;
    float3  F0;
    bool    mirrorSurface;
};

SpecularParameters SpecularParameters_Init(float roughness, float refractiveIndex)
{
    SpecularParameters result;
    result.roughness = roughness;
    result.F0 = RefractiveIndexToF0(refractiveIndex);
    result.mirrorSurface = false;
    return result;
}

SpecularParameters SpecularParameters_RoughF0(float roughness, float3 F0, bool mirrorSurface = false)
{
    SpecularParameters result;
    result.roughness = roughness;
    result.F0 = F0;
    result.mirrorSurface = mirrorSurface;
    return result;
}

float3 CalculateSpecular(
    float3 normal, float3 directionToEye,
    float3 negativeLightDirection, float3 halfVector,
    SpecularParameters parameters)
{
    #if SPECULAR_METHOD==0
        return CalculateSpecular_CookTorrence(
            normal, directionToEye, negativeLightDirection,
            parameters.roughness, parameters.F0).xxx;
    #elif SPECULAR_METHOD==1
        return CalculateSpecular_GGX(
            normal, directionToEye, negativeLightDirection, halfVector,
            parameters.roughness, parameters.F0, parameters.mirrorSurface);
    #endif
}


#endif
