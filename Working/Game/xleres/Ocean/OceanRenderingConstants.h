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
    float3		OpticalThickness;
    float		FoamBrightness;
    float		SkyReflectionBrightness;
    float		UpwellingScale;
    float		RefractiveIndex					= 1.333f;		// refractive index for pure water should be about 1.333f
    float		ReflectionBumpScale;
    float		DetailNormalFrequency;
    float		SpecularityFrequency;
    float		MatSpecularMin;
    float		MatSpecularMax;
    float		MatRoughness;
}

#endif
