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

struct RangeFogDesc
{
	float3 	Inscatter;
	float3	OpticalThickness;
};

struct LightDesc
{
    float3	Position; 		float	CutoffRange;
	float3	Diffuse; 		float	SourceRadiusX;
	float3	Specular; 		float	SourceRadiusY;
	float3	OrientationX; 	float	DiffuseWideningMin;
	float3	OrientationY; 	float	DiffuseWideningMax;
	float3	OrientationZ; 	uint	Dummy;
};

struct CascadeAddress
{
	float4  frustumCoordinates;
	int     cascadeIndex;
	float4  miniProjection;
};

#endif
