// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)


#include "../Utility/MathConstants.h"
#include "../TextureAlgorithm.h"
#include "../TransformAlgorithm.h"
#include "../CommonResources.h"

cbuffer CircleHighlightParameters
{
	float3 HighlightCenter;
	float Radius;
}

Texture2D_MaybeMS<float>	DepthTexture;
Texture2D					HighlightResource;

float GetLinear0To1Depth(int2 pixelCoords, uint sampleIndex)
{
	float depth = LoadFloat1(DepthTexture, pixelCoords.xy, sampleIndex);
	return NDCDepthToLinear0To1(depth);
}

float3 CalculateWorldPosition(int2 pixelCoords, uint sampleIndex, float3 viewFrustumVector)
{
	float linear0To1Depth = GetLinear0To1Depth(pixelCoords, sampleIndex);
	return CalculateWorldPosition(
		viewFrustumVector, linear0To1Depth, WorldSpaceView);
}


float4 ps_circlehighlight(	float4 position : SV_Position,
							float2 texCoord : TEXCOORD0,
							float3 viewFrustumVector : VIEWFRUSTUMVECTOR,
							SystemInputs sys) : SV_Target0
{
	int2 pixelCoords	= position.xy;

		//	draw a cylindrical highlight around the given position, with
		//	the given radius. The cylinder is always arranged so +Z is up
	float3 worldPosition = CalculateWorldPosition(pixelCoords, GetSampleIndex(sys), viewFrustumVector);

	float2 xyOffset = worldPosition.xy - HighlightCenter.xy;
	float r = length(xyOffset);
	if (r > Radius)
		discard;

	float theta = atan2(xyOffset.y, xyOffset.x);
	const float wrappingCount = 32;

	float a = HighlightResource.Sample(DefaultSampler, float2(theta/(2.f*pi)*wrappingCount, lerp(1.f/128.f, 127/128.f, r/Radius))).r;
	return float4(1.0.xxx*a*.25f, .25f*a);
}


cbuffer RectangleHighlightParameters
{
	float3 Mins, Maxs;
}

float4 ps_rectanglehighlight(	float4 position : SV_Position,
								float2 texCoord : TEXCOORD0,
								float3 viewFrustumVector : VIEWFRUSTUMVECTOR,
								SystemInputs sys) : SV_Target0
{
	int2 pixelCoords	= position.xy;

	float3 worldPosition = CalculateWorldPosition(pixelCoords, GetSampleIndex(sys), viewFrustumVector);

		//	find how this worldPosition relates to the min/max rectangle we're
		//	rendering. The texture we're using was build for circles. But we
		//	can try to map it around the edges of the rectangle, as well.
		//		texCoord.y should be a distance from the edge
		//		texCoord.x should be a parameter for the dotted line

	float distances[4] = 
	{
			// (note -- ignoring z)
		worldPosition.x - Mins.x,
		worldPosition.y - Mins.y,
		Maxs.x - worldPosition.x,
		Maxs.y - worldPosition.y
	};
	float minDist = min(min(min(distances[0], distances[1]), distances[2]), distances[3]);
	if (minDist < 0.f)
		discard;

	texCoord.y = saturate(minDist / 100.f);
	texCoord.x = worldPosition.x + worldPosition.y;

	float a = HighlightResource.Sample(DefaultSampler, texCoord).r;
	return float4(1.0.xxx*a, a);
}

