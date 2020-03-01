// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../../Math/TextureAlgorithm.hlsl"

Texture2D_MaybeMS<float4>	InputTexture;

cbuffer Constants : register(b0)
{
	float4	FilteringWeights0;		// weights for gaussian filter
	float4	FilteringWeights1;
	float4	FilteringWeights2;
}

float4 HorizontalBlur_AlphaWeight(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
	const int offset[] = { -3, -2, -1, 0, 1, 2, 3 };
	const uint sampleCount = 7;

	float FixedWeights[7];
	FixedWeights[0] = FilteringWeights0.x;
	FixedWeights[1] = FilteringWeights0.y;
	FixedWeights[2] = FilteringWeights0.z;
	FixedWeights[3] = FilteringWeights0.w;
	FixedWeights[4] = FilteringWeights1.x;
	FixedWeights[5] = FilteringWeights1.y;
	FixedWeights[6] = FilteringWeights1.z;

	float totalWeight = 0.f;
	float totalFixedWeights = 0.f;
	float4 result = 0.0.xxxx;
	float accumAlpha = 0.f;
	[unroll] for (uint c=0; c<sampleCount; c++) {
		uint2 pixelCoord = uint2(position.xy);
		float4 texSample = LoadFloat4(InputTexture, pixelCoord + int2(offset[c], 0), 0);
		float weight = texSample.a * FixedWeights[c];
		totalWeight += weight;
		result.rgba += texSample.rgba * weight;

		totalFixedWeights += FixedWeights[c];
		accumAlpha += texSample.a * FixedWeights[c];
	}

	if (totalWeight > 0.0001f && totalFixedWeights > 0.0001f) {
		result.rgba /= totalWeight;
		result.a = accumAlpha / totalFixedWeights;
	} else {
		result.rgba = 0.0.xxxx;
	}
	return result;
}

float4 VerticalBlur_AlphaWeight(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
	const int offset[]		= { -3, -2, -1, 0, 1, 2, 3 };
	const uint sampleCount	= 7;

	float FixedWeights[7];
	FixedWeights[0] = FilteringWeights0.x;
	FixedWeights[1] = FilteringWeights0.y;
	FixedWeights[2] = FilteringWeights0.z;
	FixedWeights[3] = FilteringWeights0.w;
	FixedWeights[4] = FilteringWeights1.x;
	FixedWeights[5] = FilteringWeights1.y;
	FixedWeights[6] = FilteringWeights1.z;

	float totalWeight		= 0.f;
	float totalFixedWeights = 0.f;
	float4 result			= 0.0.xxxx;
	float accumAlpha		= 0.f;
	[unroll] for (uint c=0; c<sampleCount; c++) {
		uint2 pixelCoord = uint2(position.xy);
		float4 texSample = LoadFloat4(InputTexture, pixelCoord + int2(0, offset[c]), 0);
		float weight = texSample.a * FixedWeights[c];
		totalWeight += weight;
		result.rgba += texSample.rgba * weight;

		totalFixedWeights += FixedWeights[c];
		accumAlpha += texSample.a * FixedWeights[c];
	}

	if (totalWeight > 0.0001f && totalFixedWeights > 0.0001f) {
		result.rgba /= totalWeight;
		result.a = accumAlpha / totalFixedWeights;
	} else {
		result.rgba = 0.0.xxxx;
	}
	return result;
}


float4 HorizontalBlur(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
	const int offset[] = { -3, -2, -1, 0, 1, 2, 3 };
	const uint sampleCount = 7;

	float FixedWeights[7];
	FixedWeights[0] = FilteringWeights0.x;
	FixedWeights[1] = FilteringWeights0.y;
	FixedWeights[2] = FilteringWeights0.z;
	FixedWeights[3] = FilteringWeights0.w;
	FixedWeights[4] = FilteringWeights1.x;
	FixedWeights[5] = FilteringWeights1.y;
	FixedWeights[6] = FilteringWeights1.z;

	int2 inputDims;
	#if MSAA_SAMPLERS != 0
		int ignore;
		InputTexture.GetDimensions(inputDims.x, inputDims.y, ignore);
	#else
		InputTexture.GetDimensions(inputDims.x, inputDims.y);
	#endif

	float2 outputDims = position.xy / texCoord.xy;
	float2 coordScale = float2(inputDims.xy) / outputDims.xy;

	float4 result = 0.0.xxxx;
	uint2 pixelCoord = uint2(position.xy);
	[unroll] for (uint c=0; c<sampleCount; c++) {
		uint2 p;	// clamp may distort the blur around the edges, but it's the cheapest option
					// note that the "+ 0.5f" here to to deal with round errors float -> uint
		p.x = uint(max(0, min(inputDims.x-1, coordScale.x * (pixelCoord.x + offset[c]))) + .5f);
		p.y = uint(coordScale.y * pixelCoord.y + .5f);
		float4 texSample = LoadFloat4(InputTexture, p, 0);
		result.rgba += texSample.rgba * FixedWeights[c];
	}
	return result;
}

