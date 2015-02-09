// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(RESOLVE_UTIL_H)
#define RESOLVE_UTIL_H

#include "../gbuffer.h"
#include "../Utility/MathConstants.h"
#include "../Colour.h"
#include "../TextureAlgorithm.h"
#include "../System/LoadGBuffer.h"
#include "../Lighting/LightingAlgorithm.h"
#include "../Lighting/SpecularMethods.h"
#include "../TransformAlgorithm.h"


cbuffer MaterialOverride : register(b9)
{
	float MO_Metallic;
	float MO_Roughness;
	float MO_Specular;
	float MO_Specular2;
	float MO_Material;
	float MO_Material2;

	float MO_DiffuseScale;
	float MO_ReflectionsScale;
	float MO_ReflectionsBoost;
	float MO_Specular0Scale;
	float MO_Specular1Scale;
}

#if 0
	float GetF0_0()	{ return lerp(0.0f, 0.2f, MO_Specular); }
	float GetF0_1()	{ return lerp(0.0f, 0.2f, MO_Specular2); }
#else
		//	this method is a little more expensive if we do the index-of-refraction -> F0
		//	conversion in the shader. But there is more control at lower levels of the "specular" parameter
	float GetF0_0()	{ return RefractiveIndexToF0(lerp(1.0f, 2.5f, MO_Specular)); }
	float GetF0_1()	{ return RefractiveIndexToF0(lerp(1.0f, 2.5f, MO_Specular2)); }
#endif

float3 GetMaterialColor(float materialValue)
{
		// here's some sample specular colours we can use to get an ideal of the model
		//		source:
		//	http://seblagarde.wordpress.com/2011/08/17/feeding-a-physical-based-lighting-mode/
	float3 MaterialTable[] =
	{
	float3(0.971519f, 0.959915f, 0.915324f),  /*Silver      */
	float3(0.913183f, 0.921494f, 0.924524f),  /*Aluminium   */
	float3(1.0f,      0.765557f, 0.336057f),  /*Gold        */
	float3(0.955008f, 0.637427f, 0.538163f),  /*Copper      */

	float3(0.549585f, 0.556114f, 0.554256f),  /*Chromium    */
	float3(0.659777f, 0.608679f, 0.525649f),  /*Nickel      */
	float3(0.541931f, 0.496791f, 0.449419f),  /*Titanium    */
	float3(0.662124f, 0.654864f, 0.633732f),  /*Cobalt      */

	float3(0.672411f, 0.637331f, 0.585456f)	  /*Platinum    */
	};

	float a = materialValue * 8.f;	// (table dim - 1)
	uint i0 = uint(a);
	uint i1 = min(8, i0+1);

	return lerp(MaterialTable[i0], MaterialTable[i1], frac(a));
}

float3 GetSpecularColor0()
{
	return GetMaterialColor(MO_Material);
	// return SRGBToLinear(float3(1.f, 0.765557f, 0.336057f));
	// return SRGBToLinear(float3(0.955008f, 0.637427f, 0.538163f));
	// return SRGBToLinear(float3(0.955008f, 0.637427f, 0.538163));

//	return lerp(
//		SRGBToLinear(float3(0.541931f, 0.496791f, 0.449419)),
//		SRGBToLinear(float3(0.955008f, 0.637427f, 0.538163)),
//		MO_Material);
}

float3 GetSpecularColor1()
{
	return GetMaterialColor(MO_Material2);
	// return SRGBToLinear(float3(0.5f, 0.557f, 0.876057f));
	// return SRGBToLinear(float3(1.f, 0.865557f, 0.536057f));
	// return SRGBToLinear(float3(0.955008f, 0.637427f, 0.538163f));
	// return SRGBToLinear(float3(0.955008f, 0.637427f, 0.538163));
}

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

cbuffer LightBuffer
{
	float3 	NegativeLightDirection;
	float	LightRadius;
	float3	LightColour;
	float	LightPower;
}

float3 CalculateDiffuse(GBufferValues sample, float3 negativeLightDirection, float3 lightColor)
{
	float light = MO_DiffuseScale * saturate(dot(negativeLightDirection, sample.worldSpaceNormal.xyz));
	return light * lightColor * sample.diffuseAlbedo.rgb;
}

float3 CalculateSpecular(	GBufferValues sample,
							float3 viewDirection,
							float3 negativeLightDirection,
							float3 lightColor)
{
	// float refractiveIndex0 = GetRefractiveIndex0();
	// float refractiveIndex1 = GetRefractiveIndex1();
	float F0_0 = GetF0_0();
	float F0_1 = GetF0_1();
	float3 specularColor0 = GetSpecularColor0();
	float3 specularColor1 = GetSpecularColor1();

	float roughnessValue = MO_Roughness;

		// scale roughness value by the "reflectivity" value in the gbuffer.
		// this is actually scratchiness -- when it's high, roughness should be increased
	roughnessValue = saturate(roughnessValue + .5f * (1.0f - sample.reflectivity));
	SpecularParameters param0 = SpecularParameters_RoughF0(roughnessValue, F0_0);
	SpecularParameters param1 = SpecularParameters_RoughF0(3.f * roughnessValue, F0_1);

	float spec0 = MO_Specular0Scale * CalculateSpecular(sample.worldSpaceNormal, viewDirection, negativeLightDirection, param0);
	float spec1 = MO_Specular1Scale * CalculateSpecular(sample.worldSpaceNormal, viewDirection, negativeLightDirection, param1);
	float scale = sample.cookedAmbientOcclusion;
	float3 result = (saturate(spec0) * scale) * lerp(lightColor, specularColor0, MO_Metallic);
	result += (saturate(spec1) * scale) * specularColor1;
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
