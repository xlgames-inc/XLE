// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(TRANSFORM_ALGORITHM_H)
#define TRANSFORM_ALGORITHM_H

#include "Transform.h"

float NDCDepthToLinearDepth(float clipSpaceDepth, float2 projRatio)
{
	return projRatio.y / (clipSpaceDepth - projRatio.x);		// undo projection matrix transformation
}

float NDCDepthToLinearDepth_Ortho(float clipSpaceDepth, float2 projRatio)
{
    return (clipSpaceDepth - projRatio.y) / projRatio.x;
}

float NDCDepthDifferenceToLinear_Ortho(float clipSpaceDepthDifference, float2 projRatio)
{
    return clipSpaceDepthDifference / projRatio.x;
}

float LinearDepthToNDCDepth(float worldSpaceDepth, float2 projRatio)
{
	return (projRatio.y / worldSpaceDepth) + projRatio.x;
}

float LinearDepthToNDCDepth_Ortho(float worldSpaceDepth, float2 projRatio)
{
	return (worldSpaceDepth * projRatio.x) + projRatio.y;
}

float LinearDepthDifferenceToNDC_Ortho(float worldSpaceDepthDifference, float2 projRatio)
{
	return worldSpaceDepthDifference * projRatio.x;
}

float NDCDepthToLinearDepth(float clipSpaceDepth)
{
	return NDCDepthToLinearDepth(clipSpaceDepth, DepthProjRatio);
}

float LinearDepthToNDCDepth(float worldSpaceDepth)
{
	return LinearDepthToNDCDepth(worldSpaceDepth, DepthProjRatio);
}

float3 CalculateWorldPosition(
    float3 viewFrustumVector, float linearDepth,
    float nearClip, float farClip, float3 viewPosition)
{
    return viewPosition 
        + viewFrustumVector * (farClip / nearClip * linearDepth)
        ;
}

#endif
