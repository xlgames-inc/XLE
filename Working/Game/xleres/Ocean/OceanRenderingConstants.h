// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(OCEAN_RENDERING_CONSTANTS_H)
#define OCEAN_RENDERING_CONSTANTS_H

cbuffer OceanRenderingConstants
{
    uint GridWidth, GridHeight;
    float SpectrumFade;
    uint UseScreenSpaceGrid;
    float DetailNormalsStrength;
    float GridShift;
    row_major float4x4 WorldToReflection;
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

#endif
