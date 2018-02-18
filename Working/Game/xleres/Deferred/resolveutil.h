// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(RESOLVE_UTIL_H)
#define RESOLVE_UTIL_H

#include "../TextureAlgorithm.h"		// for LoadFloat1
#include "../TransformAlgorithm.h"		// float NDC->linear conversions
#include "../Binding.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////

Texture2D_MaybeMS<float>	DepthTexture	 	BIND_NUMERIC_T4;

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
	float depth = GetLinear0To1Depth(pixelCoords, sampleIndex);
	// if (depth >= 1.f) clip(-1); // maybe would be ideal to discard these pixels with stencil buffer
	return CalculateWorldPosition(viewFrustumVector, depth, WorldSpaceView);
}

#endif
