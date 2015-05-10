// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(RESOLVE_UTIL_H)
#define RESOLVE_UTIL_H

#include "../Utility/MathConstants.h"
#include "../TextureAlgorithm.h"		// for LoadFloat1
#include "../TransformAlgorithm.h"		// float NDC->linear conversions

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
