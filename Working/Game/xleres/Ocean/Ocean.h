// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(OCEAN_H)
#define OCEAN_H

cbuffer OceanRenderingConstants
{
	uint GridWidth, GridHeight;
	float SpectrumFade;
	uint UseScreenSpaceGrid;
    float DetailNormalsStrength;
    float GridShift;
    row_major float4x4 WorldToReflection;
}

cbuffer OceanMaterialSettings
{
        // these settings are also used while simulating the
        // shallow water
    float PhysicalWidth, PhysicalHeight;
    float StrengthConstantXY;
    float StrengthConstantZ;
    float ShallowGridPhysicalDimension;
    float WaterBaseHeight;

	float FoamThreshold = .3f;
	float FoamIncreaseSpeed = 8.f / .33f;
	float FoamIncreaseClamp = 8.f;
	int FoamDecrease = 1;
}

cbuffer OceanLightingSettings
{
	float3		SpecularReflectionBrightness	= 1.f * float3(1.f, 0.775f, 0.65f);
	float		FoamBrightness					= 1.5f;
	float3		OpticalThickness				= float3(0.15f, 0.075f, 0.05f);
	float		SkyReflectionBrightness			= 1.f;
	float		SpecularPower					= 256.f;
	float		UpwellingScale					= .25f;
	float		RefractiveIndex					= 1.333f;		// refractive index for pure water should be about 1.333f
	float		ReflectionBumpScale				= 0.1f;
	float		DetailNormalFrequency			= 6.727f;
	float		SpecularityFrequency			= 1.f;
	float		MatSpecularMin					= .3f;
	float		MatSpecularMax					= .6f;
	float		MatRoughness					= .15f;
}

// #define DO_FREQ_BOOST 1
#if DO_FREQ_BOOST==1
    static const float StrengthConstantMultiplier = 4.f * 1.0f / 512.f; //256.f / 2.f;
#else
    static const float StrengthConstantMultiplier = 1.0f / 512.f; // 256.f / 2.f;
#endif

float OceanTextureCustomInterpolate(Texture2D<float> inputTex, uint2 texDimensions, float2 textureCoords)
{
		//	we need a custom interpolation, because each grid cell changes
		//	sign in a chess-board pattern
	textureCoords = frac(textureCoords);
    float2 explodedCoords = textureCoords * float2(texDimensions);
	uint2 baseCoords = uint2(explodedCoords);
	float sample00 = inputTex[(baseCoords + uint2(0,0))%texDimensions];
	float sample10 = inputTex[(baseCoords + uint2(1,0))%texDimensions];
	float sample01 = inputTex[(baseCoords + uint2(0,1))%texDimensions];
	float sample11 = inputTex[(baseCoords + uint2(1,1))%texDimensions];

	if (((baseCoords.x + baseCoords.y)&1)==1) {
		sample00 = -sample00;
		sample11 = -sample11;
	} else {
		sample01 = -sample01;
		sample10 = -sample10;
	}

	float2 fractionalPart = frac(explodedCoords);
    #if 1
	    float weight00 = (1.f - fractionalPart.x) * (1.f - fractionalPart.y);
	    float weight01 = (1.f - fractionalPart.x) * fractionalPart.y;
	    float weight10 = fractionalPart.x * (1.f - fractionalPart.y);
	    float weight11 = fractionalPart.x * fractionalPart.y;
    #else
        float weight00 = 1.f;
	    float weight01 = 0.f;
	    float weight10 = 0.f;
	    float weight11 = 0.f;
    #endif

	return	sample00 * weight00
		+	sample01 * weight01
		+	sample10 * weight10
		+	sample11 * weight11
		;
}

#endif
