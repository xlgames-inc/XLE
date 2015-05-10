// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(LIGHT_DESC_H)
#define LIGHT_DESC_H

struct AmbientDesc
{
	float3	Colour;
	float 	SkyReflectionScale;
	float	SkyReflectionBlurriness;
	float	Dummy0, Dummy1, Dummy2;
};

struct LightColors
{
	float3 diffuse;
	float3 specular;
	float nonMetalSpecularBrightness;
};

struct LightDesc
{
    float3 		NegativeDirection;
	float		Radius;
	LightColors Color;
	float		Power;
	float		DiffuseWideningMin;
	float		DiffuseWideningMax;
	float		Dummy;
};

#endif