float4 VerticalBlur(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
	const int offset[]		= { -3, -2, -1, 0, 1, 2, 3 };
	const uint sampleCount	= 7;

	float FixedWeights[7];
	FixedWeights[0] = FilteringWeights0.x;
	FixedWeights[1] = FilteringWeights0.y;
	FixedWeights[2] = FilteringWeights0.z;
	FixedWeights[3] = FilteringWeights0.w;
	FixedWeights[4] = FilteringWeights1.x;
	FixedWeights[5] = FilteringWeights1.y;
	FixedWeights[6] = FilteringWeights1.z;

	int2 inputDims;
	#if MSAA_SAMPLERS != 0
		int ignore;
		InputTexture.GetDimensions(inputDims.x, inputDims.y, ignore);
	#else
		InputTexture.GetDimensions(inputDims.x, inputDims.y);
	#endif

	float2 outputDims = position.xy / texCoord.xy;
	float2 coordScale = float2(inputDims.xy) / outputDims.xy;

	float4 result = 0.0.xxxx;
	int2 pixelCoord = uint2(position.xy);
	[unroll] for (uint c=0; c<sampleCount; c++) {
		uint2 p;	// clamp may distort the blur around the edges, but it's the cheapest option
					// note that the "+ 0.5f" here to to deal with round errors float -> uint
		p.x = uint(coordScale.x * pixelCoord.x + .5f);
		p.y = uint(max(0, min(inputDims.y-1, coordScale.y * (pixelCoord.y+offset[c]))) + .5f);
		float4 texSample = LoadFloat4(InputTexture, p, 0);
		result.rgba += texSample.rgba * FixedWeights[c];
	}
	return result;
}

cbuffer ClampingWindow : register(b1) { float2 clampingMins; float2 clampingMaxs; }

float4 HorizontalBlur11(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
	const int offset[] = { -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5 };
	const uint sampleCount = 11;

	float FixedWeights[11];
	FixedWeights[ 0] = FilteringWeights0.x;
	FixedWeights[ 1] = FilteringWeights0.y;
	FixedWeights[ 2] = FilteringWeights0.z;
	FixedWeights[ 3] = FilteringWeights0.w;
	FixedWeights[ 4] = FilteringWeights1.x;
	FixedWeights[ 5] = FilteringWeights1.y;
	FixedWeights[ 6] = FilteringWeights1.z;
	FixedWeights[ 7] = FilteringWeights1.w;
	FixedWeights[ 8] = FilteringWeights2.x;
	FixedWeights[ 9] = FilteringWeights2.y;
	FixedWeights[10] = FilteringWeights2.z;

	int2 inputDims;
	#if MSAA_SAMPLERS != 0
		int ignore;
		InputTexture.GetDimensions(inputDims.x, inputDims.y, ignore);
	#else
		InputTexture.GetDimensions(inputDims.x, inputDims.y);
	#endif

	float2 outputDims = position.xy / texCoord.xy;
	float2 coordScale = float2(inputDims.xy) / outputDims.xy;

	float4 result = 0.0.xxxx;
	int2 pixelCoord = int2(position.xy);
	[unroll] for (uint c=0; c<sampleCount; c++) {
		uint2 p;	// clamp may distort the blur around the edges, but it's the cheapest option
		float unclamped = coordScale.x * (pixelCoord.x+offset[c]);
		#if USE_CLAMPING_WINDOW==1
			p.x = uint(max(clampingMins.x, min(clampingMaxs.x, unclamped)) + 0.5f);
		#else
			p.x = uint(max(0, min(inputDims.x-1, unclamped)) + 0.5f);
		#endif
		p.y = pixelCoord.y;
		float4 texSample = LoadFloat4(InputTexture, p, 0);
		result.rgba += texSample.rgba * FixedWeights[c];
	}
	return result;
}

float4 VerticalBlur11(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
	const int offset[] = { -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5 };
	const uint sampleCount = 11;

	float FixedWeights[11];
	FixedWeights[ 0] = FilteringWeights0.x;
	FixedWeights[ 1] = FilteringWeights0.y;
	FixedWeights[ 2] = FilteringWeights0.z;
	FixedWeights[ 3] = FilteringWeights0.w;
	FixedWeights[ 4] = FilteringWeights1.x;
	FixedWeights[ 5] = FilteringWeights1.y;
	FixedWeights[ 6] = FilteringWeights1.z;
	FixedWeights[ 7] = FilteringWeights1.w;
	FixedWeights[ 8] = FilteringWeights2.x;
	FixedWeights[ 9] = FilteringWeights2.y;
	FixedWeights[10] = FilteringWeights2.z;

	int2 inputDims;
	#if MSAA_SAMPLERS != 0
		int ignore;
		InputTexture.GetDimensions(inputDims.x, inputDims.y, ignore);
	#else
		InputTexture.GetDimensions(inputDims.x, inputDims.y);
	#endif

	float2 outputDims = position.xy / texCoord.xy;
	float2 coordScale = 1.f; // float2(inputDims.xy) / outputDims.xy;

	float4 result = 0.0.xxxx;
	int2 pixelCoord = int2(position.xy);
	[unroll] for (uint c=0; c<sampleCount; c++) {
		uint2 p;	// clamp may distort the blur around the edges, but it's the cheapest option
		p.x = pixelCoord.x;
		float unclamped = coordScale.y * (pixelCoord.y+offset[c]);
		#if USE_CLAMPING_WINDOW==1
			p.y = uint(max(clampingMins.y, min(clampingMaxs.y, unclamped)) + 0.5f);
		#else
			p.y = uint(max(0, min(inputDims.y-1, unclamped)) + 0.5f);
		#endif
		float4 texSample = LoadFloat4(InputTexture, p, 0);
		result.rgba += texSample.rgba * FixedWeights[c];
	}
	return result;
}

float4 SingleStepDownSample(float4 position : SV_Position, float2 texCoord : TEXCOORD0) : SV_Target0
{
	uint2 pixelCoord = uint2(2.f*position.xy);
	float4 input00 = LoadFloat4(InputTexture, pixelCoord + uint2(0, 0), 0);
	float4 input01 = LoadFloat4(InputTexture, pixelCoord + uint2(0, 1), 0);
	float4 input10 = LoadFloat4(InputTexture, pixelCoord + uint2(1, 0), 0);
	float4 input11 = LoadFloat4(InputTexture, pixelCoord + uint2(1, 1), 0);
	return (input00 + input01 + input10 + input11) * 0.25f;
}
