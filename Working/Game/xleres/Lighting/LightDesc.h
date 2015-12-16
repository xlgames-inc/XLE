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

///////////////////////////////////////////////////////////////////////////////////////////////////
	//   structures used by resolvers...

struct CascadeAddress
{
	float4  frustumCoordinates;
	int     cascadeIndex;
	float4  miniProjection;
};

struct LightScreenDest
{
    int2 pixelCoords;
    uint sampleIndex;
};

struct LightSampleExtra
{
    float screenSpaceOcclusion;
};

LightScreenDest LightScreenDest_Create(int2 pixelCoords, uint sampleIndex)
{
	LightScreenDest result;
	result.pixelCoords = pixelCoords;
	result.sampleIndex = sampleIndex;
	return result;
}

CascadeAddress CascadeAddress_Invalid()
{
	CascadeAddress result;
	result.cascadeIndex = -1;
	return result;
}

CascadeAddress CascadeAddress_Create(float4 frustumCoordinates, int cascadeIndex, float4 miniProjection)
{
	CascadeAddress result;
	result.cascadeIndex = cascadeIndex;
	result.frustumCoordinates = frustumCoordinates;
	result.miniProjection = miniProjection;
	return result;
}

#endif
