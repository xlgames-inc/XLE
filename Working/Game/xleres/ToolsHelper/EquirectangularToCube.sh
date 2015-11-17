// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../Utility/MathConstants.h"

Texture2D Input;
SamplerState DefaultSampler;

float2 EquirectangularMappingCoord(float3 direction, bool hemi)
{
		// note -- 	the trigonometry here is a little inaccurate. It causes shaking
		//			when the camera moves. We might need to replace it with more
		//			accurate math.
	float theta = atan2(direction.x, direction.y);
	float inc = atan(direction.z * rsqrt(dot(direction.xy, direction.xy)));

	float x = 0.5f + 0.5f*(theta / (1.f*pi));
	float y = .5f-(inc / pi);
	if (hemi)
		y = saturate(1.f-(inc / (.5f*pi)));
	return float2(x, y);
}

void Panel(inout float4 result, float2 tc, float2 tcMins, float2 tcMaxs, float3 panel[3], bool hemi)
{
    float3 plusX = panel[0];
    float3 plusY = panel[1];
    float3 center = panel[2];

    if (    tc.x >= tcMins.x && tc.y >= tcMins.y
        &&  tc.x <  tcMaxs.x && tc.y <  tcMaxs.y) {

        tc.x = 2.0f * (tc.x - tcMins.x) / (tcMaxs.x - tcMins.x) - 1.0f;
        tc.y = 2.0f * (tc.y - tcMins.y) / (tcMaxs.y - tcMins.y) - 1.0f;

        float3 finalDirection = center + plusX * tc.x + plusY * tc.y;
        float2 finalCoord = EquirectangularMappingCoord(normalize(finalDirection), hemi);

		// note -- 	There isn't a 1:1 relationship between input pixel and output
		// 			pixel. The solid angle of the cubemap pixel and the solid
		//			angle of the equirectangular input pixel changes...!
		//			We can a better result by sampling in a regular pattern over
		//			the full range of the output pixel...
		//			Note that we could build mipmaps in this way as well. it
		//			should give a correctly balanced result, and it would mean
		//			that the mip maps are built from the original texture, not
		//			some derived texture
        result.rgb = Input.SampleLevel(DefaultSampler, finalCoord, 0);
        result.a = 1.f;
    }
}

static const float3 HorizontalPanels[6][3] =
{
    { float3(0,0,1), float3(0,1,0), float3(-1,0,0) },
    { float3(1,0,0), float3(0,1,0), float3(0,0,1) },
    { float3(0,0,-1), float3(0,1,0), float3(1,0,0) },
    { float3(-1,0,0), float3(0,1,0), float3(0,0,-1) },

    { float3(1,0,0), float3(0,0,1), float3(0,-1,0) },
    { float3(1,0,0), float3(0,0,-1), float3(0,1,0) }
};

static const float3 VerticalPanels[6][3] =
{
	{ float3(1,0,0), float3(0,0,1), float3(0,1,0) },
    { float3(1,0,0), float3(0,-1,0), float3(0,0,1) },
	{ float3(1,0,0), float3(0,0,-1), float3(0,-1,0) },
    { float3(1,0,0), float3(0,1,0), float3(0,0,-1) },

	{ float3(0,0,1), float3(0,-1,0), float3(-1,0,0) },
	{ float3(0,0,-1), float3(0,-1,0), float3(1,0,0) }
};

float4 Horizontal(float2 texCoord, bool hemi)
{
	float4 result = 0.0.xxxx;
	Panel(
		result,
		texCoord,
		float2(0.0f, 1.0f/3.0f), float2(1.0f/4.0f, 2.0f/3.0f),
		HorizontalPanels[0], hemi);

	Panel(
		result,
		texCoord,
		float2(1.0f/4.0f, 1.0f/3.0f), float2(2.0f/4.0f, 2.0f/3.0f),
		HorizontalPanels[1], hemi);

	Panel(
		result,
		texCoord,
		float2(2.0f/4.0f, 1.0f/3.0f), float2(3.0f/4.0f, 2.0f/3.0f),
		HorizontalPanels[2], hemi);

	Panel(
		result,
		texCoord,
		float2(3.0f/4.0f, 1.0f/3.0f), float2(1.0f, 2.0f/3.0f),
		HorizontalPanels[3], hemi);

	Panel(
		result,
		texCoord,
		float2(1.0f/4.0f, 0.0f), float2(2.0f/4.0f, 1.0f/3.0f),
		HorizontalPanels[4], hemi);

	Panel(
		result,
		texCoord,
		float2(1.0f/4.0f, 2.0f/3.0f), float2(2.0f/4.0f, 1.0f),
		HorizontalPanels[5], hemi);
	return result;
}

float4 Vertical(float2 texCoord, bool hemi)
{
	float4 result = 0.0.xxxx;
	Panel(
		result,
		texCoord,
		float2(1.0f/3.0f, 0.0f), float2(2.0f/3.0f, 1.0f/4.0f),
		VerticalPanels[0], hemi);

	Panel(
		result,
		texCoord,
		float2(1.0f/3.0f, 1.0f/4.0f), float2(2.0f/3.0f, 2.0f/4.0f),
		VerticalPanels[1], hemi);

	Panel(
		result,
		texCoord,
		float2(1.0f/3.0f, 2.0f/4.0f), float2(2.0f/3.0f, 3.0f/4.0f),
		VerticalPanels[2], hemi);

	Panel(
		result,
		texCoord,
		float2(1.0f/3.0f, 3.0f/4.0f), float2(2.0f/3.0f, 1.0f),
		VerticalPanels[3], hemi);

	Panel(
		result,
		texCoord,
		float2(0.0f, 1.0f/4.0f), float2(1.0f/3.0f, 2.0f/4.0f),
		VerticalPanels[4], hemi);

	Panel(
		result,
		texCoord,
		float2(2.0f/3.0f, 1.0f/4.0f), float2(1.0f, 2.0f/4.0f),
		VerticalPanels[5], hemi);
	return result;
}

float4 main(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
    uint2 dims = uint2(position.xy / texCoord);
    if (dims.x >= dims.y) {
		return Horizontal(texCoord, false);
    } else {
		return Vertical(texCoord, false);
    }
}

float4 hemi(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
	uint2 dims = uint2(position.xy / texCoord);
	if (dims.x >= dims.y) {
		return Horizontal(texCoord, true);
	} else {
		return Vertical(texCoord, true);
	}
}
